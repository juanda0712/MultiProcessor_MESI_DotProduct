#pragma once
#include <vector>
#include "processing_element.hpp"

class TaskDistributor {
public:
    void distributeWork(std::vector<ProcessingElement*>& pes,
                        uint64_t baseA, uint64_t baseB, uint64_t partialBase,
                        size_t total_count);
};
