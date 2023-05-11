// Copyright (c) 2011-2023 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "system.hpp"

#include "../../../../../tools/esp-noxim/src/NoC.h"
#include "../../../../../tools/esp-noxim/src/ConfigurationManager.h"
#include "../../../../../tools/esp-noxim/src/GlobalStats.h"
#include "../../../../../tools/esp-noxim/src/GlobalParams.h"

#define RESET_PERIOD (30 * CLOCK_PERIOD)

system_t * testbench = NULL;

unsigned int drained_volume;
NoC *noc;

extern void esc_elaborate()
{
	// Creating the whole system
	testbench = new system_t("testbench");
}

extern void esc_cleanup()
{
	// Deleting the system
	delete testbench;
}

int sc_main(int argc, char *argv[])
{
	// Kills a Warning when using SC_CTHREADS
	//sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
	sc_report_handler::set_actions (SC_WARNING, SC_DO_NOTHING);

	esc_initialize(argc, argv);
	esc_elaborate();

	sc_clock        clk("clk", CLOCK_PERIOD, SC_PS);
	sc_signal<bool> rst("rst");
	sc_signal<bool> pos_rst("pos_rst");

	testbench->clk(clk);
	testbench->rst(rst);

	configure(argc, argv);
	noc = new NoC("NoC");
	noc->clock(clk);
	noc->reset(pos_rst);
	testbench->noc = noc;

	// sc_start(RESET_PERIOD, SC_PS);

	rst.write(false);
	pos_rst.write(true);
	cout << "Reset for " << (int)(GlobalParams::reset_time) << " cycles... ";
    srand(GlobalParams::rnd_generator_seed);
    sc_start(GlobalParams::reset_time, SC_NS);

	rst.write(true);
	pos_rst.write(false);
    cout << " done! " << endl;
    // cout << " Now running for " << GlobalParams::simulation_time << " cycles..." << endl;
    // sc_start(GlobalParams::simulation_time, SC_NS);

	sc_start();

    cout << "Noxim simulation completed.";
    cout << " (" << sc_time_stamp().to_double() / GlobalParams::clock_period_ps << " cycles executed)" << endl;
    cout << endl;

	GlobalStats gs(noc);
    gs.showStats(std::cout, GlobalParams::detailed);

    esc_cleanup();

	return 0;
}
