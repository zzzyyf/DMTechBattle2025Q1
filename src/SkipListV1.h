#pragma once

#include "MemChunkList.h"
#include "RNG.h"
#include <cstdint>
#include <mutex>
#include <vector>

namespace dm {

template <uint32_t MaxLevel = 10>
struct Node
{
    static constexpr uint32_t   stMaxLevel = MaxLevel;

    std::string_view    mKey;
    uint64_t            mValue = 0;

    Node               *mNext[MaxLevel];
};

// skiplist
// only insert and find are needed.
// find: search in each level
// insert: find, get max level, insert node

template <class Node, uint32_t NextLevelP = 50>
class SkipListV1
{
private:
    static constexpr uint32_t   stMaxLevel = Node::stMaxLevel;

    std::vector<MemChunkList>   mChunkLists;

    Node   *mHeader = nullptr;

    RNG     mRandom;

    std::mutex  mLock;

public:
    SkipListV1(uint32_t n_thrds, uint32_t mem_size_per_thread = 0)
    {
        for (uint32_t i = 0; i < n_thrds; i++)
        {
            mChunkLists.emplace_back(mem_size_per_thread);
        }

        // create a dummy header to make insert easier
        char *buf = mChunkLists[0].alloc(sizeof(Node));
        assert(buf);
        Node *node = new (buf) Node{};
        node->mKey = "";    // smallest
        node->mValue = 0;

        mHeader = node;
        memset(node->mNext, 0, stMaxLevel * sizeof(Node *));
    }

    // must success
    void insert(const std::string_view &key, uint64_t value, uint32_t thrd_id)
    {
        assert(thrd_id < mChunkLists.size());
        char *buf = mChunkLists[thrd_id].alloc(sizeof(Node));
        assert(buf);
        Node *node = new (buf) Node{};
#pragma message("Copy key into chunk list if needed.")
        node->mKey = key;
        node->mValue = value;

        Node *update_nodes[stMaxLevel];

        std::lock_guard lock(mLock);

        Node *prev = findPrev(key, update_nodes);
        assert(prev);

        auto lvl = getMaxLevel();

        // update downwards
        memcpy(node->mNext + lvl, prev->mNext + lvl, lvl * sizeof(Node *));
        for (uint32_t l = lvl; l < stMaxLevel; l++) {
            prev->mNext[l] = node;
        }

        // update upwards
        for (int32_t l = lvl - 1; l >= 0 && update_nodes[l]; l--)
        {
            node->mNext[l] = update_nodes[l]->mNext[l];
            update_nodes[l]->mNext[l] = node;
        }
    }

    // must found
    uint64_t find(const std::string_view &key)
    {
        uint32_t l = 0;
        for (; l < stMaxLevel && !mHeader->mNext[l]; l++)
            ;

        assert(l < stMaxLevel);
        Node *current = mHeader;
        while (current->mNext[l])
        {
            int32_t rslt = current->mNext[l]->mKey.compare(key);
            if (!rslt)
            {
                return current->mNext[l]->mValue;
            }
            if (rslt < 0)
            {
                current = current->mNext[l];
                continue;
            }

            // move to the next level
            l++;
            assert(l < stMaxLevel); // must found
        }

        assert(false);
        return 0;
    }

private:
    Node* findPrev(const std::string_view &key, Node **update_nodes)
    {
        uint32_t l = 0;
        for (; l < stMaxLevel && !mHeader->mNext[l]; l++) {
            update_nodes[l] = nullptr;
        }

        if unlikely(l == stMaxLevel) {
            return mHeader;
        }
        Node *current = mHeader;
        while (current->mNext[l])
        {
            int32_t rslt = current->mNext[l]->mKey.compare(key);
            assert(rslt);   // duplicate key is not permitted

            if (rslt < 0)
            {
                current = current->mNext[l];
                continue;
            }

            // move to the next level
            update_nodes[l] = current;
            l++;
            if (l == stMaxLevel) {
                return current;
            }
        }

        return current;
    }

    uint32_t getMaxLevel()
    {
        for (uint32_t l = 1; l < stMaxLevel; l++)
        {
            if (mRandom.rand() % 100 > NextLevelP)
            {
                return l;
            }
        }
        return stMaxLevel;
    }
};

}