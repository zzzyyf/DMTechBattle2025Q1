#pragma once

#include "MemChunkList.h"
#include "RNG.h"
#include "RequestGenerator.h"
#include <cstdint>
#include <iostream>
#include <mutex>
#include <vector>

namespace dm {

template <uint32_t MaxLevel = 20>
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
    // for each level, go down when cur->next > key
// insert: find, get max level, insert node
    // 1. find
    // 2. get max *level*
    // 3. insert node up to *level*


template <class Node, uint32_t NextLevelP = 25>
class SkipListV1
{
private:
    static constexpr uint32_t   stMaxLevel = Node::stMaxLevel;

    std::vector<MemChunkList *>   mChunkLists;

    Node   *mHeader = nullptr;

    RNG     mRandom;

    std::mutex  mLock;

public:
    SkipListV1(uint32_t n_thrds, uint32_t mem_size_per_thread = 0)
    {
        for (uint32_t i = 0; i < n_thrds; i++)
        {
            mChunkLists.emplace_back(new MemChunkList(mem_size_per_thread));
        }

        // create a dummy header to make insert easier
        char *buf = mChunkLists[0]->alloc(sizeof(Node));
        assert(buf);
        Node *node = new (buf) Node{};
        node->mKey = "";    // smallest
        node->mValue = 0;

        mHeader = node;
        memset(node->mNext, 0, stMaxLevel * sizeof(Node *));
    }

    ~SkipListV1()
    {
        for (auto chunks : mChunkLists)
        {
            delete chunks;
        }
    }

    // must success
    void insert(const std::string_view &key, uint64_t value, uint32_t thrd_id)
    {
        // if (key == "16v7i")
        // {
        //     std::cout << "break at here!" << std::endl;
        // }
        assert(thrd_id < mChunkLists.size());
        char *buf = mChunkLists[thrd_id]->alloc(sizeof(Node));
        assert(buf);
        Node *node = new (buf) Node{};
#pragma message("Copy key into chunk list if needed.")
        node->mKey = key;
        node->mValue = value;

        Node *update_nodes[stMaxLevel];

        std::lock_guard lock(mLock);

        Node *prev = findPrev(key, update_nodes);
        assert(prev);

        auto n_lvl = getMaxLevel();
        // update_start_lvl = stMaxLevel - lvl
        // untouched lvls: [0, stMaxLevel - lvl)

        // upper untouched levels: nullptr
        if (n_lvl < stMaxLevel)
        {
            memset(node->mNext, 0, sizeof(Node *) * (stMaxLevel - n_lvl));
        }

        // lower levels: insert
        for (uint32_t l = stMaxLevel - n_lvl; l < stMaxLevel && update_nodes[l]; l++)
        {
            // assert(key > update_nodes[l]->mKey);
            // assert(!update_nodes[l]->mNext[l] || update_nodes[l]->mNext[l]->mKey > key);
            node->mNext[l] = update_nodes[l]->mNext[l];
            update_nodes[l]->mNext[l] = node;
        }
    }

    // must found
    uint64_t find(const std::string_view &key)
    {
        uint32_t l = 0;
        // uint32_t l = stMaxLevel - 1;

        // skip empty levels
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
                if ((current->mNext[l])) {
                    continue;
                }
            }

            // move down to find next until reached bottom
            while (true)
            {
                l++;

                assert(l < stMaxLevel);
                if ((current->mNext[l])) {
                    break;
                }
            }
        }

        assert(false);
        std::cerr << "missing key: " << key << "\n";
        exit(1);
    }

    void checkBottom()
    {
        uint32_t l = stMaxLevel - 1;
        auto p = mHeader;
        uint32_t n = 0;
        auto last_key = p->mKey;
        while (p->mNext[l])
        {
            if (last_key.compare(p->mNext[l]->mKey) < 0)
                // && n == RequestGenerator::GetIDFromKey(p->mNext[l]->mKey))
            {
                ++n;
                p = p->mNext[l];
                last_key = p->mKey;
                continue;
            }
            std::cout << "n: " << n << ", last_key: " << last_key << ", next_key: " << p->mNext[l]->mKey << std::endl;
            assert(false);
        }

        if (n != 2'000'000)
        {
            std::cout << "actual n: " << n << std::endl;
            assert(false);
        }
    }

private:
    Node* findPrev(const std::string_view &key, Node **update_nodes)
    {
        uint32_t l = 0;

        // skip empty levels
        for (; l < stMaxLevel && !mHeader->mNext[l]; l++) {
            update_nodes[l] = mHeader;
        }
        if unlikely(l == stMaxLevel) {
            return mHeader;
        }
        Node *current = mHeader;

        // current always < key
        while (current->mNext[l])
        {
            int32_t rslt = current->mNext[l]->mKey.compare(key);
            assert(rslt);   // duplicate key is not permitted

            // cur < key:
                // 1. move to next
                // 2. find next in current lvl
                // 3. find next in all lower levels
            // cur > key:
                // find next in all lower levels
            if (rslt < 0)
            {
                current = current->mNext[l];
                if (current->mNext[l]) {
                    continue;
                }
            }

            // move down to find next until reached bottom
            while (true)
            {
                update_nodes[l] = current;
                l++;

                if (l == stMaxLevel) {
                    return current;
                }
                if (current->mNext[l]) {
                    break;
                }
            }
        }

        for (; l < stMaxLevel; l++) {
            update_nodes[l] = current;
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