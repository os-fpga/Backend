import xml.etree.ElementTree as ET
import sys
import os
import datetime
from collections import defaultdict
from collections import Counter
import cProfile
import xlsxwriter
from fpga_data import *
import json
import re

g_base = 'rr_graph'
flow_inputs_dir = ''
route_addrs = ''
l0_map          = {}
mux_data        = {}    # (mux_file_name, mux_name) ==> [input_pin ==> usage]
segment_info    = {}    # segment_id ==> [name, length]
g_nid2info      = {}    # nid ==> [xl, yl, xh, yh, ptc, pin/chan, sgid/side]
g_nid2info_2    = {}    # nid ==> [xl, yl, xh, yh, ptc, pin/chan, sgid/side, tag, simple_names[], mux_type, mux_name, mux_x, mux_y]
g_ninfo2nid     = {}    # (mux_name, mux_x, mux_y) ==> nid
g_grids         = {}    # (x,y) ==> [block_id, width_offset, height_offset]
g_muxes         = [
    # middle rows
    'clb',              # 0: middle columns
    'left',             # 1: left IOTiles
    'right',            # 2: right IOTiles
    # bottom rows
    'bottom',           # 3: bottom IOTiles
    'blcorner',         # 4: bottom left IOTiles
    'brcorner',         # 5: bottom right IOTiles
    # top rows
    'top',              # 6: top IOTiles
    'tlcorner',         # 7: top left IOTiles
    'trcorner',         # 8: top right IOTiles
    # other
    'other',            # 9: BRAM and DSP
]
g_xcoords           = []
g_ycoords           = []
g_clb_id            = None  # id for CLB
g_bid2info          = {} # bid ==> [name, width, height]
distance2tap = {
    0: 'B',
    1: 'Q',
    2: 'M',
    3: 'P',
    4: 'E',
    5: 'F', # Temporary - should not happen
}
tap2distance = {
    'B': 0, 
    'Q': 1, 
    'M': 2, 
    'P': 3, 
    'E': 4, 
    'F': 5,  # Temporary - should not happen
}

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log


def out(text, filename):
    """write to specific file"""
    with open(filename, "a") as f:
        f.write(text)
# def out


def build_l0_map(jumpmap_addr):

    global l0_map

    # Open the file for reading
    with open(jumpmap_addr, 'r') as file:
        for line in file:
            # Split each line into two parts (e.g., O16	N0E2)
            parts = line.strip().split()
            if len(parts) == 2:
                value, key = parts
                l0_map[key] = value
                log(f"l0_map[{key}] = {value}\n")
            else:
                log(f"ERROR: length = {len(parts)}\n")
                exit()
# def build_l0_map(jumpmap_addr)


def get_mux_type(x,y):
    # get the type of mux based on mux coordinates
    yidx    = 3 * (1 if y == g_ycoords[0] else 2 if y == g_ycoords[-1] else 0)
    xidx    = 1 * (1 if x == g_xcoords[0] else 2 if x == g_xcoords[-1] else 0)
    idx     = yidx + xidx
    # change from 0 (clb) to 9 (non-clb) based on floorplan
    if not idx and g_grids[(x, y)][0] != g_clb_id: idx = 9
    return g_muxes[idx]
# def get_mux_type(x,y)


def get_input_pin_name(mux_type, mux_name, potential_input_names, tap):
    input_pins = mux_data[(mux_type, mux_name)]
    log(f"in get_input_pin_name:\n")
    for (input_pin,usage) in input_pins.items():
        log(f"\tinput_pin = {input_pin}\n")
        
        # regular wires
        for p_i_n in potential_input_names:
            log(f"\tusing tap name for regular wires\t mux_type = {mux_type}, mux_name = {mux_name}, p_i_n = {p_i_n}\n")
            if input_pin.startswith('O'):
                if input_pin == p_i_n[3]:
                    log(f"\t\tFound regular OPIN. mux_type = {mux_type}, mux_name = {mux_name}, input_pin = {input_pin} <==> p_i_n[3] = {p_i_n[3]}\n")
                    return input_pin
            elif input_pin == f'{p_i_n[3][:2]}{tap}{p_i_n[3][3:]}' and input_pin == p_i_n[3]:
                log(f"\t\tFound regular CHAN. mux_type = {mux_type}, mux_name = {mux_name}, input_pin = {input_pin} <==> p_i_n[3] = {p_i_n[3]}\n")
                return input_pin
        
        # irregular wires --> should not happen
        for p_i_n in potential_input_names:
            log(f"\tusing potential_input_names for irregular wires\t mux_type = {mux_type}, mux_name = {mux_name}, p_i_n = {p_i_n}\n")
            if (not input_pin.startswith('O')) and input_pin == p_i_n[3]:
                log(f"\t\tWARNING: Found irregular CHAN. mux_type = {mux_type}, mux_name = {mux_name}, input_pin = {input_pin} <==> p_i_n[3] = {p_i_n[3]}, tap = {tap}\n")
                return input_pin
            
    return -1
# def get_input_pin_name(mux_type, mux_name, potential_input_names, tap)


def parse_device_info(device_addr):
    # read some info about FPGA device from JSON file produced by stamper

    global segment_info # segment_id ==> [name, length]
    global g_nid2info   # nid ==> [xl, yl, xh, yh, ptc, pin/chan, sgid/side]
    global g_xcoords, g_ycoords
    global g_bid2info
    global g_clb_id

    with open(device_addr, 'r') as file:
        data = json.load(file)
    
    # Extract "segments" and "rr_nodes" from the JSON data
    segments_data = data.get("segments", {})
    rr_nodes_data = data.get("rr_nodes", {})
    grids_data = data.get("grid", [])
    block_types_data = data.get("block_types", {})

    # Populate the segment_info dictionary
    for segment_id, info in segments_data.items():
        name, length = info
        segment_info[segment_id] = [name, length]
        log(f"segment_info[{segment_id}] = [{name}, {length}]\n")
    
    # Populate the g_nid2info dictionary
    for nid, info in rr_nodes_data.items():
        xl, yl, xh, yh, ptc, pin_chan, sgid_side = info
        g_nid2info[int(nid)] = [int(xl), int(yl), int(xh), int(yh), int(ptc), pin_chan, str(sgid_side)]
        log(f"g_nid2info[{nid}] = [{int(xl)}, {int(yl)}, {int(xh)}, {int(yh)}, {int(ptc)}, {pin_chan}, {str(sgid_side)}]\n")

    # Populate the g_bid2info dictionary
    for id, block_info in block_types_data.items():
        g_bid2info[int(id)] = [block_info[0], block_info[1], block_info[2]]
        log(f"g_bid2info[id={int(id)}] = [name={block_info[0]}, width={block_info[1]}, height={block_info[2]}]\n")
        if block_info[0] == 'clb':
            g_clb_id = int(id)


    # Populate the g_grids dictionary
    for row in grids_data:
        if len(row) >= 2:
            g_grids[(row[0], row[1])] = [row[2], row[3], row[4]]
            log(f"g_grids[{(row[0], row[1])}] = [{row[2]}, {row[3]}, {row[4]}]\n")
    
    

    # compute bbox
    g_xcoords = sorted({ x for (x, y) in g_grids if g_grids[(x, y)][0] != 0 })
    g_ycoords = sorted({ y for (x, y) in g_grids if g_grids[(x, y)][0] != 0 })
    log(f"g_xcoords = {g_xcoords}\n")
    log(f"g_ycoords = {g_ycoords}\n")
# def parse_device_info(device_addr)


def read_mux_files(directory):
    global mux_data
    if us:
        # iterate over each file of flow_inputs directory
        for filename in os.listdir(directory):
            # find .mux files
            if filename.endswith(".mux"):
                # the name of .mux file
                mux_file_name = filename.split('.')[0]
                # get the full path of .mux file
                file_path = os.path.join(directory, filename)
                # write the information of the mux file
                print_info(file_path)
                # open the file
                with open(file_path, 'r') as file:
                    # read its content
                    lines = file.readlines()
                    # initilize mux_input_map for checking the inputs to be unique
                    mux_input_map = {}
                    for line in lines:
                        content = line.split()
                        if content:
                            # the first word is the mux name
                            mux_name = content[0]
                            # rest of the entries are input pin names
                            input_pins = content[1:]
                            for input_pin in input_pins:
                                
                                # # Check if we have same wire with different taps in the same mux (tlcorner and blcorner excluded) (OPINs are not checked obviously)
                                # if not input_pin.startswith('O'):
                                #     if (mux_name, f'{input_pin[:2]}{input_pin[3:]}') in mux_input_map and not mux_file_name in ('tlcorner', 'blcorner'):
                                #         log(f"ERROR: same wire ({input_pin[:2]}{input_pin[3:]}) is being added again to the same mux= {mux_name}.\n")
                                #         log(f"\tprevious input: {mux_input_map[(mux_name, f'{input_pin[:2]}{input_pin[3:]}')]}\n")
                                #         log(f"\tcurrent input: {input_pin}\n")
                                #         exit()
                                #     else:
                                #         mux_input_map[(mux_name, f'{input_pin[:2]}{input_pin[3:]}')] = input_pin
                                # # if

                                mux_data.setdefault((mux_file_name, mux_name), {})[input_pin] = 0
                                log(f"mux_data[{(mux_file_name, mux_name)}] = input_pin: {input_pin}, usage: 0\n")
                    # for
                # open
            # if
        # for
    # if
    
    # TODO: to be removed if we will not use this script for non-SR
    else:
        mux_adresses = []
        
        # find out how many different muxes we have
        print_info(f'{flow_inputs_dir}/{g_base}.tilemap')
        file_path = f'{flow_inputs_dir}/{g_base}.tilemap'
        with open(file_path, 'r') as file:
            lines = file.readlines()
            for line in lines:
                if len(line.split()) == 4:
                    log(f"reading new mux pattern: {line}\n")
                    mux_adresses.apend(line.split()[3])
        # open

        # read unique muxes
        for mux_address in mux_adresses:
            # get the full path of .mux file
            file_path = f'{flow_inputs_dir}/{mux_address}'
            # the name of .mux file
            mux_file_name = file_path.split('/')[-1].split('.')[0]
            # write the information of the mux file
            print_info(file_path)
            # open the file
            with open(file_path, 'r') as file:
                # read its content
                lines = file.readlines()
                for line in lines:
                    content = line.split()
                    if content:
                        # the first word is the mux name
                        mux_name = content[0]
                        # rest of the entries are input pin names
                        input_pins = content[1:]
                        for input_pin in input_pins:
                            mux_data.setdefault((mux_file_name, mux_name), {})[input_pin] = 0
                            log(f"mux_data[{(mux_file_name, mux_name)}] = input_pin: {input_pin}, usage: 0\n")
                # for
            # open
        # for
    # else

    lines = 'Initialization of mux_data:\n'
    lines += 'mux_type\tmux_name <==\tinput_pin [usage]\n\n'
    for key, input_pins in mux_data.items():
        (mux_file_name, mux_name) = key
        lines += f"{mux_file_name}\t{mux_name} <==\t"
        for (input_pin, usage) in input_pins.items():
            lines += f"{input_pin} [{usage}]\t"
        lines +="\n"
    out(lines, 'mux_input_map')
# read_mux_files(directory)


def create_mux_data_excel():
    # Create a new workbook and worksheet
    top_min = sys.maxsize
    top_max = 0
    top_mid = 0
    excel_name = "mux_input_map"
    if os.path.exists(f'{excel_name}.xlsx'):
        # If the file exists, remove it first
        os.remove(f'{excel_name}.xlsx')
    workbook = xlsxwriter.Workbook(f'{excel_name}.xlsx')
    bold = workbook.add_format({'bold': True, 'align': 'center'})
    cell_format = workbook.add_format({'align': 'center'})

    # worksheets = {}
    # for mux_type in g_muxes:
    #     worksheets[mux_type] = workbook.add_worksheet()

    for mux_type in g_muxes:
        # Define some formatting for the headers and cells
        row = col = 0
        min_value = sys.maxsize
        max_value = 0
        mux_file_name = ''
        worksheet = workbook.add_worksheet(mux_type)
        for key, input_pins in mux_data.items():
            (mux_file_name, mux_name) = key
            if not mux_type == mux_file_name:
                continue
            row += 1
            cell_val = mux_name
            worksheet.write(row, col, cell_val, bold)
            col += 1
            for (input_pin, usage) in input_pins.items():
                row -= 1
                cell_val = usage
                min_value = min(min_value, usage)
                max_value = max(max_value, usage)
                worksheet.write(row, col, cell_val, cell_format)
                row += 1
                cell_val = input_pin
                worksheet.write(row, col, cell_val, cell_format)
                col += 1
            row += 1
            col = 0
        mid_value = int((min_value + max_value)/2)
        
        # In case we want to have min/max value across all spreadsheets (not only the current one)
        top_min = min(top_min, min_value)
        top_max = min(top_max, max_value)
        top_mid = int((top_min + top_max)/2)

        if max_value == 0:
            worksheet.conditional_format(0, 0, worksheet.dim_rowmax, worksheet.dim_colmax, 
                                        {'type': '3_color_scale',
                                        'min_color': 'green',
                                        'mid_color': 'green',
                                        'max_color': 'green',
                                        'min_type': 'num', # Set the lowest value 
                                        'mid_type': 'num',# Set the midpoint at the 50th percentile (median)
                                        'max_type': 'num',   # Set the highest value
                                        'min_value': min_value,
                                        'mid_value': mid_value,
                                        'max_value': max_value
                                        })
        else:
            worksheet.conditional_format(0, 0, worksheet.dim_rowmax, worksheet.dim_colmax, 
                                        {'type': '3_color_scale',
                                        'min_color': 'green',
                                        'mid_color': 'yellow',
                                        'max_color': 'red',
                                        'min_type': 'num', # Set the lowest value 
                                        'mid_type': 'num',# Set the midpoint at the 50th percentile (median)
                                        'max_type': 'num',   # Set the highest value
                                        'min_value': min_value,
                                        'mid_value': mid_value,
                                        'max_value': max_value
                                        })
    workbook.close()
# def create_mux_data_excel()


def parse_wire_map(map_addr):
    simple_edge_list = {}
    global g_nid2info_2
    global g_ninfo2nid

    header_pattern = re.compile(r'^([\s\+\-]+)([\d]+) \(([\d]+),([\d]+),([\d]+),([\d]+),([\d]+),([\w]+),([\w]+)\)')
    data_pattern = re.compile(r'^([\s\+\-]+)([\d]+),([\d]+),([\w]+)')

    with open(map_addr, 'r') as file:
        current_header = None
        current_data = []

        for line in file:
            header_match = header_pattern.match(line)
            if header_match:
                # If a new header is found, process the previous one
                if current_header:
                    tag = current_header[0].strip()
                    nid = int(current_header[1].strip())
                    xl = int(current_header[2].strip())
                    yl = int(current_header[3].strip())
                    xh = int(current_header[4].strip())
                    yh = int(current_header[5].strip())
                    ptc = int(current_header[6].strip())
                    pin_chan = current_header[7].strip()
                    sgid_side = current_header[8].strip()
                    simple_names = current_data
                    mux_x = mux_y = -1
                    mux_name = ''
                    
                    if simple_names[0][3].startswith('I') or simple_names[0][3].startswith('O'):
                        mux_x = int(simple_names[0][1])
                        mux_y = int(simple_names[0][2])
                        mux_name = simple_names[0][3]
                    else: # lazy way to find the third letter in the mux name. E.g., 'B' in E4B7
                        for simple_name in simple_names:
                            if simple_name[3][2] == 'E':
                                mux_x = int(simple_name[1])
                                mux_y = int(simple_name[2])
                                mux_name = simple_name[3]
                                break
                        for simple_name in simple_names:
                            if simple_name[3][2] == 'P':
                                mux_x = int(simple_name[1])
                                mux_y = int(simple_name[2])
                                mux_name = simple_name[3]
                                break
                        for simple_name in simple_names:
                            if simple_name[3][2] == 'M':
                                mux_x = int(simple_name[1])
                                mux_y = int(simple_name[2])
                                mux_name = simple_name[3]
                                break
                        for simple_name in simple_names:
                            if simple_name[3][2] == 'Q':
                                mux_x = int(simple_name[1])
                                mux_y = int(simple_name[2])
                                mux_name = simple_name[3]
                                break
                        for simple_name in simple_names:
                            if simple_name[3][2] == 'B':
                                mux_x = int(simple_name[1])
                                mux_y = int(simple_name[2])
                                mux_name = simple_name[3]
                                break
                            
                    mux_type = get_mux_type(mux_x, mux_y)
                    if not tag:
                        tag = ' '
                    g_nid2info_2[nid] = [xl, yl, xh, yh, ptc, pin_chan, sgid_side, tag, simple_names, mux_type, mux_name, mux_x, mux_y]
                    log(f'g_nid2info_2[{nid}] = [{xl}, {yl}, {xh}, {yh}, {ptc}, {pin_chan}, {sgid_side}, {tag}, {simple_names}, {mux_type}, {mux_name}, {mux_x}, {mux_y}]\n')
                    for simple_name in simple_names:
                        mux_x = int(simple_name[1])
                        mux_y = int(simple_name[2])
                        g_ninfo2nid[(simple_name[3], mux_x, mux_y)] = nid
                        log(f"g_ninfo2nid[{(simple_name[3], mux_x, mux_y)}] = [{nid}]\n")
                    
                # Extract information from the new header
                current_header = header_match.groups()
                current_data = []

            data_match = data_pattern.match(line)
            if data_match:
                # Extract information from the data line and add it to the current data list
                data = data_match.groups()
                tag = data[0].strip()
                x = data[1].strip()
                y = data[2].strip()
                name = data[3].strip()
                if not tag:
                    tag = ' '
                current_data.append((tag, x, y, name))
# def parse_wire_map(map_addr)


def process_net(node_list):

    # process nodes in each net --> increment edge usage and update mux surface excel file
    for node in node_list:
        (src_node_id, src_node_type, src_node_index) = node
        if src_node_index == len(node_list) - 1: # visiting the last node which is SINK
            log(f"Last node -- > src_node_id = {src_node_id}, src_node_type = {src_node_type}\n")
            break
        (dst_node_id, dst_node_type, _) = node_list[src_node_index + 1]
        log(f"src_node_id = {src_node_id}, src_node_type = {src_node_type}\n")
        log(f"dst_node_id = {dst_node_id}, dst_node_type = {dst_node_type}\n")

        if src_node_type in ('SOURCE', 'SINK') or dst_node_type in ('SOURCE', 'SINK'):
            continue

        src_nid = int(src_node_id)
        dst_nid = int(dst_node_id)

        # increment mux input usage
        if dst_nid not in g_nid2info:
            log(f"ERROR: dest node in .route file with id = {dst_nid} is not in g_nid2info\n")
            exit()
        if src_nid not in g_nid2info:
            log(f"ERROR: source node in .route file with id = {src_nid} is not in g_nid2info\n")
            exit()
        if dst_nid not in g_nid2info_2:
            log(f"ERROR: dest node in .route file with id = {dst_nid} is not in g_nid2info_2\n")
            exit()
        if src_nid not in g_nid2info_2:
            log(f"ERROR: source node in .route file with id = {src_nid} is not in g_nid2info_2\n")
            exit()
        
        (sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side) = g_nid2info[src_nid]
        (dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side) = g_nid2info[dst_nid]
        (sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2, stag, ssimple_names, smux_type, smux_name, smux_x, smux_y) = g_nid2info_2[src_nid]
        (dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2, dtag, dsimple_names, dmux_type, dmux_name, dmux_x, dmux_y) = g_nid2info_2[dst_nid]
        if not (sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side) == (sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2):
            log(f"ERROR: g_nid2info not match g_nid2info_2\n")
            log(f"{(sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side)}\n")
            log(f"{(sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2)}\n")
            exit()
        if not (dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side) == (dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2):
            log(f"ERROR: g_nid2info not match g_nid2info_2\n")
            log(f"{(dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side)}\n")
            log(f"{(dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2)}\n")
            exit()

        log(f'\t before accessing l0_map, src_nid = {src_nid}\n')
        log(f'\t before accessing l0_map, dst_nid = {dst_nid}\n')

        smap = False
        seen = set()

        while smux_name in l0_map:
            smux_name = l0_map[smux_name]
            smap = True
            if smux_name in seen:
                log(f"\t\t - ERROR: L0 cycle on node {smux_name}\n")
                break
            seen.add(smux_name)
        
        seen = set()
        while dmux_name in l0_map:
            dmux_name = l0_map[dmux_name]
            if dmux_name in seen:
                log(f"\t\t - ERROR: L0 cycle on node {dmux_name}\n")
                break
            seen.add(dmux_name)
        
        src_nid = g_ninfo2nid[(smux_name, smux_x, smux_y)]
        dst_nid = g_ninfo2nid[(dmux_name, dmux_x, dmux_y)]

        (sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side) = g_nid2info[src_nid]
        (dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side) = g_nid2info[dst_nid]
        (sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2, stag, ssimple_names, smux_type, smux_name, smux_x, smux_y) = g_nid2info_2[src_nid]
        (dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2, dtag, dsimple_names, dmux_type, dmux_name, dmux_x, dmux_y) = g_nid2info_2[dst_nid]
        if not (sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side) == (sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2):
            log(f"ERROR: g_nid2info not match g_nid2info_2\n")
            log(f"{(sxl, syl, sxh, syh, sptc, spin_chan, ssgid_side)}\n")
            log(f"{(sxl_2, syl_2, sxh_2, syh_2, sptc_2, spin_chan_2, ssgid_side_2)}\n")
            exit()
        if not (dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side) == (dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2):
            log(f"ERROR: g_nid2info not match g_nid2info_2\n")
            log(f"{(dxl, dyl, dxh, dyh, dptc, dpin_chan, dsgid_side)}\n")
            log(f"{(dxl_2, dyl_2, dxh_2, dyh_2, dptc_2, dpin_chan_2, dsgid_side_2)}\n")
            exit()

        log(f'\t after accessing l0_map, src_nid = {src_nid}\n')
        log(f'\t after accessing l0_map, dst_nid = {dst_nid}\n')

        if src_nid == dst_nid:
            log(f'src_nid == dst_nid\n')
            continue

        # ignore connections that its dest is OPIN:
        if dpin_chan.startswith('O') or dmux_name.startswith('O'):
            continue
        
        # compute intersection point containing downstream mux
        # match (spin_chan, dpin_chan):
        # OPIN --> IPIN
        if (spin_chan, dpin_chan) == ('OPIN', 'IPIN'):
            if smap:    (mx, my) = (sxl, syl)
        # OPIN --> CHAN
        if (spin_chan, dpin_chan) == ('OPIN', 'CHANE'):     (mx, my) = (sxl, syl)
        elif (spin_chan, dpin_chan) == ('OPIN', 'CHANN'):     (mx, my) = (sxl, syl)
        elif (spin_chan, dpin_chan) == ('OPIN', 'CHANW'):     (mx, my) = (sxl, syl)
        elif (spin_chan, dpin_chan) == ('OPIN', 'CHANS'):     (mx, my) = (sxl, syl)
        # CHANX --> CHANY
        elif (spin_chan, dpin_chan) == ('CHANE', 'CHANN'):    (mx, my) = (dxl, syl)
        elif (spin_chan, dpin_chan) == ('CHANE', 'CHANS'):    (mx, my) = (dxl, syl)
        elif (spin_chan, dpin_chan) == ('CHANW', 'CHANN'):    (mx, my) = (dxl, syl)
        elif (spin_chan, dpin_chan) == ('CHANW', 'CHANS'):    (mx, my) = (dxl, syl)
        # CHANY --> CHANX
        elif (spin_chan, dpin_chan) == ('CHANN', 'CHANE'):    (mx, my) = (sxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANN', 'CHANW'):    (mx, my) = (sxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANS', 'CHANE'):    (mx, my) = (sxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANS', 'CHANW'):    (mx, my) = (sxl, dyl)
        # stitch (max almost certainly not needed)
        elif (spin_chan, dpin_chan) == ('CHANE', 'CHANE'):    (mx, my) = (max(dxl-1,us), dyl)
        elif (spin_chan, dpin_chan) == ('CHANN', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,us))
        elif (spin_chan, dpin_chan) == ('CHANW', 'CHANW'):    (mx, my) = (dxh, dyh)
        elif (spin_chan, dpin_chan) == ('CHANS', 'CHANS'):    (mx, my) = (dxh, dyh)
        # reversal (max almost certainly not needed)
        elif (spin_chan, dpin_chan) == ('CHANE', 'CHANW'):    (mx, my) = (dxh, dyh)
        elif (spin_chan, dpin_chan) == ('CHANN', 'CHANS'):    (mx, my) = (dxh, dyh)
        elif (spin_chan, dpin_chan) == ('CHANW', 'CHANE'):    (mx, my) = (max(dxl-1,us), dyl)
        elif (spin_chan, dpin_chan) == ('CHANS', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,us))
        # CHAN --> IPIN
        elif (spin_chan, dpin_chan) == ('CHANE', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANN', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANW', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (spin_chan, dpin_chan) == ('CHANS', 'IPIN'):     (mx, my) = (dxl, dyl)
        # other edges we ignore
        else:          
            i += 1
            log(f'other edges we ignore\n')        
            continue
        # match

        log(f'(mx, my) = ({(mx, my)})\n')
        # compute upstream mux location
        (ux, uy) = (mx, my)
        # match spin_chan:
        if spin_chan == 'CHANE':
            ux = max(sxl - 1, us)
        elif spin_chan == 'CHANN':
            uy = max(syl - 1, us)
        elif spin_chan == 'CHANW':
            ux = sxh
        elif spin_chan == 'CHANS':
            uy = syh
        # match
        log(f'(ux, uy) = ({(ux, uy)})\n')

        # Connection between: (ux, uy) ==> (mx, my)
        # save this by source node
        # overwrite by values with greater travel
        # this is the length traveled
        travel = abs(ux - mx) + abs(uy - my)
        log(f'travel = {travel}\n')

        if (abs(sxl - sxh) + abs(syl - syh)) > 1:
            tap = distance2tap[travel + tap2distance[smux_name[2]]]
        else:
            tap = 'B' if travel == 0 else 'E'
        log(f'tap = {tap}\n')

        
        # determin input_pin_name:
        input_pin_name = get_input_pin_name(dmux_type, dmux_name, ssimple_names, tap)
        if input_pin_name == -1:
            log(f"ERROR: input_pin_name not found\n")
            log(f"dmux_type = {dmux_type}, dmux_name = {dmux_name}, ssimple_names = {ssimple_names}\n")
            exit()

        # increment the input_pin usage
        log(f"incrementing mux_data[{dmux_type},{dmux_name}][{input_pin_name}]\t src_nid={src_nid}, dst_nid={dst_nid}\n")
        mux_data[dmux_type,dmux_name][input_pin_name] += 1
# def process_net(node_list)


def parse_route_file(route_addr):
    # reading the routing file to generate reports

    route_file = open(route_addr, "r")
    route_content = route_file.readlines()
    i = 0
    while i < len(route_content): # read every line of the .route file
        if 'Net' in route_content[i] and 'global' not in route_content[i]: # if there is a new Net
            net_id = route_content[i].split()[1]
            log(f"net_id = {net_id}\n")
            net_name = route_content[i].split()[2][1:-1]
            log(f"net_name = {net_name}\n")
            net_line_idx = i # keep the index of Net in the list
            j = net_line_idx + 1 # skip the this line that contains 'Net'
            while j < len(route_content) and not route_content[j].strip(): # skip the lines that has nothing
                j += 1
            if j >= len(route_content): # check if reaches the end of file
                break
            node_list = list()
            node_index = 0
            while ('Node' in route_content[j]):
                node_id = route_content[j].split()[1]
                log(f"\tnode_id = {node_id}\n")
                node_type = route_content[j].split()[2]
                log(f"\tnode_type = {node_type}\n")
                node = (node_id, node_type, node_index)
                node_list.append(node)
                j = j + 1
                node_index += 1
                if (j >= len(route_content)):
                    break
            i = j
            while i < len(route_content) and not 'Net' in route_content[i]: #  go to next "Net" line
                i += 1
            process_net(node_list)
        else:
            i = i + 1
            continue

    route_file.close()

    # after iterating the routing file:
    lines = f'\n\n\n\nAfter iterating {route_addr}:\n'
    lines += 'mux_type\tmux_name <==\tinput_pin [usage]\n\n'
    for key, input_pins in mux_data.items():
        (mux_file_name, mux_name) = key
        lines += f"{mux_file_name}\t{mux_name} <==\t"
        for (input_pin, usage) in input_pins.items():
            lines += f"{input_pin} [{usage}]\t"
        lines +="\n"
    out(lines, 'mux_data')
# def parse_route_file(route_addr)


def print_info(addr):
    # print files info
    lines = ""
    ################ Device info File ################

    # Get the file name and absolute path
    basename = os.path.basename(addr)
    abspath = os.path.abspath(addr)

    # Get the last modified date and time
    mtime = os.path.getmtime(addr)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

    
    # Print the file name, address, and last modified date
    lines += "File name: {}\n".format(basename)
    lines += "File address: {}\n".format(abspath)
    lines += "File Last modified: {}\n".format(modtime)
    lines += '\n'
    ################ Device info File ################

    out(lines, 'files_info')
# def print_info(addr)


def toplogic():
    # the main flow

    
    # write the information of the device info file
    print_info(f'{flow_inputs_dir}/{g_base}.json')

    # parse the device info file
    parse_device_info(f'{flow_inputs_dir}/{g_base}.json')

    # write the information of the wire map
    print_info(f'{flow_inputs_dir}/{g_base}.xlate')

    # parse the wire map
    parse_wire_map(f'{flow_inputs_dir}/{g_base}.xlate')

    # read .mux files
    read_mux_files(flow_inputs_dir)
    
    

    if us:
        print_info(f'{flow_inputs_dir}/{g_base}.jumpmap')

        # build l0_map
        build_l0_map(f'{flow_inputs_dir}/{g_base}.jumpmap')

    # loop on routing files
    for route_addr in route_addrs:
        print_info(route_addr)
        parse_route_file(route_addr)
    
    # create mux_data_excel
    create_mux_data_excel()
# def toplogic()

# profiler = cProfile.Profile()
# profiler.enable()

######################## add rr_graph.jummap + .mux files as well
arguments = sys.argv
if len(arguments) > 1:
    version = arguments[1]
    flow_inputs_dir = arguments[2]
    route_addrs = arguments[3:]
else:
    log("bad arguments\n")
    exit()

if version == "SR":
        us = 1
        log('using stamped rr_graph\n')
elif version == "NS":
    log('using non-stamped rr_graph\n')
else:
    log('ERROR: first argument (version) must be either "SR" or "NS"\n')
    exit()

toplogic()

log('\n\nEnd of script.\n')

# profiler.disable()
# profiler.print_stats(sort='tottime')