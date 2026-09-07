#include "check.h"

#include "core/Format.h"

using namespace citron;

TEST_CASE(format_bytes) {
    CHECK_EQ(format::bytes(0), "0 B");
    CHECK_EQ(format::bytes(512), "512 B");
    CHECK_EQ(format::bytes(2048), "2 KB");
    CHECK_EQ(format::bytes(12u * 1024 * 1024 + 400 * 1024), "12.4 MB");
    CHECK_EQ(format::bytes(2142892032ull), "2.00 GB");
    CHECK_EQ(format::bytes(1331439862ull), "1.24 GB");
}

TEST_CASE(format_speed_and_eta) {
    CHECK_EQ(format::speed(12.4 * 1024 * 1024), "12.4 MB/s");
    CHECK_EQ(format::speed(-5), "0 B/s");
    CHECK_EQ(format::eta(26), "0:26");
    CHECK_EQ(format::eta(185), "3:05");
    CHECK_EQ(format::eta(3723), "1:02:03");
    CHECK_EQ(format::eta(-1), "--:--");
}

TEST_CASE(format_percent) {
    CHECK_EQ(format::percent(0.68), "68 %");
    CHECK_EQ(format::percent(1.5), "100 %");
    CHECK_EQ(format::percent(-0.2), "0 %");
}
