#pragma once
#include "RNG.h"

#include <memory>

namespace dm {

template <class T>
using Ptr = std::shared_ptr<T>;

constexpr static uint32_t request_min_size = 20;
constexpr static uint32_t request_max_size = 60;
static_assert(request_min_size <= request_max_size, "`request_min_size` should not be greater than `request_max_size`!");

}   // end of namespace dm;

#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
