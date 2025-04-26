#pragma once
#include "Common.h"
#include <cstdint>
#include <cstring>
#include <list>
#include <string_view>

namespace dm {

class MemChunk
{
private:
    const uint32_t  mCapacity = 2 * 1024 * 1024;    // 2MB
    uint32_t        mUsed = 0;

    char           *mData = nullptr;

public:
    MemChunk()
    : mData(new char[mCapacity])
    {

    }

    MemChunk(uint32_t capacity)
    : mCapacity(capacity)
    , mData(new char[mCapacity])
    {

    }

    ~MemChunk()
    {
        delete[] mData;
    }

    inline char *alloc(uint32_t size)
    {
        if (mCapacity - mUsed < size) {
            return nullptr;
        }
        char *pos = mData + mUsed;
        mUsed += size;
        return pos;
    }

    inline char *alloc(const std::string_view &data)
    {
        auto size = data.size();
        char *pos = alloc(size);
        if (pos) {
            memcpy(pos, data.data(), size);
        }
        return pos;
    }

    inline uint32_t getCapacity() const { return mCapacity; }
};


class MemChunkList
{
private:
    std::list<MemChunk *>   mChunks;

public:
    MemChunkList(uint32_t init_size = 0)
    {
        int64_t size = init_size;
        do  // make it non-empty
        {
            auto chunk = mChunks.emplace_back(new MemChunk());
            size -= chunk->getCapacity();
        } while (size > 0);
    }

    ~MemChunkList()
    {
        for (auto chunk : mChunks) {
            delete chunk;
        }
    }

    inline char *alloc(uint32_t size)
    {
        auto chunk = mChunks.back();
        char *pos = chunk->alloc(size);
        if (pos) {
            return pos;
        }

        chunk = mChunks.emplace_back(new MemChunk());
        return chunk->alloc(size);
    }

    inline char *alloc(const std::string_view &data)
    {
        uint32_t size = data.size();
        char *pos = alloc(size);
        memcpy(pos, data.data(), size);
        return pos;
    }
};

}