#pragma once
#include <cassert>
#include <cstdint>
#include <sys/random.h>

namespace dm {

class RNG
{
private:
    __uint128_t mLehmerState = 0x1245983174;

public:
    RNG()
    {
        char *buf = (char *)&mLehmerState;
        int rslt = getrandom(buf, 16/*length*/, 0/*flags*/);
        assert(rslt == 16);
    }

    uint64_t rand()
    {
        mLehmerState *= 0xda942042e4dd58b5;
        return mLehmerState >> 64;
    }
};

}   // end of namespace dm
