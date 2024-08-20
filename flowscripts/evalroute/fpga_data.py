import xml.etree.ElementTree as ET
import sys
import os
import pickle
from collections import defaultdict


node_type2dir = {
    ('CHANX', 'INC_DIR'): 'E',
    ('CHANX', 'DEC_DIR'): 'W',
    ('CHANY', 'INC_DIR'): 'N',
    ('CHANY', 'DEC_DIR'): 'S',
}


CHAN_span2ptcs = defaultdict(set) # all CHANs that have the same direction and coordinates share the same set of node_id and ptc#


g_max_bit = {} # maximum bit number for each wire type (e.g., E4 --> 16)


# save object to reuse
def save_object(obj, obj_name):
    os.makedirs("objects", exist_ok=True) # if the objects directory exists do not create it again
    with open(f"objects/{obj_name}.pkl", "wb") as f:
        pickle.dump(obj, f)
# save_object


# load object to reuse
def load_object(obj_name):

    
    loaded_obj = None

    if os.path.isfile(f"objects/{obj_name}.pkl"): # if the object file exists, return it. Otherwise, return None
        with open(f"objects/{obj_name}.pkl", "rb") as f:
            loaded_obj = pickle.load(f)
    
    if loaded_obj == None:
        print(f"{obj_name} object file does not exist. Creating the object file...\n")
    return loaded_obj
# load_object


# get the root of architecture xml to parse
def get_arch_xml_root(address):
    #log('getting architecture xml file\n')
    architecture_file = ET.parse(address)
    #log('returning the root (architecture) xml tag from architecture xml file\n')
    return architecture_file.getroot()
# def get_arch_xml_root


# get the root of rr graph xml to parse
def get_rrg_xml_root(address):
    #log('getting rr graph file\n')
    rrg_file = ET.parse(address)
    #log('returning the root (rr_graph) xml tag from architecture xml file\n')
    return rrg_file.getroot()
# def get_rrg_xml_root


# read the entire routing file to parse
def get_route_file(address):
    #log('opening .route file\n')
    return open(address, "r")
# def get_route_file

# read the entire timing file to parse
def get_timing_file(address):
    #log('opening .route file\n')
    return open(address, "r")
# def get_timing_file

def get_pinmap_file(address):
    #log('opening pinmap file\n')
    return open(address, "r")
# def get_pinmap_file

def get_file(address):
    return open(address, "r")
# def get_file


# get pin map file information
def get_pinmap_info(pinmap_file):
    pin_map_list = {}
    # Skip the lines until the header line is reached
    for line in pinmap_file:
        if line.startswith('id x y BLOCK'):
            break
    
    # Create a dictionary for each subsequent line
    n=0
    for line in pinmap_file:
        if not line.strip():
            continue
        n += 1
        fields = line.strip().split()
        new_pin = {
            'id': int(fields[0]),
            'x_offset': int(fields[1]),
            'y_offset': int(fields[2]),
            'block_type': fields[3],
            'stamp_port': fields[4],
            'direction': fields[5],
            'ptc': int(fields[6]),
            'block_port': fields[7]
        }
        #log(f"new pin map visited with: id = {new_pin['id']},\t x_offset = {new_pin['x_offset']},\t y_offset = {new_pin['y_offset']},\t block_type = {new_pin['block_type']},\t stamp_port = {new_pin['stamp_port']},\t direction = {new_pin['direction']},\t ptc = {new_pin['ptc']},\t block_port = {new_pin['block_port']},\n")
        pin_map_list[fields[7]] = new_pin
    #log(f" - Visited {n} <pin map>\n")
    return pin_map_list
# get_pinmap_info


# close routing file
def close_route_file(route_file):
    #log('closing .route file\n')
    route_file.close()
# def close_route_file


# close pinmap file
def close_pinmap_file(pinmap_file):
    #log('closing pinmap file\n')
    pinmap_file.close()
# def close_pinmap_file


# get the list of segments found in architecture xml file --> node-based function
def get_segment_list(architecture):
    #log('getting segment list from architecture xml file\n')
    segmentlist = []
    n = 0
    for segment in architecture.iter('segment'):
        n += 1
        new_segment = {'name': segment.get('name'), 'freq':segment.get('freq'), 'length':segment.get('length')}
        segmentlist.append(new_segment)
        #log(f"new segment visited with name: {new_segment['name']}\t, freq: {new_segment['freq']}\t, length: {new_segment['length']} \n")
    #log(f" - Visited {n} <segment>\n")
    
    return segmentlist
# def get_segment_list


# get the list of segments from rr graph file and assign their length from architecture xml file --> node-based function
def get_segment_list_rrg_nb(rrg_root, arch_segments):
    #arch_segment_list = get_segment_list(architecture)
    #log('getting segment list from rr graph file and assign their length from architecture xml file\n')
    segmentlist = []
    n = 0
    for segment in rrg_root.iter('segment'):
        for arch_seg in arch_segments:
            if segment.get('name') == arch_seg['name']:
                n += 1
                new_segment = {'id': segment.get('id'), 'name':segment.get('name'), 'length': arch_seg['length'], 'freq': arch_seg['freq']}
                #log(f"new segment visited with id: {new_segment['id']}\t, length: {new_segment['length']}\n")
                segmentlist.append(new_segment)
    #log(f" - Visited {n} <segment>\n")
    #print(segmentlist)
    
    return segmentlist
# def get_segment_list_rrg_nb


# get the list of channel width from rr graph file --> node-based function
def get_chanwidth_list(rrg_root):
    # log('getting the list of channel width from rr graph file\n')
    chan_x_width_list = []
    chan_y_width_list = []
    n = 0
    for channel in rrg_root.iter('x_list'):
        n += 1
        new_chan_width = {'index': channel.get('index'), 'info':channel.get('info')}
        # log(f"new x channel visited with index: {new_chan_width['index']}\t and width: {new_chan_width['info']}\n")
        chan_x_width_list.append(new_chan_width)
    # log(f" - Visited {n} <x_list>\n")

    n = 0
    for channel in rrg_root.iter('y_list'):
        n = 0
        new_chan_width = {'index': channel.get('index'), 'info':channel.get('info')}
        # log(f"new y channel visited with index: {new_chan_width['index']}\t and width: {new_chan_width['info']}\n")
        chan_y_width_list.append(new_chan_width)
    # log(f" - Visited {n} <y_list>\n")

    return chan_x_width_list, chan_y_width_list
# def get_chanwidth_list


# get the list of segments found in architecture xml file --> edge-based function
def get_segment_list_arch(architecture):
    #log('getting segment list from architecture xml file\n')
    segmentlist = dict()
    n = 0
    for segment in architecture.iter('segment'):
        n += 1
        new_segment = {'name': segment.get('name'), 'freq':segment.get('freq'), 'length':segment.get('length')}
        segmentlist[new_segment['name']] = new_segment
        #log(f"new segment visited with name: {new_segment['name']},\t freq: {new_segment['freq']},\t and length: {new_segment['length']} \n")
    #log(f" - Visited {n} <segment>\n")
    
    return segmentlist
# def get_segment_list


# get the size of the device (including two empty rows and colomns)
def get_device_size(rrg_root):
    #log('getting the size of the device from the list of channel width from rr graph file\n')
    max_x_ = 0
    max_y_ = 0

    for channel in rrg_root.iter('x_list'):
        max_y_ += 1
        
    for channel in rrg_root.iter('y_list'):
        max_x_ += 1

    #log(f"device size is: {max_x}X{max_y}\n")
    max_size = [max_x_, max_y_]
    return max_size
# get_device_size


# get the list of segments from rr graph file and assign their length from architecture xml file
def get_switch_list_rrg(rrg_root):

    switchlist = {}
    n = 0
    switch_tags = rrg_root.findall('.//switches/switch')
    for switch in switch_tags:
        n += 1
        id = int(switch.get('id'))
        name = switch.get('name')
        if name == '__vpr_delayless_switch__':
            switchlist[id] = 'LR'
        elif 'cb' in name:
            switchlist[id] = 'CB'
        else:
            switchlist[id] = name[:2]
    #log(f" - Visited {n} <switches>\n")
    #print(switchlist)
    
    return switchlist
# def get_switch_list_rrg


# get the list of segments from rr graph file and assign their length from architecture xml file
def get_segment_list_rrg(rrg_root, arch_segments):
    #arch_segment_list = get_segment_list(architecture)
    #log('getting segment list from rr graph file and assign their length from architecture xml file\n')
    segmentlist = dict()
    n = 0
    segment_tags = rrg_root.findall('.//segments/segment')
    for segment in segment_tags:
        n += 1
        #log(f"segment name = {segment.get('name')}\n")
        new_segment = {'id': segment.get('id'), 'name':segment.get('name'), 'length': arch_segments[segment.get('name')]['length'], 'freq': arch_segments[segment.get('name')]['freq']}
        #log(f"new segment visited with id: {new_segment['id']}\t, length: {new_segment['length']}, and freq: {new_segment['freq']}\n")
        segmentlist[new_segment['id']] = new_segment
    #log(f" - Visited {n} <segment>\n")
    #print(segmentlist)
    
    return segmentlist
# def get_segment_list_rrg


# get the list of nodes and their physical length
def get_rr_node_list(rrg_root, rrg_segments):
    #log('getting the list of nodes from rr graph file\n')
    node_list = {}
    global CHAN_span2ptcs
    global g_max_bit
    n = 0
    segment_list = rrg_segments
    for node in rrg_root.iter('node'):
        n += 1
        direction = None
        length = -1
        ph_length = -1
        side = None
        segment_id = '-1'

        if node.get('type').startswith('CHAN'):
            segment_id = node.find('segment').get('segment_id')
            direction = node_type2dir[node.get('type'), node.get('direction')] # Example: CHANX, INC_DIR --> E
            length = segment_list[segment_id]['length']
            ph_length = (int(node.find('loc').get('xhigh')) - int(node.find('loc').get('xlow')) + int(node.find('loc').get('yhigh')) - int(node.find('loc').get('ylow')) + 1)
        elif node.get('type') == 'IPIN' or node.get('type') == 'OPIN':
            side = node.find('loc').get('side')
        
        new_node = {'id': int(node.get('id')),
                    'type': node.get('type'),
                    'direction': direction,
                    'side': side,
                    'ptc': int(node.find('loc').get('ptc')),
                    'bit': -1, # by default is -1, but then it needs to be changed for CHANX/CHANY
                    'x_low': int(node.find('loc').get('xlow')),
                    'x_high': int(node.find('loc').get('xhigh')),
                    'y_low': int(node.find('loc').get('ylow')),
                    'y_high': int(node.find('loc').get('yhigh')),
                    'length': int(length),
                    'ph_length': ph_length,
                    'segment_id': segment_id}

        # determining the bit value ONLY for CHANX and CHANY:
        if node.get('type').startswith('CHAN'):
            CHAN_span2ptcs[((new_node['direction']+str(new_node['length'])), new_node['x_low'], new_node['x_high'], new_node['y_low'], new_node['y_high'])].add(((new_node['ptc']), (new_node['id'])))
        
        node_list[new_node['id']] = new_node
        #log(f"new node visited with id: {new_node['id']},\t type: {new_node['type']},\t direction: {new_node['direction']},\t side: {new_node['side']},\t ptc: {new_node['ptc']},\t x_low: {new_node['x_low']},\t x_high: {new_node['x_high']},\t y_low: {new_node['y_low']},\t y_high: {new_node['y_high']},\t length: {new_node['length']}\n")

    # Works fine --> all sets have 16 elements
    span_counts = {}
    for key, value in CHAN_span2ptcs.items():
        span_counts[key] = len(value)
        if key[0] in g_max_bit:
            if len(value) > g_max_bit[key[0]]:
                g_max_bit[key[0]] = len(value)
        else:
            g_max_bit[key[0]] = len(value)
        # if(span_counts[key] != 16):
        #     print(f"ERROR: Not 16 -->{key}: {span_counts[key]}\n")
    #print("span_counts:",span_counts,"\n")
    
    # all CHANs that have the same direction and coordinates share the same set of node_id and ptc#
    for key in CHAN_span2ptcs:
        bits = sorted(CHAN_span2ptcs[key]) # first, sort the values of each key
        for n, pair in enumerate(bits, start=0): # then, get the (ptc,id) pair as the value and start counting from 0
            (ptc, id) = pair
            node_list[id]['bit'] = n # assign bit value to each node inside the node list
            #log(f"node-{id} bit: {node_list[id]['bit']}\n")

    #log(f" - Visited {n} <node>\n")

    return node_list
# def get_rr_node_list


# get the list of nodes that are CHANX or CHANY and their physical length
def get_rr_CHAN_node_list(rrg_root):
    # log('getting the list of nodes that are CHANX or CHANY from rr graph file\n')
    CHAN_node_list = {}
    n = 0
    for node in rrg_root.iter('node'):
        if node.get('type') == 'CHANX':
            n += 1
            x_start = x_end = y_start = y_end = 0
            ph_length = 0
            ph_length = (int(node.find('loc').get('xhigh')) - int(node.find('loc').get('xlow')) + int(node.find('loc').get('yhigh')) - int(node.find('loc').get('ylow')) + 1)
            if node.get('direction') == 'DEC_DIR':
                x_start = int(node.find('loc').get('xhigh'))
                x_end = int(node.find('loc').get('xlow'))
                y_start = int(node.find('loc').get('yhigh'))
                y_end = int(node.find('loc').get('ylow'))
            elif node.get('direction') == 'INC_DIR':
                x_start = int(node.find('loc').get('xlow'))
                x_end = int(node.find('loc').get('xhigh'))
                y_start = int(node.find('loc').get('ylow'))
                y_end = int(node.find('loc').get('yhigh'))
            new_node = {'id': node.get('id'),
                        'type': node.get('type'),
                        'dir': node.get('direction'),
                        'ptc': node.find('loc').get('ptc'),
                        'x_start': x_start,
                        'x_end': x_end,
                        'y_start': y_start,
                        'y_end': y_end,
                        'ph_length': ph_length,
                        'segment_id': node.find('segment').get('segment_id')}
            # log(f"new node visited with id: {new_node['id']}\t, type: {new_node['type']}\t, dir: {new_node['dir']}\t, ptc: {new_node['ptc']}\t, x_start: {new_node['x_start']}\t, x_end: {new_node['x_end']}\t, y_start: {new_node['y_start']}\t, y_end: {new_node['y_end']}\t, ph_length: {new_node['ph_length']}\t, segment_id: {new_node['segment_id']}\n")
            CHAN_node_list[new_node['id']] = new_node

        elif node.get('type') == 'CHANY':
            n += 1
            x_start = x_end = y_start = y_end = 0
            ph_length = 0
            ph_length = (int(node.find('loc').get('xhigh')) - int(node.find('loc').get('xlow')) + int(node.find('loc').get('yhigh')) - int(node.find('loc').get('ylow')) + 1)
            if node.get('direction') == 'DEC_DIR':
                x_start = int(node.find('loc').get('xhigh'))
                x_end = int(node.find('loc').get('xlow'))
                y_start = int(node.find('loc').get('yhigh'))
                y_end = int(node.find('loc').get('ylow'))
            elif node.get('direction') == 'INC_DIR':
                x_start = int(node.find('loc').get('xlow'))
                x_end = int(node.find('loc').get('xhigh'))
                y_start = int(node.find('loc').get('ylow'))
                y_end = int(node.find('loc').get('yhigh'))
            new_node = {'id': node.get('id'),
                        'type': node.get('type'),
                        'dir': node.get('direction'),
                        'ptc': node.find('loc').get('ptc'),
                        'x_start': x_start,
                        'x_end': x_end,
                        'y_start': y_start,
                        'y_end': y_end,
                        'ph_length': ph_length,
                        'segment_id': node.find('segment').get('segment_id')}
            # log(f"new node visited with id: {new_node['id']}\t, type: {new_node['type']}\t, dir: {new_node['dir']}\t, ptc: {new_node['ptc']}\t, x_start: {new_node['x_start']}\t, x_end: {new_node['x_end']}\t, y_start: {new_node['y_start']}\t, y_end: {new_node['y_end']}\t, ph_length: {new_node['ph_length']}\t, segment_id: {new_node['segment_id']}\n")
            CHAN_node_list[new_node['id']] = new_node
    # log(f" - Visited {n} <node>\n")

    return CHAN_node_list
# def get_rr_CHAN_node_list


# get the list of edges in two different sets: one with key = source_id, and one with key = sink_id
def get_rr_edge_list(rrg_root):
    #log('getting the list of edges from rr graph file\n')
    edge_list = []
    n = 0
    for edge in rrg_root.iter('edge'):
        n += 1
        new_edge = {'source_id': int(edge.get('src_node')),
                    'sink_id': int(edge.get('sink_node')),
                    'switch_id': int(edge.get('switch_id'))}
        #log(f"new edge visited with source_id: {new_edge['source_id']},\t sink_id: {new_edge['sink_id']},\t switch_id: {new_edge['switch_id']}\n")
        edge_list.append(new_edge)
    #log(f" - Visited {n} <edge>\n")
    
    return edge_list
# def get_rr_edge_list


# get the list of grids with their info: ['block_type_id'], ['block_type'], ['height_offset'], ['width_offset'], ['x'], ['y']
def get_rr_grid_list(rrg_root, rr_block_type_list):
    #log('getting the list of grids from rr graph file\n')
    grid_list = {}
    n = 0
    for grid in rrg_root.iter('grid_loc'):
        n += 1
        new_grid = {'block_type_id': int(grid.get('block_type_id')),
                    'block_type': rr_block_type_list[int(grid.get('block_type_id'))]['name'],
                    'height_offset': int(grid.get('height_offset')),
                    'width_offset': int(grid.get('width_offset')),
                    'x': int(grid.get('x')),
                    'y': int(grid.get('y')),
                    }
        #log(f"new grid visited with block_type_id: {new_grid['block_type_id']},\t block_type: {new_grid['block_type']},\t height_offset: {new_grid['height_offset']},\t width_offset: {new_grid['width_offset']},\t x: {new_grid['x']},\t y: {new_grid['y']}\n")
        grid_list[(new_grid['x'], new_grid['y'])] = new_grid
    #log(f" - Visited {n} <grid>\n")

    return grid_list
# def get_rr_grid_list


# get the list of block_type with all their info: id, name, width, height, list of pin_class, type and list of pins for each pin_class, ptc# and name of each pin
def get_rr_block_type_list(rrg_root):
    #log('getting the list of blocks from rr graph file\n')
    block_type_list = {}
    n = 0
    block_tpye_tags = rrg_root.findall('.//block_types/block_type')
    for block_type in block_tpye_tags:
        n += 1
        all_pin_list = {} # this stores all the pins in different pin classes for current block. Because we want to access the list of pins from a higher level (block)
        id = int(block_type.get('id'))
        block_name = block_type.get('name')
        width = int(block_type.get('width'))
        height = int(block_type.get('height'))
        #log(f"new block type visited with id = {id},\t block_name = {block_name},\t width = {width},\t height = {height}\n")
        pin_classes = []
        pin_classes_tags = block_type.findall('.//pin_class')
        #log("\tIts pin classes are listed below:\n")
        for pin_class in pin_classes_tags: # for each pin class of the current block type
            pin_class_type = pin_class.get('type')
            #log(f"\tpin class type = {pin_class_type}\n")
            pin_class_pins = {}
            pin_tags = pin_class.findall('.//pin')
            #log(f"\t\tIts pins are listed below:\n")
            for pin in pin_tags: # for each pin of the current pin class, add the pin to the pin_class_pins
                ptc = int(pin.get('ptc'))
                pin_name = pin.text
                #log(f"\t\tpin_ptc# = {ptc},\t pin_name = {pin_name}\n")
                pin_class_pins[ptc] = {'ptc': ptc, 'name': pin_name}
            pin_classes.append({'pin_class_type': pin_class_type, 'pin_list':pin_class_pins})
            all_pin_list.update(pin_class_pins) # add pins of the current pin_class to the list of all pins
        new_block_type = {'id': id,
                          'name': block_name,
                          'width': width,
                          'height': height,
                          'pin_classes': pin_classes,
                          'all_pin_list': all_pin_list}
        block_type_list[id] = new_block_type
    #log(f" - Visited {n} <block_type>\n")

    return block_type_list
# def get_rr_block_type_list


