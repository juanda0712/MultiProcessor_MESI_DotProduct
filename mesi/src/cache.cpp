// Implementación de las operaciones internas de la caché que participan en el protocolo MESI.
#include <vector>
#include <array>
#include <optional>
#include <iostream>
#include <cstdint>

#include "core.cpp"
#include "fsm.cpp"

class Interconnect; struct MainMemory; struct SnoopMessage;
struct CacheLine { bool valid{false}; bool dirty{false}; uint64_t tag{0}; MESI state{MESI::I}; std::array<uint64_t,WORDS_PER_LINE> data{}; };
struct PLRU { int bit{0}; int pick(){int v=bit; bit^=1; return v;} void touch(int way){bit=1-way;} };

class Cache {
public:
	Cache(int id, Interconnect* ic, MainMemory* mem);
	int id() const;
	uint64_t cpu_load(uint64_t addr);
	void     cpu_store(uint64_t addr, uint64_t data);
	struct SnoopRet { bool hit=false; bool hitM=false; bool invalidated=false; std::array<uint64_t,WORDS_PER_LINE> wb{}; };
	SnoopRet on_snoop(const SnoopMessage& sm);
	std::optional<std::array<uint64_t,WORDS_PER_LINE>> evict_if_M_and_get_line(uint64_t base);
	void dump_state();
private:
	struct WayRef { int set=-1; int way=-1; CacheLine* cl=nullptr; };
	WayRef lookup(uint64_t base);
	WayRef find_free_or_victim(uint64_t base, std::optional<uint64_t>& evict_base, std::optional<std::array<uint64_t,WORDS_PER_LINE>>& evict_line, bool& was_M);
	void install(uint64_t base, const std::array<uint64_t,WORDS_PER_LINE>& line, MESI st, bool dirty=false);
	int id_;
	Interconnect* ic_;
	MainMemory* mem_;
	std::vector<std::array<CacheLine,2>> sets_;
	std::vector<PLRU> lru_;
	uint64_t hits_{0}, misses_{0}, trans_{0};
};


#define MESI_HAVE_CACHE_DECL
#include "interconnect.cpp"

static constexpr size_t NUM_SETS  = 16;
static constexpr size_t NUM_WAYS  = 2;



// lookup
// - Busca en el conjunto correspondiente a la base de línea una entrada válida
//   cuya etiqueta coincida con `base` y cuyo estado no sea inválido.
// - Parámetros:
//    - base: dirección base de la línea buscada.
// - Retorno: `WayRef` con set/way y puntero a la línea si se encuentra;
//   de lo contrario un `WayRef` vacío.
Cache::WayRef Cache::lookup(uint64_t base){ size_t set = line_index(base) % NUM_SETS; for (int w=0; w<NUM_WAYS; ++w){ CacheLine& cl = sets_[set][w]; if (cl.valid && cl.tag==base && cl.state!=MESI::I){ return WayRef{(int)set, w, &cl}; } } return WayRef{}; }

// evict_if_M_and_get_line
// - Si la línea indicada por `base` existe y está en estado M, marca la
//   línea como invalidada y devuelve su contenido para que el interconnect
//   pueda escribirla en memoria (writeback). En caso contrario retorna
//   std::nullopt.
std::optional<std::array<uint64_t,WORDS_PER_LINE>> Cache::evict_if_M_and_get_line(uint64_t base){ WayRef wr = lookup(base); if (wr.cl && wr.cl->state==MESI::M){ auto line = wr.cl->data; wr.cl->dirty=false; wr.cl->valid=true; wr.cl->state=MESI::I; return line; } return std::nullopt; }

// find_free_or_victim
// - Localiza una vía libre en el conjunto correspondiente o selecciona una
//   víctima según la política PLRU. Si la víctima estaba en estado M, se
//   devuelve su base y su contenido a través de `evict_base` y `evict_line`.
// - Parámetros:
//    - base: base de línea que se desea instalar.
//    - evict_base: salida opcional con la base de la línea evictada.
//    - evict_line: salida opcional con los datos de la línea evictada.
//    - was_M: salida booleana que indica si la víctima estaba en estado M.
Cache::WayRef Cache::find_free_or_victim(uint64_t base, std::optional<uint64_t>& evict_base, std::optional<std::array<uint64_t,WORDS_PER_LINE>>& evict_line, bool& was_M){ size_t set = line_index(base) % NUM_SETS; for (int w=0; w<NUM_WAYS; ++w){ CacheLine& cl = sets_[set][w]; if (!cl.valid || cl.state==MESI::I){ return WayRef{(int)set, w, &cl}; } } int vic = lru_[set].pick(); CacheLine& v = sets_[set][vic]; evict_base.reset(); evict_line.reset(); was_M = false; if (v.valid && v.state==MESI::M){ was_M = true; evict_base = v.tag; evict_line = v.data; } v.valid = false; v.dirty=false; v.state = MESI::I; return WayRef{(int)set, vic, &v}; }

// install
// - Instala una línea en la caché en la posición determinada por
//   `find_free_or_victim`. Actualiza flags (valid, dirty), la etiqueta y el
//   estado MESI, así como la estructura de reemplazo LRU.
void Cache::install(uint64_t base, const std::array<uint64_t,WORDS_PER_LINE>& line, MESI st, bool dirty){ std::optional<uint64_t> ebase; std::optional<std::array<uint64_t,WORDS_PER_LINE>> eline; bool wasM=false; WayRef w = find_free_or_victim(base, ebase, eline, wasM); if (w.cl){ w.cl->valid = true; w.cl->dirty = dirty; w.cl->tag = base; w.cl->data = line; w.cl->state = st; lru_[w.set].touch(w.way); } }

// cpu_load
// - Acción provocada por la CPU para leer una dirección. Comportamiento:
//    1. Busca la línea en caché (lookup). Si hay acierto, devuelve la palabra.
//    2. Si hay fallo, prepara la posible evicción (si la víctima estaba en M
//       llama a `writeback_from_evict` del interconnect), solicita BusRd al
//       interconnect y finalmente instala la línea recibida.
// - Retorno: el valor de la palabra solicitada.
uint64_t Cache::cpu_load(uint64_t addr){ uint64_t base = line_base(addr); WayRef w = lookup(base); size_t word_off = ((addr - base) / WORD_SIZE) % WORDS_PER_LINE; if (w.cl){ hits_++; return w.cl->data[word_off]; } misses_++; std::optional<uint64_t> ebase; std::optional<std::array<uint64_t,WORDS_PER_LINE>> eline; bool wasM=false; find_free_or_victim(base, ebase, eline, wasM); if (ebase && wasM){ ic_->writeback_from_evict(*ebase, *eline); } BusRequest br{BusCmd::BusRd, base, id_}; BusResponse resp = ic_->process(br); install(base, resp.rline, resp.shared ? MESI::S : MESI::E, false); trans_++; WayRef w2 = lookup(base); return w2.cl->data[word_off]; }

// cpu_store
// - Acción provocada por la CPU para escribir en una dirección. Comportamiento:
//    1. Si la línea está presente y en estado S/E/M: actualiza estado y datos
//       adecuadamente (posible Upgrade si estaba en S).
//    2. Si hay fallo, prepara la evicción (si procede) y solicita BusRdX al
//       interconnect para obtener exclusividad; instala la línea en estado M y
//       escribe el dato.
void Cache::cpu_store(uint64_t addr, uint64_t data){ uint64_t base = line_base(addr); WayRef w = lookup(base); size_t word_off = ((addr - base)/WORD_SIZE) % WORDS_PER_LINE; if (w.cl){ if (w.cl->state==MESI::S){ BusRequest up{BusCmd::Upgrade, base, id_}; BusResponse r = ic_->process(up); (void)r; w.cl->state = MESI::M; w.cl->dirty = true; trans_++; w.cl->data[word_off] = data; return; } if (w.cl->state==MESI::E){ w.cl->state = MESI::M; w.cl->dirty = true; trans_++; w.cl->data[word_off] = data; return; } if (w.cl->state==MESI::M){ w.cl->dirty = true; w.cl->data[word_off] = data; return; } } std::optional<uint64_t> ebase; std::optional<std::array<uint64_t,WORDS_PER_LINE>> eline; bool wasM=false; find_free_or_victim(base, ebase, eline, wasM); if (ebase && wasM){ ic_->writeback_from_evict(*ebase, *eline); } BusRequest rfo{BusCmd::BusRdX, base, id_}; BusResponse resp = ic_->process(rfo); install(base, resp.rline, MESI::M, true); WayRef w2 = lookup(base); w2.cl->data[word_off] = data; trans_++; }

// on_snoop
// - Método invocado por el interconnect para que la caché responda a un
//   SnoopMessage. Debe inspeccionar la línea indicada y actuar según el
//   comando: BusRd, BusRdX o Upgrade. Retorna un SnoopRet que indica si hubo
//   acierto, si la línea estaba en M y si la entrada fue invalidada.
Cache::SnoopRet Cache::on_snoop(const SnoopMessage& sm){ SnoopRet out{}; uint64_t base = line_base(sm.addr); WayRef w = lookup(base); if (!w.cl) return out; CacheLine& cl = *w.cl; switch (sm.cmd){ case BusCmd::BusRd: if (cl.state==MESI::M){ out.hit = true; out.hitM = true; out.wb = cl.data; cl.dirty = false; cl.state = MESI::S; trans_++; } else if (cl.state==MESI::E){ out.hit = true; cl.state = MESI::S; trans_++; } else if (cl.state==MESI::S){ out.hit = true; } break; case BusCmd::BusRdX: case BusCmd::Upgrade: if (cl.state==MESI::M){ out.hit = true; out.hitM = true; out.wb = cl.data; cl.dirty = false; cl.state = MESI::I; cl.valid = true; out.invalidated = true; trans_++; } else if (cl.state==MESI::E || cl.state==MESI::S){ out.hit = true; cl.state = MESI::I; cl.valid = true; out.invalidated = true; trans_++; } break; default: break; } return out; }

// dump_state
// - Imprime el estado actual de la caché: para cada conjunto y vía muestra
//   la base, el estado MESI y el flag dirty cuando la entrada es válida.
void Cache::dump_state(){ std::cout << "PE" << id_ << " dump:\n"; for (int s=0;s<(int)NUM_SETS;s++){ for (int w=0; w<NUM_WAYS; w++){ const CacheLine& cl = sets_[s][w]; if (cl.valid && cl.state!=MESI::I){ std::cout << "  set " << s << " way " << w << " base 0x" << std::hex << cl.tag << std::dec << " state " << mesi_str(cl.state) << " dirty " << cl.dirty << "\n"; } } } }

// Constructor
// - Inicializa la caché con identificador `id`, apunta al interconnect y a la
//   memoria principal proporcionados. Reserva las estructuras internas para
//   los conjuntos y la información del reemplazo.
Cache::Cache(int id, Interconnect* ic, MainMemory* mem): id_(id), ic_(ic), mem_(mem), sets_(NUM_SETS), lru_(NUM_SETS){ for (size_t s=0;s<NUM_SETS;s++){ for (size_t w=0; w<NUM_WAYS; ++w){ sets_[s][w] = CacheLine(); } } }

// id
// - Devuelve el identificador de la caché.
int Cache::id() const { return id_; }
