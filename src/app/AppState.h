#pragma once

#include "core/Error.h"
#include "core/Settings.h"
#include "minecraft/InstallState.h"
#include "minecraft/MinecraftService.h"
#include "minecraft/Version.h"
#include "platform/windows/Gdk.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace citron {

enum class Page {
    Play,
    Versions,
    Settings,
};

enum class VersionFilter {
    All,
    Release,
    Preview,
    Installed,
};

enum class UpdateStatus {
    Unknown,
    Checking,
    UpToDate,
    Available,
    Failed,
};

struct VersionRow {
    VersionId id;
    std::uint64_t size = 0;
    bool installed = false;
    bool downloaded = false;
    bool deployed = false;
    bool selected = false;
    bool inCatalog = false;
    std::uint64_t partialSize = 0;
    InstallProgress progress;
};

struct ToastState {
    std::wstring text;
    bool error = false;
    std::optional<Error> details;
    double shownAt = 0.0;
};

struct DialogState {
    std::wstring title;
    std::vector<std::wstring> paragraphs;
    std::wstring confirmLabel;
    std::wstring cancelLabel;
    bool destructive = false;
    std::optional<VersionId> subject;
};

struct AppState {
    Page page = Page::Play;
    Settings settings;
    std::vector<VersionRow> versions;
    std::optional<VersionId> selected;
    bool catalogLoading = false;
    std::optional<Error> catalogError;
    std::optional<platform::GdkEnvironment> environment;
    bool launching = false;
    std::wstring search;
    VersionFilter filter = VersionFilter::All;
    std::optional<VersionId> armedRemove;
    std::optional<ToastState> toast;
    std::optional<DialogState> dialog;
    UpdateStatus updateStatus = UpdateStatus::Unknown;
    std::wstring updateVersion;
    std::int64_t updateCheckedAt = 0;
    std::wstring pendingRoot;

    const VersionRow* selectedRow() const;
    const VersionRow* find(const VersionId& id) const;
    std::vector<const VersionRow*> installedRows() const;
    std::vector<const VersionRow*> visibleRows() const;
    std::vector<const VersionRow*> activeOperations() const;
};

std::vector<VersionRow> buildRows(const ServiceSnapshot& snapshot, const std::optional<VersionId>& selected);
std::optional<VersionId> chooseSelection(const std::vector<VersionRow>& rows, const std::optional<VersionId>& previous);

}
