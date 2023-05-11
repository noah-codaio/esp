// Copyright (c) 2011-2023 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include <random>
#include <sstream>
#include "system.hpp"

#include "../../../../../tools/esp-noxim/src/DataStructs.h"
#include "../../../../../tools/esp-noxim/src/Utils.h"

// Helper random generator
static std::uniform_real_distribution<float> *dis;
static std::random_device rd;
static std::mt19937 *gen;


static void init_random_distribution(void)
{
    const float LO = -10.0;
    const float HI = 10.0;

    gen = new std::mt19937(rd());
    dis = new std::uniform_real_distribution<float>(LO, HI);
}

static float gen_random_float(void)
{
    return (*dis)(*gen);
}



// Process
void system_t::config_proc()
{

    // Reset
    {
        conf_done.write(false);
        conf_info.write(conf_info_t());
        wait();
    }

    ESP_REPORT_INFO("reset done");

    // Config
    load_memory();
    {
        conf_info_t config;
        // Custom configuration
        /* <<--params-->> */
        config.do_peak = do_peak;
        config.do_bitrev = do_bitrev;
        config.log_len = log_len;
        config.batch_size = batch_size;

        wait(); conf_info.write(config);
        conf_done.write(true);
    }

    ESP_REPORT_INFO("config done");

    // Compute
    {
        // Print information about begin time
        sc_time begin_time = sc_time_stamp();
        ESP_REPORT_TIME(begin_time, "BEGIN - fft");

        // Wait the termination of the accelerator
        do { wait(); } while (!acc_done.read());
        debug_info_t debug_code = debug.read();

        // Print information about end time
        sc_time end_time = sc_time_stamp();
        ESP_REPORT_TIME(end_time, "END - fft");

        esc_log_latency(sc_object::basename(), clock_cycle(end_time - begin_time));
        wait(); conf_done.write(false);
    }

    // Validate
    {
        const int ERROR_COUNT_TH = 0.001;
        dump_memory(); // store the output in more suitable data structure if needed
        // check the results with the golden model
        if ((validate() / (batch_size * len)) > ERROR_COUNT_TH)
        {
            ESP_REPORT_ERROR("Exceeding error count threshold: validation failed!");
        } else
        {
            ESP_REPORT_INFO("Not exceeding error count threshold: validation passed!");
        }
    }

    // Conclude
    {
        bool data_stable = false;
        while (!data_stable) {
            wait();
            data_stable = true;
            for (Coord mem_coord : mem_tile_coords)
                if (noc->t[mem_coord.x][mem_coord.y]->pe->local_memory.size() < in_size + out_size)
                    data_stable = false;
        }
        std::cout << "in size: " << in_size << std::endl;
        std::cout << "out size: " << out_size << std::endl;
        std::cout << "0 size: " << noc->t[0][0]->pe->local_memory.size() << std::endl;
        std::cout << "5 size: " << noc->t[1][1]->pe->local_memory.size() << std::endl;
        sc_stop();
    }
}

// Functions
void system_t::load_memory()
{
    // Optional usage check
#ifdef CADENCE
    if (esc_argc() != 1)
    {
        ESP_REPORT_INFO("usage: %s\n", esc_argv()[0]);
        sc_stop();
    }
#endif

    // Input data and golden output (aligned to DMA_WIDTH makes your life easier)
#if (DMA_WORD_PER_BEAT == 0)
    in_words_adj = 2 * len * batch_size;
    out_words_adj = 2 * len * batch_size;
#else
    in_words_adj = round_up(2 * len * batch_size, DMA_WORD_PER_BEAT);
    out_words_adj = round_up(2 * len * batch_size, DMA_WORD_PER_BEAT);
#endif

    in_size = in_words_adj;
    out_size = out_words_adj;

    init_random_distribution();
    in = new float[in_size];
    for (int j = 0; j < 2 * len * batch_size; j++) {
        in[j] = gen_random_float();
    }

    // preprocess with bitreverse (fast in software anyway)
    // TODO since batch_size was introduced this is not correct anymore
    if (!do_bitrev)
        fft_bit_reverse(in, len, log_len);

    // Compute golden output
    gold = new float[out_size];
    memcpy(gold, in, out_size * sizeof(float));
    for (int j = 0; j < batch_size; j++) {
        fft_comp(&gold[j * 2 * len], len, log_len,  -1,  do_bitrev);
    }

    // Memory initialization:
#if (DMA_WORD_PER_BEAT == 0)
    for (int i = 0; i < in_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv(fp2bv<FPDATA, WORD_SIZE>(FPDATA(in[i])));
        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            mem[DMA_BEAT_PER_WORD * i + j] = data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH);
    }
#else
    for (int i = 0; i < in_size / DMA_WORD_PER_BEAT; i++)  {
        sc_dt::sc_bv<DMA_WIDTH> data_bv;
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            data_bv.range((j+1) * DATA_WIDTH - 1, j * DATA_WIDTH) = fp2bv<FPDATA, WORD_SIZE>(FPDATA(in[i * DMA_WORD_PER_BEAT + j]));
        mem[i] = data_bv;
    }
#endif

    // Initialize memory tiles and place read requests from accelerator tiles
    for (Coord mem_coord : mem_tile_coords)
        for (int i = 0; i < in_size; i++) {
            int data_int = (int) in[i];
            noc->t[mem_coord.x][mem_coord.y]->pe->pushDataToLocalMemory(i, 1, &data_int);
        }
    for (Coord *acc_mem_pair : acc_mem_pairs)
        noc->t[acc_mem_pair[0].x][acc_mem_pair[0].y]->pe->pushReadRequest(coord2Id(acc_mem_pair[1]), 0, in_size);

    ESP_REPORT_INFO("load memory completed");
}

void system_t::dump_memory()
{
    // Get results from memory
    out = new float[out_size];
    uint32_t offset = 0;

#if (DMA_WORD_PER_BEAT == 0)
    offset = offset * DMA_BEAT_PER_WORD;
    for (int i = 0; i < out_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv;

        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH) = mem[offset + DMA_BEAT_PER_WORD * i + j];

        FPDATA out_fx = bv2fp<FPDATA, WORD_SIZE>(data_bv);
        out[i] = (float) out_fx;
    }
#else
    offset = offset / DMA_WORD_PER_BEAT;
    for (int i = 0; i < out_size / DMA_WORD_PER_BEAT; i++)
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++) {
            FPDATA out_fx = bv2fp<FPDATA, WORD_SIZE>(mem[offset + i].range((j + 1) * DATA_WIDTH - 1, j * DATA_WIDTH));
            out[i * DMA_WORD_PER_BEAT + j] = (float) out_fx;
        }
#endif
    
    // Save output data to accelerator tiles and place write requests to memory tiles
    for (Coord *acc_mem_pair : acc_mem_pairs) {
        for (int i = 0; i < out_size; i++) {
            int data_int = (int) out[i];
            noc->t[acc_mem_pair[0].x][acc_mem_pair[0].y]->pe->pushDataToLocalMemory(in_size + i, 1, &data_int);
        }
        noc->t[acc_mem_pair[0].x][acc_mem_pair[0].y]->pe->pushWriteRequest(coord2Id(acc_mem_pair[1]), in_size, out_size);
    }

    ESP_REPORT_INFO("dump memory completed");
}


int system_t::validate()
{
    // Check for mismatches
    uint32_t errors = 0;
    const float ERR_TH = 0.05;

    for (int j = 0; j < batch_size * 2 * len; j++) {
        // std::cout << j << " : " << gold[j] << " : " << out[j] << std::endl;
        if ((fabs(gold[j] - out[j]) / fabs(gold[j])) > ERR_TH) {
            errors++;
        }
    }

    ESP_REPORT_INFO("Relative error > %.02f for %d output values out of %d\n",
                    ERR_TH, errors, batch_size * 2 * len);

    delete [] in;
    delete [] out;
    delete [] gold;

    return errors;
}
