import xml.etree.ElementTree as ET
import sys
import os
import cProfile
import datetime
import re
import subprocess
import matplotlib.pyplot as plt
from fpga_data import *

arch = ''
rrg = ''
route = ''
timing = ''

rr_CHAN_node_list = {}
rr_node_list = {}
rr_edge_list = []
arch_segments_dict = {}
rrg_segments_dict = {}
routes = []
used_grids = set()
unique_OPINs = set()
unique_IPINs = set()
wires_tap_position = {} # a dict: (length, #block_away) --> number_of_wires
l0_map = {}
node_to_slack = {} # node_id --> (slack, path#)
net_info = {} # net id --> (pins_xy_set, slack, detour, traveled, inside, longest_path, longest_used_path, longest_sneak_path)

debug = 1
us = 0

get_chan_type = {
    ('CHANX', 'INC_DIR'): 'CHANE',
    ('CHANX', 'DEC_DIR'): 'CHANW',
    ('CHANY', 'INC_DIR'): 'CHANN',
    ('CHANY', 'DEC_DIR'): 'CHANS',
}
def log(text):
    """write to stderr"""
    if not debug:
        return
    sys.stderr.write(text)
# def log

def out(text, filename):
    """write to specific file"""
    with open(filename, "w") as f:
        f.write(text)
# def out

# def out(text):
#     """write to stdout"""
#     sys.stdout.write(text)
#     sys.stdout.flush()
# # def out

   
# calculate the total available wires and available wires of each type based on the size of the device from architecture file and rr graph file
def get_total_available_wirelength(rrg_root):
    log('calculate the total available wires and available wires of each type based on the size of the device from architecture file and rr graph file\n')
    total_available_x_wire_length = 0
    total_available_y_wire_length = 0
    total_available_Ln_x_wire = []
    total_available_Ln_y_wire = []
    #total_available_Ln_wire = []

    #arch_segments = get_segment_list(architecture)
    chan_x_width_list, chan_y_width_list = get_chanwidth_list(rrg_root)

    for channel in chan_x_width_list:
        total_available_x_wire_length = total_available_x_wire_length + int(channel['info']) * len(chan_y_width_list)
    
    for channel in chan_y_width_list:
        total_available_y_wire_length = total_available_y_wire_length + int(channel['info']) * len(chan_x_width_list)
    
    for (segment_name, segment) in arch_segments_dict.items():
        new_segment = {'length': segment['length'], 'freq': segment['freq']}
        new_Ln_length = float(new_segment['freq']) * total_available_x_wire_length
        total_available_Ln_x_wire.append({'length': new_segment['length'], 'total_length': new_Ln_length})

    for (segment_name, segment) in arch_segments_dict.items():
        new_segment = {'length': segment['length'], 'freq': segment['freq']}
        new_Ln_length = float(new_segment['freq']) * total_available_y_wire_length
        total_available_Ln_y_wire.append({'length': new_segment['length'], 'total_length': new_Ln_length})
    

    print('total available wires based on the size of the device and channels width:', str(total_available_x_wire_length + total_available_y_wire_length))

    print('\ttotal available wires in X channels:\t'+ str(total_available_x_wire_length))
    for length in total_available_Ln_x_wire:
        print('\t\ttotal available L' + str(int(length['length'])-us) + ' wires in X channels: '+ str(length['total_length']))

    print('\ttotal available wires in Y channels:\t'+ str(total_available_y_wire_length))
    for length in total_available_Ln_y_wire:
        print('\t\ttotal available L' + str(int(length['length'])-us) + ' wires in Y channels: '+ str(length['total_length']))

    print('\n')
    return total_available_x_wire_length + total_available_y_wire_length
# def get_total_available_wirelength


# calculate the total available wires based on the rr graph nodes and their LOGICAL length from architecture file and rr graph file
def get_total_available_wirelength_2():
    log('calculate the total available wires based on the rr graph nodes and their LOGICAL length using architecture file and rr graph file\n')
    total_available_x_wire_length = 0
    total_available_y_wire_length = 0
    total_available_Ln_x_wire = {}
    total_available_Ln_y_wire = {}

    #CHAN_nodes = rr_CHAN_node_list
    for (id, segment) in rrg_segments_dict.items():
        total_available_Ln_x_wire[int(segment['length'])] = 0
        total_available_Ln_y_wire[int(segment['length'])] = 0
    log(f"init total_available_Ln_x_wire = {total_available_Ln_x_wire}\n")
    log(f"init total_available_Ln_y_wire = {total_available_Ln_y_wire}\n")

    for node in rr_CHAN_node_list.values():
        if node['type'] == 'CHANX':
            if node['segment_id'] in rrg_segments_dict:
                segment = rrg_segments_dict[node['segment_id']]
                total_available_x_wire_length = total_available_x_wire_length + int(segment['length'])
                total_available_Ln_x_wire[int(segment['length'])] += int(segment['length'])
        elif node['type'] == 'CHANY':
            if node['segment_id'] in rrg_segments_dict:
                segment = rrg_segments_dict[node['segment_id']]
                total_available_y_wire_length = total_available_y_wire_length + int(segment['length'])
                total_available_Ln_y_wire[int(segment['length'])] += int(segment['length'])
    
    log(f"final total_available_Ln_x_wire = {total_available_Ln_x_wire}\n")
    log(f"final total_available_Ln_y_wire = {total_available_Ln_y_wire}\n")

    print('total available wires based on rr_graph nodes and their LOGICAL length:', str(total_available_x_wire_length + total_available_y_wire_length))

    print('\ttotal available LOGICAL wires in X channels:\t'+ str(total_available_x_wire_length))
    for length, value in total_available_Ln_x_wire.items():
        print('\t\ttotal available LOGICAL L' + str(int(length)-us) + ' wires in X channels: '+ str(value))

    print('\ttotal available LOGICAL wires in Y channels:\t'+ str(total_available_y_wire_length))
    for length, value in total_available_Ln_y_wire.items():
        print('\t\ttotal available LOGICAL L' + str(int(length)-us) + ' wires in Y channels: '+ str(value))

    print('\n')
    return total_available_x_wire_length + total_available_y_wire_length
# def get_total_available_wirelength_2


# calculate the total available wires based on the rr graph nodes and their PHYSICAL length from architecture file and rr graph file
def get_total_available_wirelength_3():
    log('calculate the total available wires based on the rr graph nodes and their PHYSICAL length, from architecture file and rr graph file\n')
    total_available_x_wire_length = 0
    total_available_y_wire_length = 0
    total_available_Ln_x_wire = {}
    total_available_Ln_y_wire = {}

    for (id, segment) in rrg_segments_dict.items():
        total_available_Ln_x_wire[int(segment['length'])] = 0
        total_available_Ln_y_wire[int(segment['length'])] = 0
    log(f"init total_available_Ln_x_wire = {total_available_Ln_x_wire}\n")
    log(f"init total_available_Ln_y_wire = {total_available_Ln_y_wire}\n")

    for node in rr_CHAN_node_list.values():
        if node['type'] == 'CHANX':
            if node['segment_id'] in rrg_segments_dict:
                segment = rrg_segments_dict[node['segment_id']]
                total_available_x_wire_length = total_available_x_wire_length + node['ph_length']
                total_available_Ln_x_wire[int(segment['length'])] += node['ph_length']
        elif node['type'] == 'CHANY':
            if node['segment_id'] in rrg_segments_dict:
                segment = rrg_segments_dict[node['segment_id']]
                total_available_y_wire_length = total_available_y_wire_length + + node['ph_length']
                total_available_Ln_y_wire[int(segment['length'])] += node['ph_length']
    
    log(f"final total_available_Ln_x_wire = {total_available_Ln_x_wire}\n")
    log(f"final total_available_Ln_y_wire = {total_available_Ln_y_wire}\n")



    print('total available wires based on rr_graph nodes and their PHYSICAL length:', str(total_available_x_wire_length + total_available_y_wire_length))

    print('\ttotal available PHYSICAL wires in X channels:\t'+ str(total_available_x_wire_length))
    for length, value in total_available_Ln_x_wire.items():
        print('\t\ttotal available PHYSICAL L' + str(int(length)-us) + ' wires in X channels: '+ str(value))

    print('\ttotal available PHYSICAL wires in Y channels:\t'+ str(total_available_y_wire_length))
    for length, value in total_available_Ln_y_wire.items():
        print('\t\ttotal available PHYSICAL L' + str(int(length)-us) + ' wires in Y channels: '+ str(value))

    print('\n')
    
    return total_available_x_wire_length + total_available_y_wire_length
# def get_total_available_wirelength_3


# calculating the physical length of each node (CHANX/Y) that is outside bounding-box
def calculate_length_outbbox(start, end, bbox, CHAN_type):
    log('calculating the physical length of each node (CHANX/Y) that is outside bounding-box\n')
    
    length_out_bbox = 0
    bbox_low = bbox_high = 0
    node_low = node_high = 0

    node_low = min(start, end)
    node_high = max(start, end)

    if CHAN_type == 'CHANX':
        bbox_low = bbox[0]
        bbox_high = bbox[2]

    elif CHAN_type == 'CHANY':
        bbox_low = bbox[1]
        bbox_high = bbox[3]
    
    if node_high < bbox_low or node_low > bbox_high: # the node is completely outside the bbox
        length_out_bbox = node_high - node_low + 1
        
    else: # part of the node might be outside
        num1 = bbox_low - node_low # part of the node lower than bbox_low
        num2 = node_high - bbox_high # part of the node higher than bbox_high

        if num1 > 0: # if there is any --> add to length
            length_out_bbox += num1
        if num2 > 0: # if there is any --> add to length
            length_out_bbox += num2
    
    return length_out_bbox
#calculate_length_outbbox


def get_driving_mux_coordinate(node):
    node_type2dir = {
    ('CHANX', 'INC_DIR'): 'E',
    ('CHANX', 'DEC_DIR'): 'W',
    ('CHANY', 'INC_DIR'): 'N',
    ('CHANY', 'DEC_DIR'): 'S',
    }
    direction = node_type2dir[node['type'],node['dir']]
    
    # determingin the X, Y coordinates and mux position other than the default position ('B')
    x = node['x_start']
    y = node['y_start']
    
    if direction == 'E':
        x = max(x - 1, us)
    elif direction == 'N':
        y = max(y - 1, us)
    
    return x, y
# get_driving_mux_coordinate

def getxy(txt):
    """pull x,y out of (-,x,y) or (x,y)"""
    (*l, x, y) = re.sub(r'[\s,()]+', r' ', txt).strip().split() 
    assert not l or len(l) == 1 and l[0] == '0'
    return (int(x), int(y))


def build_l0_map():
    # STEP 1: do this over all rr_edges in routing graph
    # look through L0s: build maps

    incheck = defaultdict(set)
    for edge in rr_edge_list:
        src_nid = int(edge['source_id'])
        dst_nid = int(edge['sink_id'])
        dest_node = rr_node_list[dst_nid]
        if dest_node['type'].startswith('CHAN') and 'segment_id' in dest_node and rrg_segments_dict[dest_node['segment_id']]['name'] == "L0":
            incheck[dst_nid].add(src_nid)
            l0_map[dst_nid] = src_nid
        # if
    # for edge
    
    # check that L0s don't have any in-convergence (i.e. they are wires)
    bad = 0
    for src_nids in incheck.values():
        if len(src_nids) > 1: bad += 1
    if bad:
        log(f" - ERROR: {bad:,d} L0s have >1 input\n")
# def build_l0_map()

# process each net to add all its nodes to an array of dictionary and return them
def process_net(net_and_nodes):
    log('process each net to add all its nodes to an array of dictionary and return them\n')

    net_bbox = [0, 0, 0, 0] # x_low, y_low, x_high, y_high
    half_perimeter = 0
    
    net_index = net_and_nodes[0].split(' ')[1] # get the index of net and remove it from the array (to only process nodes)
    del net_and_nodes[0]

    nodes = []
    i = 0
    bbox_initilized = 0
    for node in net_and_nodes:
        
        node_replica = 1 # which replica of this node is visited
        length_out_bbox = 0
        

        # replace all \t and '  ' with ' '
        node = node.replace('  ', ' ')
        node = node.replace('\t', ' ')
        node = node.replace('  ', ' ')
        node = node.replace('\n', ' ')
        
        # store the net index of each node
        node_net_index = net_index

        # node id, type, and the first coordinate is common for all node types
        node_id = node.split(' ')[1]
        node_type = node.split(' ')[2]
        node_start = node.split(' ')[3]
        
        # for the rest of the information there are different cases
        node_end = node_start # default location for end is the same as start location
        node_ptc = ''
        node_pin_num = ''
        node_switch = ''
        node_net_pin_index = ''
        driving_mux_x = -1
        driving_mux_y = -1
        
        
        if node.split(' ').count('to') > 0: # if node has two coordinates (having keyword 'to') this is how we find end, ptc, and switch number
            node_end =  node.split(' ')[5] # default is INC_DIR, but we need to check if it is DEC_DIR
            if node_type in ('CHANX', 'CHANY'):
                if rr_CHAN_node_list[node_id]['dir'] == 'DEC_DIR': # swap the start and end
                    node_end, node_start = node_start, node_end
            node_ptc = node.split(' ')[7]
            node_switch = node.split(' ')[9]
       
        elif node.split(' ').count('OPIN') > 0 or node.split(' ').count('IPIN') > 0:  # if node is OPIN or IPIN this is how we find ptc, pin number and switch number
            node_ptc = node.split(' ')[5]
            node_pin_num = node.split(' ')[6]
            node_switch = node.split(' ')[8]
        
        else: # otherwise, this is how we find the ptc and switch number
            node_ptc = node.split(' ')[5]
            node_switch = node.split(' ')[7]
        
        if node.split(' ').count('Net_pin_index:') > 0: # in the case of having SINK, we also have Net_pin_index
            node_net_pin_index = node.split('Net_pin_index:')[1]

        for n in nodes: # increase the value of replica everytime visiting the same node
            if node_id == n['id']:
                node_replica += 1
        
        # calculate bounding-box:
        if node_type == 'OPIN' and bbox_initilized == 0: # initialize the bounding-box with source coordinate (Note: start_loc = end_loc for SOURCE)
            log(f'initilizing bounding-box for Net {node_net_index}\n')
            bbox_initilized = 1
            (net_bbox[0], net_bbox[1]) = getxy(node_start) # x_low = source_x_start, y_low = source_y_start
            (net_bbox[2], net_bbox[3]) = getxy(node_end)   # x_high = source_x_end, y_high = source_y_end
            log(f'bbox initialized with x_low:{net_bbox[0]}, y_low:{net_bbox[1]}, x_high:{net_bbox[2]}, y_high:{net_bbox[3]}\n')

        elif node_type in ('OPIN', 'IPIN'): # every time a PIN is visited, update the bbox if neccessary (Note: start_loc = end_loc for IPIN/OPIN)
            log(f'calculating bounding-box for Net {node_net_index}\n')
            (xmin, ymin) = getxy(node_start) # x_low, y_low
            (xmax, ymax) = getxy(node_end)   # x_high, y_high
            net_bbox[0] = min(net_bbox[0], xmin)
            net_bbox[2] = max(net_bbox[2], xmax)
            net_bbox[1] = min(net_bbox[1], ymin)
            net_bbox[3] = max(net_bbox[3], ymax)
            log(f'bbox updated with x_low:{net_bbox[0]}, y_low:{net_bbox[1]}, x_high:{net_bbox[2]}, y_high:{net_bbox[3]}\n')

        # update used grids for entire design
        if node_type in ('OPIN', 'IPIN'):
            used_grids.add(node_start)
        
        # count the number of unique OPINs
        if node_type == 'OPIN':
            unique_OPINs.add(node_id)

        # count the number of unique IPINs
        if node_type == 'IPIN':
            unique_IPINs.add(node_id)
        
        # get driving mux coordinate
        elif node_type in ('CHANX', 'CHANY'):
            driving_mux_x, driving_mux_y = get_driving_mux_coordinate(rr_CHAN_node_list[node_id])
        
        new_node = {'net_index': node_net_index, # creating a dictionary for this node
                    'id': node_id,
                    'type': node_type,
                    'start_loc': node_start,
                    'end_loc': node_end,
                    'ptc': node_ptc,
                    'pin_number' : node_pin_num,
                    'switch#': node_switch,
                    'net_pin_index': node_net_pin_index,
                    'replica': node_replica,
                    'length_out_bbox': length_out_bbox,
                    'driving_mux_x': driving_mux_x,
                    'driving_mux_y': driving_mux_y}
        log(f"new node visited with id: {new_node['id']}\t, type: {new_node['type']}\t, start_loc: {new_node['start_loc']}\t, end_loc: {new_node['end_loc']}\t, ptc: {new_node['ptc']}\t, driving_mux_x: {new_node['driving_mux_x']}\t, driving_mux_y: {new_node['driving_mux_y']}\t, pin_number: {new_node['pin_number']}\t, switch#: {new_node['switch#']}\t, net_index: {new_node['net_pin_index']}\t, replica: {new_node['replica']}\n")
        nodes.append(new_node) # add this node to the list of nodes of this net
        i += 1
    log(f" - Visited {i} Node of Net-{net_index}\n")

    try:
        print_node_net_index = f'{node_net_index}'
    except UnboundLocalError:
        print_node_net_index = "UnboundLocalError"
    log(f'bounding-box for Net-{print_node_net_index} is x_low: {net_bbox[0]}, y_low: {net_bbox[1]}, x_high: {net_bbox[2]}, y_high: {net_bbox[3]}\n')

    half_perimeter = (net_bbox[2] - net_bbox[0] + 1) + (net_bbox[3] - net_bbox[1] + 1)
    log(f'half perimeter for Net-{print_node_net_index} = {half_perimeter}\n')

    log(f'Net-{print_node_net_index}: assigning bounding-box to each node and calculating the portion of node outside the bbox of\n')
    for node in nodes:
        node['bbox'] = net_bbox

        # calculate the portion of the Node (CHAN) that is outside the bounding box (not related to used portion)
        (x_start, y_start) = getxy(node['start_loc'])
        (x_end,   y_end  ) = getxy(node['end_loc'])

        if node['type'] == 'CHANX':
            # if (y_start < net_bbox[1] and y_end < net_bbox[1]) or (y_start > net_bbox[3] and y_end > net_bbox[3]): # CHANX node is bellow or above the bbox
            #     node['length_out_bbox'] = abs(x_end - x_start) + 1
            # else:
            node['length_out_bbox'] = calculate_length_outbbox(x_start, x_end, net_bbox,'CHANX')

        elif node['type'] == 'CHANY':
            # if (x_start < net_bbox[0] and x_end < net_bbox[0]) or (x_start > net_bbox[2] and x_end > net_bbox[2]): # the CHANY node is left or righ side of the bbox
            #     node['length_out_bbox'] = abs(y_end - y_start) + 1
            # else:
            node['length_out_bbox'] = calculate_length_outbbox(y_start, y_end, net_bbox,'CHANY')            
            
    return nodes
# def process_net


# traverse all nets in a .route file and return an array of nets with all their nodes
def route_traverse(route_file):
    log('traverse all nets in a .route file and return an array of nets with all their nodes\n')
    route_content = route_file.readlines()
    i = 0
    Net_list = []
    while i < len(route_content): # read every line of the .route file
        if 'Net' in route_content[i] and 'global' not in route_content[i]: # if there is a new Net
            net_and_nodes = [route_content[i]] # keep the Net info and nodes info of this Net
            net_line_idx = i # keep the index of Net in the list
            j = net_line_idx + 1 # skip the this line that contains 'Net'
            while j < len(route_content) and not route_content[j].strip(): # skip the lines that has nothing
                j += 1
            if j >= len(route_content): # check if reaches the end of file
                break
            while ('Node' in route_content[j]): 
                net_and_nodes.append(route_content[j]) # add a new node to the list of nodes of this net
                j += 1
                if j >= len(route_content):
                    break
            i = j
            while i < len(route_content) and not 'Net' in route_content[i]: #  go to next Net line
                i += 1
            processed_net = process_net(net_and_nodes)
            Net_list.append(processed_net) # process the nodes of this net and return the info of the route
        else:
            i = i + 1
            continue
    return Net_list
# def route_traverse


# calculate the LOGICAL wire length of a net and return it
def get_net_logical_used_wire_length(net, total_wire_length_list):
    log('calculate the LOGICAL wire length of a net and return it\n')

    CHAN_id_list = []
    CHAN_length_list = []
    
    for node in net: # for each node in the net: if it is not visited and it is CHANX/Y --> look for its id in rr graph to determine the segment id and its length 
        if node['id'] not in CHAN_id_list and 'CHAN' in node['type']:
            CHAN_id_list.append(node['id'])
            if rr_CHAN_node_list.get(node['id'], False):
                if rr_CHAN_node_list[node['id']]['segment_id'] in rrg_segments_dict:
                    segment = rrg_segments_dict[rr_CHAN_node_list[node['id']]['segment_id']]
                    for element in total_wire_length_list:
                        if element['id'] == segment['id']:
                            element['total_wire_length'] = element['total_wire_length'] + int(segment['length'])
                    CHAN_length_list.append(segment['length'])
    
    net_total_wire_length = 0
    for length in CHAN_length_list: # add all lengths together
        net_total_wire_length = net_total_wire_length + int(length)
    
    log(f'Net total logical wire length:\t {net_total_wire_length}\n')
    return net_total_wire_length, total_wire_length_list
# def get_net_logical_used_wire_length


# calculate the total LOGICAL used wire length for a desing 
def get_total_logical_used_wire_length():
    log('calculate the total LOGICAL used wire length for a desing\n')

    total_wire_length_list = []
    total_wire_length = 0

    for (id, segment) in rrg_segments_dict.items(): # create a list of wire length with different L
        total_wire_length_list.append({'id': id, 'length': segment['length'], 'total_wire_length': 0})

    #total_wire_length = 0
    for net in routes: # calculate the total wire length for each net
        net_total_wire_length, total_wire_length_list = get_net_logical_used_wire_length(net, total_wire_length_list)
        total_wire_length = total_wire_length + net_total_wire_length
    #close_route_file(route)

    
    #print ('total length of all wires with different L used in the design is:\t', total_wire_length_list)

    return total_wire_length, total_wire_length_list
# get_total_logical_used_wire_length


def src_is_dst(src_nid, dst_nid):
    seen = set()
    while src_nid in l0_map:
        src_nid = l0_map[src_nid]
        if src_nid in seen:
            log(f" - ERROR: L0 loop on node {src_nid}\n")
            i += 1
            break
        seen.add(src_nid)

    seen = set()
    while dst_nid in l0_map:
        dst_nid = l0_map[dst_nid]
        if dst_nid in seen:
            log(f" - ERROR: L0 loop on node {dst_nid}\n")
            i += 1
            break
        seen.add(dst_nid)
    if src_nid == dst_nid:
        return True
    else:
        return False 


# calculate the PHYISICAL used wire length of a net and return it
def get_net_physical_wire_length(net, list1, list2, list3, list4):
    log('calculate the PHYISICAL wire length of a net and how much of that is used and return them\n')
    #rrg_CHAN_node_list = rr_CHAN_node_list # list of rr graph nodes that are only CHANX or CHANY

    global net_info

    xy_set = set() # set of pins' (x,y) coordinates as well as 'OPIN'/'IPIN' (useful for FLUTE program)

    CHAN_id_list = []
    CHAN_physical_length_list = []
    CHAN_physical_length_outbb_list = []
    CHAN_physical_used_length_list = []
    CHAN_physical_used_length_outbb_list = []
    
    OPIN_sneak_total_length = 0
    IPIN_sneak_total_length = 0
    DIRECT_sneak_total_length = 0

    path = 0 # length of each node in the path between OPIN <--> IPIN
    path_dict = {} # length of path from begining until this node (inclusive) in the path between OPIN <--> IPIN
    longest_path = 0 # longest length of the path between OPIN <--> IPIN in a net
    used_path = 0 # used length of each node in the path between OPIN <--> IPIN
    used_path_dict = {} # used length of path from begining until this node (inclusive) in the path between OPIN <--> IPIN
    longest_used_path = 0 # used longest length of the path between OPIN <--> IPIN in a net
    sneak_path = 0 # length of the sneak in the path between OPIN <--> IPIN
    longest_sneak_path = 0 # length of the longest path + sneak length between OPIN <--> IPIN

    total_traveled = 0
    total_detoure = 0
    total_inside = 0

    i = 0
    traveled = {}
    detoured = {}
    log('before while loop\n')
    while i < (len(net) - 1):
        log(f'i = {i}\n')
        # STEP 2: do this over each edge as seen in .route
        # reminder not to do edges into repeated nodes
        src_node = net[i]
        dst_node = net[i+1]

        src_nid = int(src_node['id'])
        dst_nid = int(dst_node['id'])

        log(f'before accessing l0_map, src_node = {rr_node_list[src_nid]}\n')
        log(f'before accessing l0_map, dst_node = {rr_node_list[dst_nid]}\n')

        smap = False
        seen = set()

        while src_nid in l0_map:
            src_nid = l0_map[src_nid]
            smap = True
            if src_nid in seen:
                log(f" - ERROR: L0 loop on node {src_nid}\n")
                i += 1
                break
            seen.add(src_nid)
        
        seen = set()
        while dst_nid in l0_map:
            dst_nid = l0_map[dst_nid]
            if dst_nid in seen:
                log(f" - ERROR: L0 loop on node {dst_nid}\n")
                i += 1
                break
            seen.add(dst_nid)
        
        log(f'after accessing l0_map, src_node = {rr_node_list[src_nid]}\n')
        log(f'after accessing l0_map, dst_node = {rr_node_list[dst_nid]}\n')

        if src_nid == dst_nid:
            i += 1
            log(f'src_nid == dst_nid\n')
            continue

        (st, dt) = (rr_node_list[src_nid]['type'], rr_node_list[dst_nid]['type'])
        if 'CHAN' in st:
            st = f"CHAN{rr_node_list[src_nid]['direction']}"
            log(f'st = {st}\n')
        if 'CHAN' in dt:
            dt = f"CHAN{rr_node_list[dst_nid]['direction']}"
            log(f'dt = {dt}\n')

        (sxl, sxh, syl, syh) = (rr_node_list[src_nid]['x_low'], rr_node_list[src_nid]['x_high'], rr_node_list[src_nid]['y_low'], rr_node_list[src_nid]['y_high'])
        (dxl, dxh, dyl, dyh) = (rr_node_list[dst_nid]['x_low'], rr_node_list[dst_nid]['x_high'], rr_node_list[dst_nid]['y_low'], rr_node_list[dst_nid]['y_high'])
        
        xmin = ymin = 1



        # compute intersection point containing downstream mux
        # match (st, dt):
        # OPIN --> IPIN
        if (st, dt) == ('OPIN', 'IPIN'):
            if smap:    (mx, my) = (sxl, syl)
        # OPIN --> CHAN
        if (st, dt) == ('OPIN', 'CHANE'):     (mx, my) = (sxl, syl)
        elif (st, dt) == ('OPIN', 'CHANN'):     (mx, my) = (sxl, syl)
        elif (st, dt) == ('OPIN', 'CHANW'):     (mx, my) = (sxl, syl)
        elif (st, dt) == ('OPIN', 'CHANS'):     (mx, my) = (sxl, syl)
        # CHANX --> CHANY
        elif (st, dt) == ('CHANE', 'CHANN'):    (mx, my) = (dxl, syl)
        elif (st, dt) == ('CHANE', 'CHANS'):    (mx, my) = (dxl, syl)
        elif (st, dt) == ('CHANW', 'CHANN'):    (mx, my) = (dxl, syl)
        elif (st, dt) == ('CHANW', 'CHANS'):    (mx, my) = (dxl, syl)
        # CHANY --> CHANX
        elif (st, dt) == ('CHANN', 'CHANE'):    (mx, my) = (sxl, dyl)
        elif (st, dt) == ('CHANN', 'CHANW'):    (mx, my) = (sxl, dyl)
        elif (st, dt) == ('CHANS', 'CHANE'):    (mx, my) = (sxl, dyl)
        elif (st, dt) == ('CHANS', 'CHANW'):    (mx, my) = (sxl, dyl)
        # stitch (max almost certainly not needed)
        elif (st, dt) == ('CHANE', 'CHANE'):    (mx, my) = (max(dxl-1,us), dyl)
        elif (st, dt) == ('CHANN', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,us))
        elif (st, dt) == ('CHANW', 'CHANW'):    (mx, my) = (dxh, dyh)
        elif (st, dt) == ('CHANS', 'CHANS'):    (mx, my) = (dxh, dyh)
        # reversal (max almost certainly not needed)
        elif (st, dt) == ('CHANE', 'CHANW'):    (mx, my) = (dxh, dyh)
        elif (st, dt) == ('CHANN', 'CHANS'):    (mx, my) = (dxh, dyh)
        elif (st, dt) == ('CHANW', 'CHANE'):    (mx, my) = (max(dxl-1,us), dyl)
        elif (st, dt) == ('CHANS', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,us))
        # CHAN --> IPIN
        elif (st, dt) == ('CHANE', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (st, dt) == ('CHANN', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (st, dt) == ('CHANW', 'IPIN'):     (mx, my) = (dxl, dyl)
        elif (st, dt) == ('CHANS', 'IPIN'):     (mx, my) = (dxl, dyl)
        # other edges we ignore
        else:          
            i += 1
            log(f'other edges we ignore\n')        
            continue
        # match

        log(f'(mx, my) = ({(mx, my)})\n')
        # compute upstream mux location
        (ux, uy) = (mx, my)
        # match st:
        if st == 'CHANE':
            ux = max(sxl - 1, us)
        elif st == 'CHANN':
            uy = max(syl - 1, us)
        elif st == 'CHANW':
            ux = sxh
        elif st == 'CHANS':
            uy = syh
        # match
        log(f'(ux, uy) = ({(ux, uy)})\n')

        # Connection between: (ux, uy) ==> (mx, my)
        # save this by source node
        # overwrite by values with greater travel
        # this is the length traveled
        travel = abs(ux - mx) + abs(uy - my)
        if src_nid not in traveled or traveled[src_nid][0] < travel:
            traveled[src_nid] = (travel, ux, uy, mx, my)
        # info accumulates in traveled[]
        log(f'travel = {travel}\n')

        i += 1
    log('after while loop\n')

    # STEP 3: do after each net
    for src_nid in traveled:
        log(f'calculating inside and detour for node_id = {src_nid}\n')
        (travel, ux, uy, mx, my) = traveled[src_nid]
        st = rr_node_list[src_nid]['type']
        if 'CHAN' in st:
            st = f"CHAN{rr_node_list[src_nid]['direction']}"

        # make mx,my min coordinate, ux,uy max coordinate
        (mx, ux) = (min(mx, ux), max(mx, ux))
        (my, uy) = (min(my, uy), max(my, uy))
        bbxmin = src_node['bbox'][0]
        bbxmax = src_node['bbox'][2]
        bbymin = src_node['bbox'][1]
        bbymax = src_node['bbox'][3]
        
        # compute travel inside and outside net bbox
        if   st in ('CHANE', 'CHANW'):
            (mx, ux) = (max(bbxmin, mx), min(bbxmax, ux))
            inside = max(0, ux - mx)
        elif st in ('CHANN', 'CHANS'):
            (my, uy) = (max(bbymin, my), min(bbymax, uy))
            inside = max(0, uy - my)
        else:
            inside = travel # should be 0
        detour = travel - inside
        detoured[src_nid] = (detour, ux, uy, mx, my)
        log(f'\tinside = {inside}\n')
        log(f'\tdetour = {detour}\n')
        log(f'\ttravel = {travel}\n')
        
        
        # accumulate travel, inside, detour
        total_traveled += travel
        total_detoure += detour
        total_inside += inside
        


    for node in net: # for each node in the net --> store its min and max coordinates. visit the following node to find the used length
        
        length = 0          # traveled length
        length_outbb = 0    # detour
        max_length = 0      # phyical length
        sneak_length = 0    # sneak connection

        if node['type'] == 'OPIN':

            log(f'finding the length of OPIN sneak connection for node: {node}\n')
            log(f'following node is: {net[(net.index(node)+1)]}\n')
            (x_o, y_o) = getxy(node['start_loc'])
            log(f'adding the OPIN coordinates to xy_set for node: {node}\n')
            xy_set.add((int(x_o), int(y_o), 'OPIN'))

            if net[(net.index(node)+1)]['type'] in ('CHANX', 'CHANY'):
                dest_x_start = int(net[(net.index(node)+1)]['driving_mux_x'])
                dest_y_start = int(net[(net.index(node)+1)]['driving_mux_y'])
                log(f'OPIN - X: {x_o}\n')
                log(f'OPIN - Y: {y_o}\n')
                log(f'CHAN driving mux - X: {dest_x_start}\n')
                log(f'CHAN driving mux - Y: {dest_y_start}\n')
                sneak_length = abs(dest_x_start - x_o) + abs(dest_y_start - y_o)
                if src_is_dst(int(node['id']), int(net[(net.index(node)+1)]['id'])): # ignore connections in L0_map that have same src/dst
                    sneak_length = 0
                log(f'OPIN - CHAN sneak length: {sneak_length}\n')
                OPIN_sneak_total_length += sneak_length
                sneak_path += sneak_length

            elif net[(net.index(node)+1)]['type'] == 'IPIN':
                (dest_x_start, dest_y_start) = getxy(net[(net.index(node)+1)]['start_loc'])
                log(f'OPIN - X: {x_o}\n')
                log(f'OPIN - Y: {y_o}\n')
                log(f'IPIN - X: {dest_x_start}\n')
                log(f'IPIN - Y: {dest_y_start}\n')
                sneak_length = abs(dest_x_start - x_o) + abs(dest_y_start - y_o)
                log(f'OPIN - IPIN sneak length: {sneak_length}\n')
                DIRECT_sneak_total_length += sneak_length

        elif node['type'] == 'IPIN':


            log(f'finding the length of IPIN sneak connection for node: {node}\n')
            log(f'previous node is: {net[(net.index(node)-1)]}\n')
            (x_o, y_o) = getxy(node['start_loc'])

            log(f'adding the IPIN coordinates to xy_set for node: {node}\n')
            xy_set.add((int(x_o), int(y_o), 'IPIN'))

            if net[(net.index(node)-1)]['type'] in ('CHANX', 'CHANY'):
                (prev_x_start, prev_y_start) = getxy(net[(net.index(node)-1)]['start_loc'])

                log(f'IPIN - X: {x_o}\n')
                log(f'IPIN - Y: {y_o}\n')
                log(f'Previous CHAN - X_start: {prev_x_start}\n')
                log(f'Previous CHAN - Y_start: {prev_y_start}\n')
            
                if net[(net.index(node)-1)]['type'] == 'CHANX':
                    sneak_length = abs(prev_y_start - y_o)
                elif net[(net.index(node)-1)]['type'] == 'CHANY':
                    sneak_length = abs(prev_x_start - x_o)
                if src_is_dst(int(node['id']), int(net[(net.index(node)-1)]['id'])): # ignore connections in L0_map that have same src/dst
                    sneak_length = 0
            
            IPIN_sneak_total_length += sneak_length
            log(f'CHAN - IPIN sneak length: {sneak_length}\n')

            sneak_path += sneak_length
            # updating the longest path in a Net
            if path > longest_path:
                log(f'updating longest path from {longest_path} to {path}\n')
                longest_path = path
                longest_sneak_path = longest_path + sneak_path
            path = 0
            sneak_path = 0

            if used_path > longest_used_path:
                log(f'updating longest used path from {longest_used_path} to {used_path}\n')
                longest_used_path = used_path
            used_path = 0

        elif node['type'] in ('CHANX', 'CHANY'):
            log(f"visiting CHANX/Y node {node['id']} in net: {node['net_index']}\n")

            # find x_start from (x1,y1), find y_start from (x1,y1)
            (x_start, y_start) = getxy(node['start_loc'])
            # find x_end from (x2,y2), find y_end from (x2,y2)
            (x_end, y_end) = getxy(node['end_loc'])

            log(f'x_start is: {x_start}\n')
            log(f'x_end is: {x_end}\n')
            log(f'y_start is: {y_start}\n')
            log(f'y_end is: {y_end}\n')
            
                
            node_logical_length = 0
            if rr_CHAN_node_list[node['id']]['segment_id'] in rrg_segments_dict:
                segment = rrg_segments_dict[rr_CHAN_node_list[node['id']]['segment_id']]
                node_logical_length = segment['length']
            
            ###### if it is CHANX
            if node['type'] == 'CHANX':
                log('node is CHANX\n')
                max_length = abs(x_end - x_start) + 1 # maximum length of this CHAN node
            ###### if it is CHANY
            elif node['type'] == 'CHANY':
                log('node is CHANY\n')
                max_length = abs(y_end - y_start) + 1 # maximum length of this CHAN node

            max_length = min(max_length, int(node_logical_length) - us)
            log(f'max length is: {max_length}\n')

            if int(node['id']) in traveled: length = traveled[int(node['id'])][0] 
            else: length = 0 # length = 1
            log(f'travel is: {length}\n')
            
            key = (node_logical_length, length)
            wires_tap_position[key] = wires_tap_position.get(key, 0) + 1
            log(f'tap point key (logical_length, length) = ({node_logical_length}, {length})\n')

            if int(node['id']) in detoured: length_outbb = detoured[int(node['id'])][0] 
            else: length_outbb = 0 # length_outbb = length
            log(f'detour is: {length_outbb}\n')
                

            log(f'max_length is {max_length}\n')
            log(f'used length (travel) is {length}\n')
            if int(node['id']) in traveled:
                log(f"length out of bounding box is {node['length_out_bbox']}\n")
            else:
                log(f"length out of bounding box is 0 (because it is not in traveled dict)\n")
            log(f'used length out of bounding box (detour) is {length_outbb}\n')
            
            if node['id'] not in CHAN_id_list: # if it is not visited
                log('node is visited for the first time\n')
                if rr_CHAN_node_list[node['id']]['segment_id'] in rrg_segments_dict:
                    segment = rrg_segments_dict[rr_CHAN_node_list[node['id']]['segment_id']]
                    if not segment['name'] == 'L0':
                        path += max_length
                        path_dict[node['id']] = path
                        used_path += length
                        used_path_dict[node['id']] = used_path
                        log(f'path is {path}\n')
                        log(f"path_dict[node_id] is {path_dict[node['id']]}\n")
                        log(f'used_path is {used_path}\n')
                        log(f"used_path_dict[node_id] is {used_path_dict[node['id']]}\n")

                CHAN_id_list.append(node['id']) # add node to the list of visited nodes
                CHAN_physical_used_length_list.append(length) # add its length to the list of lengths
                CHAN_physical_used_length_outbb_list.append(length_outbb) # add its used length out of bbox to the list of used lengths out of bbox
                CHAN_physical_length_list.append(max_length)
                if int(node['id']) in traveled:
                    CHAN_physical_length_outbb_list.append(node['length_out_bbox'])
                else:
                    CHAN_physical_length_outbb_list.append(0)

            elif node['id'] in CHAN_id_list: # if it is already visited, only update the length if it is neccessary
                log('node is already visited\n')
                
                if rr_CHAN_node_list[node['id']]['segment_id'] in rrg_segments_dict:
                    segment = rrg_segments_dict[rr_CHAN_node_list[node['id']]['segment_id']]
                    if not segment['name'] == 'L0':
                        path = path_dict[node['id']]
                        used_path = used_path_dict.get(node['id'], 0)
                        log(f'path is {path}\n')
                        log(f'used_path is {used_path}\n')

                if CHAN_physical_used_length_list[CHAN_id_list.index(node['id'])] < length:
                    log('updating node used length\n')
                    used_path = used_path_dict.get(node['id'], 0) - CHAN_physical_used_length_list[CHAN_id_list.index(node['id'])] + length
                    used_path_dict[node['id']] = used_path
                    log(f'used_path is {used_path}\n')
                    CHAN_physical_used_length_list[CHAN_id_list.index(node['id'])] = length
                
                if CHAN_physical_used_length_outbb_list[CHAN_id_list.index(node['id'])] < length_outbb:
                    log('updating node used length out of bbox\n')
                    CHAN_physical_used_length_outbb_list[CHAN_id_list.index(node['id'])] = length_outbb

    log(f'longest path is {longest_path}\n')
    log(f'longest used path is {longest_used_path}\n')
    log(f'longest path including sneak length is {longest_sneak_path}\n')

    log('assigning used length to L categories\n')
    for node_id in CHAN_id_list:
        rrg_node = rr_CHAN_node_list[node_id]
        if rrg_node['segment_id'] in rrg_segments_dict:
            segment = rrg_segments_dict[rrg_node['segment_id']]
            for element in list1:
                if element['id'] == segment['id']: # find the Lx wires
                    element['total_wire_length'] = element['total_wire_length'] + CHAN_physical_length_list[CHAN_id_list.index(node_id)]
            
            for element in list2:
                if element['id'] == segment['id']: # find the Lx wires
                    element['total_wire_length'] = element['total_wire_length'] + CHAN_physical_length_outbb_list[CHAN_id_list.index(node_id)]
            
            for element in list3:
                if element['id'] == segment['id']: # find the Lx wires
                    element['total_wire_length'] = element['total_wire_length'] + CHAN_physical_used_length_list[CHAN_id_list.index(node_id)]
            
            for element in list4:
                if element['id'] == segment['id']: # find the Lx wires
                    element['total_wire_length'] = element['total_wire_length'] + CHAN_physical_used_length_outbb_list[CHAN_id_list.index(node_id)]

        
    net_total_physical_wire_length = 0
    for length in CHAN_physical_length_list: # add all lengths together
        net_total_physical_wire_length = net_total_physical_wire_length + length
    print_net0_net_index = net[0]['net_index'] if net else "no-net-found"
    log(f"Net {print_net0_net_index} has total physical wire length of :\t {net_total_physical_wire_length}\n")

    net_total_physical_wire_length_out_of_bbox = 0
    for length in CHAN_physical_length_outbb_list: # add all lengths together
        net_total_physical_wire_length_out_of_bbox = net_total_physical_wire_length_out_of_bbox + length
    log(f"Net {print_net0_net_index} has total physical wire length out of bounding box of :\t {net_total_physical_wire_length_out_of_bbox}\n")

    net_total_used_physical_wire_length = 0
    for length in CHAN_physical_used_length_list: # add all lengths together
        net_total_used_physical_wire_length = net_total_used_physical_wire_length + length
    #overriding the results of my scripts:
    net_total_used_physical_wire_length = total_traveled
    log(f"Net {print_net0_net_index} has total used physical wire length of :\t {net_total_used_physical_wire_length}\n")

    net_total_used_physical_wire_length_out_of_bbox = 0
    for length in CHAN_physical_used_length_outbb_list: # add all lengths together
        net_total_used_physical_wire_length_out_of_bbox = net_total_used_physical_wire_length_out_of_bbox + length
    # overriding the results of my scripts:
    net_total_used_physical_wire_length_out_of_bbox = total_detoure
    log(f"Net {print_net0_net_index} has total used physical wire length out of bounding box of :\t {net_total_used_physical_wire_length_out_of_bbox}\n")


    # update net_info
    # net[1] is alway OPIN and that is what we need for slack info
    if net[1]['id'] in node_to_slack: # check if the net is even in timing report
        net1_net_index = net[1]['net_index'] if net else "no-net-found"
        slack = node_to_slack[net[1]['id']]['slack']
        path_num = node_to_slack[net[1]['id']]['path_num']
        net_info[net1_net_index] = {'pins_xy_set': xy_set,
                                    'slack': slack,
                                    'path_num': path_num,
                                    'OPIN_ID': net[1]['id'],
                                    'detour': total_detoure, 
                                    'traveled': total_traveled, 
                                    'inside': total_inside, 
                                    'longest_path': longest_path, 
                                    'longest_used_path': longest_used_path, 
                                    'longest_sneak_path': longest_sneak_path}
        log(f"net info for: {net1_net_index}: {net_info[net1_net_index]}\n")
    else:
        net1_net_index = net[1]['net_index'] if net else "no-net-found"
        slack = 'N/A'
        path_num = 'N/A'
        net_info[net1_net_index] = {'pins_xy_set': xy_set,
                                    'slack': slack,
                                    'path_num': path_num,
                                    'OPIN_ID': net[1]['id'],
                                    'detour': total_detoure, 
                                    'traveled': total_traveled, 
                                    'inside': total_inside, 
                                    'longest_path': longest_path, 
                                    'longest_used_path': longest_used_path, 
                                    'longest_sneak_path': longest_sneak_path}
        log(f"net info for: {net1_net_index}: {net_info[net1_net_index]}\n")

    num5 = net_total_physical_wire_length
    num6 = net_total_physical_wire_length_out_of_bbox
    num7 = net_total_used_physical_wire_length
    num8 = net_total_used_physical_wire_length_out_of_bbox
    num9 = OPIN_sneak_total_length
    num10 = IPIN_sneak_total_length
    num11 = DIRECT_sneak_total_length
    num12 = longest_path
    num13 = longest_used_path
    num14 = longest_sneak_path

    return num5, num6, num7, num8, num9, num10, num11, num12, num13, num14, list1, list2, list3, list4
# def get_net_physical_wire_length


# calculate the total PHYSICAL wire length for a desing 
def get_design_physical_wire_length():
    log('calculate the total PHYSICAL wire length for a desing and how much is actually used\n')

    list1 = []  #total_physical_wire_length_list
    list2 = []  #total_physical_wire_length_out_of_bbox_list
    list3 = []  #total_used_physical_wire_length_list
    list4 = []  #total_used_physical_wire_length_out_of_bbox_list
    num1 = 0    # total_physical_wire_length
    num2 = 0    # total_physical_wire_length_out_of_bb
    num3 = 0    # total_used_physical_wire_length
    num4 = 0    # total_used_physical_wire_length_out_of_bb
    num5 = 0    # net_total_physical_wire_length
    num6 = 0    # net_total_physical_wire_length_out_of_bbox
    num7 = 0    # net_total_used_physical_wire_length
    num8 = 0    # net_total_used_physical_wire_length_out_of_bbox
    num9 = 0    # total length of OPIN sneak connections for a Net
    num10 = 0   # total length of IPIN sneak connections for a Net
    num11 = 0   # total length of DIRECT (OPIN --> IPIN) sneak connections for a Net
    num12 = 0   # longest length of the path between OPIN <--> IPIN in a net
    num13 = 0   # used longest length of the path between OPIN <--> IPIN in a net
    num14 = 0   # length of the longest path + sneak length between OPIN <--> IPIN
    OPIN_total_sneak_length = 0 # total length of OPIN sneak connections for a Design
    IPIN_total_sneak_length = 0 # total length of IPIN sneak connections for a Design
    DIRECT_total_sneak_length = 0 # total length of DIRECT (OPIN --> IPIN) sneak connections for a Design
    total_longest = 0
    total_longest_used = 0
    total_longest_sneak = 0

    for (id, segment) in rrg_segments_dict.items(): # create a list of wire length with different L
        list1.append({'id': id, 'length': segment['length'], 'total_wire_length': 0})
        list2.append({'id': id, 'length': segment['length'], 'total_wire_length': 0})
        list3.append({'id': id, 'length': segment['length'], 'total_wire_length': 0})
        list4.append({'id': id, 'length': segment['length'], 'total_wire_length': 0})

    for net in routes: # calculate the total wire length for each net
        num5, num6, num7, num8, num9, num10, num11, num12, num13, num14, list1, list2, list3, list4 = get_net_physical_wire_length(net, list1, list2, list3, list4)

        num1 += num5
        num2 += num6
        num3 += num7
        num4 += num8
        OPIN_total_sneak_length += num9
        IPIN_total_sneak_length += num10
        DIRECT_total_sneak_length += num11
        total_longest += num12
        total_longest_used += num13
        total_longest_sneak += num14

    return num1, num2, num3, num4, OPIN_total_sneak_length, IPIN_total_sneak_length, DIRECT_total_sneak_length, total_longest, total_longest_used, total_longest_sneak, list1, list2, list3, list4
# get_design_physical_wire_length


# print wire length statistics based on nodes in .route file
def print_wire_length_statistics(rrg_root):

    print('################################\tTotal Available Wires\t################################\n')
    get_total_available_wirelength(rrg_root)
    # print('total available wires based on the size of the device is: ', str(total_available_wirelength), '\n')

    
    total_available_logical = get_total_available_wirelength_2()
    # print('total available wires based on rr_graph nodes and their LOGICAL length: ', str(total_available_wirelength), '\n')

    

    total_available_physical = get_total_available_wirelength_3()
    # print('total available wires based on rr_graph nodes and their PHYSICAL length: ', str(total_available_wirelength), '\n')
    print('\n\n\n')


    print('################################\tTotal Wires of the Design\t################################\n')
    print('\t################\tLogical Wire Length\t################\n')
    total_logical_wire_length, total_logical_wire_length_list = get_total_logical_used_wire_length()
    percentage = (total_logical_wire_length / total_available_logical) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal LOGICAL length of all wires in the design:', total_logical_wire_length, f"({percentage}% of total available LOGICAL wires)") 
    for length in total_logical_wire_length_list:
        percentage = (length['total_wire_length'] / total_available_logical) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal LOGICAL length of all L'+ str(int(length['length'])-us), 'wires in the design:', length['total_wire_length'], f"({percentage}% of total available LOGICAL wires)")
        if int(length['length']) == 0:
            print ('\t\tnumber of touched L'+ str(int(length['length'])-us), 'wires in the design:', int(length['total_wire_length']/1))
        else:
            print ('\t\tnumber of touched L'+ str(int(length['length'])-us), 'wires in the design:', int(length['total_wire_length']/int(length['length'])))
    print('\n')
    print('\n\n')
    
    

    print('\t################\tPhyiscal Wire Length\t################\n')
    num1, num2, num3, num4, OPIN_total_sneak_length, IPIN_total_sneak_length, DIRECT_total_sneak_length, total_longest, total_longest_used, total_longest_sneak, list1, list2, list3, list4 = get_design_physical_wire_length()
    percentage = (num1 / total_available_physical) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal PHYSICAL length of all wires in the design (same as VPR report):', num1, f"({percentage}% of total available PHYSICAL wires)") 
    for length in list1:
        percentage = (length['total_wire_length'] / total_available_physical) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires in the design:', length['total_wire_length'], f"({percentage}% of total available PHYSICAL wires)")
    print('\n')

    percentage = (num2 / num1) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal PHYSICAL length of all wires -outside- the bounding box in the design:', num2, f"({percentage}% of all PHYSICAL wires in the design)") 
    for length in list2:
        percentage = (length['total_wire_length']/ num1) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires -outside- the bounding box in the design:', length['total_wire_length'], f"({percentage}% of all PHYSICAL wires in the design)")
    print('\n')

    percentage = (abs(num2 - num1)/ num1) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal PHYSICAL length of all wires -inside- the bounding box in the design:', abs(num2 - num1), f"({percentage}% of all PHYSICAL wires in the design)") 
    for length in list2:
        percentage = (abs(length['total_wire_length'] - list1[list2.index(length)]['total_wire_length']) / num1) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires -inside- the bounding box in the design:', abs(length['total_wire_length'] - list1[list2.index(length)]['total_wire_length']), f"({percentage}% of all PHYSICAL wires in the design)")
    print('\n')
    print('\ttotal length of OPIN sneak connections:', OPIN_total_sneak_length)
    print('\ttotal length of IPIN sneak connections:', IPIN_total_sneak_length)
    print('\ttotal length of all sneak connections:', (OPIN_total_sneak_length + IPIN_total_sneak_length))
    ratio = (OPIN_total_sneak_length + IPIN_total_sneak_length) / num1
    ratio = float("{:.2f}".format(ratio))
    print(f'\tSneak wirelength / PHYSICAL wirelength ratio ({(OPIN_total_sneak_length + IPIN_total_sneak_length)}/{num1}):', ratio)
    print('\n')

    print('\ttotal length of non-routable (OPIN --> IPIN) sneak connections:', DIRECT_total_sneak_length)

    
    
    # percentage = ((OPIN_total_sneak_length + IPIN_total_sneak_length + DIRECT_total_sneak_length + num1) / total_available_physical) * 100
    # percentage = "{:.2f}".format(percentage)
    # print('\ttotal PHYSICAL length of all wires in the design including sneak connections: ', (OPIN_total_sneak_length + IPIN_total_sneak_length + DIRECT_total_sneak_length + num1), f"({percentage}% of total available PHYSICAL wires)")
    
    print('\t\n\n')


    print('\t################\tUsed Phyiscal Wire Length (w.r.t tap points)\t################\n')
    percentage = (num3 / total_available_physical) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal USED PHYSICAL length of all wires in the design:', num3, f"({percentage}% of total available physical wires)") 
    for length in list3:
        percentage = (length['total_wire_length'] / total_available_physical) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal USED PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires in the design:', length['total_wire_length'], f"({percentage}% of total available physical wires)")
    print('\n')

    percentage = (num4 / num3) * 100
    percentage = "{:.2f}".format(percentage)
    print ('\ttotal USED PHYSICAL length of all wires -outside- the bounding box in the design:', num4, f"({percentage}% of all USED PHYSICAL wires in the design)") 
    for length in list4:
        percentage = (length['total_wire_length']/ num3) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal USED PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires -outside- the bounding box in the design:', length['total_wire_length'], f"({percentage}% of all USED PHYSICAL wires in the design)")
    print('\n')

    percentage = (abs(num4 - num3)/ num3) * 100
    percentage = "{:.2f}".format(percentage)    
    print ('\ttotal USED PHYSICAL length of all wires -inside- the bounding box in the design:', abs(num4 - num3), f"({percentage}% of all USED PHYSICAL wires in the design)") 
    for length in list4:
        percentage = (abs(length['total_wire_length'] - list3[list4.index(length)]['total_wire_length']) / num3) * 100
        percentage = "{:.2f}".format(percentage)
        print ('\t\ttotal USED PHYSICAL length of all L'+ str(int(length['length'])-us), 'wires -inside- the bounding box in the design:', abs(length['total_wire_length'] - list3[list4.index(length)]['total_wire_length']), f"({percentage}% of all USED PHYSICAL wires in the design)")
    print('\n\n')

    print("Total length of longest paths in all nets:", total_longest)
    print("Total length of longest paths including sneak connections in all nets:", total_longest_sneak)
    print("Total USED length of longest paths in all nets:", total_longest_used)
    print('\n\n')
# print_wire_length_statistics


def print_info(arch_address, rrg_address, route_address, timing_address):

    global arch
    global rrg
    global route
    global timing
    arch = get_arch_xml_root(arch_address)
    rrg = get_rrg_xml_root(rrg_address)
    route = get_file(route_address)
    timing = get_file(timing_address)
    
    ################ Architecture File ################
    arch_filename = arch_address

    # Get the file name and absolute path
    basename = os.path.basename(arch_filename)
    abspath = os.path.abspath(arch_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(arch_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

    # Print the file name, address, and last modified date
    print("Architecture File name: {}".format(basename))
    print("Architecture File address: {}".format(abspath))
    print("Architecture File Last modified: {}".format(modtime))
    print('\n')
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
    print("RR Graph File name: {}".format(basename))
    print("RR Graph File address: {}".format(abspath))
    print("RR Graph File Last modified: {}".format(modtime))
    # print("RR Graph tool_comment:", tool_comment)
    # print("RR Graph tool_name:", tool_name)
    # print("RR Graph tool_version:", tool_version)
    print('\n')
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
    print("Routing File name: {}".format(basename))
    print("Routing File address: {}".format(abspath))
    print("Routing File Last modified: {}".format(modtime))
    print("Routing File - Placement File:", placement_file)
    print("Routing File - Placement ID:", placement_id)
    print("Routing Array Size:", array_size)
    print('\n\n\n')
    ################ Routing File ################



    ################ Timing File ################
    timing_filename = timing_address

    # Get the file name and absolute path
    basename = os.path.basename(timing_filename)
    abspath = os.path.abspath(timing_filename)

    # Get the last modified date and time
    mtime = os.path.getmtime(timing_filename)
    modtime = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')


    # Print the file name, address, and last modified date
    print("Timing File name: {}".format(basename))
    print("Timing File address: {}".format(abspath))
    print("Timing File Last modified: {}".format(modtime))
    print('\n')
    ################ Timing File ################
# print_info


def get_node_slack():
    global timing
    global node_to_slack

    log('traverse all paths in the timing report file and return map b/w node and slack value\n')
    timing_content = timing.readlines()

    path_num = -1
    i = 0
    keywords = ['OPIN', 'IPIN', 'CHANX', 'CHANY']
    node_to_slack_ = {}
    while i < len(timing_content): # read every line of the timing report file
        
        # #Path
        if '#Path' in timing_content[i]:
            node_to_slack_ = {}
            path_num = timing_content[i].split()[1]
            log(f'#Path {path_num}\n')
            log(f'\tinternal node_to_slack_ is cleared: {node_to_slack_}\n')
        # if
        

        # slack
        elif 'slack' in timing_content[i]:
            slack = timing_content[i].split()[-1]
            log(f'\tslack = {slack}\n')
            for node_id in node_to_slack_:
                if node_id in node_to_slack:
                    log(f'\t\tnode-id: {node_id} is already in node_to_slack dict\n')
                    if float(slack) < float(node_to_slack[node_id]['slack']):
                        log(f"\t\t\tworse slack is visited for node-id: {node_id} --> {slack} < {node_to_slack[node_id]['slack']} --> updating slack\n")
                        node_to_slack_[node_id]['slack'] = slack
                    else:
                        log(f"\t\t\tno need to update slack for node-id: {node_id} --> slack is: {node_to_slack[node_id]['slack']}\n")
                        node_to_slack_[node_id]['slack'] = node_to_slack[node_id]['slack']
                else:
                    log(f'\t\tnode-id: {node_id} is added to node_to_slack dict for the first time\n')
                    node_to_slack_[node_id]['slack'] = slack

            # log(f'\tinternal node_to_slack_ is updated for all nodes: {node_to_slack_}\n')
            node_to_slack.update(node_to_slack_)
            # log(f'\tnode_to_slack get updated: {node_to_slack}\n')
        # elif

        else:
            # Node
            node_id = -1
            for keyword in keywords:
                if keyword in timing_content[i]:
                    pattern = r'\({}:(\d+)\s'.format(keyword)
                    match = re.search(pattern, timing_content[i])
                    if match:
                        node_id = match.group(1)
                        log(f"{keyword} ID: {node_id}\n")
                        if node_id in node_to_slack_:
                            log(f'\tERROR: node_id already added to node_to_slack_\n')
                        node_to_slack_[node_id] = {'slack': 0, 'path_num': path_num}
                    else:
                        print(f"\tERROR: Node ID not found in the {timing_content[i]}")
                # if
            # for
        # else
        i += 1
    # while
# get_node_slack()


def get_FLUTE_info(net_id, xy_set):
    script_directory = os.path.dirname(os.path.abspath(__file__))
    program_directory = f"{script_directory}/flute-3.1"
    executable_path = f"{program_directory}/flute-net"
    coordinates = ""
    all_edges = list()

    for (x,y,p_type) in xy_set:
        coordinates += f"{x} {y} \n"

    w1 = w2 = 0
    if len(xy_set) == 1:
        return w1, w2, all_edges
    try:
        command = [executable_path]
        result = subprocess.run(command, input=coordinates, text=True, cwd=program_directory, capture_output=True, check=True)
        
        # Parse the output to extract wirelength values
        output_lines = result.stdout.splitlines()
        log(f"output_lines = {output_lines}\n")
        w1 = int(output_lines[0].split()[-1])
        w2 = int(output_lines[1].split()[-1])

        i = 0
        for line in output_lines:
            if i == 0 or i == 1:
                i += 1
                continue
            if i < (len(output_lines) - 1) and output_lines[i]:
                (x1, y1, x2, y2) = int(output_lines[i].split()[0]), int(output_lines[i].split()[1]), int(output_lines[i+1].split()[0]), int(output_lines[i+1].split()[1])
                all_edges.append((x1, y1, x2, y2))
                i += 2
            else:
                i += 1
            
        
        log(f"For Net {net_id}:")
        log(f"\tFLUTE wirelength = {w1}\n")
        log(f"\tFLUTE wirelength (without RSMT construction) = {w2}\n")
        log(f"\tAll Edges = {all_edges}\n")
        
    except subprocess.CalledProcessError as e:
        log(f"Error for Net {net_id}: {e}")
        log(f"Command output: {e.output}")
        exit()

    return w1, w2, all_edges
# def get_FLUTE_info(net_id, xy_set)


def plot_tree(net, all_edges, original_points):

    # Initialize min and max values
    min_x = min_y = int(sys.maxsize)
    max_x = max_y = 0

    # Iterate through the list and update min and max values
    for (x1, y1, x2, y2) in all_edges:
        min_x = min(min_x, x1, x2)
        max_x = max(max_x, x1, x2)
        min_y = min(min_y, y1, y2)
        max_y = max(max_y, y1, y2)

    for (x, y, p_type) in original_points:
        min_x = min(min_x, x)
        max_x = max(max_x, x)
        min_y = min(min_y, y)
        max_y = max(max_y, y)

    plt.figure(net)
    plt.xlabel('X-coordinates')
    plt.ylabel('Y-coordinates')
    plt.title('Rectilinear Steiner Minimal Tree (RSMT)')
    plt.xticks(range(int(min_x) - 1, int(max_x) + 2, 1))
    plt.yticks(range(int(min_y) - 1, int(max_y) + 2, 1))
    plt.tight_layout()  # Adjust layout to prevent clipping when saving to PDF
    plt.grid(True)

    # draw original points
    for (x, y, p_type) in original_points:
        if p_type == 'OPIN':
            plt.scatter(x, y, color='green')
        elif p_type == 'IPIN':
            plt.scatter(x, y, color='red')
    
    # draw steiner points and then edges
    for (x1, y1, x2, y2) in all_edges:
        
        # steiner points
        if (x1, y1, 'OPIN') not in original_points and (x1, y1, 'IPIN') not in original_points:
            plt.scatter(x1, y1, color='blue')
        if (x2, y2, 'OPIN') not in original_points and (x2, y2, 'IPIN') not in original_points:
            plt.scatter(x2, y2, color='blue')
        # edges
        x_values = [x1, x2, x2]
        y_values = [y1, y1, y2]
        plt.plot(x_values, y_values, color='black')
    
    
    # Set the x-axis and y-axis ticks with a step of 1
    plt.savefig(f"RSMT_trees/net-{net}.pdf")
# def plot_tree(net_id, all_edges, original_points)


def print_net_info():
    global net_info
    lines = 'NET_ID, FLUTE wirelength, FLUTE wirelength (without RSMT construction), slack, path_num, OPIN_ID, detour, traveled, inside, longest_path, longest_used_path, longest_sneak_path\n'
    if not os.path.exists('RSMT_trees'):
        # If not, create it
        os.makedirs('RSMT_trees')
    for net in net_info:
        FLUTE_W1, FLUTE_W2, ALL_EDGES = get_FLUTE_info(net, net_info[net]['pins_xy_set'])
        lines += f"{net}, {FLUTE_W1}, {FLUTE_W2}, {net_info[net]['slack']}, {net_info[net]['path_num']}, {net_info[net]['OPIN_ID']}, {net_info[net]['detour']}, {net_info[net]['traveled']}, {net_info[net]['inside']}, {net_info[net]['longest_path']}, {net_info[net]['longest_used_path']}, {net_info[net]['longest_sneak_path']}\n"
        plot_tree(net, ALL_EDGES, net_info[net]['pins_xy_set'])
    file_name = 'stamp_net_info.csv' if us else 'vpr_net_info.csv'
    out(lines, file_name)
# print_net_info()



def generate_report():
    
    global rr_CHAN_node_list
    global rr_edge_list
    global rr_node_list
    global rrg_segments_dict
    global routes
    global arch_segments_dict
    
    log("get rr_CHAN_node_list\n")
    rr_CHAN_node_list = get_rr_CHAN_node_list(rrg)
    log("get rr_edge_list\n")
    rr_edge_list = get_rr_edge_list(rrg)
    log("get arch_segments_dict\n")
    arch_segments_dict = get_segment_list_arch(arch)
    log("get rrg_segments_dict\n")
    rrg_segments_dict = get_segment_list_rrg(rrg, arch_segments_dict)
    log("get rr_node_list\n")
    rr_node_list = get_rr_node_list(rrg, rrg_segments_dict)

    if us:
        # build l0_map
        build_l0_map()

    log("before route_traverse\n")
    routes = route_traverse(route)
    log("after route_traverse\n")

    print_wire_length_statistics(rrg)
    print_net_info()
    close_route_file(route)

    print("Number of unique OPINs that are touched by this design:", len(unique_OPINs))
    print("Number of unique IPINs that are touched by this design:", len(unique_IPINs))
    print("Number of unique grids that their IPIN/OPIN are touched by this design:", len(used_grids))
    print("\n\n")

    print("Following report shows how each of the wire types is tapped in the design.")
    print("Note that overlapping might be possible if a wire is used more than once.\n")
    total_num_of_Ln_wires = {} # L --> total number
    for (L, tap_pos), number in wires_tap_position.items():
        total_num_of_Ln_wires[L] = total_num_of_Ln_wires.get(L, 0) + number
    
    sorted_dict = dict(sorted(wires_tap_position.items(), key=lambda x: (int(x[0][0]), int(x[0][1]))))
    for (L, tap_pos), number in sorted_dict.items():
        percentage = (number / total_num_of_Ln_wires[L]) * 100
        percentage = "{:.2f}".format(percentage)
        print(f"\t{number} ({percentage}%) of L{int(L)-us} wires tapped {tap_pos} block(s) away")
# generate_report


# profiler = cProfile.Profile()
# profiler.enable()
arguments = sys.argv
if len(arguments) > 1:
    arch_addr = arguments[1]
    rrg_addr = arguments[2]
    route_addr = arguments[3]
    timing_addr = arguments[4]
    if "stamp" in rrg_addr or "stamp" in route_addr:
        us = 1
        log('using stamped rr_graph\n')
    else:
        log('using non-stamped rr_graph\n')
    print_info(arch_addr, rrg_addr, route_addr, timing_addr)
    get_node_slack()
    generate_report()
else:
    log(f"bad arguments: {arguments}\n")
# profiler.disable()
# profiler.print_stats(sort='tottime')
