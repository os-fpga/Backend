# generate PrimeTime script for user design bitstream mapped to fabric
import argparse
import xml.etree.ElementTree as ET
import re
import logging
import inspect


def generate_pre_pd_set_case_analysis(xml_bitstream):
    # emit set_case analysis for each CCFF bit in fabric_bitstream.xml
    logging.debug("entering generate_pre_pd_set_case_analysis")
    snippet = '\n'
    newline = '\n' #fstrings don't like backslashes
    tree = ET.parse(xml_bitstream)
    root = tree.getroot()
    for el in root.iter(tag='bit'):
        path = el.attrib['path']
        path = path.replace('fpga_top.','')
        path = path.replace('.','/')
        value = el.attrib['value']
        snippet += f"set_case_analysis {value} {path}{newline}"
    return snippet

def generate_post_pd_set_case_analysis(xml_bitstream):
    # emit set_case analysis for each CCFF bit in post-PD fabric_bitstream.xml
    # WIP proceed with caution
    # this should be used on recompiled bitstream generated by openfpga-physical
    # I think the simultaneous use of path and new path solves in the xml solves the reordering of the bitstream
    # but it's fairly low confidence right now
    logging.debug("entering generate_post_pd_set_case_analysis")
    snippet = '\n'
    newline = '\n' #fstrings don't like backslashes
    tree = ET.parse(xml_bitstream)
    root = tree.getroot()
    for el in root.iter(tag='bit'):
        new_path = el.attrib['new_path']
        new_path = new_path.replace('fpga_top.','')
        new_path = new_path.replace('.','/')
        value = el.attrib['value']
        snippet += f"set_case_analysis {value} {new_path}{newline}"
    return snippet

def set_io_from_pinmap(pinmap):
    # process PinMapping.xml
    logging.debug("entering set_io_from_pinmap")
    snippet = '\n'
    newline = '\n' #fstrings don't like backslashes
    snippet += '\n'
    snippet += f'set all_inputs [list]{newline}'
    snippet += '\n'
    snippet += f'set all_outputs [list]{newline}'
    tree = ET.parse(pinmap)
    root = tree.getroot()
    for el in root.iter(tag='io'):
        name = el.attrib['name']
        name = re.sub(':[\d]+', '', name)
        net = el.attrib['net']
        direction = el.attrib['dir']
        if direction == 'input':
            if 'clock' in net: # WORKAROUND TODO POSSIBLE RAPTOR BUG, why is the clock net A2F* in PinMapping.xml? Should be clk*
                continue
            snippet += '\n'
            snippet += f'set all_inputs [lappend all_inputs {name}]'
        if direction == 'output':
            snippet += '\n'
            snippet += f'set all_outputs [lappend all_outputs {name}]'
    return snippet


def generate_script(argv):
    logging.debug("entering generate_script")
    pt_script = ""
    if "load" in argv:
        logging.debug("load argument set, writing lib and netlist paths")
        # emit technology dependent search paths, link_library

        pt_script += '\n'
        pt_script += inspect.cleandoc("""
        # ------------
        # TECH ENV
        # ------------
        set STD_CELL_PATH {0}
        set TARGET_LIB {1}
        set BRAM_MEM_PATH {2}
        set BRAM_MEM {3}
        set fp [open "{4}"]
        set RTL_SRC [split [read $fp] "\n"]
        close $fp;

        set search_path [concat $search_path ${{STD_CELL_PATH}} ]
        set search_path [concat $search_path ${{BRAM_MEM_PATH}} ]
        set link_library   [concat * \
        ${{BRAM_MEM}}.db \
        ${{TARGET_LIB}}.db \
        ]

        # slow/slow, -40C (temperature inversion WC)
        set target_library   [concat [list] \
        ${{BRAM_MEM}}.db \
        ${{TARGET_LIB}}.db \
        ]

        #set_dont_use [get_lib_cell */*D0*]

        # ------------
        # READ NETLIST
        # ------------
        pwd
        set svr_enable_vpp true
        set svr_keep_unconnected_nets false
        """.format(args["STD_CELL_PATH"], args["TARGET_LIB"], args["BRAM_MEM_PATH"], args["BRAM_MEM"], args["filelist"]))
    pt_script +='\nforeach file $RTL_SRC {read_verilog $file}'
    pt_script +='\ncurrent_design fpga_top'
    pt_script +='\nlink'
    pt_script += '\n'
    #pt_script += '\nset_false_path -from [find pin -hierarchy */*_reg*/CK] -to [find pin -hierarchy */*_reg*/D]\n' # internal dsp and bram paths (fmax)
    pt_script += '\nset_false_path -from [find pin -hierarchy */*_reg*/CK]\n' # internal dsp and bram paths (fmax)
    pt_script += inspect.cleandoc('''
        set_case_analysis 1 global_resetn
        set_case_analysis 0 scan_en 
        set_case_analysis 0 scan_mode
        ''')
    pt_script += '\n'
    if "recompiled" in argv["bitstream"]:
        pt_script += generate_post_pd_set_case_analysis(argv["bitstream"])
    else:
        pt_script += generate_pre_pd_set_case_analysis(argv["bitstream"])
    pt_script += set_io_from_pinmap(argv["pinmapping"])
    pt_script += '\n'
    pt_script += inspect.cleandoc('''
        create_clock -period 0 -name clk clk*
        set_max_delay -ignore_clock_latency 0 -from clk -to $all_outputs
        set_max_delay -ignore_clock_latency 0 -from $all_inputs -to clk
        set_max_delay -ignore_clock_latency 0 -from $all_inputs -to $all_outputs
        ''')
    pt_script += '\n'
    #report timing
<<<<<<< HEAD
    if "full" in argv:
        logging.debug("full path argument set, writing report_timing commands")
        newline = '\n'
        pt_script += f'echo "--- PTBIT FULL PATH COVER REPORT ---"{newline}report_timing -path_type full -cover_design > cover_full.rpt {newline}'
        max_paths = 100
        nworst = 1
        pt_script += f'echo "--- PTBIT FULL PATH CLK2CLK REPORT ---"{newline}report_timing -path_type full -max_paths {max_paths} -nworst {nworst} > clk2clk_full.rpt {newline}'
        pt_script += f'echo "--- PTBIT FULL PATH CLK2OUTPUT REPORT ---"{newline}report_timing -to $all_outputs -path_type full -max_paths {max_paths} -nworst {nworst} > clk2output_full.rpt {newline}'
        pt_script += f'echo "--- PTBIT FULL PATH INPUT2CLK REPORT ---"{newline}report_timing -from $all_inputs -path_type full -max_paths {max_paths} -nworst {nworst} > input2clk_full.rpt {newline}'
        pt_script += f'echo "--- PTBIT FULL PATH INPUT2OUTPUT REPORT ---"{newline}report_timing -from $all_inputs -to $all_outputs -path_type full -max_paths {max_paths} -nworst {nworst} > input2output_full.rpt {newline}'
    else:
        logging.debug("writing report_timing commands")
        newline = '\n'
        max_paths = 10000
        nworst = 10
        pt_script += f'echo "--- PTBIT END PATH CLK2CLK REPORT ---"{newline}report_timing -nosplit -path_type end -max_paths {max_paths} -nworst {nworst} > clk2clk_end.rpt {newline}'
        pt_script += f'echo "--- PTBIT END PATH CLK2OUTPUT REPORT ---"{newline}report_timing -nosplit -to $all_outputs -path_type end -max_paths {max_paths} -nworst {nworst} > clk2output_end.rpt {newline}'
        pt_script += f'echo "--- PTBIT END PATH INPUT2CLK REPORT ---"{newline}report_timing -nosplit -from $all_inputs -path_type end -max_paths {max_paths} -nworst {nworst} > input2clk_end.rpt {newline}'
        pt_script += f'echo "--- PTBIT END PATH INPUT2OUTPUT REPORT ---"{newline}report_timing -nosplit -from $all_inputs -to $all_outputs -path_type end -max_paths {max_paths} -nworst {nworst} > input2output_end.rpt {newline}'
=======
    logging.debug("writing report_timing commands (full)")
    newline = '\n'
    max_paths = 100
    nworst = 1
    pt_script += f'echo "--- PTBIT FULL PATH CLK2CLK REPORT ---"{newline}report_timing -path_type full -max_paths {max_paths} -nworst {nworst} > clk2clk_full.rpt {newline}'
    pt_script += f'echo "--- PTBIT FULL PATH CLK2OUTPUT REPORT ---"{newline}report_timing -to $all_outputs -path_type full -max_paths {max_paths} -nworst {nworst} > clk2output_full.rpt {newline}'
    pt_script += f'echo "--- PTBIT FULL PATH INPUT2CLK REPORT ---"{newline}report_timing -from $all_inputs -path_type full -max_paths {max_paths} -nworst {nworst} > input2clk_full.rpt {newline}'
    pt_script += f'echo "--- PTBIT FULL PATH INPUT2OUTPUT REPORT ---"{newline}report_timing -from $all_inputs -to $all_outputs -path_type full -max_paths {max_paths} -nworst {nworst} > input2output_full.rpt {newline}'

    logging.debug("writing report_timing commands (end)")
    newline = '\n'
    max_paths = 10000
    nworst = 10
    pt_script += f'echo "--- PTBIT END PATH CLK2CLK REPORT ---"{newline}report_timing -nosplit -path_type end -max_paths {max_paths} -nworst {nworst} > clk2clk_end.rpt {newline}'
    pt_script += f'echo "--- PTBIT END PATH CLK2OUTPUT REPORT ---"{newline}report_timing -nosplit -to $all_outputs -path_type end -max_paths {max_paths} -nworst {nworst} > clk2output_end.rpt {newline}'
    pt_script += f'echo "--- PTBIT END PATH INPUT2CLK REPORT ---"{newline}report_timing -nosplit -from $all_inputs -path_type end -max_paths {max_paths} -nworst {nworst} > input2clk_end.rpt {newline}'
    pt_script += f'echo "--- PTBIT END PATH INPUT2OUTPUT REPORT ---"{newline}report_timing -nosplit -from $all_inputs -to $all_outputs -path_type end -max_paths {max_paths} -nworst {nworst} > input2output_end.rpt {newline}'
>>>>>>> 5334aa6a696c84098f5e5968b5a15ce2c1d7f919
    logging.debug("returning generated pt script")
    return pt_script

def write_script(content, filename):
    try:
        logging.debug("Opening file for writing: %s", filename)
        with open(filename, "w", encoding="utf-8") as writer:
            # emit header
            writer.write('# GENERATED BY PTBIT')
            writer.write('# USAGE:')
            writer.write('# module load synopsys/1.0')
            writer.write(f'# pt_shell -file {filename}')
            # emit formatted script
            logging.debug("writing formatted script:")
            writer.write(content)
            if args["exit"]:
                writer.write('\nexit\n')
    except OSError as e:
        logging.exception('')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Script to generate PrimeTime script from Raptor bitstream for architecture exploration and timing analysis"
    )
    parser.add_argument("--pinmapping", help="PinMapping.xml file from Raptor", default="PinMapping.xml")
    parser.add_argument("--bitstream", help="fabric_bitstream.xml file from Raptor or *_recompiled_bitstream.xml from openfpga-physical", default="fabric_bitstream.xml")
    parser.add_argument("--output", help="generated TCL script file name", default="ptbit.tcl")
    parser.add_argument("--filelist", help="fabric verilog", default="/home/rliston/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/FPGA10x8_gemini_compact_pnr/fabric_pre_restruct.f")
    parser.add_argument("--STD_CELL_PATH", help="environment variable", default="/cadlib/gemini/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/db/")
    parser.add_argument("--TARGET_LIB", help="environment variable", default="dti_tm16ffc_90c_7p5t_stdcells_ssgnp_0p72v_125c_rev1p0p0")
    parser.add_argument("--BRAM_MEM_PATH", help="environment variable", default="/cadlib/gemini/TSMC16NMFFC/library/memory/dti/memories/rev_111022/101122_tsmc16ffc_DP_RAPIDSILICON_GEMINI_rev1p0p3_BE/dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc/")
    parser.add_argument("--BRAM_MEM", help="environment variable", default="dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc_ssgnp125c")
    parser.add_argument("--full", help="produce full timing path report", default=False, action="store_true")
    parser.add_argument("--load", help="load and link fabric netlist in ptbit.tcl", default=False, action="store_true")
    parser.add_argument("--exit", help="exit ptbit.tcl on completion", default=1, type=int)
    args = vars(parser.parse_args())
    #Todo: move logging level into verbosity arg
    logging.basicConfig(filename='ptbit.log', format='%(levelname)s:%(message)s', level=logging.DEBUG)
    logging.info(args)
    
    script_content = generate_script(args)
    write_script(script_content, args["output"])
