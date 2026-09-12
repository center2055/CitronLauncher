#include "app/Application.h"

#include "BuildInfo.h"
#include "core/Format.h"
#include "core/Json.h"
#include "core/Logger.h"
#include "core/PathSafety.h"
#include "core/Text.h"
#include "download/Http.h"
#include "platform/windows/FileOps.h"
#include "platform/windows/Gdk.h"
#include "platform/windows/Shell.h"
#include "resource.h"

#include <format>

namespace citron {

namespace {

constexpr unsigned kToastTimer = 1;
constexpr wchar_t kInstanceMutex[] = L"Local\\CitronLauncher.SingleInstance";
constexpr char kDefaultCatalogUrl[] = "https://raw.githubusercontent.com/center2055/CitronLauncher/main/catalog/versions.json";
constexpr char kReleasesApi[] = "https://api.github.com/repos/center2055/CitronLauncher/releases/latest";
constexpr wchar_t kGithubUrl[] = L"https://github.com/center2055/CitronLauncher";
constexpr wchar_t kDiscordUrl[] = L"https://discord.gg/f7JTg86r8A";
constexpr wchar_t kKofiUrl[] = L"https://ko-fi.com/center2055";
constexpr std::int64_t kUpdateInterval = 6 * 3600;

std::wstring pathText(const std::filesystem::path& path) {
    return path.wstring();
}

}

Application::Application(HINSTANCE instance) : instance_(instance), scheduler_(2), window_(instance) {}

Application::~Application() {
    scheduler_.shutdown();
    if (instanceMutex_ != nullptr) {
        CloseHandle(instanceMutex_);
    }
}

std::span<const std::uint8_t> Application::resource(int id) {
    HRSRC found = FindResourceW(instance_, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (found == nullptr) {
        return {};
    }
    HGLOBAL loaded = LoadResource(instance_, found);
    if (loaded == nullptr) {
        return {};
    }
    const auto* data = static_cast<const std::uint8_t*>(LockResource(loaded));
    const DWORD size = SizeofResource(instance_, found);
    if (data == nullptr || size == 0) {
        return {};
    }
    return {data, size};
}

std::string Application::catalogUrl() const {
    return state_.settings.catalogUrl.empty() ? kDefaultCatalogUrl : state_.settings.catalogUrl;
}

bool Application::initialize() {
    instanceMutex_ = CreateMutexW(nullptr, TRUE, kInstanceMutex);
    if (instanceMutex_ != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"CitronLauncherWindow", nullptr)) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return false;
    }

    const auto defaults = paths::defaultLayout();
    auto loaded = loadSettings(defaults.settingsFile);
    state_.settings = loaded.settings;
    layout_ = defaults;
    if (!state_.settings.rootDirectory.empty()) {
        const std::filesystem::path custom = text::toWide(state_.settings.rootDirectory);
        if (custom.is_absolute()) {
            layout_ = paths::layoutFor(custom);
            layout_.settingsFile = defaults.settingsFile;
        }
    }
    if (auto ensured = paths::ensureDirectories(layout_); !ensured) {
        layout_ = defaults;
        if (auto fallback = paths::ensureDirectories(layout_); !fallback) {
            MessageBoxW(nullptr, text::toWide(fallback.error().summary()).c_str(), L"Citron Launcher", MB_ICONERROR | MB_OK);
            return false;
        }
    }
    log::open(layout_.logs);
    log::setMinimumLevel(state_.settings.verboseLogging ? log::Level::Debug : log::Level::Info);
    log::info("citron launcher {} ({}) starting", CITRON_VERSION_STRING, CITRON_COMMIT);
    log::info("build {} {} x64", CITRON_COMPILER, CITRON_BUILD_DATE);
    log::info("settings loaded at {:.1f} ms{}", log::elapsedMs(), loaded.recovered ? " (recovered defaults)" : "");
    log::info("root {}", layout_.root.string());

    const auto catalog = resource(IDR_CATALOG);
    embeddedCatalog_.assign(reinterpret_cast<const char*>(catalog.data()), catalog.size());

    state_.selected = VersionId::parse(state_.settings.selectedVersion);

    ui::WindowOptions options;
    options.width = state_.settings.windowWidth;
    options.height = state_.settings.windowHeight;
    options.maximized = state_.settings.windowMaximized;
    options.title = L"Citron Launcher";
    options.icon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_CITRON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    options.smallIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_CITRON), IMAGE_ICON, 16, 16, LR_SHARED));
    if (auto created = window_.create(options, resource(IDR_FONT_BODY), resource(IDR_FONT_TITLE), resource(IDR_FONT_MONO)); !created) {
        log::error("window: {}", created.error().summary());
        MessageBoxW(nullptr, text::toWide(created.error().summary()).c_str(), L"Citron Launcher", MB_ICONERROR | MB_OK);
        return false;
    }
    log::info("window and graphics ready at {:.1f} ms", log::elapsedMs());
    window_.setDispatcher(&dispatcher_);

    service_ = std::make_unique<MinecraftService>(layout_, scheduler_, dispatcher_, embeddedCatalog_);
    service_->setChangeHandler([this] { syncFromService(); });

    buildUi();
    applyTheme();
    applyStrings();
    window_.setOnFirstFrame([this] {
        dispatcher_.post([this] {
            service_->start(catalogUrl());
            if (state_.settings.checkUpdates && platform::unixNow() - state_.settings.lastUpdateCheck > kUpdateInterval) {
                checkForUpdates(false);
            }
        });
    });
    window_.setOnClosing([this] {
        closing_ = true;
        persistSettings();
        log::info("closing");
    });
    window_.setOnResize([this] {
        shell_->setMaximized(window_.maximized());
        play_->update(state_, strings());
        versions_->update(state_, strings());
    });
    return true;
}

void Application::buildUi() {
    ui::ShellActions shellActions;
    shellActions.navigate = [this](int index) { navigate(static_cast<Page>(index)); };
    shellActions.toggleTheme = [this] { toggleTheme(); };
    shellActions.minimize = [this] { window_.minimize(); };
    shellActions.toggleMaximize = [this] { window_.toggleMaximize(); };
    shellActions.close = [this] { window_.close(); };
    shell_ = std::make_unique<ui::Shell>(std::move(shellActions));

    ui::PageActions actions;
    actions.navigate = [this](Page page) { navigate(page); };
    actions.select = [this](const VersionId& id) { selectVersion(id); };
    actions.launch = [this] { launch(); };
    actions.install = [this](const VersionId& id) { install(id); };
    actions.cancel = [this](const VersionId& id) { cancel(id); };
    actions.remove = [this](const VersionId& id) { removeVersion(id); };
    actions.dismiss = [this](const VersionId& id) { dismiss(id); };
    actions.refreshCatalog = [this] { refreshCatalog(); };
    actions.search = [this](const std::wstring& text) {
        state_.search = text;
        state_.armedRemove.reset();
        versions_->update(state_, strings());
    };
    actions.filter = [this](VersionFilter filter) {
        state_.filter = filter;
        state_.armedRemove.reset();
        versions_->update(state_, strings());
    };
    actions.setLanguage = [this](const std::string& language) { setLanguage(language); };
    actions.setCloseOnLaunch = [this](bool on) {
        state_.settings.closeOnLaunch = on;
        persistSettings();
    };
    actions.setCheckUpdates = [this](bool on) {
        state_.settings.checkUpdates = on;
        persistSettings();
    };
    actions.browseRoot = [this] { browseRoot(); };
    actions.applyRoot = [this] { applyRoot(); };
    actions.resetRoot = [this] { resetRoot(); };
    actions.checkUpdates = [this] { checkForUpdates(true); };
    actions.openGithub = [this] { openUrl(kGithubUrl); };
    actions.openDiscord = [this] { openUrl(kDiscordUrl); };
    actions.openKofi = [this] { openUrl(kKofiUrl); };
    actions.openStore = [this] { openUrl(platform::gamingServicesStoreUrl()); };

    ui::AboutInfo about;
    about.version = text::toWide(CITRON_VERSION_STRING);

    play_ = shell_->pageHost()->add(std::make_unique<ui::PlayPage>(actions));
    versions_ = shell_->pageHost()->add(std::make_unique<ui::VersionsPage>(actions));
    settingsPage_ = shell_->pageHost()->add(std::make_unique<ui::SettingsPage>(actions, about));
    versions_->setVisible(false);
    settingsPage_->setVisible(false);

    window_.setRoot(shell_.get());
    window_.setCaptionHitTest([this](ui::Point point) { return shell_->hitTestCaption(point); });
    shell_->setMaximized(window_.maximized());
    refreshUi();
}

void Application::applyTheme() {
    const bool dark = state_.settings.theme != "light";
    window_.setTheme(dark ? ui::Theme::darkTheme() : ui::Theme::lightTheme());
    shell_->setDarkTheme(dark);
}

void Application::applyStrings() {
    shell_->setStrings(strings());
    refreshUi();
}

void Application::refreshUi() {
    const Strings& s = strings();
    shell_->setPage(static_cast<int>(state_.page));
    play_->setVisible(state_.page == Page::Play);
    versions_->setVisible(state_.page == Page::Versions);
    settingsPage_->setVisible(state_.page == Page::Settings);
    play_->update(state_, s);
    versions_->update(state_, s);
    settingsPage_->update(state_, s, pathText(layout_.root), pathText(layout_.installers), pathText(layout_.logs));
    window_.layout();
    window_.invalidate();
}

void Application::syncFromService() {
    const ServiceSnapshot snapshot = service_->snapshot();
    const auto before = state_.versions;
    state_.versions = buildRows(snapshot, state_.selected);
    const auto chosen = chooseSelection(state_.versions, state_.selected);
    if (chosen != state_.selected) {
        state_.selected = chosen;
        state_.versions = buildRows(snapshot, state_.selected);
        state_.settings.selectedVersion = chosen ? chosen->key() : std::string();
        persistSettings();
    }
    state_.catalogLoading = snapshot.catalogLoading;
    state_.catalogError = snapshot.catalogError;
    state_.environment = snapshot.environment;
    state_.launching = snapshot.launching;
    for (const auto& row : state_.versions) {
        const VersionRow* old = nullptr;
        for (const auto& b : before) {
            if (b.id == row.id) {
                old = &b;
                break;
            }
        }
        if (old != nullptr && isBusy(old->progress.stage) && row.progress.stage == InstallStage::Downloaded) {
            showToast(strings().toastDownloaded + L" " + text::toWide(row.id.number.toString()));
            service_->dismiss(row.id);
        }
        if (old != nullptr && isBusy(old->progress.stage) && row.progress.stage == InstallStage::Completed) {
            showToast(strings().toastInstalled + L" " + text::toWide(row.id.number.toString()));
            service_->dismiss(row.id);
        }
        if (old != nullptr && isBusy(old->progress.stage) && row.progress.stage == InstallStage::Failed && row.progress.error) {
            showError(strings().errInstallTitle, *row.progress.error);
        }
    }
    if (state_.armedRemove && (state_.find(*state_.armedRemove) == nullptr || !state_.find(*state_.armedRemove)->installed)) {
        state_.armedRemove.reset();
    }
    refreshUi();
}

void Application::persistSettings() {
    int w = 0;
    int h = 0;
    window_.windowRect(w, h);
    if (w > 0 && h > 0) {
        state_.settings.windowWidth = w;
        state_.settings.windowHeight = h;
    }
    state_.settings.windowMaximized = window_.maximized();
    if (auto saved = saveSettings(state_.settings, layout_.settingsFile); !saved) {
        log::warn("settings could not be saved: {}", saved.error().summary());
    }
}

void Application::showToast(std::wstring text, bool error, std::optional<Error> details) {
    std::wstring action;
    std::function<void()> onAction;
    if (details) {
        action = strings().errDetails;
        Error copy = *details;
        std::wstring title = text;
        onAction = [this, copy, title] {
            std::vector<std::wstring> paragraphs;
            paragraphs.push_back(text::toWide(copy.message));
            if (!copy.detail.empty()) {
                paragraphs.push_back(text::toWide(copy.detail));
            }
            if (copy.code != 0) {
                paragraphs.push_back(L"Error " + text::toWide(copy.codeText()));
            }
            hideToast();
            shell_->dialog().show(title, std::move(paragraphs), strings().dialogCancel, L"", false, nullptr, nullptr);
            window_.layout();
            window_.invalidate();
        };
    }
    shell_->toast().show(std::move(text), error, std::move(action), std::move(onAction), window_.now());
    window_.layout();
    window_.invalidate();
    window_.setTimeout(kToastTimer, error ? 6000 : 2200, [this] { hideToast(); });
}

void Application::showError(const std::wstring& title, const Error& error) {
    log::error("{}: {}", text::toUtf8(title), error.summary());
    showToast(title, true, error);
}

void Application::hideToast() {
    window_.cancelTimeout(kToastTimer);
    shell_->toast().hide();
    window_.layout();
    window_.invalidate();
}

void Application::navigate(Page page) {
    if (state_.page == page) {
        return;
    }
    state_.page = page;
    state_.armedRemove.reset();
    log::info("page {}", static_cast<int>(page));
    refreshUi();
    if (page == Page::Versions) {
        versions_->focusSearch();
    }
}

void Application::selectVersion(const VersionId& id) {
    const VersionRow* row = state_.find(id);
    if (row == nullptr || !row->installed) {
        return;
    }
    if (state_.selected && *state_.selected == id) {
        return;
    }
    state_.selected = id;
    state_.settings.selectedVersion = id.key();
    state_.versions = buildRows(service_->snapshot(), state_.selected);
    persistSettings();
    showToast(strings().toastSelect + L" " + text::toWide(id.number.toString()));
    refreshUi();
}

void Application::launch() {
    const VersionRow* row = state_.selectedRow();
    if (row == nullptr || !row->installed || state_.launching) {
        return;
    }
    if (state_.environment && !state_.environment->gamingServices) {
        showError(strings().errLaunchTitle, Error::make(ErrorCategory::Prerequisite, "launch", text::toUtf8(strings().errGamingServices)));
        return;
    }
    const VersionId id = row->id;
    auto start = [this, id] {
        log::info("launch requested for {}", id.key());
        service_->launch(id, [this, id](Result<void> result) {
            if (!result) {
                if (!result.error().isCancelled()) {
                    showError(strings().errLaunchTitle, result.error());
                }
                return;
            }
            showToast(strings().toastLaunch + L" " + text::toWide(id.number.toString()));
            if (state_.settings.closeOnLaunch) {
                window_.setTimeout(kToastTimer + 1, 600, [this] { window_.close(); });
            }
        });
        refreshUi();
    };
    // Installed versions are now isolated under Citron's managed folder. They
    // launch directly from there and never replace the Store/Xbox package.
    start();
}

void Application::install(const VersionId& id) {
    state_.armedRemove.reset();
    service_->install(id);
}

void Application::cancel(const VersionId& id) {
    service_->cancel(id);
    showToast(strings().toastCancelled + L" " + text::toWide(id.number.toString()));
}

void Application::dismiss(const VersionId& id) {
    service_->dismiss(id);
}

void Application::removeVersion(const VersionId& id) {
    if (!state_.armedRemove || *state_.armedRemove != id) {
        state_.armedRemove = id;
        versions_->update(state_, strings());
        window_.invalidate();
        return;
    }
    state_.armedRemove.reset();
    service_->remove(id, [this, id](Result<void> result) {
        if (!result) {
            showError(strings().errRemoveTitle, result.error());
            return;
        }
        showToast(strings().toastRemoved + L" " + text::toWide(id.number.toString()));
    });
    refreshUi();
}

void Application::refreshCatalog() {
    service_->refreshCatalog(catalogUrl());
    service_->refreshInstalled();
    service_->checkEnvironment();
}

void Application::setLanguage(const std::string& language) {
    if (state_.settings.language == language) {
        return;
    }
    state_.settings.language = language;
    persistSettings();
    applyStrings();
}

void Application::toggleTheme() {
    state_.settings.theme = state_.settings.theme == "light" ? "dark" : "light";
    persistSettings();
    applyTheme();
    window_.invalidate();
}

void Application::browseRoot() {
    auto picked = platform::pickFolder(window_.hwnd(), layout_.root);
    if (!picked) {
        return;
    }
    state_.pendingRoot = picked->wstring();
    refreshUi();
}

void Application::applyRoot() {
    if (state_.pendingRoot.empty()) {
        return;
    }
    const std::filesystem::path root = state_.pendingRoot;
    paths::Layout layout = paths::layoutFor(root);
    layout.settingsFile = layout_.settingsFile;
    if (auto applied = service_->setRoot(layout); !applied) {
        showError(strings().errInstallTitle, applied.error());
        return;
    }
    layout_ = layout;
    state_.settings.rootDirectory = text::toUtf8(root.wstring());
    state_.pendingRoot.clear();
    persistSettings();
    log::info("root changed to {}", layout_.root.string());
    showToast(strings().toastSettingsSaved);
    refreshUi();
}

void Application::resetRoot() {
    state_.pendingRoot.clear();
    if (state_.settings.rootDirectory.empty()) {
        refreshUi();
        return;
    }
    paths::Layout layout = paths::defaultLayout();
    if (auto applied = service_->setRoot(layout); !applied) {
        showError(strings().errInstallTitle, applied.error());
        return;
    }
    layout_ = layout;
    state_.settings.rootDirectory.clear();
    persistSettings();
    showToast(strings().toastPathReset);
    refreshUi();
}

void Application::checkForUpdates(bool manual) {
    if (state_.updateStatus == UpdateStatus::Checking) {
        return;
    }
    state_.updateStatus = UpdateStatus::Checking;
    refreshUi();
    scheduler_.run([this, manual](std::stop_token token) {
        auto body = http::get(kReleasesApi, 512 * 1024, token);
        std::optional<std::string> latest;
        std::optional<Error> error;
        bool noRelease = false;
        if (body) {
            auto parsed = json::parse(*body);
            if (parsed) {
                std::string tag = (*parsed)["tag_name"].asString();
                if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) {
                    tag.erase(0, 1);
                }
                latest = tag;
            } else {
                error = Error::make(ErrorCategory::Network, "update check", "The update information could not be read.", parsed.error());
            }
        } else if (body.error().httpStatus == 404u) {
            // nothing has been published yet, so the running build is the newest one
            noRelease = true;
        } else {
            error = body.error();
        }
        dispatcher_.post([this, manual, latest, error, noRelease] {
            state_.updateCheckedAt = platform::unixNow();
            state_.settings.lastUpdateCheck = state_.updateCheckedAt;
            if (latest) {
                const auto current = text::split(CITRON_VERSION_STRING, '.');
                const auto remote = text::split(*latest, '.');
                bool newer = false;
                for (size_t i = 0; i < std::max(current.size(), remote.size()); ++i) {
                    const int c = i < current.size() ? std::atoi(current[i].c_str()) : 0;
                    const int r = i < remote.size() ? std::atoi(remote[i].c_str()) : 0;
                    if (r != c) {
                        newer = r > c;
                        break;
                    }
                }
                state_.updateStatus = newer ? UpdateStatus::Available : UpdateStatus::UpToDate;
                state_.updateVersion = text::toWide(*latest);
                log::info("update check: latest {} ({})", *latest, newer ? "newer" : "current");
                if (newer) {
                    showToast(std::vformat(strings().toastUpdate, std::make_wformat_args(state_.updateVersion)));
                } else if (manual) {
                    showToast(strings().toastUpToDate);
                }
            } else if (noRelease) {
                state_.updateStatus = UpdateStatus::UpToDate;
                state_.updateVersion.clear();
                log::info("update check: no release published yet");
                if (manual) {
                    showToast(strings().toastUpToDate);
                }
            } else {
                state_.updateStatus = UpdateStatus::Failed;
                log::warn("update check failed: {}", error ? error->summary() : "unknown");
                if (manual && error) {
                    showError(strings().updateFailed, *error);
                }
            }
            persistSettings();
            refreshUi();
        });
    });
}

void Application::openUrl(const wchar_t* url) {
    if (!platform::openUrl(url)) {
        log::warn("could not open {}", text::toUtf8(url));
    }
}

int Application::run() {
    if (!initialize()) {
        return 0;
    }
    window_.show();
    log::info("window shown at {:.1f} ms", log::elapsedMs());
    const int code = window_.runMessageLoop();
    scheduler_.shutdown();
    service_.reset();
    log::close();
    return code;
}

}
