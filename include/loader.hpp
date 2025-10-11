#pragma once
#include "parser.hpp"
#include "processing_element.hpp"
#include "cache.hpp"
#include <vector>
#include <string>

class Loader {
public:
    void loadAndDistribute(const std::vector<std::string>& programFiles,
                           std::vector<ProcessingElement*>& pes);
};
