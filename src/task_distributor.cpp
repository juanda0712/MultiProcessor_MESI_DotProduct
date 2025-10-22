#include "task_distributor.hpp"

void TaskDistributor::distributeWork(std::vector<ProcessingElement*>& pes,
                                     uint64_t baseA, uint64_t baseB, uint64_t partialBase,
                                     size_t total_count)
{
    size_t numPEs = pes.size();
    size_t block = total_count / numPEs;

    for (size_t i = 0; i < numPEs; ++i) {
        uint64_t a = baseA + i * block * WORD_SIZE;
        uint64_t b = baseB + i * block * WORD_SIZE;
        uint64_t result = partialBase + i * WORD_SIZE;

        pes[i]->initializeRegisters((double)a, (double)b, (double)result, (double)block);

        std::cout << "[TaskDistributor] PE" << i 
                  << " assigned dataA=0x" << std::hex << a 
                  << " dataB=0x" << b 
                  << " result=0x" << result 
                  << " count=" << std::dec << block << std::endl;
    }
}
