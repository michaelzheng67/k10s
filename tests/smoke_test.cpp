#include <catch2/catch_test_macros.hpp>

// Proves the test harness builds and runs. Real tests arrive with M1.
TEST_CASE("test harness works") {
    REQUIRE(1 + 1 == 2);
}
