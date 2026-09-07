# Bedrock packages on Windows

Notes on the mechanisms the launcher relies on. All of them are observable
with the package manager and the deployment event log; none of them require
bypassing Gaming Services.

## Package identity

| Channel | Package name                     | Family                                         |
| ------- | -------------------------------- | ---------------------------------------------- |
| Release | Microsoft.MinecraftUWP           | Microsoft.MinecraftUWP_8wekyb3d8bbwe           |
| Preview | Microsoft.MinecraftWindowsBeta   | Microsoft.MinecraftWindowsBeta_8wekyb3d8bbwe   |

The package version encodes the game version: display `1.26.45.01` is
package `1.26.4501.0` (third component times 100 plus the fourth). Older
packages spell the release name in capitals; comparisons are case
insensitive.

## Packages

Game builds ship as `.msixvc` files, XVC containers with an encrypted payload.
The launcher never opens them. They are handed to
`PackageManager.AddPackageAsync`, which routes them to the Gaming Services
data source. Deployment stages the content into `WindowsApps`, generates the
package manifest (the application executable becomes the store launch helper)
and registers the package for the user. On this machine a 2 GB package
deploys in about a minute and reports progress through `DeploymentProgress`.

The deployment occasionally fails to register with "the splash screen image
cannot be loaded" although the file is present. The same call succeeds on a
retry, so the launcher tries up to three times.

Removal uses `RemovePackageAsync` with `PreserveRoamableApplicationData`.
`PreserveApplicationData` is only valid for development mode packages and
fails with 0x80073CFA on store signed ones. Game data lives in
`%AppData%\Minecraft Bedrock` and is not part of the package either way.

Windows keeps one registered package per family and user, so switching
between versions of one channel always means remove and deploy. Deployed
executables are not readable by the user account, which rules out copying a
deployment into a loose folder.

## Starting the game

Activating the package's app list entry runs the store launch helper. The
helper checks for updates and installs the newest version before the game
starts, which defeats pinning a version. The launcher therefore starts the
game executable inside the package identity through the desktop package
activator (`CLSID_DesktopAppxActivator`, the object the Appx PowerShell
module uses for `Invoke-CommandInDesktopPackage`). The executable name is
read from `MicrosoftGame.Config`. The helper path is used as a fallback and
whenever GameInput is missing, because the helper installs the redistributable
that ships inside the package.

## Prerequisites

- Gaming Services (`Microsoft.GamingServices`), required for deployment,
  licensing and start
- GameInput, detected through `HKLM\SOFTWARE\Microsoft\GameInput`
- `Microsoft.VCLibs.140.00.UWPDesktop` and `Microsoft.WindowsAppRuntime.1.8`,
  declared as package dependencies and reported by the deployment error when
  missing

## Catalog format

```json
{
  "format": 1,
  "updated": 1788730454,
  "hosts": ["assets1.xboxlive.com", "assets2.xboxlive.com"],
  "versions": [
    {
      "version": "1.26.45.01",
      "channel": "release",
      "released": 1761667200,
      "size": 2490000000,
      "md5": "d1a1c8526374596be545b046db3f6e47",
      "path": "/Z/.../Microsoft.MinecraftUWP_1.26.4501.0_x64__8wekyb3d8bbwe.msixvc"
    }
  ]
}
```

Every host is tried in order with the same path. Entries with an invalid
version, checksum or path are dropped while parsing. The content delivery
hosts answer over plain HTTP; integrity comes from the checksum in the
catalog, which itself is fetched over HTTPS.
