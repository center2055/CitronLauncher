#pragma once

#include "app/AppState.h"
#include "minecraft/Version.h"

#include <functional>
#include <string>

namespace citron::ui {

struct PageActions {
    std::function<void(Page)> navigate;
    std::function<void(const VersionId&)> select;
    std::function<void()> launch;
    std::function<void(const VersionId&)> install;
    std::function<void(const VersionId&)> cancel;
    std::function<void(const VersionId&)> remove;
    std::function<void(const VersionId&)> dismiss;
    std::function<void()> refreshCatalog;
    std::function<void(const std::wstring&)> search;
    std::function<void(VersionFilter)> filter;
    std::function<void(const std::string&)> setLanguage;
    std::function<void(bool)> setCloseOnLaunch;
    std::function<void(bool)> setCheckUpdates;
    std::function<void()> browseRoot;
    std::function<void()> applyRoot;
    std::function<void()> resetRoot;
    std::function<void()> checkUpdates;
    std::function<void()> openGithub;
    std::function<void()> openDiscord;
    std::function<void()> openKofi;
    std::function<void()> openStore;
};

}
