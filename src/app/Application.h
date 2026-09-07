#pragma once

#include "app/AppState.h"
#include "app/Strings.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "core/TaskScheduler.h"
#include "minecraft/MinecraftService.h"
#include "ui/Shell.h"
#include "ui/Window.h"
#include "ui/pages/PlayPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/VersionsPage.h"

#include <windows.h>

#include <memory>
#include <span>
#include <string>

namespace citron {

class Application {
public:
    explicit Application(HINSTANCE instance);
    ~Application();

    int run();

private:
    bool initialize();
    void buildUi();
    void syncFromService();
    void refreshUi();
    void applyTheme();
    void applyStrings();
    void persistSettings();
    void showToast(std::wstring text, bool error = false, std::optional<Error> details = std::nullopt);
    void showError(const std::wstring& title, const Error& error);
    void hideToast();
    void navigate(Page page);
    void selectVersion(const VersionId& id);
    void launch();
    void install(const VersionId& id);
    void cancel(const VersionId& id);
    void removeVersion(const VersionId& id);
    void dismiss(const VersionId& id);
    void refreshCatalog();
    void setLanguage(const std::string& language);
    void toggleTheme();
    void browseRoot();
    void applyRoot();
    void resetRoot();
    void checkForUpdates(bool manual);
    void openUrl(const wchar_t* url);
    std::span<const std::uint8_t> resource(int id);
    std::string catalogUrl() const;
    const Strings& strings() const { return stringsFor(state_.settings.language); }

    HINSTANCE instance_;
    paths::Layout layout_;
    TaskScheduler scheduler_;
    Dispatcher dispatcher_;
    std::unique_ptr<MinecraftService> service_;
    AppState state_;
    ui::Window window_;
    std::unique_ptr<ui::Shell> shell_;
    ui::PlayPage* play_ = nullptr;
    ui::VersionsPage* versions_ = nullptr;
    ui::SettingsPage* settingsPage_ = nullptr;
    std::string embeddedCatalog_;
    HANDLE instanceMutex_ = nullptr;
    bool closing_ = false;
};

}
