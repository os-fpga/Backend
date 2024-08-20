import xml.etree.ElementTree as ET
import xml.dom.minidom as minidom
import sys
import json
import glob
import os


max_x = max_y = 0

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log


def read_netlist(netlist_path):
    tree = ET.parse(netlist_path)
    root = tree.getroot()

    inputs_list = None
    outputs_list = None

    inputs_tag = root.find('inputs')
    if inputs_tag is not None:
        inputs_list = inputs_tag.text.strip().split()

    if inputs_list is None:
        print("Inputs tag not found in the XML file.")
        return
    
    outputs_tag = root.find('outputs')
    if outputs_tag is not None:
        outputs_list = outputs_tag.text.strip().split()

    if outputs_list is None:
        print("Outputs tag not found in the XML file.")
        return

    return inputs_list, outputs_list


def read_verilog(hdl_path):

    inputs_list = []
    outputs_list = []

    # Open the HDL file for reading
    with open(hdl_path, 'r') as hdl_file:
        lines = hdl_file.readlines()

    # Parse the HDL file line by line
    for line in lines:

        signal_type = ""
        signal_length = ""
        signal_name = ""
        line_include_in_out = False

        # Remove leading/trailing whitespace and split the line by spaces
        tokens = line.replace('reg',' ').replace(',',' ').replace(';',' ').replace('(',' ').replace(')',' ').strip().split()
        for index, token in enumerate(tokens):
            if 'input' in token or 'output' in token:
                signal_type = token
                tokens = tokens[(index+1):]
                line_include_in_out = True
                break

        if not line_include_in_out:
            continue
        
        index = 0
        while index < len(tokens):
            token = tokens[index]
            if '[' in token: # input [7:0] a
                signal_length = token
                signal_name = tokens[index + 1]
                range_start = int(signal_length.split(':')[1].rstrip(']'))
                range_end = int(signal_length.split(':')[0].lstrip('['))
                if range_start > range_end:
                    range_start, range_end = range_end, range_start
                for i in range(range_start, range_end + 1):
                    signal = f'{signal_name}[{i}]'
                    if signal_type == 'input':
                        inputs_list.append(signal)
                    elif signal_type == 'output':
                        outputs_list.append(f'out:{signal}')
                index += 2
                continue
            else:
                signal_name = token
                if signal_type == 'input':
                    inputs_list.append(signal_name)
                elif signal_type == 'output':
                    outputs_list.append(f'out:{signal_name}')
                index += 1

    # print (inputs_list, outputs_list)
    return inputs_list, outputs_list


def read_vhdl(hdl_path):
    inputs_list = []
    outputs_list = []

    # Open the HDL file for reading
    with open(hdl_path, 'r') as hdl_file:
        lines = hdl_file.readlines()
    # Parse the HDL file line by line
    for line in lines:

        signal_type = ""
        signal_length = ""
        signal_name = ""
        line_include_in_out = False

        # Remove leading/trailing whitespace and split the line by spaces
        tokens = line.replace(':',' ').replace(';',' ').strip().split()
        for index, token in enumerate(tokens):
            if token in ['in', 'out']:
                signal_type = token
                signal_name = tokens[(index-1)]
                signal_length = tokens[(index+1):]
                line_include_in_out = True
                break
        if not line_include_in_out:
            continue
        
        if 'vector' in signal_length[0]:
            range_start = int(signal_length[2].split(')')[0])
            range_end = int(signal_length[0].split('(')[1])
            if range_start > range_end:
                range_start, range_end = range_end, range_start
            for i in range(range_start, range_end + 1):
                signal = f'{signal_name}[{i}]'
                if signal_type == 'in':
                    inputs_list.append(signal)
                elif signal_type == 'out':
                    outputs_list.append(f'out:{signal}')
        else:
            if signal_type == 'in':
                inputs_list.append(signal_name)
            elif signal_type == 'out':
                outputs_list.append(f'out:{signal_name}')

    #print(inputs_list, outputs_list)
    return inputs_list, outputs_list


def read_json(ports_path):
    # Load JSON data from the file
    with open(ports_path, 'r') as json_file:
        data = json.load(json_file)

    module_data = data[0]

    # Extract inputs and outputs
    inputs_list = []
    outputs_list = []

    for port in module_data['ports']:
        if port['direction'] == 'input':
            inputs_list.append(port['name'])
        elif port['direction'] == 'output':
            outputs_list.append(f"out:{port['name']}")

    return inputs_list, outputs_list


def read_ports(ports_path):

    # Extract inputs and outputs
    inputs_list = []
    outputs_list = []
    
    # Use the glob function to find matching files
    matching_directories = glob.glob(ports_path)
    if matching_directories:
        print(f"matching_directories: {matching_directories}\n")
    else:
        print(f"nothing\n")

    found_json = False
    found_net = False
    target_file = "post_synth_ports.json"
    for directory in matching_directories:
        print(f"directory: {directory}\n")
        for root, dirs, files in os.walk(directory):
            if target_file in files and "packing" in root:
                ports_path = os.path.join(root, target_file)
                print(f"found:\t {ports_path}\n")
                found_json = True
                inputs_list, outputs_list = read_json(ports_path)
                return inputs_list, outputs_list
    
    if found_json == False:
        print(f"no post_synth_ports.json found\n")
        target_file = ".net"
        found_net = False
        for directory in matching_directories:
            print(f"directory: {directory}\n")
            for root, dirs, files in os.walk(directory):
                net_files = [file for file in files if file.endswith('.net')]
                if net_files and "packing" in root:
                    net_file = net_files[0]
                    ports_path = os.path.join(root, net_file)
                    print(f"found:\t {ports_path}\n")
                    found_net = True
                    inputs_list, outputs_list = read_netlist(ports_path)
                    return inputs_list, outputs_list
    
    if found_net == False:
        print(f"no post_synth_ports.json or .net found\n")
        exit

    return inputs_list, outputs_list




def modify_constraint_file(golden_file_path, generated_file_path, inputs, outputs):
    tree = ET.parse(golden_file_path)
    root = tree.getroot()

    partitions = root.findall("./partition_list/partition")
    if partitions:
        for partition in partitions:
            partition_name = partition.get("name")
            if partition_name == "f2a_i":
                for output in outputs:
                    new_tag = ET.Element(f'add_atom name_pattern="{output}"')
                    partition.append(new_tag)
                for i in range(24,72):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="1" x_high="{max_x}" y_high="1" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(24,72):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="{max_y}" x_high="{max_x}" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(24,72):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="1" x_high="1" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(24,72):
                    new_tag = ET.Element(f'add_region x_low="{max_x}" y_low="1" x_high="{max_x}" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
            
            elif partition_name == "a2f_o":
                for input in inputs:
                    new_tag = ET.Element(f'add_atom name_pattern="{input}"')
                    partition.append(new_tag)
                for i in range(0,24):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="1" x_high="{max_x}" y_high="1" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(0,24):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="{max_y}" x_high="{max_x}" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(0,24):
                    new_tag = ET.Element(f'add_region x_low="1" y_low="1" x_high="1" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
                for i in range(0,24):
                    new_tag = ET.Element(f'add_region x_low="{max_x}" y_low="1" x_high="{max_x}" y_high="{max_y}" subtile="{i}"')
                    partition.append(new_tag)
                    

    # Serialize the modified XML tree to a string
    xml_string = ET.tostring(root, encoding='utf-8')

    # Use minidom to format the XML string with proper indentation and line breaks
    dom = minidom.parseString(xml_string)
    formatted_xml = dom.toprettyxml(indent="    ")

    # Write the formatted XML to the output file
    with open(generated_file_path, 'w') as output_file:
        output_file.write(formatted_xml)
    
    #tree.write(generated_file_path)


def allocate_total_coordinates(min_slot_num, max_slot_num, num_of_slots):
    middle_x = int(max_x/2)
    middle_y = int(max_y/2)
    
    max_z_per_iteration = 2
    i = 0

    direction = 1 # 1 --> go right/up, -1 --> go left/down
    distance = 0
    total_coor = []

    while True:
        

        # io_bottom
        x, y, z = middle_x, 1, (min_slot_num + i*max_z_per_iteration)
        direction = 1 # 1 --> go right/up, -1 --> go left/down
        distance = 0
        if z > max_slot_num:
            break
        
        while True:
            total_coor.append((x,y,z))
            z += 1
            # if z > max_slot_num:
            #     break
            if z == (min_slot_num + (i+1)*max_z_per_iteration):
                z = (min_slot_num + i*max_z_per_iteration)
                if direction == 1:
                    distance += 1
                x = middle_x + (direction*distance)
                direction *= -1
                if x < 1:
                    break
                if x > max_x:
                    continue
        
        # io_top
        x, y, z = middle_x, max_y, (min_slot_num + i*max_z_per_iteration)
        distance = 0
        direction = 1
        while True:
            total_coor.append((x,y,z))
            z += 1
            # if z > max_slot_num:
            #     break
            if z == (min_slot_num + (i+1)*max_z_per_iteration):
                z = (min_slot_num + i*max_z_per_iteration)
                if direction == 1:
                    distance += 1
                x = middle_x + (direction*distance)
                direction *= -1
                if x < 1:
                    break
                if x > max_x:
                    continue

        
        # io_left
        x, y, z = 1, middle_y, (min_slot_num + i*max_z_per_iteration)
        distance = 0
        direction = 1
        while True:
            total_coor.append((x,y,z))
            z += 1
            # if z > max_slot_num:
            #     break
            if z == (min_slot_num + (i+1)*max_z_per_iteration):
                z = (min_slot_num + i*max_z_per_iteration)
                if direction == 1:
                    distance += 1
                y = middle_y + (direction*distance)
                direction *= -1
                if y <= 1:
                    break
                if y >= max_y:
                    continue

        # io_right
        x, y, z = max_x, middle_y, (min_slot_num + i*max_z_per_iteration)
        distance = 0
        direction = 1
        while True:
            total_coor.append((x,y,z))
            z += 1
            # if z > max_slot_num:
            #     break
            if z == (min_slot_num + (i+1)*max_z_per_iteration):
                z = (min_slot_num + i*max_z_per_iteration)
                if direction == 1:
                    distance += 1
                y = middle_y + (direction*distance)
                direction *= -1
                if y <= 1:
                    break
                if y >= max_y:
                    continue

        
        

        
        i += 1
    
    # Print the assigned coordinates
    # print("coordinates:")
    # for coord in total_coor:
    #     print(coord)
    return total_coor

    
def create_placement_file(generated_file_path, inputs, outputs):

    num_of_in_ports = 24
    num_of_out_ports = 48

    in_ports_beg = 0
    in_ports_end = 23
    out_ports_beg = 24
    out_ports_end = 71
    
    # Check if the sizes of inputs and outputs are valid
    max_legal_inputs = 2*(max_x + max_y) * num_of_in_ports  # Max legal coordinates for inputs
    max_legal_outputs = 2*(max_x + max_y) * num_of_out_ports  # Max legal coordinates for outputs

    if len(inputs) > max_legal_inputs:
        raise ValueError("Number of inputs exceeds max legal coordinates.\n")
    if len(outputs) > max_legal_outputs:
        raise ValueError("Number of outputs exceeds max legal coordinates.\n")
    
    
    total_in_coor = allocate_total_coordinates(in_ports_beg,in_ports_end, num_of_in_ports)
    total_out_coor = allocate_total_coordinates(out_ports_beg,out_ports_end, num_of_out_ports)
    
    # Assign coordinates to inputs
    input_coordinates = []
    i = 0
    for input_name in inputs:
        input_coordinates.append((input_name,total_in_coor[i]))
        i += 1
    
    # Assign coordinates to outputs
    output_coordinates = []
    i = 0
    for output_name in outputs:
        output_coordinates.append((output_name,total_out_coor[i]))
        i += 1


    # Write the coordinates to a placement file
    with open(generated_file_path, 'w') as output_file:
        output_file.write("#Block Name   x   y   z\n")
        output_file.write("#------------ --  --  -\n")

        for block_name, (x, y, z) in input_coordinates:
            output_file.write(f"{block_name}\t{x}\t{y}\t{z}\n")

        for block_name, (x, y, z) in output_coordinates:
            output_file.write(f"{block_name}\t{x}\t{y}\t{z}\n")

    print(f"Coordinates have been written to {generated_file_path}.")
    
    # # Print the assigned coordinates
    # print("Input coordinates:")
    # for coord in input_coordinates:
    #     print(coord)

    # print("Output coordinates:")
    # for coord in output_coordinates:
    #     print(coord)

input_file = ""
golden_constraints_path = ""
output_file = ""
max_x = ""
max_y = ""

arguments = sys.argv
if len(arguments) > 1:
    for index, argument in enumerate(arguments):
        print(argument)
        if argument == "--input_file":
            input_file = arguments[index + 1]
        elif argument == "--golden_const_file":
            golden_constraints_path = arguments[index + 1]
        elif argument == "--output_file":
            output_file = arguments[index + 1]
        elif argument == "--max_x":
            max_x = int(arguments[index + 1])
        elif argument == "--max_y":
            max_y = int(arguments[index + 1])

else:
    raise ValueError("bad arguments\n")

# if input_file.endswith('.vhd'):
#     inputs, outputs = read_vhdl(input_file)
#     #modify_constraint_file(golden_constraints_path, output_file, inputs, outputs)
#     create_placement_file(output_file, inputs, outputs)
# elif input_file.endswith('.v'):
#     inputs, outputs = read_verilog(input_file)
#     #modify_constraint_file(golden_constraints_path, output_file, inputs, outputs)
#     create_placement_file(output_file, inputs, outputs)
# elif input_file.endswith('.net'):
#     inputs, outputs = read_netlist(input_file)
#     #modify_constraint_file(golden_constraints_path, output_file, inputs, outputs)
#     create_placement_file(output_file, inputs, outputs)
# else:
inputs, outputs = read_ports(input_file)
create_placement_file(output_file, inputs, outputs)
