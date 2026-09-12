#pragma once

#include <string>
#include <string_view>

namespace citron {

struct Strings {
    std::wstring tabPlay, tabVersions, tabSettings;
    std::wstring launcher, toggleTheme, minimize, maximize, restore, close;
    std::wstring gameName, launch, launching, manageVersions, noVersion, noVersionHint, downloadingUpper;
    std::wstring searchVersion, refresh, filterAll, filterRelease, filterPreview, filterInstalled;
    std::wstring release, preview, statusInstalled, statusDownloaded, statusNotDownloaded, statusDownloading, statusVerifying, statusExtracting, statusRemoving, statusActive, statusFailed, statusUnavailable;
    std::wstring download, install, cancel, remove, confirmRemove, retry, dismiss, noResults, versionsEmpty;
    std::wstring settingsUpper, pathsUpper, aboutUpper;
    std::wstring language, languageSub, closeOnLaunch, closeOnLaunchSub, checkUpdates, checkUpdatesSub;
    std::wstring rootDir, browse, installers, logs, apply, reset;
    std::wstring aboutSub, upToDate, updateAvailable, updateFailed, notChecked, checkedAgo, checkNow, checking, links;
    std::wstring toastLaunch, toastSelect, toastInstalled, toastDownloaded, toastRemoved, toastUpToDate, toastUpdate, toastSettingsSaved, toastPathReset, toastCancelled;
    std::wstring errLaunchTitle, errInstallTitle, errRemoveTitle, errDetails, errGamingServices, dialogCancel;
    std::wstring envGamingServices, envHint;
    std::wstring minutesAgo, hoursAgo, justNow;
};

const Strings& stringsFor(std::string_view language);

}
