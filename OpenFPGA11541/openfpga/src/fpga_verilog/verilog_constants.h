#ifndef VERILOG_CONSTANTS_H
#define VERILOG_CONSTANTS_H

/* global parameters for dumping synthesizable verilog */

constexpr char* VERILOG_NETLIST_FILE_POSTFIX = ".v";
constexpr float VERILOG_SIM_TIMESCALE = 1e-9; // Verilog Simulation time scale (minimum time unit) : 1ns

constexpr char* VERILOG_TIMING_PREPROC_FLAG = "ENABLE_TIMING"; // the flag to enable timing definition during compilation
constexpr char* MODELSIM_SIMULATION_TIME_UNIT = "ms";

constexpr char* FABRIC_INCLUDE_VERILOG_NETLIST_FILE_NAME = "fabric_netlists.v";
constexpr char* TOP_VERILOG_TESTBENCH_INCLUDE_NETLIST_FILE_NAME_POSTFIX = "_include_netlists.v";
constexpr char* VERILOG_TOP_POSTFIX = "_top.v";
constexpr char* FORMAL_VERIFICATION_VERILOG_FILE_POSTFIX = "_top_formal_verification.v"; 
constexpr char* TOP_TESTBENCH_VERILOG_FILE_POSTFIX = "_top_tb.v"; /* !!! must be consist with the modelsim_testbench_module_postfix */ 
constexpr char* AUTOCHECK_TOP_TESTBENCH_VERILOG_FILE_POSTFIX = "_autocheck_top_tb.v"; /* !!! must be consist with the modelsim_autocheck_testbench_module_postfix */ 
constexpr char* RANDOM_TOP_TESTBENCH_VERILOG_FILE_POSTFIX = "_formal_random_top_tb.v"; 
constexpr char* DEFINES_VERILOG_FILE_NAME = "fpga_defines.v";
constexpr char* SUBMODULE_VERILOG_FILE_NAME = "sub_module.v";
constexpr char* LOGIC_BLOCK_VERILOG_FILE_NAME = "logic_blocks.v";
constexpr char* LUTS_VERILOG_FILE_NAME = "luts.v";
constexpr char* ROUTING_VERILOG_FILE_NAME = "routing.v";
constexpr char* MUX_PRIMITIVES_VERILOG_FILE_NAME = "mux_primitives.v";
constexpr char* MUXES_VERILOG_FILE_NAME = "muxes.v";
constexpr char* LOCAL_ENCODER_VERILOG_FILE_NAME = "local_encoder.v";
constexpr char* ARCH_ENCODER_VERILOG_FILE_NAME = "arch_encoder.v";
constexpr char* MEMORIES_VERILOG_FILE_NAME = "memories.v";
constexpr char* SHIFT_REGISTER_BANKS_VERILOG_FILE_NAME = "shift_register_banks.v";
constexpr char* WIRES_VERILOG_FILE_NAME = "wires.v";
constexpr char* ESSENTIALS_VERILOG_FILE_NAME = "inv_buf_passgate.v";
constexpr char* CONFIG_PERIPHERAL_VERILOG_FILE_NAME = "config_peripherals.v";
constexpr char* USER_DEFINED_TEMPLATE_VERILOG_FILE_NAME = "user_defined_templates.v";

constexpr char* VERILOG_MUX_BASIS_POSTFIX = "_basis";
constexpr char* VERILOG_MEM_POSTFIX = "_mem";

constexpr char* SB_VERILOG_FILE_NAME_PREFIX = "sb_";
constexpr char* LOGICAL_MODULE_VERILOG_FILE_NAME_PREFIX = "logical_tile_";
constexpr char* GRID_VERILOG_FILE_NAME_PREFIX = "grid_";

constexpr char* FORMAL_VERIFICATION_TOP_MODULE_POSTFIX = "_top_formal_verification";
constexpr char* FORMAL_VERIFICATION_TOP_MODULE_PORT_POSTFIX = "_fm";
constexpr char* FORMAL_VERIFICATION_TOP_MODULE_UUT_NAME = "U0_formal_verification";

constexpr char* FORMAL_RANDOM_TOP_TESTBENCH_POSTFIX = "_top_formal_verification_random_tb";

#define VERILOG_DEFAULT_SIGNAL_INIT_VALUE 0

#endif
