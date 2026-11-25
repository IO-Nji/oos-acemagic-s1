#pragma once
#include "data/data_types.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Refreshes data providers on a background thread, each at its own interval.
// Use one Collector for cheap sources and another for slow ones (subprocesses) so a
// slow source never delays a fast one.
class Collector {
public:
    ~Collector() { stop(); }
    
    void add(std::shared_ptr<DataProvider> provider, std::chrono::milliseconds interval) {
        jobs_.push_back({std::move(provider), interval, std::chrono::steady_clock::now()});
    }
    
    void start() {
        if (thread_.joinable()) return;
        stopping_ = false;
        thread_ = std::thread([this] { run(); });
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }
    
private:
    struct Job {
        std::shared_ptr<DataProvider> provider;
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point next;
    };
    
    void run() {
        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto earliest = now + std::chrono::seconds(1);
            
            for (auto& job : jobs_) {
                if (stopped()) return;
                if (job.next <= now) {
                    try {
                        job.provider->update();
                    } catch (const std::exception& e) {
                        std::cerr << job.provider->name() << " provider: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << job.provider->name() << " provider failed" << std::endl;
                    }
                    // Schedule from completion so a slow source cannot fall behind and loop
                    job.next = std::chrono::steady_clock::now() + job.interval;
                }
                earliest = std::min(earliest, job.next);
            }
            
            std::unique_lock<std::mutex> lock(mutex_);
            if (wake_.wait_until(lock, earliest, [this] { return stopping_; })) return;
        }
    }
    
    bool stopped() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopping_;
    }
    
    std::vector<Job> jobs_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
};
