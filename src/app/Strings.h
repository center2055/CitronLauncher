#pragma once

#include <string>
#include <string_view>

namespace citron {

struct Strings {
    std::wstring tabPlay, tabVersions, tabSettings;
    std::wstring launcher, toggleTheme, minimize, maximize, restore, close;
    std::wstring gameName, launch, launching, preparing, manageVersions, noVersion, noVersionHint, downloadingUpper, installedUpper;
    std::wstring searchVersion, refresh, filterAll, filterRelease, filterPreview, filterInstalled;
    std::wstring release, preview, statusInstalled, statusNotDownloaded, statusDownloading, statusVerifying, statusInstalling, statusActive, statusFailed, statusUnavailable;
    std::wstring download, cancel, remove, confirmRemove, retry, dismiss, noResults, versionsEmpty;
    std::wstring settingsUpper, pathsUpper, aboutUpper;
    std::wstring language, languageSub, closeOnLaunch, closeOnLaunchSub, keepInstallers, keepInstallersSub, checkUpdates, checkUpdatesSub;
    std::wstring rootDir, browse, installers, logs, apply, reset, openFolder;
    std::wstring aboutSub, upToDate, updateAvailable, updateFailed, notChecked, checkedAgo, checkNow, checking, build, links, website, licenses, legal, disclaimer;
    std::wstring toastLaunch, toastSelect, toastInstalled, toastRemoved, toastUpToDate, toastUpdate, toastSettingsSaved, toastPathReset, toastCancelled;
    std::wstring errLaunchTitle, errInstallTitle, errRemoveTitle, errDetails, errGamingServices, errGetGamingServices, errRunning;
    std::wstring replaceTitle, replaceBody, replaceStore, replaceKeep, replaceConfirm, dialogCancel;
    std::wstring envMissing, envGamingServices, envGameInput, envVcLibs, envAppRuntime, envHint;
    std::wstring minutesAgo, hoursAgo, justNow;
};

const Strings& stringsFor(std::string_view language);

}
