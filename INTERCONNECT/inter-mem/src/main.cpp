#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>
#include <memory>
#include <sstream>
#include "message.h"
#include "memory.h"
#include "interconnect.h"

// Simple cache stub that only logs snoop messages and receives responses
class CacheStub {
public:
    CacheStub(uint8_t id, Interconnect &ic) : id_(id), interconnect_(ic) {
        interconnect_.register_snoop_callback(id_, [this](const BusMessage &m){ this->on_bus_message(m); });
    }

    void on_bus_message(const BusMessage &m) {
        std::ostringstream oss;
        oss << "CacheStub PE" << int(id_) << " received bus msg type=" << int(m.type)
            << " addr=0x" << std::hex << m.address;
        std::cout << oss.str() << std::endl;
    }

    // helper to send requests to interconnect
    void send_request(const BusMessage &m) {
        interconnect_.send_request(m);
    }

private:
    uint8_t id_;
    Interconnect &interconnect_;
};

int main() {
    constexpr uint8_t NUM_PES = 4;
    MainMemory mem(512, 20); // 512 slots, 20 us latency (example)
    Interconnect inter(mem, NUM_PES);

    inter.start();

    // create cache stubs
    std::vector<std::unique_ptr<CacheStub>> caches;
    for (uint8_t i = 0; i < NUM_PES; ++i) caches.emplace_back(std::make_unique<CacheStub>(i, inter));

    // load some initial data into memory (example)
    std::vector<uint64_t> init;
    for (int i = 0; i < 64; ++i) init.push_back(0x3FF0000000000000ULL + static_cast<uint64_t>(i));
    mem.load_initial_data(init, 0x0200); // load at base 0x0200

    // PE threads
    std::atomic<bool> stop{false};
    std::vector<std::thread> pes;
    for (uint8_t pe = 0; pe < NUM_PES; ++pe) {
        pes.emplace_back([pe, &inter]() {
            if (pe == 0) {
                // issue READ
                BusMessage m{};
                m.msg_id = 1;
                m.src = pe;
                m.type = MsgType::READ;
                m.address = 0x0200;
                m.size = 8;
                m.data = 0;
                inter.send_request(m);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else if (pe == 1) {
                // wait a little, then WRITE
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                BusMessage m{};
                m.msg_id = 2;
                m.src = pe;
                m.type = MsgType::WRITE;
                m.address = 0x0200;
                m.size = 8;
                m.data = 0x4010000000000000ULL;
                inter.send_request(m);
            } else {
                // other PEs idle for demo
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        });
    }

    // join PE threads
    for (auto &t : pes) if (t.joinable()) t.join();

    // let interconnect finish processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    inter.stop();

    std::cout << "Total transactions: " << inter.get_total_transactions() << std::endl;
    mem.dump_region(0x0200, 0x0220);

    return 0;
}
