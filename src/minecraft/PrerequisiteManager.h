#pragma once

#include "core/TaskScheduler.h"
#include "platform/windows/Gdk.h"

#include <functional>

namespace citron {

class PrerequisiteManager {
public:
    explicit PrerequisiteManager(TaskScheduler& scheduler);

    void check(std::function<void(platform::GdkEnvironment)> done);

private:
    TaskScheduler& scheduler_;
};

}
