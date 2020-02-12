#pragma once


#include <vector>
#include <limits>
#include <array>
#include <map>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <atomic>
#include <assert.h>


typedef uint32_t VeIndex;
constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();

typedef uint64_t VeHandle;
constexpr VeHandle VE_NULL_HANDLE = std::numeric_limits<VeHandle>::max();

typedef std::size_t VeSize;
constexpr VeSize VE_NULL_SIZE = std::numeric_limits<VeSize>::max();


