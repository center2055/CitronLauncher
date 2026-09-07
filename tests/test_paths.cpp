#include "check.h"

#include "core/PathSafety.h"
#include "core/Paths.h"

using namespace citron;

TEST_CASE(paths_safe_names) {
    CHECK(pathsafety::isSafeFileName("Microsoft.MinecraftUWP_1.26.4501.0_x64__8wekyb3d8bbwe.msixvc"));
    CHECK(pathsafety::isSafeFileName("settings.json"));
    CHECK(!pathsafety::isSafeFileName(""));
    CHECK(!pathsafety::isSafeFileName(".."));
    CHECK(!pathsafety::isSafeFileName("."));
    CHECK(!pathsafety::isSafeFileName("a/b"));
    CHECK(!pathsafety::isSafeFileName("a\\b"));
    CHECK(!pathsafety::isSafeFileName("con"));
    CHECK(!pathsafety::isSafeFileName("CON.txt"));
    CHECK(!pathsafety::isSafeFileName("trailing."));
    CHECK(!pathsafety::isSafeFileName("bad:name"));
    CHECK(!pathsafety::isSafeFileName(std::string("x\x01y")));
}

TEST_CASE(paths_inside_checks) {
    const std::filesystem::path root = L"C:\\Users\\Someone\\AppData\\Local\\CitronLauncher";
    CHECK(pathsafety::isInside(root, root));
    CHECK(pathsafety::isInside(root, root / L"installers" / L"a.msixvc"));
    CHECK(pathsafety::isInside(root, L"c:\\users\\someone\\appdata\\local\\citronlauncher\\logs"));
    CHECK(!pathsafety::isInside(root, L"C:\\Users\\Someone\\AppData\\Local\\CitronLauncherX"));
    CHECK(!pathsafety::isInside(root, root / L".." / L"Other"));
    CHECK(!pathsafety::isInside(root, L"C:\\Windows"));
    CHECK(!pathsafety::isInside(root, L""));
}

TEST_CASE(paths_child_of) {
    const std::filesystem::path root = L"D:\\data\\root";
    auto ok = pathsafety::childOf(root, "file.msixvc");
    CHECK(ok.has_value());
    CHECK(pathsafety::isInside(root, *ok));
    CHECK(!pathsafety::childOf(root, "..\\escape").has_value());
    CHECK(!pathsafety::childOf(root, "../escape").has_value());
    CHECK(!pathsafety::childOf(root, "").has_value());
}

TEST_CASE(paths_layout) {
    auto layout = paths::layoutFor(L"D:\\citron");
    CHECK_EQ(layout.settingsFile, std::filesystem::path(L"D:\\citron\\settings.json"));
    CHECK_EQ(layout.installers, std::filesystem::path(L"D:\\citron\\installers"));
    CHECK_EQ(layout.logs, std::filesystem::path(L"D:\\citron\\logs"));
    CHECK(!paths::localAppData().empty());
}
