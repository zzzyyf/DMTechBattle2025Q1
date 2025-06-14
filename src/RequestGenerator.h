#pragma once
#include "Common.h"
#include "MemChunkList.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>


namespace dm {

class RequestGenerator
{
public:
    std::string_view    mKey;
    uint64_t            mValue = 0;

private:
    std::string     mAlphabets;
    RNG             mRandom;

    uint32_t        mNumberEntries = 0;
    uint32_t        mThreadID = 0;

    MemChunkList    mChunks;

public:
    // RequestGenerator(std::atomic<bool> *stopped)
    RequestGenerator(uint32_t n_entries, uint32_t thrd_id)
    : mNumberEntries(n_entries)
    , mThreadID(thrd_id)
    , mChunks(n_entries * (request_max_size + 1))
    {
        init();
    }

    ~RequestGenerator()
    {

    }

private:
    bool init()
    {
        int pos = 0;
        while (pos < 64)
        {
            for (char c = '0'; c <= '9' && pos < 64; ++c, ++pos) {
                mAlphabets.push_back(c);
            }
            for (char c = 'a'; c <= 'z' && pos < 64; ++c, ++pos) {
                mAlphabets.push_back(c);
            }
        }

        return true;
    }

public:
    void generateRequest()
    {
        uint32_t size = mRandom.rand() % (request_max_size + 1 - request_min_size) + request_min_size;

        char *data = mChunks.alloc(size + 1);    // for null termination
        assert(data);
        mKey = std::string_view(data, size);
    #pragma message("Make unique header to avoid duplication")
        char *pos = data;
        for (int l = size; l > 0; l -= 10)
        {
            generateSequence(pos, l >= 10 ? 10 : l);
        }
        data[size] = 0;

        mValue = mRandom.rand();
    }

    // void generateRequest2()
    // {
    //     uint32_t size = 5;

    //     char *data = mChunks.alloc(size + 1);    // for null termination
    //     assert(data);
    //     mKey = std::string_view(data, size);
    //     char *pos = data;
    //     generateFromID(pos, mNumberEntries++);
    //     data[size] = 0;

    //     mValue = mRandom.rand();
    // }

    void generateSequence(char *&pos, int size)
    {
        // 0-9a-z has 36 characters.
        // 36^10 < 64^10 = 2^60, which is in the range of int64_t,
        // so we can use 1 rand() call to generate up to 10 characters at once.
        assert(size <= 10);

        uint64_t result = mRandom.rand();
        for (; size > 0; --size)
        {
            uint64_t bits = result & 0x0000'0000'0000'003Fllu;
            *pos = mAlphabets[bits];

            result = result >> 6;
            ++pos;
        }
    }

    void generateFromID(char *&pos, int id)
    {
        // 0-9a-z has 36 characters.
        // 2'000'000 < 36^5, we need 5 chars for max
        int divider = 36 * 36 * 36 * 36;
        for (uint32_t i = 0; i < 5; i++)
        {
            *pos = mAlphabets[id / divider];
            ++pos;

            id = id % divider;
            divider /= 36;
        }
    }

    static uint32_t GetIDFromKey(const std::string_view &key)
    {
        uint32_t id = 0;
        uint32_t base = 1;
        for (int32_t i = key.size() - 1; i >= 0; i--)
        {
            char c = key[i];
            id += (c < 'a' ? c - '0' : 10 + c - 'a') * base;
            base *= 36;
        }
        return id;
    }

};

}