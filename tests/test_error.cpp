#include "check.h"

#include "core/Error.h"

using namespace citron;

TEST_CASE(error_hresult_formatting) {
    Error e = Error::fromHresult(ErrorCategory::Package, "deploy", static_cast<long>(0x80073CF6), "Minecraft could not be registered.");
    CHECK_EQ(e.codeText(), "0x80073CF6");
    CHECK(e.category == ErrorCategory::Package);
    CHECK(!e.detail.empty());
    CHECK(e.summary().find("0x80073CF6") != std::string::npos);
    CHECK(!e.isCancelled());
}

TEST_CASE(error_win32_mapping) {
    Error e = Error::fromWin32(ErrorCategory::Filesystem, "open", 2, "The file could not be opened.");
    CHECK_EQ(e.codeText(), "0x80070002");
    CHECK(!e.detail.empty());
}

TEST_CASE(error_cancelled_and_names) {
    Error c = Error::cancelled("download");
    CHECK(c.isCancelled());
    CHECK_EQ(c.codeText(), "");
    CHECK_EQ(categoryName(ErrorCategory::Gdk), "gdk");
    CHECK_EQ(categoryName(ErrorCategory::Network), "network");
    Result<int> ok = 3;
    CHECK(ok.has_value());
    Result<int> bad = std::unexpected(c);
    CHECK(!bad.has_value());
    CHECK(bad.error().isCancelled());
}
