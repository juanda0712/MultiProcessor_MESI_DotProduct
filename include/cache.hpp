#pragma once
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>
#include <string>      

class Cache {
public:
    double read(size_t addr);
    void write(size_t addr, double value);
    void loadProgram(const std::vector<std::string>& program);

private:
    std::unordered_map<size_t, double> data_;
    std::mutex mtx_;
};
