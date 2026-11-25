#pragma once
#include "data/data_types.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

// Base for data providers. collect() gathers fresh data and may block (files,
// subprocesses); it runs on a collector thread. data() hands the render thread a
// copy of the last published result, so drawing never waits on collection.
template <typename T>
class SnapshotProvider : public DataProvider {
public:
    void update() final {
        if (frozen_.load()) return;
        T fresh = collect();
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = std::move(fresh);
        generation_++;
    }
    
    T data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }
    
    // Increases every time new data is published
    uint64_t generation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generation_;
    }
    
    bool frozen() const { return frozen_.load(); }
    
    // Preview/test hook: publish fixed data and stop live collection
    void setData(const T& d) {
        frozen_ = true;
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = d;
        generation_++;
    }
    
protected:
    virtual T collect() = 0;
    
private:
    mutable std::mutex mutex_;
    T data_;
    uint64_t generation_ = 0;
    std::atomic<bool> frozen_{false};
};

// Run a shell command and return its stdout. The command is killed after
// timeoutSec so a hung tool cannot stall its collector thread for good.
inline std::string runCommand(const std::string& cmd, int timeoutSec = 10) {
    std::string full = "timeout " + std::to_string(timeoutSec) + " " + cmd;
    std::string result;
    FILE* pipe = popen(full.c_str(), "r");
    if (!pipe) return result;
    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}
