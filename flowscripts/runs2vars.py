#!/usr/bin/env python3
"""analyze the output from: grep "^/.*/vpr " */raptor.log > runs"""

import re

g_mode = {
    '--pack',
    '--place',
    '--route',
    '--analysis',
}
g_always = {
    'sdc_file',
    'clock_modeling',
    'timing_report_npaths',
    'top',
    'route_chan_width',
    'suppress_warnings',
    'absorb_buffer_luts',
    'skip_sync_clustering_and_routing_results',
    'constant_net_method',
    'post_place_timing_report',
    'device',
    'allow_unrelated_clustering',
    'allow_dangling_combinational_nodes',
    'place_delta_delay_matrix_calculation_method',
    'post_synth_netlist_unconn_inputs',
    'inner_loop_recompute_divider',
    'max_router_iterations',
    'timing_report_detail',
    'net_file',
    'place_file',
    'route_file',
}

def toplogic():
    """summarize all VPR runs"""
    lines = {}
    with open("runs", encoding='ascii') as ifile:
        for line in ifile:
            args = line.split()
            topd = args[0].split('/')[0]
            del args[:3]
            mode = [a for a in args if a     in g_mode]
            assert len(mode) == 1
            mode = mode[0]
            args = [a for a in args if a not in g_mode]
            assert not 1 & len(args)

            aa = {'mode': mode[2:] }
            check = set(g_always)
            part = ""
            for o, v in zip(args[0::2], args[1::2]):
                assert o.startswith("--")
                o = o[2:]
                if o == "number_of_molecules_in_partition": part = v
                if o in check:
                    if o == "top": topm = v
                    if o == "place_file":
                        desn = re.sub(r'^.*_golden/([^/]+)/run_1/.*$', r'\1', v)
                    if o == "device": size = re.sub(r'^.*?(\d+x\d+).*$', r'\1', v)
                    check.remove(o)
                else:
                    aa[o] = v
            # for

            assert not check, f"check={check}"

            if aa['mode'] == "analysis":
                assert 'gen_post_synthesis_netlist' not in aa
            else:
                assert 'gen_post_synthesis_netlist'     in aa
                del aa['gen_post_synthesis_netlist']

            if aa['mode'] == "place" and 'fix_clusters' in aa:
                del aa['fix_clusters']

            lines[topd] = f"TOPM={topm} DESN={desn} SIZE={size} PART={part}\n"
        # for
    # with

    for topd, line in lines.items():
        filename = f"{topd}/vars.sh"
        with open(filename, "w", encoding='ascii') as ofile:
            ofile.write(line)
        # with
    # for
# def toplogic

toplogic()

# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
