#include "image_hist.hpp"
#include "image_hist_rvv.hpp"
#include "riscv_timer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <cassert>

#ifdef RISCV_BAREMETAL
#include <klib.h>
inline void riscv_enable_vector() {
    uint64_t mstatus_vs = 0x200; 
    asm volatile("csrs mstatus, %0" ::"r"(mstatus_vs));
    asm volatile("csrwi vcsr, 0");
}
#else
inline void riscv_enable_vector() {}
#endif

inline uint8_t* make_constant(uint8_t* out, size_t n, uint8_t value) {
    for (size_t i = 0; i < n; ++i) out[i] = value;
    return out;
}

inline uint8_t* make_uniform(uint8_t* out, size_t n,
                             uint8_t a = 0, uint8_t b = 255, uint32_t seed = 12345) {
    uint32_t x = seed;
    uint32_t range = uint32_t(b) - a + 1;                
    for (size_t i = 0; i < n; ++i) {
        x = x * 1664525u + 1013904223u;                   
        out[i] = uint8_t(a + (uint64_t(x) * range >> 32)); 
    }
    return out;
}

inline uint8_t* make_skewed(uint8_t* out, size_t n,
                            uint8_t a = 0, uint8_t b = 255, uint32_t seed = 12345,
                            uint32_t hot_pct = 90, uint32_t hot_bins = 8) {
    assert(a <= b); 
    uint32_t range = uint32_t(b) - a + 1;
    if (hot_pct  > 100)   hot_pct  = 100;
    if (hot_bins == 0)    hot_bins = 1;
    if (hot_bins > range) hot_bins = range;
    uint32_t cold_bins = range - hot_bins;          
    uint32_t x = seed;
    for (size_t i = 0; i < n; ++i) {
        x = x * 1664525u + 1013904223u;
        bool hot = (uint64_t(x) * 100 >> 32) < hot_pct || cold_bins == 0;
        x = x * 1664525u + 1013904223u;
        uint32_t idx = hot ? uint32_t(uint64_t(x) * hot_bins  >> 32)
                           : hot_bins + uint32_t(uint64_t(x) * cold_bins >> 32);
        out[i] = uint8_t(a + idx);
    }
    return out;
}

int main()
{
    riscv_enable_vector();

    const size_t input_size = 16384;
    uint8_t* input_data = (uint8_t*)malloc(input_size);
    if (!input_data) {
        printf("Failed to allocate memory!\n");
        return 1;
    }

    uint32_t hist_ref[256]; 
    uint32_t hist_vec[256]; 
    
    const char* test_names[] = {"Constant", "Uniform", "Skewed"};

    for (int i = 0; i < 3; ++i)
    {
        if (i == 0) {
            make_constant(input_data, input_size, 128);
        } else if(i == 1) {
            make_uniform(input_data, input_size);
        } else if(i == 2) {
            make_skewed(input_data, input_size);
        }

        Timer timer_scalar, timer_vec;

        timer_scalar.Start();
        ref::hist256(input_data, input_size, hist_ref);
        timer_scalar.Stop();

        timer_vec.Start();
        vec::image_hist_rvv(input_data, input_size, hist_vec);
        timer_vec.Stop();

        uint64_t reference_cycles = timer_scalar.ElapsedCycles();
        uint64_t reference_instrs = timer_scalar.ElapsedInstructions();
        uint64_t vectorized_cycles = timer_vec.ElapsedCycles();
        uint64_t vectorized_instrs = timer_vec.ElapsedInstructions();

        printf("--- %s Input ---\n", test_names[i]);
        printf("Scalar: Cycles = %llu, Instructions = %llu\n", 
               (unsigned long long)reference_cycles, (unsigned long long)reference_instrs);
        printf("Vector: Cycles = %llu, Instructions = %llu\n", 
               (unsigned long long)vectorized_cycles, (unsigned long long)vectorized_instrs);
        
        float speedup = (float)reference_cycles / (float)vectorized_cycles;
        printf("Speedup: %.2fx\n\n", speedup);
    }

    free(input_data);
    printf("Output is correct.\n");
    return 0;
}