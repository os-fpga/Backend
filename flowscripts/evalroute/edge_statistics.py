import xml.etree.ElementTree as ET
import sys
import os
import datetime
from collections import defaultdict
from collections import Counter
import cProfile
import pickle
import xlsxwriter
from fpga_data import *


mux_id2type = {}
#     0: 'LR',
#     1: 'CB',
#     2: 'L1',
#     # no L2 so far
#     3: 'L4',
# }

max_x = max_y = 0

include_OPINs = 0


arch = ''
rrg = ''
route = ''
pinmap_file = ''

# dictionary of dictionaries:
#                            {'id' --> { 'id'--> id,
#                                       'type' --> type,
#                                       'direction' --> direction,
#                                       'side' --> side,
#                                       'ptc' --> ptc,
#                                       'bit' --> bit,
#                                       'x_low' --> x_low,
#                                       'x_high' --> x_high,
#                                       'y_low' --> y_low,
#                                       'y_high' --> y_high,
#                                       'length' --> length,
#                                       'ph_length' --> ph_length}}, ... }
rr_node_list = {}

# list of dictionaries: 
#                       [{'source_id' --> source_id,
#                       'sink_id' --> sink_id,
#                       'switch_id' --> switch_id}}, ...]
rr_edge_list = []


# dictionary of dictionaries:
#                            {'(x, y)' --> {'block_type_id' --> block_type_id,
#                                           'block_type' --> block_type,
#                                           'height_offset' --> height_offset,
#                                           'width_offset' --> width_offset,
#                                           'x' --> x
#                                           'y' --> y}}, ... }
rr_grid_list = {}


# dictionary of dictionaries:
#                            {'id' -->  {   'id' --> id,
#                                           'name' --> name,
#                                           'width' --> width,
#                                           'height' --> height,
#                                           'pin_classes' --> pin_classes
#                                           'all_pin_list' --> all_pin_list}}, ... }
rr_block_type_list = {}


# dictionary of dictionaries:
#                            {'name' -->  { 'name' --> name,
#                                           'freq' --> freq,
#                                           'length' --> length}, ... }
arch_segments = {}


# dictionary of dictionaries:
#                            {'(x, y)' --> {'id' --> id,
#                                           'name' --> name,
#                                           'length' --> length,
#                                           'freq' --> freq}, ... }
rrg_segments = {}


# dictionary of dictionaries:
#                            {'sink_id' --> {'driver_mux_name' --> driver_mux_name,
#                                           'mux_x' --> mux_x,
#                                           'mux_y' --> mux_y,
#                                           'input_pin' --> {src_id --> (stamp_pin_name, usage)}}, ... }
simple_edge_list = {}

sorted_simple_edge_list = {}



CHAN_span2ptcs = defaultdict(set) # all CHANs that have the same direction and coordinates share the same set of node_id and ptc#


# dictionary of dictionaries:
#                            {'block_port' -->  {   'id' --> id,
#                                                   'x_offset' --> x_offset,
#                                                   'y_offset' --> y_offset,
#                                                   'block_type' --> block_type,
#                                                   'stamp_port' --> stamp_port,
#                                                   'direction' --> direction,
#                                                   'ptc' --> ptc
#                                                   'block_port' --> block_port}}, ... }
pin_map = {} # pin map


# dictionaries contatining the content of report file and excel sheets
mux_surface_excel_include_OPINs = {} # (mux_name, key2) --> {'value': value, 'format': format, 'coordinates': coordinates}
mux_surface_excel_no_OPINs = {} # (mux_name, key2) --> {'value': value, 'format': format, 'coordinates': coordinates}


def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log


def out(text, filename):
    """write to specific file"""
    with open(filename, "w") as f:
        f.write(text)
# def out

# (driver_loc, tap) --> tap name
def node_distance2tap(driver, tap):
    distance2tap = {
    0: 'B',
    1: 'Q',
    2: 'M',
    3: 'P',
    4: 'E',
    5: 'F', # Temporary - should not happen
    }
    distance = tap - driver
    return distance2tap[distance]
# def node_distance2tap

# determine the name of tap based on sink and source nodes coordinates -- both are CHAN nodes
# MUX_X and MUX_Y are driver's coordinates
def get_tap_name(MUX_X, MUX_Y, source_node):
    global max_x, max_y
    tap = ''
    (src_xl,src_xh,src_yl,src_yh) = (source_node['x_low'],source_node['x_high'],source_node['y_low'],source_node['y_high'])
    src_dir = source_node['direction']


    if src_dir == 'E':
        x = src_xl - 1 # the x coordinate of the source driver mux
        if x < 0: x = 0 # if the x is outside array
        if (source_node['ph_length'] < source_node['length'] and x < source_node['length'] and source_node['x_high'] < (max_x-2)): # near the left boundry, the node is cut off
            tap = node_distance2tap((x - (source_node['length'] - source_node['ph_length'])), MUX_X) # tap distance = MUX_X - x + (length - ph_length)
        else:
            tap = 'E' if (source_node['length'] == 1) else node_distance2tap(x, MUX_X) # if the length is 1, we always have E as the tap name for INC_DIR
    
    elif src_dir == 'W':
        x = src_xh # the source driver mux's x coordinate
        if (source_node['ph_length'] < source_node['length'] and x > source_node['length']): # near the right boundry, the node is cut off
            tap = node_distance2tap(MUX_X, x + (source_node['length'] - source_node['ph_length'])) # x + (length - ph_length) - MUX_X
        else:
            # we can have tap at B and E for L1 DEC_DIR (Q is not correct)
            tap = 'E' if (source_node['length'] == 1 and node_distance2tap(MUX_X, x) == 'Q') else node_distance2tap(MUX_X, x)
    
    elif src_dir == 'N':
        y = src_yl - 1 # the source driver mux's y coordinate
        if y < 0: y = 0 # boundary y
        if (source_node['ph_length'] < source_node['length'] and y < source_node['length'] and source_node['y_high'] < (max_y-2)): # near the bottom boundry, the node is cut off
            tap = node_distance2tap((y - (source_node['length'] - source_node['ph_length'])), MUX_Y) # tap distance = MUX_Y - y + (length - ph_length)
        else:
            tap = 'E' if (source_node['length'] == 1) else node_distance2tap(y, MUX_Y) # if the length is 1, we only have E as the tap name for INC_DIR
    
    elif src_dir == 'S':
        y = src_yh # the source driver mux's y coordinate
        if (source_node['ph_length'] < source_node['length'] and y > source_node['length']): # near the top boundry, the node is cut off
            tap = node_distance2tap(MUX_Y, y + (source_node['length'] - source_node['ph_length'])) # y + (length - ph_length) - MUX_Y
        else:
            # we can have tap at B and E for L1 DEC_DIR (Q is not correct)
            tap = 'E' if (source_node['length'] == 1 and node_distance2tap(MUX_Y, y) == 'Q') else node_distance2tap(MUX_Y, y)
    # if tap == 'F':
    #     log(f"tapped at 5 blocks away:\n")
    #     log(f"mux coordinates: ({MUX_X}, {MUX_Y})\n")
    #     log(f"source node:\n {source_node}\n\n")
    return tap
# def get_tap_name


def get_mux_tap_name(node):
    global max_x, max_y
    direction = node['direction']
    tap_name = 'B' # Default value for the mux position on the node
    
    # determingin the X, Y coordinates and mux position other than the default position ('B')
    if direction == 'E':
        x = node['x_low'] - 1
        y = node['y_low'] # low or high really doesn't matter for this
        if x < 0: x = 0
        if (node['ph_length'] < node['length'] and x < node['length'] and node['x_high'] < (max_x-2)): # near the left boundry the node is cut off
            tap_name = node_distance2tap(node['ph_length'], node['length']) # mux is not in the beginning ('B') by default
            #log(f"direction:{direction},\t x:{x},\t y:{y},\t physical length:{node['ph_length']},\t logical length:{node['length']}\n")
    
    elif direction == 'W':
        x = node['x_high']
        y = node['y_high'] # low or high really doesn't matter for this
        if (node['ph_length'] < node['length'] and x > node['length']): # near the right boundry the node is cut off
            tap_name = node_distance2tap(node['ph_length'], node['length']) # mux is not in the beginning ('B') by default
            #log(f"direction:{direction},\t x:{x},\t y:{y},\t physical length:{node['ph_length']},\t logical length:{node['length']}\n")
    
    elif direction == 'N':
        y = node['y_low'] - 1
        x = node['x_low'] # low or high really doesn't matter for this
        if y < 0: y = 0
        if (node['ph_length'] < node['length'] and y < node['length'] and node['y_high'] < (max_y-2)): # near the bottom boundry the node is cut off
            tap_name = node_distance2tap(node['ph_length'], node['length']) # mux is not in the beginning ('B') by default
            #log(f"direction:{direction},\t x:{x},\t y:{y},\t physical length:{node['ph_length']},\t logical length:{node['length']},\t max_y:{max_y}\n")

    elif direction == 'S':
        y = node['y_high']
        x = node['x_high'] # low or high really doesn't matter for this
        if (node['ph_length'] < node['length'] and y > node['length']): # near the top boundry the node is cut off
            tap_name = node_distance2tap(node['ph_length'], node['length']) # mux is not in the beginning ('B') by default
            #log(f"direction:{direction},\t x:{x},\t y:{y},\t physical length:{node['ph_length']},\t logical length:{node['length']}\n")
    return x, y, tap_name
# get_mux_tap_name


# process each edge, create and add the wire names and mux input names to the appropriate list
def process_edges():
    _simple_edge_list = {}
    ignored_pin = set()
    
    #log('processing the edge list in rr graph\n')
    for edge in rr_edge_list:
        source_id = edge['source_id']
        #log(f"source_id is {source_id}\n")
        source_node = rr_node_list[source_id]
        #log(f"source_node is {source_node}\n")
        sink_id = edge['sink_id']
        #log(f"sink_id is {sink_id}\n")
        sink_node = rr_node_list[sink_id]
        #log(f"sink_node is {sink_node}\n")
        mux_type = mux_id2type[edge['switch_id']]
        #log(f"mux_type is {mux_type}\n")
        if mux_type == 'LR':
            #log(f"edge is in local routing. will be skipped.\n")
            continue
        # now we have source_node --> sink_node with switch_type (CB, L1, L4, ...)

        driver_mux_name = ''
        input_pin_name = ''
        MUX_Y = MUX_Y = 0


        #################### determining the driver mux name (e.g., E4B7) ####################
        if sink_id in _simple_edge_list: # if the sink is already visited from another edge
            driver_mux_name = _simple_edge_list[sink_id]['driver_mux_name']
            MUX_X = _simple_edge_list[sink_id]['mux_x']
            MUX_Y = _simple_edge_list[sink_id]['mux_y']
            #log(f"sink is already visited\n")
            #log(f"driver mux name is {driver_mux_name}\n")
            #log(f"MUX_X, MUX_Y = {MUX_X}, {MUX_Y}\n")
        
        else:
            
            if sink_node['type'].startswith('CHAN'):

                (MUX_X, MUX_Y, tap_name) = get_mux_tap_name(sink_node)

                # determining the first letter (direction)
                direction = sink_node['direction']
                #log(f"First letter of sink (direction) is {direction}\n")

                # determining the second letter (length)
                length = str(sink_node['length'])
                #log(f"Second letter of sink (length) is {length}\n")

                # determining the third letter (tap point) - at boundries we might have a tap name other than 'B' (default is 'B')
                tap = tap_name
                #log(f"Third letter of sink (tap point) is {tap}\n")

                # determining the fourth letter (bit#)
                bit = str(sink_node['bit'])
                #log(f"fourth letter of sink (bit#) is {bit}\n")

                driver_mux_name = direction + length + tap + bit

                #log(f"CHAN driver mux name is {driver_mux_name}\n")
                #log(f"MUX_X, MUX_Y = {MUX_X}, {MUX_Y}\n")
            
            # elif sink_node['type'] == 'OPIN': ############### It is not possible for inter-tile routing (only local routings SOURCE --> OPIN) ###############
            
            elif sink_node['type'] == 'IPIN':
                grid = rr_grid_list[(sink_node['x_low'],sink_node['y_low'])] # return the grid using x,y coordinate

                # now that we have grid, we can find find the block type:
                block_tpye = rr_block_type_list[grid['block_type_id']]
                #log(f"sink block type id = {grid['block_type_id']}\n")
                # now look for pin name based on its ptc number:
                all_pin_list = block_tpye['all_pin_list']
                #log(f"sink all_pin_list = {all_pin_list}\n")
                pin_vpr_name = all_pin_list[sink_node['ptc']]['name']
                #log(f"sink pin_vpr_name = {pin_vpr_name}\n")
                #pin_stamp_name = pin_map[pin_vpr_name]['stamp_port']
                pin_stamp_name = pin_map.get(pin_vpr_name, {}).get('stamp_port', None)
                if pin_stamp_name == None:
                    log(f"WARNING: sink block type id = {grid['block_type_id']}, pin name= {pin_vpr_name} is not assigned to an stamper pin name\n")
                    ignored_pin.add(pin_vpr_name)
                    continue
                #log(f"sink pin_stamp_name = {pin_stamp_name}\n")
                MUX_X = sink_node['x_low']
                MUX_Y = sink_node['y_low']
                #log(f"MUX_X, MUX_Y = {MUX_X}, {MUX_Y}\n")
                
                driver_mux_name = pin_stamp_name
        #################### determining the driver mux name (e.g., E4B7) ####################

        
        #################### determining the source name of driver mux pin (e.g., E4E7 or O13) ####################
        if source_node['type'].startswith('CHAN'):

            # determining the first letter (direction)
            direction = source_node['direction']
            #log(f"First letter of source (direction) is {direction}\n")

            # determining the second letter (length)
            length = str(source_node['length'])
            #log(f"Second letter of source (length) is {length}\n")

            # determining the third letter (tap point)
            tap = get_tap_name(MUX_X, MUX_Y, source_node)
            
            # determining the fourth letter (bit#)
            bit = str(source_node['bit'])
            #log(f"fourth letter of source (bit#) is {bit}\n")

            input_pin_name = direction + length + tap + bit
            #log(f"source input_pin_name = {input_pin_name}\n")

        elif source_node['type'] == 'OPIN':
            # determining the input pin name
            grid = rr_grid_list[(source_node['x_low'],source_node['y_low'])] # return the grid using x,y coordinate
            
            # now that we have grid, we can find find the block type:
            block_tpye = rr_block_type_list[grid['block_type_id']]
            
            #log(f"source block type id = {grid['block_type_id']}\n")
            # now look for pin name based on its ptc number:
            all_pin_list = block_tpye['all_pin_list']
            pin_vpr_name = all_pin_list[source_node['ptc']]['name']
            
            pin_stamp_name = pin_map.get(pin_vpr_name, {}).get('stamp_port', None)
            
            if pin_stamp_name == None:
                log(f"WARNING: source block type id = {grid['block_type_id']}, pin name = {pin_vpr_name} is not assigned to an stamper pin name\n")
                continue
            #log(f"source input_pin_name = {pin_stamp_name}\n")

            input_pin_name = pin_stamp_name

        #################### determining the source name of driver mux pin (e.g., E4B7 or O13) ####################

        if sink_id in _simple_edge_list:
            _simple_edge_list[sink_id]['input_pin'][source_id] = (input_pin_name, 0)
        
        else:
            new_simple_edge = {
            'driver_mux_name': driver_mux_name,
            'mux_x': MUX_X,
            'mux_y': MUX_Y,
            'input_pin': {source_id: (input_pin_name, 0)} # source_id --> (stamp_pin_name, usage)
            }
            
            _simple_edge_list[sink_id] = new_simple_edge

    lines = ''   
    for pin in ignored_pin:
        lines += f"ignored pin = {pin}\n"
    out(lines, "ignored_pins")
    
    return _simple_edge_list
# process_edges()


# return the key for sorting muxes below
def sort_key_mux(item):
    global rrg_segments

    sorted_segment_list = sorted(rrg_segments.values(), key=lambda x: int(x['length']))
    node_types = {}

    i = 0
    for segment in sorted_segment_list:
        node_types[f"E{segment['length']}"] = i
        node_types[f"W{segment['length']}"] = i+1
        node_types[f"N{segment['length']}"] = i+2
        node_types[f"S{segment['length']}"] = i+3
        i += 4
    node_types['I'] = i+1
    node_types['IS'] = i+2
    
    #order of node types
    # node_types = {
    # 'E0': 0,
    # 'W0': 1,
    # 'N0': 2,
    # 'S0': 3,
    # 'E1': 4,
    # 'W1': 5,
    # 'N1': 6,
    # 'S1': 7,
    # 'E2': 9,
    # 'W2': 10,
    # 'N2': 11,
    # 'S2': 12,
    # 'E4': 13,
    # 'W4': 14,
    # 'N4': 15,
    # 'S4': 16,
    # 'E5': 17, 
    # 'W5': 18, 
    # 'N5': 19,
    # 'S5': 20,
    # 'I': 21,
    # 'IS': 22,
    # }

    # separating the name and bit number
    
    # IS nodes
    if item[1]['driver_mux_name'].startswith('IS'):
        wire_type = item[1]['driver_mux_name'][:2]
        wire_num = int(item[1]['driver_mux_name'][2:]) + 7000 # this + 7000 is because IS nodes come after I nodes
        #log(f"IS wire_type = {wire_type}, wire_num = {wire_num}\n")

    # I nodes
    elif item[1]['driver_mux_name'].startswith('I'):
        wire_type = item[1]['driver_mux_name'][:1]
        wire_num = int(item[1]['driver_mux_name'][1:]) + 6000 # this + 6000 is because I nodes come after CHAN nodes
        #log(f"I/O wire_type = {wire_type}, wire_num = {wire_num}\n")
    
    # #L4 nodes
    # elif item[1]['driver_mux_name'][1] == '4':
    #     wire_type = item[1]['driver_mux_name'][:2]
    #     wire_num = int(item[1]['driver_mux_name'][3:]) + 16 # this + 16 is because L4 nodes come after 0-15 replicas of L1 nodes
    #     #log(f"IS wire_type = {wire_type}, wire_num = {wire_num}\n")

    # L1 wires --> comes first
    else:
        wire_type = item[1]['driver_mux_name'][:2]
        wire_num = int(item[1]['driver_mux_name'][3:]) + int(item[1]['driver_mux_name'][1])*1000
        #log(f"CHAN wire_type = {wire_type}, wire_num = {wire_num}\n")

    # order key: x, y, node_type, bit#
    return (item[1]['mux_x'], item[1]['mux_y'], int(wire_num), node_types[wire_type])
# def sort_key_mux


# return the key for sorting pins below
def sort_key_pins(item):

    global rrg_segments

    sorted_segment_list = sorted(rrg_segments.values(), key=lambda x: int(x['length']))
    node_types = {}

    i = 0
    for segment in sorted_segment_list:
        node_types[f"E{segment['length']}"] = i
        node_types[f"W{segment['length']}"] = i+1
        node_types[f"N{segment['length']}"] = i+2
        node_types[f"S{segment['length']}"] = i+3
        i += 4
    node_types['O'] = i+1

    #order of node types
    # node_types = {
    # 'E0': 0,
    # 'W0': 1,
    # 'N0': 2,
    # 'S0': 3,
    # 'E1': 4,
    # 'W1': 5,
    # 'N1': 6,
    # 'S1': 7,
    # 'E2': 9,
    # 'W2': 10,
    # 'N2': 11,
    # 'S2': 12,
    # 'E4': 13,
    # 'W4': 14,
    # 'N4': 15,
    # 'S4': 16,
    # 'E5': 17, 
    # 'W5': 18, 
    # 'N5': 19,
    # 'S5': 20,
    # 'O': 12,
    # }

    # O nodes
    if item[1][0].startswith('O'):
        wire_type = item[1][0][:1]
        wire_num = int(item[1][0][1:]) + 7000 # this + 7000 is because O nodes come after CHAN nodes
        #log(f"I/O wire_type = {wire_type}, wire_num = {wire_num}\n")
    
    # # L4 nodes
    # elif item[1][0][1] == '4':
    #     wire_type = item[1][0][:2]
    #     wire_num = int(item[1][0][3:]) + 16 # this + 16 is because L4 nodes come after 0-15 replicas of L1 nodes
    
    # L1 nodes
    else:
        wire_type = item[1][0][:2]
        wire_num = int(item[1][0][3:]) + int(item[1][0][1])*1000
        #log(f"CHAN wire_type = {wire_type}, wire_num = {wire_num}\n")

    # order key: node_type, bit#
    return (int(wire_num), node_types[wire_type])
# def sort_key_mux


# sort simple edges based on the desired sorting pattern we have
def sort_simple_edges(edge_list):

    global sorted_simple_edge_list

    # Sort the mux names based on x, y, node_type and bit#
    sorted_simple_edge_list = {sink_id: item for sink_id, item in sorted(edge_list.items(), key=sort_key_mux)}

    # Sort the input pins for each sink based on the value of stamp_pin_name with the order of node_type, bit#
    for edge in sorted_simple_edge_list.values():
        edge['input_pin'] = dict(sorted(edge['input_pin'].items(), key=sort_key_pins))


    return sorted_simple_edge_list
# def sort_simple_edges


# print mux surface info, and mux histogram
def print_mux_info(edge_list, show_ID):
    show_id = show_ID #show node_id after the name of the mux and mux input pin
    mux_histogram = {}
    # print out the muxes with their inputs
    if show_id:
        lines = '(xd,yd)\t mux_name (id) <-- pin_name[usage] (id)\t pin_name[usage] (id)\t ... pin_name[usage] (id)\n\n'
    else:
        lines = '(xd,yd)\t mux_name <-- pin_name[usage]\t pin_name[usage]\t ... pin_name[usage]\n\n'
    for key, value in edge_list.items():
        sink_id = key
        mux_name = value['driver_mux_name']
        mux_x = value['mux_x']
        mux_y = value['mux_y']
        input_pin_list = value['input_pin']
        if show_id:
            lines += f"({mux_x}, {mux_y})\t {mux_name} ({sink_id}) <-- \t"
        else:
            lines += f"({mux_x}, {mux_y})\t {mux_name} <-- \t"
        mux_width = 0
        for src_id, (input_pin_name, usage) in input_pin_list.items():
            #index = input_pin_list['id'].index(src_id)
            if show_id:
                if include_OPINs:
                    mux_width += 1
                    lines += f"{mux_width}.{input_pin_name}[{usage}] ({src_id})\t"
                else:
                    if not input_pin_name.startswith('O'):
                        mux_width += 1
                        lines += f"{mux_width}.{input_pin_name}[{usage}] ({src_id})\t"
            else:
                if include_OPINs:
                    mux_width += 1
                    lines += f"{mux_width}.{input_pin_name}[{usage}]\t"
                else:
                    if not input_pin_name.startswith('O'):
                        mux_width += 1
                        lines += f"{mux_width}.{input_pin_name}[{usage}]\t"
        
        if mux_name.startswith('I'):
            mux_histogram.setdefault((mux_x, mux_y, mux_width, 'CB'), []).append(mux_name)
        else:
            mux_histogram.setdefault((mux_x, mux_y, mux_width, 'SB'), []).append(mux_name)
        lines += "\n"
    file_name = 'mux_surface_not_OPINs'
    if include_OPINs:
        file_name = 'mux_surface_with_OPINs'
    if show_id:
        file_name += '_showID'
    out(lines, file_name)

    
    lines = 'coordinate\t #muxes\t mux_type\t width\n\n'
    sorted_histogram = dict(sorted(mux_histogram.items(), key=lambda item: (item[0][0], item[0][1], item[0][2]), reverse=True))
    for key, value in sorted_histogram.items():
        (mux_x, mux_y, mux_width, mux_type) = key
        mux_list = value
        num_of_muxes = len(value)
        lines += f"({mux_x},{mux_y})\t\t {num_of_muxes}\t\t {mux_type}\t\t\t {mux_width}\n"
    
    file_name = 'mux_histogram_not_OPINs'
    if include_OPINs:
        file_name = 'mux_histogram_with_OPINs'
    out(lines, file_name)
# def print_mux_info


# print translation table file
def print_translation_table(node_list):
    # print all nodes with same L1/L4, CHANX/Y, INC_DIR/DEC_DIR, and xl, yl, xh, yh but with different ptc#
    nodes = {}
    for id, node in node_list.items():
        X = Y = 0
        mux_name = ''
        if node['type'].startswith('CHAN'):
            (X, Y, tap_name) = get_mux_tap_name(node)
            direction = node['direction']
            length = str(node['length'])
            bit = str(node['bit'])
            mux_name = direction + length + tap_name + bit
            nodes[(node['x_low'], 
                       node['y_low'], 
                       node['x_high'], 
                       node['y_high'], 
                       f"L{node['length']}", 
                       node['type'],
                       node['direction'],
                       node['ptc'],
                       node['id'])] = f"{mux_name}_X{X}_Y{Y}"
    
    sorted_nodes = dict(sorted(nodes.items(), key=lambda item: (item[0][0], item[0][1], item[0][4], item[0][5], item[0][6])))
    lines = '(xl,yl) - \t(xh,yh) \tlength \tnode_type \t(ptc) \tdriving_mux \tid\n\n'
    for key, value in sorted_nodes.items():
        x1, y1, x2, y2, length, node_type, direction, ptc, node_id = key
        lines += f"({x1}, {y1}) \t- \t({x2}, {y2}) \t\t{length} \t\t{node_type[:4]}{direction} \t\t({ptc}) \t{value} \t{node_id}\n"
    out(lines,'translation_table')
# def print_translation_table


# NOTE:CB muxes that have 16 inputs do not belong to CLBs. They actually belong to DSP and BRAM tiles.
# I looked at other.mux file. All CB muxes of normal IPINs (I00 - I75) have 16 inputs (IS muxes are similar). 
# That is why we have CB muxes with 16 inputs in columns 4 and 7. But the problem is if I want to show all the inputs in one row (e.g., I75 inputs), 
# I would have 18 unique inputs because CB muxes in BRAM and DSP tiles have 4 inputs that are different from 14 inputs of CB muxes in CLBs.
# TODO: I should either have separate rows for DSP and BRAM or have all 18 inputs in one row. The latter is the current approach. 

#initial mux surface excel dict based on sorted_simple_edge_list
def initial_mux_surface_excel(edge_list):
    global rrg_segments
    global g_max_bit
    mux_surface = {}

    sorted_segment_list = sorted(rrg_segments.values(), key=lambda x: int(x['length']))
    

    # header info
    mux_surface[(-1, 'HEADER')]         = {'value': -1,         'format': 'bold',   'coordinates': (0,0)}
    b = 1
    for segment in sorted_segment_list:
        mux_surface[(f"L{segment['length']}-SB", 'HEADER')]    = {'value': f"L{segment['length']}-SB",    'format': 'bold',   'coordinates': (0,b)}
        b += 30
    mux_surface[('CB', 'HEADER')]       = {'value': 'CB',       'format': 'bold',   'coordinates': (0,b)}

    
    j = 0
    b = 1
    i_HEADER = 0
    for segment in sorted_segment_list:
        jE = jW = jN = jS = 0
        i_HEADER = 0
        for key, value in g_max_bit.items():
            if key[1] == segment['length']:
                for i in range(0, value):
                    row = 2*i_HEADER + 1
                    mux_surface[(f"({i_HEADER})", f'{(row,b)}-HEADER')]              = {'value': f"({i_HEADER})",         'format': 'bold', 'coordinates': (row,b)}
                    i_HEADER += 1
                #log(f" key = {key}\t value = {key}\t value = {value}\t length = {segment['length']}\n")
                for i in range(0, value):
                    #row = 4*2*i + 1
                    if key[0] == 'E':
                        row = 4*2*i + 1
                        mux_surface[(f"E{segment['length']}B{jE}", 'HEADER')]    = {'value': f"E{segment['length']}B{jE}",    'format': 'bold',   'coordinates': (row,b+1)}
                        #log(f"adding new key and value: key: (E{segment['length']}B{jE}, 'HEADER') --> value: (E{segment['length']}B{jE}), coordinate: ({(row,b+1)})\n")
                        jE += 1
                    elif key[0] == 'W':
                        row = 4*2*i + 3
                        mux_surface[(f"W{segment['length']}B{jW}", 'HEADER')]    = {'value': f"W{segment['length']}B{jW}",    'format': 'bold',   'coordinates': (row,b+1)}
                        #log(f"adding new key and value: key: (W{segment['length']}B{jW}, 'HEADER') --> value: (W{segment['length']}B{jW}), coordinate: ({(row,b+1)})\n")
                        jW += 1
                    elif key[0] == 'N':
                        row = 4*2*i + 5
                        mux_surface[(f"N{segment['length']}B{jN}", 'HEADER')]    = {'value': f"N{segment['length']}B{jN}",    'format': 'bold',   'coordinates': (row,b+1)}
                        #log(f"adding new key and value: key: (N{segment['length']}B{jN}, 'HEADER') --> value: (N{segment['length']}B{jN}), coordinate: ({(row,b+1)})\n")
                        jN += 1
                    elif key[0] == 'S':
                        row = 4*2*i + 7
                        mux_surface[(f"S{segment['length']}B{jS}", 'HEADER')]    = {'value': f"S{segment['length']}B{jS}",    'format': 'bold',   'coordinates': (row,b+1)}
                        #log(f"adding new key and value: key: (S{segment['length']}B{jS}, 'HEADER') --> value: (S{segment['length']}B{jS}), coordinate: ({(row,b+1)})\n")
                        jS += 1
        b += 30

    j = 0
    for i in range(0,48): #I00 - I75
        row = 2*i + 1
        mux_surface[(f'({i})', f'{(row,b)}-HEADER')]              = {'value': f'({i})',         'format': 'bold', 'coordinates': (row,b)}
        mux_surface[(f'I{j}{i%6}', 'HEADER')]   = {'value': f'I{j}{i%6}', 'format': 'bold',    'coordinates': (row,b+1)}
        if i%6 == 5:
            j += 1
    

    for i in range(48,54): #IS0 - IS5
        row = 2*i + 1
        mux_surface[(f'({i})', f'{(row,b)}-HEADER')]              = {'value': f'({i})',         'format': 'bold', 'coordinates': (row,b)}
        mux_surface[(f'IS{i%6}', 'HEADER')]     = {'value': f'IS{i%6}', 'format': 'bold',    'coordinates': (row,b+1)}

    mux_inputs = {} # (input_pin_name, mux_name)
    mux_input_offset = {} # mux_name --> offset


    # add all mux names and their inputs
    for key,edge in edge_list.items():
        mux_name = edge['driver_mux_name']
        input_pins = edge['input_pin']
        (mux_x, mux_y) = (edge['mux_x'], edge['mux_y'])
        if mux_x <= 1 or mux_y <= 1 or mux_x == (max_x-2) or mux_y == (max_y-2): # Ignoring IO Tiles
                continue
        if (mux_name,'HEADER') not in mux_surface: # Ignoring irregular names of muxes --> should not happen if IO tiles already ignored
            log(f"Ignoring mux_name = {mux_name} because it is not in the mux_surface regular names (e.g., E4Q3 as the mux the name)")
            continue
        #TODO: ignore DSP/BRAM tiles and add them to another mux_surface
        # if mux_y in DSP_BRAM_tiles:
            # continue # TODO: repeat the same for loop this time for DSP/BRAM

        # TODO: repeat this loop for DSP/BRAM
        for input_pin_id, (input_pin_name, input_pin_usage) in input_pins.items():
            if include_OPINs == 0 and input_pin_name.startswith('O'):
                continue
            if (mux_name, input_pin_name) in mux_inputs:
                mux_inputs[(mux_name, input_pin_name)]['usage'] = (mux_inputs[(mux_name, input_pin_name)]['usage'] + input_pin_usage)
            else: # add this key pair for the first time
                mux_input_offset[mux_name] = mux_input_offset.get(mux_name, 0) + 1
                mux_inputs[(mux_name, input_pin_name)] = {'offset': mux_input_offset[mux_name],
                                                          'usage':  input_pin_usage}
    # TODO: repeat this loop for DSP/BRAM
    for key, value in mux_inputs.items():
        (mux_name, input_pin_name) = key
        (offset, usage) = (value['offset'], value['usage'])
        (mux_row,mux_col) = mux_surface[(mux_name,'HEADER')]['coordinates']
        mux_surface[(mux_name, input_pin_name)] = {'value': input_pin_name, 'format': 'cell_format', 'coordinates': (mux_row, (mux_col + offset))}
        mux_surface[(mux_name, f"{input_pin_name}_usage")] = {'value': usage, 'format': 'cell_format', 'coordinates': ((mux_row-1), (mux_col + offset))}
    
    # TODO: return a new list for DSP/BRAM
    return mux_surface    
#def initial_mux_surface_excel


# create mux surface excel file once all the .route files are traversed
def create_mux_surface_excel(mux_surface):
    # Create a new workbook and worksheet
    excel_name = 'mux_input_map_with_OPINs' if include_OPINs else 'mux_input_map_not_OPINs'
    
    if os.path.exists(f'{excel_name}.xlsx'):
        # If the file exists, remove it first
        os.remove(f'{excel_name}.xlsx')
    
    workbook = xlsxwriter.Workbook(f'{excel_name}.xlsx')
    worksheet = workbook.add_worksheet()
    
    # Define some formatting for the headers and cells
    bold = workbook.add_format({'bold': True, 'align': 'center'})
    cell_format = workbook.add_format({'align': 'center'})
    for key, value in mux_surface.items():
        cell_val, cell_frmt, (row,col) = value['value'], value['format'], value['coordinates']
        if cell_frmt == 'bold':
            worksheet.write(row, col, cell_val, bold)
        else:
            worksheet.write(row, col, cell_val, cell_format)

    # add heatmap to mux usage
    worksheet.conditional_format(0, 2, worksheet.dim_rowmax, worksheet.dim_colmax, 
                             {'type': '3_color_scale',
                              'min_color': 'green',
                              'mid_color': 'yellow',
                              'max_color': 'red',
                              'min_type': 'min', # Set the lowest value 
                              'mid_type': 'percentile',# Set the midpoint at the 50th percentile (median)
                              'max_type': 'max',   # Set the highest value
                              'mid_value': 50
                             })
    workbook.close()
# def create_mux_surface_excel


# process each net --> increment edge usage and update mux surface excel file
def process_net(net):

    #TODO: update new mux_surfaces for BRAM/DSP here

    global mux_surface_excel_include_OPINs
    global mux_surface_excel_no_OPINs
    global sorted_simple_edge_list

    for node in net:
        (src_node_id, src_node_type, src_node_index) = node
        #log(f"\t\tsrc_node_id = {src_node_id}, src_node_type = {src_node_type}")
        if src_node_type == 'SOURCE' or src_node_type == 'SINK': # No need to process this edge
            #log(" --> skipped\n")
            continue
        #log("\n")
        (snk_node_id, snk_node_type, snk_node_index) = net[src_node_index+1]
        #log(f"\t\tsnk_node_id = {snk_node_id}, snk_node_type = {snk_node_type}")
        if snk_node_type == 'SINK': # No need to process this edge
            #log(" --> skipped\n")
            continue
        #log("\n")

        # increment mux input usage
        if int(snk_node_id) not in sorted_simple_edge_list.keys():
            print(f"ERROR: There is a sink node in .route file with id = {snk_node_id} that has no assigned MUX name (probably due to ignored pins)\n")
            continue
        if int(src_node_id) not in sorted_simple_edge_list[int(snk_node_id)]['input_pin'].keys():
            print(f"ERROR: There is a source node in .route file with id = {src_node_id} that has no assigned MUX input pin name (probably due to ignored pins)\n")
            continue
        
        (sink_name, usage) = sorted_simple_edge_list[int(snk_node_id)]['input_pin'][int(src_node_id)]
        mux_name = sorted_simple_edge_list[int(snk_node_id)]['driver_mux_name']
        
        # increment edge usage
        usage += 1 
        sorted_simple_edge_list[int(snk_node_id)]['input_pin'][int(src_node_id)] = (sink_name, usage)

        # required data for incrementing input pin usage of mux_surface_excel
        (mux_x, mux_y) = (sorted_simple_edge_list[int(snk_node_id)]['mux_x'], sorted_simple_edge_list[int(snk_node_id)]['mux_y'])

        if include_OPINs == 0 and sink_name.startswith('O'):
            continue
        if mux_x <= 1 or mux_y <= 1 or mux_x == (max_x-2) or mux_y == (max_y-2): # Ignoring IO Tiles
            continue
        
        # Update mux_surface_excel usage
        cell_format = ''
        row = col = cell_usage = 0
        if include_OPINs:
            cell_usage = mux_surface_excel_include_OPINs[(mux_name, f"{sink_name}_usage")]['value']
            cell_format = mux_surface_excel_include_OPINs[(mux_name, f"{sink_name}_usage")]['format']
            (row, col) = mux_surface_excel_include_OPINs[(mux_name, f"{sink_name}_usage")]['coordinates']
            cell_usage += 1
            mux_surface_excel_include_OPINs[(mux_name, f"{sink_name}_usage")] = {'value': cell_usage, 'format': cell_format, 'coordinates': (row, col)}
            
        else:
            cell_usage = mux_surface_excel_no_OPINs[(mux_name, f"{sink_name}_usage")]['value']
            cell_format = mux_surface_excel_no_OPINs[(mux_name, f"{sink_name}_usage")]['format']
            (row, col) = mux_surface_excel_no_OPINs[(mux_name, f"{sink_name}_usage")]['coordinates']
            cell_usage += 1
            mux_surface_excel_no_OPINs[(mux_name, f"{sink_name}_usage")] = {'value': cell_usage, 'format': cell_format, 'coordinates': (row, col)}
# def process_net


# traverse all nets in a .route file
def route_traverse(route_file):
    #log('traverse all nets in a .route file and return an array of nets with all their nodes\n')
    route_content = route_file.readlines()
    i = 0
    #Net_list = {}
    while i < len(route_content): # read every line of the .route file
        if 'Net' in route_content[i] and 'global' not in route_content[i]: # if there is a new Net
            net_id = route_content[i].split()[1]
            #log(f"net_id = {net_id}\n")
            net_name = route_content[i].split()[2][1:-1]
            #log(f"net_name = {net_name}\n")
            #Net_list[(net_id,net_name)] = {} # keep the Net info and nodes info of this Net
            net_line_idx = i # keep the index of Net in the list
            j = net_line_idx + 2 # skip the this line that contains 'Net' and the next line which is blank
            if (j >= len(route_content)): # check if reaches the end of file
                break
            node_list = list()
            node_index = 0
            while ('Node' in route_content[j]):
                node_id = route_content[j].split()[1]
                #log(f"\tnode_id = {node_id}\n")
                node_type = route_content[j].split()[2]
                #log(f"\tnode_type = {node_type}\n")
                node = (node_id, node_type, node_index)
                node_list.append(node)
                j = j + 1
                node_index += 1
                if (j >= len(route_content)):
                    break
            i = j + 2 # skip the next blank line (\n) and go to next Net line
            process_net(node_list)
            #Net_list.append(processed_net) # process the nodes of this net and return the info of the route
        else:
            i = i + 1
            continue
# def route_traverse


# print files info
def print_info(arch_address, rrg_address, route_address, pin_map_address):

    
    global route
    route = get_route_file(route_address)
    lines = ''

    ################ Architecture File ################
    arch_filename = arch_address

    # Get the file name and absolute path
    basename = os.path.basename(arch_filename)
    abspath = os.path.abspath(arch_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(arch_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

    
    # Print the file name, address, and last modified date
    lines += "Architecture File name: {}\n".format(basename)
    lines += "Architecture File address: {}\n".format(abspath)
    lines += "Architecture File Last modified: {}\n".format(modtime)
    lines += '\n'
    ################ Architecture File ################


    ################ RR Graph File ################
    rrg_filename = rrg_address

    # Get the file name and absolute path
    basename = os.path.basename(rrg_filename)
    abspath = os.path.abspath(rrg_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(rrg_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

    # Get XML root info
    # tool_comment = rrg.attrib['tool_comment']
    # tool_name = rrg.attrib['tool_name']
    # tool_version = rrg.attrib['tool_version']


    # Print the file name, address, and last modified date
    lines += "RR Graph File name: {}\n".format(basename)
    lines += "RR Graph File address: {}\n".format(abspath)
    lines += "RR Graph File Last modified: {}\n".format(modtime)
    # lines += f"RR Graph tool_comment: {tool_comment}\n"
    # lines += f"RR Graph tool_name: {tool_name}\n"
    # lines += f"RR Graph tool_version: {tool_version}\n"
    lines += '\n'
    ################ RR Graph File ################

    ################ Routing File ################
    route_filename = route_address

    # Get the file name and absolute path
    basename = os.path.basename(route_filename)
    abspath = os.path.abspath(route_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(route_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

    # Get route file info
    first_line = route.readline()
    placement_file = first_line.split(" ")[1]
    placement_id = first_line.split(" ")[3].strip()

    second_line = route.readline()
    array_size = second_line.split(":")[1].strip().split(".")[0]



    # Print the file name, address, and last modified date
    lines += "Routing File name: {}\n".format(basename)
    lines += "Routing File address: {}\n".format(abspath)
    lines += "Routing File Last modified: {}\n".format(modtime)
    lines += f"Routing File - Placement File: {placement_file}\n"
    lines += f"Routing File - Placement ID: {placement_id}\n"
    lines += f"Routing Array Size: {array_size}\n"
    lines += '\n\n\n'

    
    ################ Routing File ################


    ################ Pin Mapper File ################
    pin_map_filename = pin_map_address

    # Get the file name and absolute path
    basename = os.path.basename(pin_map_filename)
    abspath = os.path.abspath(pin_map_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(pin_map_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')


    # Print the file name, address, and last modified date
    lines += "Pin Mapper File name: {}\n".format(basename)
    lines += "Pin Mapper File address: {}\n".format(abspath)
    lines += "Pin Mapper File Last modified: {}\n".format(modtime)
    lines += '\n\n\n'

    
    ################ Routing File ################

    out(lines, 'files_info')
# def print_info()


# generate different reports and save objects if not exist
def top_module(arch_address, rrg_address, pinmap_file_addr):

    global arch
    global rrg
    global max_x
    global max_y
    global pin_map
    global arch_segments
    global mux_id2type
    global rrg_segments
    global rr_node_list
    global rr_edge_list
    global rr_block_type_list
    global rr_grid_list
    global simple_edge_list
    global sorted_simple_edge_list
    global mux_surface_excel_include_OPINs
    global mux_surface_excel_no_OPINs

    # load architecture xml file
    arch = load_object("arch_file")
    if arch == None:
        arch = get_arch_xml_root(arch_address)
        save_object(arch, "arch_file")


    # load rrg xml file
    rrg = load_object("rrg_file")
    if rrg == None:
        rrg = get_rrg_xml_root(rrg_address)
        save_object(rrg, "rrg_file")
    

    # open pin map file
    pinmap_file = get_pinmap_file(pinmap_file_addr)

    
    # make a map between vpr pin name and stamper pin name
    pin_map = load_object("pin_map")
    if pin_map == None:
        pin_map = get_pinmap_info(pinmap_file)
        save_object(pin_map, "pin_map")


    # read architecture xml file to find logical segments
    arch_segments = load_object("arch_segments")
    if arch_segments == None:
        arch_segments = get_segment_list_arch(arch)
        save_object(arch_segments, "arch_segments")


    # traverse RR graph xml
    max_size = load_object("device_size")
    if max_size == None:
        max_size = get_device_size(rrg)
        (max_x, max_y) = (max_size[0], max_size[1])
        save_object(max_size, "device_size")

    mux_id2type = load_object("mux_id2type")
    if mux_id2type == None:
        mux_id2type = get_switch_list_rrg(rrg) # list of available switches with their names
        save_object(mux_id2type, "mux_id2type")


    rrg_segments = load_object("rrg_segments")
    if rrg_segments == None:
        rrg_segments = get_segment_list_rrg(rrg, arch_segments) # list of available segments with their length
        save_object(rrg_segments, "rrg_segments")

    rr_node_list = load_object("rr_node_list")
    if rr_node_list == None:
        rr_node_list = get_rr_node_list(rrg, rrg_segments)
        save_object(rr_node_list, "rr_node_list")
    
    rr_edge_list = load_object("rr_edge_list")
    if rr_edge_list == None:
        rr_edge_list = get_rr_edge_list(rrg)
        save_object(rr_edge_list, "rr_edge_list")
    
    rr_block_type_list = load_object("rr_block_type_list")
    if rr_block_type_list == None:
        rr_block_type_list = get_rr_block_type_list(rrg)
        save_object(rr_block_type_list, "rr_block_type_list")

    rr_grid_list = load_object("rr_grid_list")
    if rr_grid_list == None:
        rr_grid_list = get_rr_grid_list(rrg, rr_block_type_list)
        save_object(rr_grid_list, "rr_grid_list")

    ################################################################# This should be the beginning of using stamper's output
    ################################################################# remove "include_OPIN" --> Always include OPINs
    ################################################################# Create mux histogram and mux surface based on .mux files and node's coordinates
    ################################################################# then traverse route file and increment usage
    ################################################################# then create excel file and update mux surface
    # convert vpr edges to simple edges
    simple_edge_list = load_object("simple_edge_list")
    if simple_edge_list == None:
        simple_edge_list = process_edges()
        save_object(simple_edge_list, "simple_edge_list")

    # sort simple edges and print them into different files including mux histogram
    sorted_simple_edge_list = load_object("sorted_simple_edge_list")
    if sorted_simple_edge_list == None:
        print("sorted_simple_edge_list object file does not exist. Creating the object file...\n")
        sorted_simple_edge_list = sort_simple_edges(simple_edge_list)
        #save_object(sorted_simple_edge_list, "sorted_simple_edge_list") # will be save at the end of execution

    # initial mux_surface dictionary #TODO: load mux_surfaces for DSP/BRAM as well
    if include_OPINs:
        mux_surface_excel_include_OPINs = load_object("mux_surface_excel_with_OPINs")
        if mux_surface_excel_include_OPINs == None:
            print("mux_surface_excel_with_OPINs object file does not exist. Creating the object file...\n")
            mux_surface_excel_include_OPINs = initial_mux_surface_excel(sorted_simple_edge_list)
            # save_object(mux_surface_excel_include_OPINs, "mux_surface_excel_include_OPINs") # will be save at the end of execution
    else:
        mux_surface_excel_no_OPINs = load_object("mux_surface_excel_not_OPINs")
        if mux_surface_excel_no_OPINs == None:
            print("mux_surface_excel_not_OPINs object file does not exist. Creating the object file...\n")
            mux_surface_excel_no_OPINs = initial_mux_surface_excel(sorted_simple_edge_list)
            # save_object(mux_surface_excel_no_OPINs, "mux_surface_excel_no_OPINs") # will be save at the end of execution
    
    # create translation table if it is not created yet
    if os.path.isfile(f"translation_table"):
        print('translation table already exists')
    else:
        print('translation table does not exist. Creating the translation table...')
        print_translation_table(rr_node_list) # print translation_table file
    
    
    ######## Following part is design dependent. All previous functions only depend on rr-graph. ########

    # traverser .route file to measure design's mux usage and ***update*** sorted_simple_edge_list and mux_surface_excel objects
    route_traverse(route) #TODO: add a for loop here

    # print mux_surface report and mux_histogram report after all routes are traversed - Only once
    print_mux_info(sorted_simple_edge_list, show_ID=1)

    # create mux_surface excel after all routes are traversed - Only once
    if include_OPINs:
        create_mux_surface_excel(mux_surface_excel_include_OPINs) # once all routes are traverse
    else:
        create_mux_surface_excel(mux_surface_excel_no_OPINs) # once all routes are traverse

    # TODO is it really necessary to save the updated versions of these objects? if different routing iterations of the same design is being processed, what happens?
    # save updated version of sorted_simple_edge_list object
    # save_object(sorted_simple_edge_list, "sorted_simple_edge_list")

    # save updated version of mux_surface_excel object #TODO: save new mux_surfaces for DSP/BRAM as well
    # if include_OPINs:
    #     save_object(mux_surface_excel_include_OPINs, "mux_surface_excel_with_OPINs")
    # else:
    #     save_object(mux_surface_excel_no_OPINs, "mux_surface_excel_not_OPINs")
    

    close_route_file(route)
    close_pinmap_file(pinmap_file)
# def top_module

# profiler = cProfile.Profile()
# profiler.enable()

arguments = sys.argv
if len(arguments) > 1:
    arch_addr = arguments[1]
    rrg_addr = arguments[2]
    route_addr = arguments[3]
    pin_map_addr = arguments[4]
    include_opins = arguments[5]
    include_OPINs = int(include_opins.split('=')[1])
    print_info(arch_addr, rrg_addr, route_addr, pin_map_addr)
    top_module(arch_addr, rrg_addr, pin_map_addr)
else:
    log("bad arguments\n")

# profiler.disable()
# profiler.print_stats(sort='tottime')