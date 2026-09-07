#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace citron {

class Dispatcher {
public:
    using Fn = std::function<void()>;

    void setWakeup(std::function<void()> wake);
    void post(Fn fn);
    void drain();

private:
    std::mutex mutex_;
    std::deque<Fn> queue_;
    std::function<void()> wake_;
};

class Operation {
public:
    using Job = std::function<void(std::stop_token)>;

    Operation(std::string name, Job job);
    ~Operation();

    void cancel();
    bool done() const { return done_.load(); }
    bool cancelled() const { return thread_.get_stop_token().stop_requested(); }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    std::atomic<bool> done_{false};
    std::jthread thread_;
};

class TaskScheduler {
public:
    explicit TaskScheduler(unsigned workers = 2);
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    void run(std::function<void(std::stop_token)> job);
    std::shared_ptr<Operation> start(std::string name, Operation::Job job);
    void shutdown();

private:
    void workerLoop(std::stop_token token);
    void prune();

    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<std::function<void(std::stop_token)>> jobs_;
    std::vector<std::jthread> workers_;
    std::vector<std::shared_ptr<Operation>> operations_;
    bool stopping_ = false;
};

}
