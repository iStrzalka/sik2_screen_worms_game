#ifndef SK2_GENERATOR
#define SK2_GENERATOR

#include <cstdint>

class Generator {
    uint64_t r;
public:
    static uint64_t const MOD = 4294967291;
    static uint64_t const GEN = 279410273;

    Generator();

    void set_seed(uint64_t seed);
    uint32_t next();
};

#endif //SK2_GENERATOR_H
