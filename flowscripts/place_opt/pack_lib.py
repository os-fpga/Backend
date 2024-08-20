from lxml import etree
import sys
from place_lib import *
import copy

debug   = 1
MAX_FLE = 8

top_net_addr                    = '' # the address of .net file
top_instance                    = '' # instance="FPGA_packed_netlist[0]" --> first line of .net file
top_architecture_id             = '' # Architecture SHA_ID --> first line of .net file
top_atom_netlist_id             = '' # Atom SHA_ID --> first line of .net file
top_inputs                      = [] # top_block inputs
top_outputs                     = [] # top_block outputs
top_clocks                      = [] # top_block clocks
top_blocks                      = [] # list of block object (tiles) --> sub-blocks of top_block (it should have been top_sub_blocks)
all_blocks                      = [] # flattened list of all block objects without any hierarchy
all_pins                        = {} # (name, assigned_name) --> pin object
atom_output_name2block          = {} # atom_output_name --> atom_block
atom2info                       = {} # (atom_block) --> (x, y, FLE#, FLE_block) (ONLY FOR CLBs)
clb_pin2fles                    = {} # (tile_instance_name, pin_name) --> list of (fle#, fle_pin_index) tuple(s), to which this pin goes
net2edges_coor                  = {} # Net_name --> edges[] each element of edges is a (src_pin_index, src_x, src_y, dst_pin_index, dst_x, dst_y) tuple
net2edges_pin_obj               = {} # Net_name --> edges[] each element of edges is a (src_Pin, dst_Pin) tuple
atom_output_name2top_most_pin   = {} # atom output name --> top-most output pin object on the tile
coor2tile                       = {} # (x,y) --> tile
src_dst_same_coor               = {} # (x,y) --> [(src_fle, dst_fle)] list of src, dst FLEs that are in the same CLB

# Block class is used for all blocks in different hierarchies
class Block:
    def __init__(self, 
                 name, 
                 instance_name,
                 mode=None, 
                 inputs=None, 
                 outputs=None, 
                 clocks=None, 
                 attributes=None, 
                 parameters=None, 
                 sub_blocks=None,
                 base_x=None,
                 base_y=None,
                 parent_block=None,
                 is_tile_block=None,
                 is_atom=None,
                 input_pin_objects=None,
                 output_pin_objects=None,
                 slot_num=None,
                 movable_objects=None):
        self.name                   = name                                                              # name
        self.instance_name          = instance_name                                                     # instance name (including block type)
        self.mode                   = mode if mode is not None else ""                                  # mode
        self.inputs                 = inputs if inputs is not None else []                              # input ports list. Each one is a tupple --> inputs[(tag1, name1, pins1), (tag2, name2, pins2), ...]
        self.outputs                = outputs if outputs is not None else []                            # output ports list. Each one is a tupple --> outputs[(tag1, name1, pins1), (tag2, name2, pins2), ...]
        self.clocks                 = clocks if clocks is not None else []                              # clock ports list. Each one is a tupple --> clocks[(tag1, name1, pins1), (tag2, name2, pins2), ...]
        self.attributes             = attributes if attributes is not None else []                      # attribute list. Each one is a tupple --> attribute[(tag1, name1, val1), (tag2, name2, val2), ...]
        self.parameters             = parameters if parameters is not None else []                      # parameter list. Each one is a tupple --> parameters[(tag1, name1, val1), (tag2, name2, val2), ...]
        self.sub_blocks             = sub_blocks if sub_blocks is not None else []                      # sub-blocks list. Each sub-block is a Block Class.
        self.base_x                 = base_x if base_x is not None else -1                              # base_x
        self.base_y                 = base_y if base_y is not None else -1                              # base_y
        self.parent_block           = parent_block                                                      # parent_block
        self.is_tile_block          = is_tile_block if is_tile_block is not None else False             # is_tile_block
        self.is_atom                = is_atom if is_atom is not None else False                         # is_atom
        self.input_pin_objects      = input_pin_objects if input_pin_objects is not None else []        # input_pin_objects 
        self.output_pin_objects     = output_pin_objects if output_pin_objects is not None else []      # output_pin_objects
        self.slot_num               = slot_num if slot_num is not None else 0                           # slot_num is used for tiles w/t capacity > 1 (e.g., IO tile)
        self.movable_objects        = movable_objects if movable_objects is not None else []            # list of movable objects [(fle_start, fle_end), ...]
    
    def set_name(self, name):
        self.name = name
    def set_instance_name(self, instance_name):
        self.instance_name = instance_name
    def set_mode(self, mode):
        self.mode = mode
    def set_base_x(self, base_x):
        self.base_x = base_x
    def set_base_y(self, base_y):
        self.base_y = base_y
    def set_parent_block(self, parent_block):
        self.parent_block = parent_block
    def set_is_tile_block(self, is_tile_block):
        self.is_tile_block = is_tile_block
    def set_is_atom(self, is_atom):
        self.is_atom = is_atom
    def set_slot_num(self, slot_num):
        self.slot_num = slot_num

    def add_input(self, input_tuple):
        self.inputs.append(input_tuple)
    def remove_input(self, input_tuple):
        if input_tuple in self.inputs:
            self.inputs.remove(input_tuple)
        else:
            log(f"ERROR: Input '{input_tuple}' not found.\n")
    
    def add_output(self, output_tuple):
        self.outputs.append(output_tuple)
    def remove_output(self, output_tuple):
        if output_tuple in self.outputs:
            self.outputs.remove(output_tuple)
        else:
            log(f"ERROR: Output '{output_tuple}' not found.\n")
    
    def add_clock(self, clock_tuple):
        self.clocks.append(clock_tuple)
    def remove_clock(self, clock_tuple):
        if clock_tuple in self.clocks:
            self.clocks.remove(clock_tuple)
        else:
            log(f"ERROR: Clock '{clock_tuple}' not found.\n")

    def add_attribute(self, attribute):
        self.attributes.append(attribute)
    def remove_attribute(self, attribute):
        if attribute in self.attributes:
            self.attributes.remove(attribute)
        else:
            log(f"ERROR: Attribute '{attribute}' not found.\n")
    
    def add_parameter(self, parameter):
        self.parameters.append(parameter)
    def remove_parameter(self, parameter):
        if parameter in self.parameters:
            self.parameters.remove(parameter)
        else:
            log(f"ERROR: Parameter '{parameter}' not found.\n")
    
    def add_sub_block(self, sub_block):
        self.sub_blocks.append(sub_block)
    def remove_sub_block(self, sub_block):
        try:
            self.sub_blocks.remove(sub_block)
        except ValueError:
            log(f"ERROR: Sub block '{sub_block}' not found.\n")
    #TODO: remove functions must get name and other arguments to remove an object. Do this for all remove functions
    def add_input_pin_object(self, input_pin_object):
        self.input_pin_objects.append(input_pin_object)
    def remove_input_pin_object(self, input_pin_object):
        try:
            self.input_pin_objects.remove(input_pin_object)
        except ValueError:
            log(f"ERROR: Input Pin object '{input_pin_object}' not found.\n")

    def add_output_pin_object(self, output_pin_object):
        self.output_pin_objects.append(output_pin_object)
    def remove_output_pin_object(self, output_pin_object):
        try:
            self.output_pin_objects.remove(output_pin_object)
        except ValueError:
            log(f"ERROR: Output Pin object '{output_pin_object}' not found.\n")
    
    def add_movable_objects(self, movable_objects):
        self.movable_objects.append(movable_objects)
    def remove_movable_objects(self, movable_objects):
        try:
            self.movable_objects.remove(movable_objects)
        except ValueError:
            log(f"ERROR: Movable object '{movable_objects}' not found.\n")
    

    def print_info(self):
        lines = ""
        lines += f"name = {self.name}\n"
        lines += f"instance_name = {self.instance_name}\n"
        lines += f"inputs:\n"
        for input in self.inputs:
            lines += f"\t{input}\n"
        lines += f"outputs:\n"
        for output in self.outputs:
            lines += f"\t{output}\n"
        lines += f"clocks:\n"
        for clock in self.clocks:
            lines += f"\t{clock}\n"
        lines += f"attributes:\n"
        for attribute in self.attributes:
            lines += f"\t{attribute}\n"
        lines += f"parameters:\n"
        for parameter in self.parameters:
            lines += f"\t{parameter}\n"
        lines += f"sub_blocks:\n"
        for sub_block in self.sub_blocks:
            lines += f"\t{sub_block.instance_name}\n"
        lines += f"base_x = {self.base_x}\n"
        lines += f"base_y = {self.base_y}\n"
        lines += f"parent_block = {self.parent_block.instance_name}\n"
        lines += f"is_tile_block = {self.is_tile_block}\n"
        lines += f"is_atom = {self.is_atom}\n"
        lines += f"slot_num = {self.slot_num}\n"
        lines += f"Pin objects:\n"
        for pin_object in self.input_pin_objects:
            for line in pin_object.print_info().split('\n'):
                lines += f"\t{line}\n"
        for pin_object in self.output_pin_objects:
            for line in pin_object.print_info().split('\n'):
                lines += f"\t{line}\n"

        return lines
# class Block


# Pin class is used only for tile's inputs/outputs
class Pin:
    def __init__(self, 
                 name,
                 assigned_name,
                 block,
                 x,
                 y,
                 plus_y,
                 sub_ys,
                 index,
                 atom_output_name=None,
                 fle_blocks=None,):
        self.name               = name                                                                  # name of the pin (I00[1])
        self.assigned_name      = assigned_name                                                         # assigned name to this pin by packer (e.g., open, abc...)
        self.block              = block                                                                 # corresponding block from Block class
        self.fle_blocks         = fle_blocks if fle_blocks is not None else []                          # FLE block from Block class
        self.x                  = x                                                                     # comes from Block base_x
        self.y                  = y                                                                     # comes from Block base_y
        self.plus_y             = plus_y                                                                # when tile's height is > 1
        self.index              = index                                                                 # index on the block
        self.sub_ys             = sub_ys                                                                # plane_number(s) for this pin
        self.atom_output_name   = atom_output_name                                                      # if the pin is an output pin, what atom's output drives it


    def set_name(self, name):
        self.name = name 
    def set_block(self, block):
        self.block = block
    def set_fle_block(self, fle_block):
        self.fle_block = fle_block
    def set_x(self, x):
        self.x = x
    def set_y(self, y):
        self.y = y
    def set_plus_y(self, plus_y):
        self.plus_y = plus_y
    def set_index(self, index):
        self.index = index
    def set_atom_output_name(self, atom_output_name):
        self.atom_output_name = atom_output_name
    def add_sub_ys(self, sub_y):
        self.sub_ys.append(sub_y)

    def print_info(self):
        lines = ""
        lines += f"name = {self.name}\n"
        lines += f"assigned_name = {self.assigned_name}\n"
        lines += f"block = {self.block.instance_name}\n"
        lines += f"fle_blocks:\n"
        for fle_block in self.fle_blocks:
            lines += f"\t{fle_block.instance_name}\n"
        lines += f"x = {self.x}\n"
        lines += f"y = {self.y}\n"
        lines += f"plus_y = {self.plus_y}\n"
        lines += f"atom_output_name = {self.atom_output_name}\n"
        lines += f"sub_y(s):\n"
        for sub_y in self.sub_ys:
            lines += f"\t{sub_y}\n"
        return lines
# class Pin   


def log(text):
    """write to stderr"""
    if not debug:
        return
    sys.stderr.write(text)
# def log(text)


def out(text, filename):
    """write to specific file"""
    with open(filename, "wb") as f:
        f.write(text)
# def out(text, filename)


# return plus_y value for each pin based on its name and the tile type
def get_plus_y(tile_name, port_name):

    # for IO and CLB tiles, all pins have plus_y = 0
    if 'clb' in tile_name or 'io' in tile_name:
        return 0
    
    # for BRAM/DSP tiles we should look at the port name
    elif 'bram' in tile_name or 'dsp' in tile_name:
        if port_name.startswith('I'):
            # port_name examples: I00, I30, IS0, I01, I31, IS1, ...
            return int(port_name[2])
        elif port_name.startswith('O'):
            # port_name examples: O0, O1, O2
            return int(port_name[1])
        # port_name examples: clk, sc_in, sc_out, ...
        else:
            return 0
    return -1
# def get_plus_y(tile_name, port_name)


# return sub_y value (0-7) for each pin based on its connection to FLE (for CLB). For other tiles, it's a look-up table.
def get_sub_y(tile, port_name, pin_index):

    tile_name = tile.instance_name.split('[')[0]

    if 'bram' in tile_name or 'dsp' in tile_name:
        bram_dsp_sub_y = -1
        if any(substring in port_name for substring in ['sc_in', 'sr_in', 'clk', 'IS']):
            bram_dsp_sub_y = 0
        # elif port_name.startswith('IS'):
        #     bram_dsp_sub_y = int(port_name[2]) * 2 + int(pin_index / 6)
        elif port_name.startswith('I'):
            bram_dsp_sub_y = int(port_name[1]) * 2 + int(pin_index / 6) # I00[0] - I00[5] --> 0*2 + (0-5)/6 = 0, I10[6] - I10[11] --> 1*2 + (6-11)/6 = 3
        elif port_name.startswith('O'):
            bram_dsp_sub_y = int(pin_index / 3)
        elif any(substring in port_name for substring in ['sc_out', 'sr_out']):
            bram_dsp_sub_y = 7
        else:
            bram_dsp_sub_y = 0
        return bram_dsp_sub_y
    
    elif 'io' in tile_name:
        io_sub_y = -1
        if "f2a_i" in port_name:
            io_sub_y = int((tile.slot_num - 24) / 6)
        elif "a2f_o" in port_name:
            io_sub_y = int(tile.slot_num / 3)
        else:
            io_sub_y = 0
        return io_sub_y
    
    elif 'clb' in tile_name:
        clb_sub_y = -1
        if any(substring in port_name for substring in ['cin', 'sc_in', 'sr_in', 'clk', 'IS']):
            clb_sub_y = 0
        elif any(substring in port_name for substring in ['cout', 'sc_out', 'sr_out']):
            clb_sub_y = 7
        else:
            if port_name.startswith('O'):
                clb_sub_y = int(pin_index / 3)
            # elif fle_block:
            #     clb_sub_y = int(fle_block.instance_name.split('[')[1].split(']')[0])
        return clb_sub_y
    
    return -1
# def get_sub_y(tile, fle_num, port_name, pin_index)


# return fle block(s) for a given tile block and pin name (I00)
def get_fle_blocks(tile, pin_name):
    log(f"finding fle_blocks for ({tile.instance_name}, {pin_name})\n")
    global clb_pin2fles
    fle_blocks = []
    
    # input pins
    if pin_name.startswith('I'):
        input_name_on_clb_lr = f"{tile.instance_name.split('[')[0]}.{pin_name}"
        log(f"\tinput_name_on_clb_lr = {input_name_on_clb_lr}\n")
        clb_lr = None
        if tile.sub_blocks:
            clb_lr = tile.sub_blocks[0]
            if not clb_lr.instance_name.startswith('clb_lr'):
                log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
                exit()
        else:
            log(f"ERROR: {tile} does not have clb_lr\n")
            exit()
        log(f"\tclb_lr = {clb_lr.instance_name}\n")
        
        #based on vpr.xml clb_lr.in[x] goes to fle#(x % 8) on its pin#(x / 8)
        for input_port in clb_lr.inputs: # all input ports of clb_lr
            (_, port_name, pin_list) = input_port
            for index, pin in enumerate(pin_list): # all pins on each input port of clb_lr
                if input_name_on_clb_lr in pin:
                    log(f"\t\tinput_name_on_clb_lr = {input_name_on_clb_lr} and pin = {pin} on index = {index}\n")
                    input_name_on_fle = f"clb_lr.in[{index}]->direct_in_{int(index/8)}"
                    log(f"\t\tinput_name_on_fle = {input_name_on_fle}\n")
                    fle_num = index % 8
                    log(f"\t\tfle_num = {fle_num}\n")
                    fle_pin_index = f"in[{int(index/8)}]"
                    log(f"\t\tfle_pin_index = {fle_pin_index}\n")
                    clb_pin2fles.setdefault((tile.instance_name, pin_name), []).append((fle_num, fle_pin_index))
                    fle_instance_name = f"fle[{fle_num}]"
                    log(f"\t\tcreating fle_instance_name = {fle_instance_name}\n")
                    for sub_block in clb_lr.sub_blocks:
                        if sub_block.instance_name == fle_instance_name:
                            log(f"\t\tFound fle block: {sub_block.instance_name}\n")
                            fle_blocks.append(sub_block)
    
    # output pins
    if pin_name.startswith('O'):
        clb_lr = None
        if tile.sub_blocks:
            clb_lr = tile.sub_blocks[0]
            if not clb_lr.instance_name.startswith('clb_lr'):
                log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
                exit()
        else:
            log(f"ERROR: {tile} does not have clb_lr\n")
            exit()
        log(f"\tclb_lr = {clb_lr.instance_name}\n")

        pin_index = int(pin_name.split('[')[1].split(']')[0])
        log(f"\toutput pin_index = {pin_index}\n")
        fle_num = int(pin_index / 3)
        log(f"\tfle_num = {fle_num}\n")
        temp = pin_index % 3
        fle_pin_index = f"out[{temp}]" if temp == 0 or temp == 1 else f"o6[0]"
        log(f"\tfle_pin_index = {fle_pin_index}\n")
        clb_pin2fles.setdefault((tile.instance_name, pin_name), []).append((fle_num, fle_pin_index))
        fle_instance_name = f"fle[{fle_num}]"
        log(f"\t\tcreating fle_instance_name = {fle_instance_name}\n")

        for sub_block in clb_lr.sub_blocks:
            if sub_block.instance_name == fle_instance_name:
                log(f"\t\tFound fle block: {sub_block.instance_name}\n")
                fle_blocks.append(sub_block)
    return fle_blocks
# def get_fle_blocks(tile, pin_name)


def get_atom_output_name(tile, pin_assigned_name):
    log(f"inside get_atom_output_name\n")
    atom_output_name = ''
    
    if tile.instance_name.split('[')[0] == 'io':
        atom_output_name = tile.name
        log(f"\ttile is io --> atom_output_name = {atom_output_name}\n")
        return atom_output_name
    
    if tile.name == 'open':
        atom_output_name = ''
        log(f"\ttile is open. skip it --> atom_output_name = {atom_output_name}\n")
        return atom_output_name
    
    log(f"\ttile name = {tile.name}, tile instance_name = {tile.instance_name}, pin_assigned_name = {pin_assigned_name}\n")
    
    # still not atom block
    if tile.sub_blocks:
        if not pin_assigned_name == 'open':
            sub_block_instance_name = pin_assigned_name.split('.')[0]
            log(f"\t\tsub_block_instance_name = {sub_block_instance_name}\n")
            sub_block_port_name = pin_assigned_name.split('.')[1].split('[')[0]
            log(f"\t\tsub_block_port_name = {sub_block_port_name}\n")
            sub_block_pin_index = int(pin_assigned_name.split('.')[1].split(']')[0].split('[')[1])
            log(f"\t\tsub_block_pin_index = {sub_block_pin_index}\n")
            for sub_block in tile.sub_blocks:
                if sub_block.instance_name == sub_block_instance_name:
                    for output_port in sub_block.outputs:
                        (_, port_name, pin_list) = output_port
                        if port_name == sub_block_port_name:
                            atom_output_name =  get_atom_output_name(sub_block, pin_list[sub_block_pin_index])
        else:
            log(f"pin_assigned_name is open. returning empty string for atom_output_name\n")
            atom_output_name = ''
    
    # now the atom block
    else:
        for output_port in tile.outputs:
            (_, port_name, pin_list) = output_port
            for pin in pin_list:
                if pin == pin_assigned_name:
                    atom_output_name = pin_assigned_name
                    log(f"\t\tatom_output_name = {atom_output_name}\n")
    
    return atom_output_name
# def get_atom_output_name(tile, pin_assigned_name)


# create pin object for each tile's pin
def make_pins():
    global atom_output_name2top_most_pin
    global all_pins

    for block in top_blocks:
        tile = block
        pin_block = tile
        pin_x = tile.base_x
        pin_y = tile.base_y
        for input_port in tile.inputs:
            (_, port_name, pin_list) = input_port
            
            for index, pin in enumerate(pin_list):
                pin_plus_y = -1
                pin_sub_ys = []
                pin_fle_blocks = []
                pin_assigned_name = pin
                if pin == 'open':
                    continue
                pin_name = f"{port_name}[{index}]"
                
                # find plus_y
                pin_plus_y = get_plus_y(tile.instance_name.split('[')[0], port_name)
                
                # find sub_y
                if tile.instance_name.split('[')[0] == 'clb' and port_name.startswith('I') and not port_name.startswith('IS'):
                    # find fle blocks
                    pin_fle_blocks = get_fle_blocks(tile, pin_name)
                    if pin_fle_blocks:
                        for pin_fle_block in pin_fle_blocks:
                            pin_sub_ys.append(int(pin_fle_block.instance_name.split('[')[1].split(']')[0]))
                else:
                    pin_sub_ys.append(get_sub_y(tile, port_name, index))
                
                log(f"new input pin: pin_name = {pin_name}, pin_assigned_name = {pin_assigned_name}, pin_block = {pin_block.instance_name}, pin_x = {pin_x}, pin_y = {pin_y}, pin_plus_y = {pin_plus_y}, pin_sub_ys = {pin_sub_ys}, pin_fle_blocks = {pin_fle_blocks}\n")
                new_pin = Pin(name=pin_name, 
                              assigned_name=pin_assigned_name,
                              block=pin_block,
                              x=pin_x,
                              y=pin_y,
                              plus_y=pin_plus_y,
                              sub_ys=pin_sub_ys,
                              index=index,
                              fle_blocks=pin_fle_blocks,)
                tile.add_input_pin_object(new_pin)
                all_pins[(new_pin.name, new_pin.assigned_name)] = new_pin
        
        for output_port in tile.outputs:
            (_, port_name, pin_list) = output_port
            
            for index, pin in enumerate(pin_list):
                pin_plus_y = -1
                pin_sub_ys = []
                pin_fle_blocks = []
                pin_assigned_name = pin
                if pin == 'open':
                    continue
                pin_name = f"{port_name}[{index}]"
                
                # find plus_y
                pin_plus_y = get_plus_y(tile.instance_name.split('[')[0], port_name)
                
                # find sub_y
                if tile.instance_name.split('[')[0] == 'clb' and port_name.startswith('O'):
                    # find fle block
                    pin_fle_blocks = get_fle_blocks(tile, pin_name)
                    if pin_fle_blocks:
                        for pin_fle_block in pin_fle_blocks:
                            pin_sub_ys.append(int(pin_fle_block.instance_name.split('[')[1].split(']')[0]))
                else:
                    pin_sub_ys.append(get_sub_y(tile, port_name, index))
                

                # find atom output name
                atom_output_name = get_atom_output_name(tile, pin_assigned_name)
                
                log(f"new output pin: pin_name = {pin_name}, pin_assigned_name = {pin_assigned_name}, pin_block = {pin_block.instance_name}, pin_x = {pin_x}, pin_y = {pin_y}, pin_plus_y = {pin_plus_y}, pin_sub_ys = {pin_sub_ys}, pin_fle_blocks = {pin_fle_blocks}, atom_output_name = {atom_output_name}\n")
                new_pin = Pin(name=pin_name, 
                              assigned_name=pin_assigned_name,
                              block=pin_block,
                              x=pin_x,
                              y=pin_y,
                              plus_y=pin_plus_y,
                              sub_ys=pin_sub_ys,
                              index=index,
                              atom_output_name=atom_output_name,
                              fle_blocks=pin_fle_blocks)
                tile.add_output_pin_object(new_pin)
                all_pins[(new_pin.name, new_pin.assigned_name)] = new_pin
                if atom_output_name:
                    atom_output_name2top_most_pin[atom_output_name] = new_pin
                else:
                    log(f'no atom_output_name ({atom_output_name}) for pin name = {new_pin.name}, assigned_name = {new_pin.assigned_name}\n')
# def make_pins()


# find Nets by looking at top blocks inputs
def make_nets():
    global net2edges_pin_obj
    global net2edges_coor
    # global src_dst_same_coor

    for block in top_blocks:
        log(f'looking at block: {block.instance_name}:\n')
        for input_object in block.input_pin_objects:
            pin_assigned_name = input_object.assigned_name
            dst_x = input_object.x
            dst_y = input_object.y
            log(f'\tlooking at input_object: {pin_assigned_name} ({dst_x}, {dst_y}):\n')
            
            if pin_assigned_name in atom_output_name2block:
                plane_num = -1
                log(f'\tpin_assigned_name = {pin_assigned_name} is in atom_output_name2block.\n')
                src_block = atom_output_name2block[pin_assigned_name]
                log(f'\t\tsrc_block from atom_output_name2block: name = {src_block.name}, instance_name = {src_block.instance_name}\n')
                
                if src_block.base_x == block.base_x and src_block.base_y == block.base_y:
                    log(f'\t\tsrc_block is connect to the same block input. \n')
                    log(f'\t\t\t src_block name = {src_block.name}, instance_name = {src_block.instance_name}\n')
                    log(f'\t\t\t dst_block name = {block.name}, instance_name = {block.instance_name}\n')
                    log(f'\t\t\t at ({src_block.base_x},{src_block.base_y})\n')
                    log(f'\t\t\tfinding fle# from atom2info: \n')
                    if src_block in atom2info:
                        (_, _, FLE_num, _) = atom2info[src_block]
                        plane_num = FLE_num
                        log(f'\t\t\tfle# = {plane_num} \n')
                    else:
                        log(f'\t\tERROR: src_block is not in atom2info.\n')
                        exit()

                    # for sub_y in input_object.sub_ys:
                    #     log(f'\t\t\tadding (src_plane#, dst_plane#) --> ({plane_num},{sub_y}) tuple for coordinate ({src_block.base_x},{src_block.base_y})\n')
                    #     src_dst_same_coor.setdefault((src_block.base_x, src_block.base_y), []).append((plane_num, sub_y))
                else:
                    if pin_assigned_name in atom_output_name2top_most_pin:
                        output_object = atom_output_name2top_most_pin[pin_assigned_name]
                        log(f'\t\tatom name found on output_object ({output_object}): name = {output_object.name}, assigned_name = {output_object.assigned_name}, plane# = {output_object.sub_ys[0]}\n')
                        plane_num = output_object.sub_ys[0]
                    else:
                        log(f'\t\tERROR: pin_assigned_name = {pin_assigned_name} is not in atom_output_name2top_most_pin.\n')
                        exit()
                src_x = src_block.base_x
                src_y = src_block.base_y
                net_name = pin_assigned_name
                edge = (plane_num, src_x, src_y, input_object, dst_x, dst_y)
                log(f'\t\tedge = (plane_num= {plane_num}, src_x = {src_x}, src_y = {src_y}, input_object = {input_object}, dst_x = {dst_x}, dst_y = {dst_y})\n')
                net2edges_pin_obj.setdefault(net_name, []).append(edge)
                net2edges_coor.setdefault(net_name, set()).add((src_x, src_y, dst_x, dst_y))
            
            else:
                log(f'\t\tERROR: pin_assigned_name = {pin_assigned_name} is not in atom_output_name2block.\n')
                exit()
# def make_nets():


# find FLEs inside a CLB that has internal connection
def find_internal_FLE_connection():
    log('inside find_internal_FLE_connection:\n')
    global src_dst_same_coor
    for tile in top_blocks:
        if 'clb' in tile.instance_name:
            log(f'\t looking at {tile.instance_name}\n')
            
            # find CLB_LR
            clb_lr = None
            if tile.sub_blocks:
                clb_lr = tile.sub_blocks[0]
                if not clb_lr.instance_name.startswith('clb_lr'):
                    log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
                    exit()
            else:
                log(f"ERROR: clb: {tile.instance_name} does not have clb_lr\n")
                exit()
            
            for input_port in clb_lr.inputs: # all input ports of clb_lr
                (_, port_name, pin_list) = input_port
                for index, pin in enumerate(pin_list): # all pins on each input port of clb_lr
                    if 'clb_lr' in pin: # input pin comming from clb_lr_out
                        log(f'\t\t clb_lr in pin: {pin} at index: {index}\n')
                        out_index = int(pin.split('[')[-1].split(']')[0])
                        log(f'\t\t out_index = {out_index}\n')
                        src_fle_num = int(out_index / 3) 
                        log(f'\t\t src_fle_num = {src_fle_num}\n')
                        dst_fle_num = int(index % MAX_FLE)
                        log(f'\t\t dst_fle_num = {dst_fle_num}\n')
                        src_dst_same_coor.setdefault((tile.base_x, tile.base_y), []).append((src_fle_num, dst_fle_num))
                        log(f'\t\t\tadding (src_fle_num, dst_fle_num) --> ({src_fle_num},{dst_fle_num}) tuple for coordinate ({tile.base_x},{tile.base_y})\n')
# def find_internal_FLE_connection()


# re-order FLEs in a CLB
# this routine has a lot of hard-coded parts and dirty codes. In the future, we should parse vpr.xml and use it here. 
def reorder_FLEs(graph):
    coor2planes = {} # (x, y) --> (prev_plane, next_plane)
    coor2FLEs   = {} # (x, y) --> (prev_fle, next_fle)
    
    # find the nodes that their plane# has changed
    for node in graph:
        (x, y) = (graph.nodes[node]['x'], graph.nodes[node]['y'])
        prev_plane = int(node.split(',')[-1])
        next_plane = int(graph.nodes[node]['plane'])
        if coor2tile[(x,y)].instance_name.startswith('clb') and prev_plane != next_plane:
            coor2planes.setdefault((x, y), []).append((prev_plane, next_plane))

    # Find the corresponding FLEs and initilize next_fle for them. It only has a different instance_name. 
    for (x,y), planes in coor2planes.items():
        clb = coor2tile[(x,y)]
        log(f'Find next FLEs\n')
        log(f'looking at clb: {clb.instance_name}\n')
        
        # find CLB_LR
        clb_lr = None
        if clb.sub_blocks:
            clb_lr = clb.sub_blocks[0]
            if not clb_lr.instance_name.startswith('clb_lr'):
                log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
                exit()
        else:
            log(f"ERROR: clb: {clb.instance_name} does not have clb_lr\n")
            exit()

        log(f'clb_lr: {clb_lr.instance_name}\n')
        # find previous FLE store a copy of that as the next FLE
        log(f'find previous FLE --> store a copy of that as the next FLE\n')
        for prev_plane, next_plane in planes:
            prev_fle = None
            next_fle = None
            for fle in clb_lr.sub_blocks:
                if fle.instance_name == f'fle[{prev_plane}]':
                    prev_fle = fle
            next_fle = copy.deepcopy(prev_fle)
            next_fle.instance_name = f'fle[{next_plane}]'
            log(f'\tprev_fle: name = {prev_fle.name}, instance_name = {prev_fle.instance_name}\n')
            log(f'\tnext_fle: name = {next_fle.name}, instance_name = {next_fle.instance_name}\n')
            coor2FLEs.setdefault((x, y), []).append((prev_fle, next_fle))
    
    # Now we should change 4 things:
        # 1. FLE inputs name
        # 2. CLB_LR outputs order and name
        # 3. CLB outputs order and name
        # 4. CLB_LR inputs order (and sometimes their name if the input comes from CLB_LR outputs)
    for (x,y), FLEs in coor2FLEs.items():
        clb = coor2tile[(x,y)]
        clb_lr = clb.sub_blocks[0]
        clb_lr_in_map = dict() # next_index --> prev_index
        clb_lr_out_map = dict() # prev_index --> next_index
        
        log(f'Modifications started:\n')
        log(f'looking at clb: {clb.instance_name}\n')
        log(f'clb_lr: {clb_lr.instance_name}\n')
        # 1. modifying FLE inputs
        log(f'modifying FLE inputs\n')
        for ii, (prev_fle, next_fle) in enumerate(FLEs):
            prev_plane = int(prev_fle.instance_name.split('[')[-1].split(']')[0])
            next_plane = int(next_fle.instance_name.split('[')[-1].split(']')[0])
            log(f'\tprev_plane = {prev_plane}\n')
            log(f'\tnext_plane = {next_plane}\n')
            
            for jj, input_port in enumerate(next_fle.inputs):
                (tag, port_name, pin_list) = input_port
                for index, pin in enumerate(pin_list):
                    if pin == 'open':
                        continue
                    log(f'\t\tpin = {pin}\n')
                    prev_clb_lr_index = int(pin.split('[')[-1].split(']')[0])
                    next_clb_lr_index = ''
                    next_direct_index = ''
                    
                    # different ports have different naming for their pins. Refer to vpr.xml for more info.

                    # <port name="in">
                    if 'clb_lr.in' in pin:
                        next_clb_lr_index = prev_clb_lr_index - (prev_plane - next_plane)
                        next_direct_index = int(next_clb_lr_index / MAX_FLE)
                        pin_list[index] = f"{pin.split('[')[0]}[{next_clb_lr_index}]{pin.split(']')[-1][:-1]}{next_direct_index}"
                        clb_lr_in_map[next_clb_lr_index] = prev_clb_lr_index

                    # <port name="reset">
                    elif 'clb_lr.reset' in pin:
                        next_clb_lr_index = prev_clb_lr_index
                        next_direct_index = next_plane
                        pin_list[index] = f"{pin.split('[')[0]}[{next_clb_lr_index}]{pin.split(']')[-1][:-1]}{next_direct_index}"
                    
                    # <port name="enable">
                    elif 'clb_lr.enable' in pin:
                        next_clb_lr_index = index + int(next_plane / 4)
                        next_direct_index = next_plane
                        pin_list[index] = f"{pin.split('[')[0]}[{next_clb_lr_index}]{pin.split(']')[-1][:-1]}{next_direct_index}"
                    
                    # <port name="cin">
                    # From vpr.xml: <direct name="carry_link" input="fle[0:6].cout" output="fle[1:7].cin">
                    # Note: There is an assumption here: we are not working on a CLB that its first FLE touches CLB's cin port. 
                    # that is why 'cout' is shown in the pin name --> cout comes from one FLE before the current one. 
                    elif 'cout' in pin:
                        next_clb_lr_index = next_plane - 1
                        pin_list[index] = f"{pin.split('[')[0]}[{next_clb_lr_index}]{pin.split(']')[-1]}"
                    
                    # <port name="sc_in">
                    # <direct name="scff_link" input="fle[6:0].sc_out" output="fle[7:1].sc_in"/>
                    elif 'sc_out' in pin or 'clb_lr.sc_in' in pin:
                        if next_plane != 0:
                            next_clb_lr_index = next_plane - 1
                            pin_list[index] = f"{pin.split('[')[0]}[{next_clb_lr_index}]{pin.split(']')[-1]}"
                        else:
                            # <direct name="scff_in" input="clb_lr.sc_in" output="fle[0].sc_in"/>
                            pin_list[index] = 'clb_lr.sc_in[0]->carry_in'


                    
                    log(f'\t\tprev_clb_lr_index = {prev_clb_lr_index}\n')
                    log(f'\t\tnext_clb_lr_index = {next_clb_lr_index}\n')
                    log(f'\t\tnext_direct_index = {next_direct_index}\n')
                    log(f'\t\tnew pin name = {pin_list[index]}\n')
                next_fle.inputs[jj] = (tag, port_name, pin_list)
            FLEs[ii] = (prev_fle, next_fle)
        coor2FLEs[x,y] = FLEs
        
        # 2. re-ordering and renaming clb_lr outputs
        log(f're-ordering and renaming clb_lr outputs\n')
        for ii, output_port in enumerate(clb_lr.outputs):
            (tag, port_name, pin_list) = output_port
            if port_name == 'out':
                pin_list_copy = [-1] * 24
                for index, pin in enumerate(pin_list):
                    if pin == 'open':
                        continue
                    # fle[0].out[0]-&gt;direct_out0_0
                    fle_num = int(pin.split(']')[0].split('[')[-1]) # find the current FLE#
                    for prev_fle, next_fle in FLEs:
                        prev_plane = int(prev_fle.instance_name.split('[')[-1].split(']')[0])
                        next_plane = int(next_fle.instance_name.split('[')[-1].split(']')[0])
                        if fle_num == prev_plane: # FLE# is found --> must be changed to the new one
                            log(f'\t\t fle_num({fle_num}) == prev_plane({prev_plane})\n')
                            next_index = index - (3 * (prev_plane - next_plane)) # for each FLE, we got three outputs: out[1:0] and o6 (refer to vpr.xml)
                            log(f'\t\t index = {index}, next_index = {next_index}\n')
                            clb_lr_out_map[index] = next_index # create a map for these outputs. This map would be useful in step 3 if the input of clb_lr comes from an output of clb_lr
                            log(f'\t\t pin = {pin}\n')
                            # Example: fle[1].out[0]->direct_out0_2
                            # build the new pin name
                            next_pin_name = f"{pin.split('[')[0]}[{next_plane}]{pin.split(']')[1]}]{pin.split(']')[-1][:-1]}{next_plane}"
                            log(f'\t\t next_pin_name = {next_pin_name}\n')
                            pin_list_copy[next_index] = next_pin_name
                            # make the previous position of the pin 'open' only if it is not occupied before
                            pin_list_copy[index] = 'open' if pin_list_copy[index] == -1 else pin_list_copy[index]
                log(f'\toutput pin_list_copy:\n')
                # for the rest of the pins that are not visited, assign the previous value from the original list: pin_list
                for index, pin in enumerate(pin_list_copy):
                    if pin == -1:
                        pin_list_copy[index] = pin_list[index]
                    log(f'\t\tindex = {index},\t prev_pin = {pin_list[index]},\t curr_pin = {pin_list_copy[index]}\n')
                clb_lr.outputs[ii] = (tag, port_name, pin_list_copy)
                break
        # just for debugging.
        if debug:
            for output_port in clb_lr.outputs:
                (_, port_name, pin_list) = output_port
                if port_name == 'out':
                    log(f'\tclb_lr output pin_list = {pin_list}\n')
                    break

        # 3. re-ordering and renaming clb outputs
        log(f're-ordering and renaming clb outputs\n')
        for ii, output_port in enumerate(clb.outputs):
            (tag, port_name, pin_list) = output_port
            if port_name == 'O0':
                pin_list_copy = [-1] * 24
                for index, pin in enumerate(pin_list):
                    if pin == 'open':
                        continue
                    # clb_lr[0].out[2]->clbouts1
                    out_index = int(pin.split('[')[-1].split(']')[0]) # find the output index
                    log(f'\tpin = {pin},\tout_index = {out_index}\n')
                    next_index = -1
                    next_pin_name = ''
                    if out_index in clb_lr_out_map:
                        next_index = clb_lr_out_map[out_index]
                        # example: clb_lr[0].out[2]->clbouts1
                        part1 = pin.split(']')[0] # clb_lr[0
                        part2 = pin.split(']')[1].split('[')[0] # .out
                        part3 = next_index # next index
                        part4 = pin.split(']')[-1][:-1] # ->clbouts
                        part5 = int(next_index / 8) + 1
                        next_pin_name = f"{part1}]{part2}[{part3}]{part4}{part5}"
                        log(f'\tnext_index = {next_index},\tnext_name = {next_pin_name}\n')
                        pin_list_copy[next_index] = next_pin_name
                        # make the previous position of the pin 'open' only if it is not occupied before
                        # pin_list_copy[index] = 'open' if pin_list_copy[index] == -1 else pin_list_copy[index]
                log(f'\toutput pin_list_copy:\n')
                # for the rest of the pins that are not visited, assign the previous value from the original list: pin_list
                for index, pin in enumerate(pin_list_copy):
                    if pin == -1:
                        pin_list_copy[index] = pin_list[index] if index not in clb_lr_out_map else 'open'
                    log(f'\t\tindex = {index},\t prev_pin = {pin_list[index]},\t curr_pin = {pin_list_copy[index]}\n')
                clb.outputs[ii] = (tag, port_name, pin_list_copy)
                break
        # just for debugging.
        if debug:
            for output_port in clb.outputs:
                (_, port_name, pin_list) = output_port
                if port_name == 'O0':
                    log(f'\tclb output pin_list = {pin_list}\n')
                    break

        
        # 4. re-ordering (and renaming) the clb_lr inputs
        log(f're-ordering (and renaming) the clb_lr inputs\n')
        log(f"clb_lr_in_map = {clb_lr_in_map}\n")
        for  ii, input_port in enumerate(clb_lr.inputs):
            (tag, port_name, pin_list) = input_port
            if port_name == 'in':
                pin_list_copy = [-1] * 48
                for index, pin in enumerate(pin_list):
                    
                    if index in clb_lr_in_map: # it is in the map --> assign new name
                        pin_list_copy[index] = pin_list[clb_lr_in_map[index]]
                        if pin == 'open': # if the previous name was 'open' --> same position in pin_list_copy must be 'open' as well
                            # find the final destination for the pin to be 'open'. For example: 4 --> 3, 3 --> 2. if 4 is originally open, 2 must be open in new pin list
                            new_index = index
                            new_open_pin = clb_lr_in_map[new_index]
                            while new_open_pin in clb_lr_in_map:
                                log(f'\t\t\t looping in clb_lr_in_map:\n')
                                log(f'\t\t\t\t new_index = {new_index}\n')
                                log(f'\t\t\t\t new_open_pin = {new_open_pin}\n')
                                new_index = new_open_pin
                                new_open_pin = clb_lr_in_map[new_open_pin]
                            log(f'\t\tpin == open ==> a position in pin_list_copy must be open as well\n')
                            log(f'\t\t\t --> pin_list_copy[{clb_lr_in_map[new_index]}] ({pin_list_copy[clb_lr_in_map[new_index]]}) = open\n')
                            pin_list_copy[clb_lr_in_map[new_index]] = 'open'
                        log(f'\t\t index({index}) in clb_lr_in_map --> pin_list[{index}]: {pin_list[index]} replaced with --> pin_list[{clb_lr_in_map[index]}]: {pin_list_copy[index]}\n')
                    
                    else: # it is not in the map --> keep whatever we have
                        pin_list_copy[index] = pin if pin_list_copy[index] == -1 else pin_list_copy[index]
                        log(f'\t\t index({index}) NOT in clb_lr_in_map --> pin_list_copy[{index}] = {pin_list_copy[index]}\n')
                    
                    # this means that the clb_lr input comes from clb_lr outputs. In case we have it in clb_lr_out_map, we need to rename the input
                    if 'clb_lr' in pin_list_copy[index] and int(pin_list_copy[index].split('[')[-1].split(']')[0]) in clb_lr_out_map:
                        # clb_lr[0].out[8]-&gt;crossbar2
                        log(f'\t\t clb_lr pin: {pin_list_copy[index]}\n')
                        prev_index = int(pin_list_copy[index].split('[')[-1].split(']')[0])
                        log(f'\t\t prev_index: {prev_index}\n')
                        next_crossbar_num = int(clb_lr_out_map[prev_index] / 4) # refer to vpr.xml for this
                        log(f'\t\t next_index: {clb_lr_out_map[prev_index]}\n')
                        log(f'\t\t next_crossbar_num: {next_crossbar_num}\n')
                        # based on next_crossbar_num, I should re-order the new pin to another place on CLB_LR inputs (limitation of crossbars for CLB_LR outputs)
                        # I know! This is too ugly. Did not have time for a clean one
                        # example: clb_lr[0].out[14]->crossbar3
                        part1 = pin_list_copy[index].split(']')[0] # clb_lr[0
                        part2 = pin_list_copy[index].split(']')[1].split('[')[0] # .out
                        part3 = clb_lr_out_map[prev_index] # next index
                        part4 = pin_list_copy[index].split(']')[-1][:-1] # ->crossbar
                        part5 = next_crossbar_num
                        pin_name = f"{part1}]{part2}[{part3}]{part4}{part5}"
                        log(f'\t\t new pin_name: {pin_name}\n')
                        new_index = next_crossbar_num * MAX_FLE + (index % MAX_FLE)
                        log(f'\t\t new index to insert the pin on CLB_LR: {new_index}\n')
                        if pin_list_copy[new_index] not in (-1, 'open'):
                            log(f"ERROR: cannot insert the pin at this new index\n")
                            exit()
                        
                        # pin_list_copy[index] = pin_name
                        pin_list_copy[new_index] = pin_name
                log(f'\tinput pin_list_copy:\n')
                for index, pin in enumerate(pin_list_copy):
                    log(f'\t\tindex = {index},\t prev_pin = {pin_list[index]},\t curr_pin = {pin}\n')
                clb_lr.inputs[ii] = (tag, port_name, pin_list_copy)
                break
        # just for debugging.
        if debug:
            for input_port in clb_lr.inputs:
                (_, port_name, pin_list) = input_port
                if port_name == 'in':
                    log(f'\tclb_lr input pin_list = {pin_list}\n')
                    break
    
    # update the CLB_LRs
    log('update the CLB_LRs\n')
    for (x,y), FLEs in coor2FLEs.items():
        clb = coor2tile[(x,y)]
        clb_lr = clb.sub_blocks[0]
        log(f'\tlooking at clb: {clb.instance_name}\n')
        log(f'\tclb_lr: {clb_lr.instance_name}\n')
        FLEs.sort(key=lambda x: x[1].instance_name)
        log(f'\tFLEs after sort: {FLEs}\n')
        for i, fle in enumerate(clb_lr.sub_blocks):
            log(f'\t\ti:{i}, fle.name = {fle.name}, fle.instance_name = {fle.instance_name}\n')
            occupied = False
            for prev_fle, next_fle in FLEs:
                log(f'\t\t\tprev_fle: name = {prev_fle.name}, instance_name = {prev_fle.instance_name}, next_fle: name = {next_fle.name}, instance_name = {next_fle.instance_name}\n')
                if fle.instance_name == next_fle.instance_name:
                    # if fle.name == 'open' and int(next_fle.instance_name.split(']')[0].split('[')[-1]) == i:
                    clb_lr.sub_blocks[i] = next_fle
                    occupied = True
                    log(f'\t\t\toccupied = True\n')
                    # else:
                    #     if not fle.name == 'open':
                    #         clb_lr.sub_blocks[i] = next_fle
                    #         occupied = True
                    #         log(f'\t\t\toccupied = True\n')
                    # if clb_lr.sub_blocks[i].name == 'open': clb_lr.sub_blocks[i].instance_name = f'fle[{i}]'
                    # if occupied:
                    log(f'\t\t\tclb_lr.sub_blocks[{i}] = next_fle: name = {next_fle.name}, instance_name = {next_fle.instance_name}\n')
                    break
            # for
            if not occupied: # it is the same as before
                log(f'\t\toccupied = False:\n')
                clb_lr.sub_blocks[i] = fle
                log(f'\t\tclb_lr.sub_blocks[{i}] = fle:{fle}\n')
                clb_lr.sub_blocks[i].instance_name = f'fle[{i}]'
                log(f'\t\tclb_lr.sub_blocks[{i}].instance_name = fle[{i}]\n')
        # probably no need for this
        clb_lr.sub_blocks.sort(key=lambda x: x.instance_name)
# def reorder_FLEs(graph)

# print xml file
def print_xml(root, address):
    content = etree.tostring(root, pretty_print=True, xml_declaration=True)
    out(content, address)
# def print_xml(root, address)


def get_block_info(element, parent_block):
    global atom2info
    global all_blocks
    global atom_output_name2block

    name = element.get("name", "")
    instance_name = element.get("instance", "")
    mode = element.get("mode", "")

    base_x = base_y = slot_num = -1
    if not parent_block.base_x == -1:
        base_x      = parent_block.base_x
        base_y      = parent_block.base_y
        slot_num    = parent_block.slot_num
    else:
        if name in block2info:
            (base_x,base_y,slot_num,_,_) = block2info[name]
    
    if base_x == -1 or base_y == -1 or slot_num == -1:
        log(f"ERROR: base_x or base_y or slot_num for {name} is -1: {base_x}, {base_y}, {slot_num}\n")
        log(f"{block2info}\n")
        exit()


    inputs = []
    for input in element.findall("inputs/*"):
        port_tag = input.tag
        port_name = input.get("name", "")
        pins = input.text.split()
        new_port = (port_tag, port_name, pins)
        inputs.append(new_port)

    outputs = []
    for output in element.findall("outputs/*"):
        port_tag = output.tag
        port_name = output.get("name", "")
        pins = output.text.split()
        new_port = (port_tag, port_name, pins)
        outputs.append(new_port)

    clocks = []
    for clock in element.findall("clocks/*"):
        port_tag = clock.tag
        port_name = clock.get("name", "")
        pins = clock.text.split()
        new_port = (port_tag, port_name, pins)
        clocks.append(new_port)

    attributes = []
    for attribute in element.findall("attributes/*"):
        attribute_tag = attribute.tag
        attribute_name = attribute.get("name", "")
        attribute_val = attribute.text
        new_attribute = (attribute_tag, attribute_name, attribute_val)
        attributes.append(new_attribute)
    
    parameters = []
    for parameter in element.findall("parameters/*"):
        parameter_tag = parameter.tag
        parameter_name = parameter.get("name", "")
        parameter_val = parameter.text
        new_parameter = (parameter_tag, parameter_name, parameter_val)
        parameters.append(new_parameter)
    
    new_block = Block(
        name = name,
        instance_name = instance_name,
        mode = mode,
        inputs = inputs,
        outputs = outputs,
        clocks = clocks,
        attributes = attributes,
        parameters = parameters,
        parent_block=parent_block,
        base_x=base_x,
        base_y=base_y,
        slot_num=slot_num
    )

    # Recursively fill sub_blocks
    for sub_block in element.findall("block"):
        sub_block_obj = get_block_info(element=sub_block, parent_block=new_block)
        new_block.add_sub_block(sub_block_obj)

    if not new_block.sub_blocks and not name == 'open':
        new_block.set_is_atom(is_atom=True)
        log(f"new_block is atom: {new_block.name}\n")
        if not new_block.outputs: #io blocks have no output port. Only the name of the block is enough
            atom_output_name2block[new_block.name] = new_block
            log(f"atom_output_name2block[{new_block.name}] = {new_block}\n")

        for output_port in new_block.outputs:
            (_, port_name, pin_list) = output_port
            log(f"\tpin_list: {pin_list}\n")
            # if len(pin_list) > 1: # some atoms have more than one output (e.g., RS_DSP_MULT_REGIN_REGOUT)
            for pin in pin_list:
                if not pin == 'open':
                    atom_output_name2block[pin] = new_block
                    log(f"atom_output_name2block[{pin}] = {new_block}\n")
            # else: # clb's atoms like lut has only one output and the name of atom is same as the name of output
            #     atom_output_name2block[new_block.name] = new_block
            #     log(f"atom_output_name2block[{new_block.name}] = {new_block}\n")

        # find fle# for atoms
        curr_block = new_block
        while not 'fle' in curr_block.instance_name:
            curr_block = curr_block.parent_block
            if curr_block is None:
                break
        if curr_block is not None:
            log(f"found fle for an atom: {new_block.instance_name} --> {curr_block.instance_name}\n")
            atom2info[new_block] = (base_x, base_y, int(curr_block.instance_name.split('[')[1].split(']')[0]), curr_block)
            log(f"atom2info[{new_block}] = {(base_x, base_y, int(curr_block.instance_name.split('[')[1].split(']')[0]), curr_block)}\n")
    
    all_blocks.append(new_block)
    return new_block
# def get_block_info(element)


def parse_net_file(address):
    global top_net_addr
    global top_instance
    global top_architecture_id
    global top_atom_netlist_id
    global top_inputs
    global top_outputs
    global top_clocks
    global top_blocks
    global coor2tile
    global all_blocks
    
    parser = etree.XMLParser(remove_comments=True, remove_blank_text=True)
    tree = etree.parse(address, parser)
    root = tree.getroot()

    top_net_addr            = root.get("name")
    top_instance            = root.get("instance")
    top_architecture_id     = root.get("architecture_id")
    top_atom_netlist_id     = root.get("atom_netlist_id")
    top_inputs = root.find("inputs").text.strip().split() if (root.find("inputs") is not None and root.find("inputs").text is not None) else ""
    top_outputs = root.find("outputs").text.strip().split() if (root.find("outputs") is not None and root.find("outputs").text is not None) else ""
    top_clocks = root.find("clocks").text.strip().split() if (root.find("clocks") is not None and root.find("clocks").text is not None) else ""
    log(f"top_inputs = {top_inputs}\n")
    log(f"top_outputs = {top_outputs}\n")
    log(f"top_clocks = {top_clocks}\n")
    root_block = Block(name=top_net_addr, instance_name=top_instance, inputs=top_inputs, outputs=top_outputs, clocks=top_clocks)
    all_blocks.append(root_block)
    for sub_block in root.findall("block"):
        top_block = get_block_info(sub_block, root_block)
        top_block.set_is_tile_block(True)
        top_blocks.append(top_block)
        coor2tile[(top_block.base_x, top_block.base_y)] = top_block
# def parse_net_file(address)


# recursively create the xml tags for each top_block and its sub-blocks
def write_block(parent, block):
    block_element = etree.SubElement(parent, "block")
    block_element.set("name", block.name)
    if block.instance_name:
        block_element.set("instance", block.instance_name)
    if block.mode:
        block_element.set("mode", block.mode)

    if block.name != 'open':
        attributes_element = etree.SubElement(block_element, "attributes")
        for (attribute_tag, attribute_name, attribute_val) in block.attributes:
            attribute_element = etree.SubElement(attributes_element, attribute_tag)
            attribute_element.set("name", attribute_name)
            attribute_element.text = attribute_val

    if block.name != 'open':
        parameters_element = etree.SubElement(block_element, "parameters")
        for (parameter_tag, parameter_name, parameter_val) in block.parameters:
            parameter_element = etree.SubElement(parameters_element, parameter_tag)
            parameter_element.set("name", parameter_name)
            parameter_element.text = parameter_val
    
    if block.name != 'open':
        inputs_element = etree.SubElement(block_element, "inputs")
        for (port_tag, port_name, pins) in block.inputs:
            port_element = etree.SubElement(inputs_element, port_tag)
            port_element.set("name", port_name)
            port_element.text = " ".join(pins)

    if block.name != 'open':
        outputs_element = etree.SubElement(block_element, "outputs")
        for (port_tag, port_name, pins) in block.outputs:
            port_element = etree.SubElement(outputs_element, port_tag)
            port_element.set("name", port_name)
            port_element.text = " ".join(pins)

    if block.name != 'open':
        clocks_element = etree.SubElement(block_element, "clocks")
        for (port_tag, port_name, pins) in block.clocks:
            port_element = etree.SubElement(clocks_element, port_tag)
            port_element.set("name", port_name)
            port_element.text = " ".join(pins)

    for sub_block in block.sub_blocks:
        write_block(block_element, sub_block)
# def write_block(parent, block)


# for each top_block in top_blocks --> calls write_block(). Then, calls print_xml to write into a .net file
def write_net_file(address):
    global top_net_addr
    top_net_addr = address
    # Create the root element
    root = etree.Element("block")
    root.set("name", top_net_addr)
    root.set("instance", top_instance)
    root.set("architecture_id", top_architecture_id)
    root.set("atom_netlist_id", top_atom_netlist_id)

    inputs_element = etree.SubElement(root, "inputs")
    inputs_element.text = " ".join(top_inputs)  # Join the list into a space-separated string

    outputs_element = etree.SubElement(root, "outputs")
    outputs_element.text = " ".join(top_outputs)  # Join the list into a space-separated string

    clocks_element = etree.SubElement(root, "clocks")
    clocks_element.text = " ".join(top_clocks)  # Join the list into a space-separated string

    for block in top_blocks:
        write_block(root, block)

    print_xml(root, address)
# write_net_file(address)
    

# Flow:
#   parse_net_file
#   get_block_info
#   make_pins
#       get_plus_y
#       get_sub_y
#       get_fle_blocks
#       get_atom_output_name
#   make_nets
#   find_internal_FLE_connection
#   reorder_FLEs
#   write_net_file
#       write_block
#       print_xml
    
    