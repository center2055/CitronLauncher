#include "download/DownloadManager.h"

#include "core/Logger.h"

namespace citron {

DownloadManager::DownloadManager(TaskScheduler& scheduler) : scheduler_(scheduler) {}

bool DownloadManager::start(const VersionId& id, DownloadRequest request, DownloadUpdate update, DownloadDone done) {
    std::lock_guard lock(mutex_);
    if (auto it = operations_.find(id); it != operations_.end() && !it->second->done()) {
        return false;
    }
    auto job = [this, id, request = std::move(request), update = std::move(update), done = std::move(done)](std::stop_token token) {
        auto result = downloadFile(request, token, [&](const DownloadProgress& p) {
            if (update) {
                update(id, p);
            }
        });
        {
            std::lock_guard inner(mutex_);
            operations_.erase(id);
        }
        if (done) {
            done(id, std::move(result));
        }
    };
    operations_[id] = scheduler_.start("download " + id.key(), std::move(job));
    return true;
}

void DownloadManager::cancel(const VersionId& id) {
    std::shared_ptr<Operation> op;
    {
        std::lock_guard lock(mutex_);
        if (auto it = operations_.find(id); it != operations_.end()) {
            op = it->second;
        }
    }
    if (op) {
        log::info("cancelling download {}", id.key());
        op->cancel();
    }
}

bool DownloadManager::active(const VersionId& id) const {
    std::lock_guard lock(mutex_);
    auto it = operations_.find(id);
    return it != operations_.end() && !it->second->done();
}

size_t DownloadManager::activeCount() const {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (const auto& [id, op] : operations_) {
        if (!op->done()) {
            ++count;
        }
    }
    return count;
}

}
