#include "core/TaskScheduler.h"

#include "core/Logger.h"

#include <algorithm>

namespace citron {

void Dispatcher::setWakeup(std::function<void()> wake) {
    std::lock_guard lock(mutex_);
    wake_ = std::move(wake);
}

void Dispatcher::post(Fn fn) {
    std::function<void()> wake;
    {
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(fn));
        wake = wake_;
    }
    if (wake) {
        wake();
    }
}

void Dispatcher::drain() {
    std::deque<Fn> pending;
    {
        std::lock_guard lock(mutex_);
        pending.swap(queue_);
    }
    for (auto& fn : pending) {
        fn();
    }
}

Operation::Operation(std::string name, Job job) : name_(std::move(name)) {
    thread_ = std::jthread([this, job = std::move(job)](std::stop_token token) {
        try {
            job(token);
        } catch (const std::exception& e) {
            log::error("operation {} threw: {}", name_, e.what());
        } catch (...) {
            log::error("operation {} threw", name_);
        }
        done_.store(true);
    });
}

Operation::~Operation() {
    thread_.request_stop();
}

void Operation::cancel() {
    thread_.request_stop();
}

TaskScheduler::TaskScheduler(unsigned workers) {
    workers = std::max(1u, workers);
    for (unsigned i = 0; i < workers; ++i) {
        workers_.emplace_back([this](std::stop_token token) { workerLoop(token); });
    }
}

TaskScheduler::~TaskScheduler() {
    shutdown();
}

void TaskScheduler::run(std::function<void(std::stop_token)> job) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        jobs_.push_back(std::move(job));
    }
    wake_.notify_one();
}

std::shared_ptr<Operation> TaskScheduler::start(std::string name, Operation::Job job) {
    auto op = std::make_shared<Operation>(std::move(name), std::move(job));
    std::lock_guard lock(mutex_);
    prune();
    operations_.push_back(op);
    return op;
}

void TaskScheduler::shutdown() {
    std::vector<std::shared_ptr<Operation>> ops;
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        jobs_.clear();
        ops.swap(operations_);
    }
    wake_.notify_all();
    for (auto& worker : workers_) {
        worker.request_stop();
    }
    workers_.clear();
    for (auto& op : ops) {
        op->cancel();
    }
    ops.clear();
}

void TaskScheduler::workerLoop(std::stop_token token) {
    while (true) {
        std::function<void(std::stop_token)> job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, token, [this] { return !jobs_.empty() || stopping_; });
            if (jobs_.empty()) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        try {
            job(token);
        } catch (const std::exception& e) {
            log::error("background job threw: {}", e.what());
        } catch (...) {
            log::error("background job threw");
        }
    }
}

void TaskScheduler::prune() {
    std::erase_if(operations_, [](const std::shared_ptr<Operation>& op) { return op->done(); });
}

}
