#pragma once

#include "core/TaskScheduler.h"
#include "download/Download.h"
#include "minecraft/Version.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace citron {

using DownloadDone = std::function<void(const VersionId& id, Result<void> result)>;
using DownloadUpdate = std::function<void(const VersionId& id, const DownloadProgress& progress)>;

class DownloadManager {
public:
    explicit DownloadManager(TaskScheduler& scheduler);

    bool start(const VersionId& id, DownloadRequest request, DownloadUpdate update, DownloadDone done);
    void cancel(const VersionId& id);
    bool active(const VersionId& id) const;
    size_t activeCount() const;

private:
    TaskScheduler& scheduler_;
    mutable std::mutex mutex_;
    std::map<VersionId, std::shared_ptr<Operation>> operations_;
};

}
