#pragma once
#include <cstdint>
#include <cstddef>

namespace ref 
{
    void hist256(const uint8_t* src, size_t n, uint32_t* hist);
    void hist16(const uint8_t* src, size_t n, uint32_t* hist);
}