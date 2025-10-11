// El interconnect simula la lógica central del bus/coherencia que coordina las
// peticiones de las caches y realiza operaciones de snooping sobre las demás
// caches. 

#include <vector>
#include <array>
#include <cstdint>
#include "core.cpp"
#include "memory.cpp"

#define MESI_HAVE_INTERCONNECT_DECL

// Forward declaration mínima: `Interconnect` mantiene punteros a `Cache`.
// La definición completa de `Cache` reside en cache.cpp y se incluye después
// de este archivo en la unidad de traducción principal.
class Cache;

#ifndef MESI_HAVE_CACHE_DECL

class Cache {
public:
    // Tipo devuelto por la operación de snoop de la caché. Indica si la
    // caché tenía la línea, si estaba en estado M y si se invalidó.
    struct SnoopRet { bool hit=false; bool hitM=false; bool invalidated=false; std::array<uint64_t,WORDS_PER_LINE> wb{}; };
    // la caché debe ofrecer este método para que el interconnect pueda realizar snoops.
    SnoopRet on_snoop(const SnoopMessage& sm);
};
#endif

// Los tipos `SnoopMessage`, `BusRequest` y `BusResponse` están definidos en
// core.cpp e incluyen la semántica de los comandos y las respuestas del bus.
struct SnoopMessage; struct BusRequest; struct BusResponse;

class Interconnect {
public:
    Interconnect(MainMemory* mem): mem_(mem) {}
    // Registra una caché participante.
    void attach(Cache* c){ caches_.push_back(c); }
    // Procesa una petición de bus y devuelve la respuesta sincronizada.
    BusResponse process(const BusRequest& req);
    // Escribe en memoria la línea provista (usada por writebacks de evicción).
    void writeback_from_evict(uint64_t base, const std::array<uint64_t,WORDS_PER_LINE>& line){ mem_->write_line(base, line); }
private:
    MainMemory* mem_;
    std::vector<Cache*> caches_;

    // Manejo interno de los comandos BusRd, BusRdX y Upgrade. Cada manejador
    // genera SnoopMessage para todas las caches y compone la respuesta final.
    BusResponse handle_BusRd(int src, uint64_t base);
    BusResponse handle_BusRdX(int src, uint64_t base);
    BusResponse handle_Upgrade(int src, uint64_t base);
};


// - Lee la petición, calcula la base de línea y delega en el manejador según el comando.
BusResponse Interconnect::process(const BusRequest& req){ BusResponse rsp{}; uint64_t base = line_base(req.addr); switch(req.cmd){ case BusCmd::BusRd: return handle_BusRd(req.src_id, base); case BusCmd::BusRdX: return handle_BusRdX(req.src_id, base); case BusCmd::Upgrade: return handle_Upgrade(req.src_id, base); case BusCmd::WriteBack: mem_->write_line(base, req.wline); rsp.grant = true; return rsp; default: return rsp; } }

// handle_BusRd
// - Envía un SnoopMessage de tipo BusRd a todas las caches. Si alguna cache
//   tenía la línea en M, se realiza writeback y la respuesta incluye la
//   línea y shared=true. Si alguna cache tenía la línea en S/E, la respuesta
//   se marca como compartida y la línea se carga desde memoria.
BusResponse Interconnect::handle_BusRd(int src, uint64_t base){ BusResponse rsp; rsp.grant = true; SnoopMessage sm{BusCmd::BusRd, base, src}; bool any_hit=false, any_hitm=false; std::array<uint64_t,WORDS_PER_LINE> owner_line{}; for (auto* c : caches_){ auto r = c->on_snoop(sm); if (r.hit){ any_hit = true; } if (r.hitM){ any_hitm = true; owner_line = r.wb; } } if (any_hitm){ mem_->write_line(base, owner_line); rsp.rline = owner_line; rsp.shared = true; } else if (any_hit){ rsp.rline = mem_->read_line(base); rsp.shared = true; } else { rsp.rline = mem_->read_line(base); rsp.shared = false; } return rsp; }

// handle_BusRdX
// - Envía un SnoopMessage de tipo BusRdX para obtener exclusividad de la
//   línea. Cuenta acknowledgements de invalidación y, si alguna caché tenía
//   la línea en M, devuelve la línea propietaria en la respuesta.
BusResponse Interconnect::handle_BusRdX(int src, uint64_t base){ BusResponse rsp; rsp.grant = true; SnoopMessage sm{BusCmd::BusRdX, base, src}; uint8_t invacks = 0; bool any_hitm=false; std::array<uint64_t,WORDS_PER_LINE> owner_line{}; for (auto* c : caches_){ auto r = c->on_snoop(sm); if (r.invalidated) invacks++; if (r.hitM){ any_hitm = true; owner_line = r.wb; } } rsp.inv_ack_target = invacks; if (any_hitm){ mem_->write_line(base, owner_line); rsp.rline = owner_line; } else { rsp.rline = mem_->read_line(base); } return rsp; }

// handle_Upgrade
// - Envía un SnoopMessage de tipo Upgrade para que otras caches supriman su
//   copia cuando el emisor solicita promover una línea compartida a modificada.
BusResponse Interconnect::handle_Upgrade(int src, uint64_t base){ BusResponse rsp; rsp.grant = true; SnoopMessage sm{BusCmd::Upgrade, base, src}; uint8_t invacks = 0; for (auto* c : caches_){ auto r = c->on_snoop(sm); if (r.invalidated) invacks++; } rsp.inv_ack_target = invacks; return rsp; }
