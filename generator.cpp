#include "generator.h"

Generator::Generator() = default;

void Generator::set_seed(uint64_t seed) {
    r = seed;
}

/* Returns next value from the generator */
uint32_t Generator::next() {
    auto old_r = r;
    r = (r * GEN) % MOD;
    return old_r;
}
