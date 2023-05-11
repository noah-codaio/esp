// Copyright (c) 2011-2023 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __SYSTEM_HPP__
#define __SYSTEM_HPP__

#include "fft_conf_info.hpp"
#include "fft_debug_info.hpp"
#include "fft.hpp"
#include "fft_directives.hpp"
#include "fft_test.hpp"

#include "esp_templates.hpp"

#include "../../../../../tools/esp-noxim/src/NoC.h"
#include "../../../../../tools/esp-noxim/src/DataStructs.h"

const size_t _MEM_SIZE = 262144 / (DMA_WIDTH/8);

#include "core/systems/esp_system.hpp"

#ifdef CADENCE
#include "fft_wrap.h"
#endif

class system_t : public esp_system<DMA_WIDTH, _MEM_SIZE>
{
public:

    // ACC instance
#ifdef CADENCE
    fft_wrapper *acc;
#else
    fft *acc;
#endif

    // Constructor
    SC_HAS_PROCESS(system_t);
    system_t(sc_module_name name)
        : esp_system<DMA_WIDTH, _MEM_SIZE>(name)
    {
        // ACC
#ifdef CADENCE
        acc = new fft_wrapper("fft_wrapper");
#else
        acc = new fft("fft_wrapper");
#endif
        // Binding ACC
        acc->clk(clk);
        acc->rst(acc_rst);
        acc->dma_read_ctrl(dma_read_ctrl);
        acc->dma_write_ctrl(dma_write_ctrl);
        acc->dma_read_chnl(dma_read_chnl);
        acc->dma_write_chnl(dma_write_chnl);
        acc->conf_info(conf_info);
        acc->conf_done(conf_done);
        acc->acc_done(acc_done);
        acc->debug(debug);

        /* <<--params-default-->> */
        log_len = 8;
        len = (1 << log_len);
        do_peak = 0;
        do_bitrev = 1;
        batch_size = 4;
    }

    // Processes

    // Configure accelerator
    void config_proc();

    // Load internal memory
    void load_memory();

    // Dump internal memory
    void dump_memory();

    // Validate accelerator results
    int validate();

    // Accelerator-specific data
    /* <<--params-->> */
    int32_t do_peak;
    int32_t do_bitrev;
    int32_t len;
    int32_t log_len;
    int32_t batch_size;

    uint32_t in_words_adj;
    uint32_t out_words_adj;
    uint32_t in_size;
    uint32_t out_size;
    float *in;
    float *out;
    float *gold;

    NoC *noc;

    // TEST 1
    Coord mem_tile_coords[2] = {
        Coord(0, 0), 
        Coord(3, 3)
    };
    Coord acc_mem_pairs[4][2] = {
        {Coord(1, 1), Coord(0, 0)},
        {Coord(2, 1), Coord(0, 0)},
        {Coord(1, 2), Coord(3, 3)},
        {Coord(2, 2), Coord(3, 3)}
    };

    // TEST 2
    // Coord mem_tile_coords[2] = {
    //     Coord(0, 0), 
    //     Coord(3, 3)
    // };
    // Coord acc_mem_pairs[4][2] = {
    //     {Coord(0, 3), Coord(0, 0)},
    //     {Coord(1, 2), Coord(0, 0)},
    //     {Coord(2, 1), Coord(3, 3)},
    //     {Coord(3, 0), Coord(3, 3)}
    // };

    // TEST 3
    // Coord mem_tile_coords[2] = {
    //     Coord(1, 1), 
    //     Coord(2, 2)
    // };
    // Coord acc_mem_pairs[4][2] = {
    //     {Coord(0, 0), Coord(1, 1)},
    //     {Coord(3, 0), Coord(1, 1)},
    //     {Coord(0, 3), Coord(2, 2)},
    //     {Coord(3, 3), Coord(2, 2)}
    // };

    // TEST 4
    // Coord mem_tile_coords[2] = {
    //     Coord(1, 1), 
    //     Coord(2, 2)
    // };
    // Coord acc_mem_pairs[4][2] = {
    //     {Coord(0, 1), Coord(1, 1)},
    //     {Coord(2, 1), Coord(1, 1)},
    //     {Coord(1, 2), Coord(2, 2)},
    //     {Coord(3, 2), Coord(2, 2)}
    // };

    // Other Functions
};

#endif // __SYSTEM_HPP__
