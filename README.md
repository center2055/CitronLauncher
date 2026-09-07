# Citron Launcher

A small native Windows launcher for Minecraft Bedrock Edition. It lists the
available game versions, downloads the one you pick, installs it through the
Windows package manager and starts it. Nothing else.

No browser runtime, no managed runtime, no bundled framework. The interface is
drawn with Direct2D and DirectWrite on a DirectComposition surface, networking
goes through WinHTTP, package handling goes through the Windows deployment API.

## What it does

- Play: shows the selected version and starts it.
- Versions: the version list with Release and Preview builds, search, filters,
  download progress and removal.
- Settings: language, close on launch, keep installer files, update checks,
  the storage folder and build information.

## Requirements

- Windows 10 (1903 or newer) or Windows 11, x64
- Gaming Services from the Microsoft Store
- A Microsoft account that owns Minecraft for Windows (Preview builds need
  the Preview entitlement)

The launcher does not touch licenses. If the account does not own the game,
Gaming Services refuses to install or start it and the launcher shows that
error.

## How a version gets from the list to the game

1. The version list comes from `catalog/versions.json`. A copy is embedded in
   the executable and a newer copy is fetched and cached when available.
2. Download writes to `downloads\<package>.msixvc.partial` with a sidecar file
   that allows the transfer to resume after a restart. Mirrors are rotated on
   failure.
3. The file is verified against the catalog checksum before it is moved to
   `installers\`. A package that fails verification is discarded.
4. Starting a version that is not the currently installed one for its channel
   deploys the package through `PackageManager`. Windows keeps one installed
   package per channel, so the installed build is replaced in place, which
   keeps worlds and settings and leaves the old build alone if the install
   fails. Packages kept in `installers\` can be switched back without
   downloading again. Release and Preview are independent, so a Preview build
   never touches a Release install.
5. The game is started with its package identity from the launcher, which
   skips the store launch helper and its forced update to the newest version.

## Storage

Everything lives under `%LocalAppData%\CitronLauncher` unless a different
root is chosen in Settings:

```text
settings.json
installers\   verified packages
downloads\    partial transfers
cache\        version list and deployment records
logs\         rotating log files
```

## Building

Visual Studio 2022 with the C++ workload and a Windows 10 SDK (10.0.22000 or
newer) are required. The project is plain CMake.

```bash
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is `build\Release\Citron.exe`. Tests build as `citron_tests`
and run with `ctest --test-dir build -C Release`.

## Updating the version list

`tools/update_catalog.py` rebuilds `catalog/versions.json` from a version index
and reads package sizes from the content delivery network:

```bash
python tools/update_catalog.py <index-url-or-file>
```

## License

GPL-3.0, see `LICENSE`. Bundled third party components are listed in
`THIRD_PARTY_NOTICES.md`.

Not affiliated with Mojang or Microsoft. Minecraft is a trademark of Mojang AB.
