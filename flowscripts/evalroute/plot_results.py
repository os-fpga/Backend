import os
import sys
import glob
import matplotlib.pyplot as plt
import numpy as np

# These are the lists of different infos for every testbench
testbenches = []
num_of_used_grids = []

total_available_logical_list = []
total_available_physical_list = []

used_logical_list = []

used_physical_list = []
used_physical_out_bbox_list = []
used_physical_in_bbox_list = []

used_physical_tapped_list = []
used_physical_tapped_out_bbox_list = []
used_physical_tapped_in_bbox_list = []

sneak_connections_list = []

architecture_file_name = []
architecture_file_addr = []
architecture_last_modified = []

rrg_file_name = []
rrg_file_addr = []
rrg_last_modified = []
rrg_tool_comment = []
rrg_tool_name = []
rrg_tool_version = []

routing_file_name = []
routing_file_addr = []
routing_last_modified = []
routing_placement_file = []
routing_placement_id = []
routing_array_size = []

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log


# parse each report file from each test bench, and all the info to different lists defined above --> Totally hard coded
def parse_report(file_contents):
    total_available_logical = {}
    total_available_physical = {}

    used_logical = {}

    used_physical = {}
    used_physical_out_bbox = {}
    used_physical_in_bbox = {}

    used_physical_tapped = {}
    used_physical_tapped_out_bbox = {}
    used_physical_tapped_in_bbox = {}

    sneak_connections = 0


    for line in file_contents:

        # Check if the line contains information about the architecture file
        if 'Architecture File name:' in line:
            architecture_file_name.append(line)
        elif 'Architecture File address:' in line:
            architecture_file_addr.append(line)
        elif 'Architecture File Last modified:' in line:
            architecture_last_modified.append(line)

        # Check if the line contains information about the rr graph file
        elif 'RR Graph File name:' in line:
            rrg_file_name.append(line)
        elif 'RR Graph File address:' in line:
            rrg_file_addr.append(line)
        elif 'RR Graph File Last modified:' in line:
            rrg_last_modified.append(line)

        # Check if the line contains information about the routing file
        elif 'Routing File name:' in line:
            routing_file_name.append(line)
        elif 'Routing File address:' in line:
            routing_file_addr.append(line)
        elif 'Routing File Last modified:' in line:
            routing_last_modified.append(line)
        elif 'Routing File - Placement File:' in line:
            routing_placement_file.append(line)
        elif 'Routing File - Placement ID:' in line:
            routing_placement_id.append(line)
        elif 'Routing Array Size:' in line:
            routing_array_size.append(line)
        
        # Check if the line contains information about the total avaialabe wires
        elif 'total available LOGICAL wires with the length of' in line:
            total_available_logical[(line.split()[8], line.split()[10])] = int(line.split()[-1]) # (L, X/Y) --> wire length
            
        elif 'total available PHYSICAL wires with the length of' in line:
            total_available_physical[(line.split()[8], line.split()[10])] = int(line.split()[-1]) # (L, X/Y) --> wire length
            
        

        # Check if the line contains information about the Logical wires
        elif 'total LOGICAL length of all wires in the design with the length of' in line:
            used_logical[line.split()[13]] = int(line.split()[15]) # L --> wire length
            
        
        # Check if the line contains information about the Physical wires
        elif 'total PHYSICAL length of all wires in the design with the length of' in line:
            used_physical[line.split()[13]] = int(line.split()[15]) # L --> wire length 
        elif 'total PHYSICAL length of all wires -outside- the bounding box in the design with the length of' in line:
            used_physical_out_bbox[line.split()[17]] = int(line.split()[19]) # L --> wire length
        elif 'total PHYSICAL length of all wires -inside- the bounding box in the design with the length of' in line:
            used_physical_in_bbox[line.split()[17]] = int(line.split()[19]) # L --> wire length
        elif 'total length of all sneak connections' in line:
            sneak_connections = int(line.split()[-1])
            


        # Check if the line contains information about the Physical tapped wires
        elif 'total USED PHYSICAL length of all wires in the design with the length of' in line:
            used_physical_tapped[line.split()[14]] = int(line.split()[16]) # L --> wire length
        elif 'total USED PHYSICAL length of all wires -outside- the bounding box in the design with the length of' in line:
            used_physical_tapped_out_bbox[line.split()[18]] = int(line.split()[20]) # L --> wire length
        elif 'total USED PHYSICAL length of all wires -inside- the bounding box in the design with the length of' in line:
            used_physical_tapped_in_bbox[line.split()[18]] = int(line.split()[20]) # L --> wire length
        
        elif 'Number of unique grids that their IPIN/OPIN are touched by this design:' in line:
            num_of_used_grids.append(int(line.split()[-1]))
    
    log(f"total_available_logical: {total_available_logical}\n")
    log(f"total_available_physical: {total_available_physical}\n")
    log(f"used_logical: {used_logical}\n")
    log(f"used_physical: {used_physical}\n")
    log(f"used_physical_out_bbox: {used_physical_out_bbox}\n")
    log(f"used_physical_in_bbox: {used_physical_in_bbox}\n")
    log(f"used_physical_tapped: {used_physical_tapped}\n")
    log(f"used_physical_tapped_out_bbox: {used_physical_tapped_out_bbox}\n")
    log(f"used_physical_tapped_in_bbox: {used_physical_tapped_in_bbox}\n")
    log(f"sneak_connections: {sneak_connections}\n")

    # adding all info of this testbench to their corresponding lists
    total_available_logical_list.append(total_available_logical)
    total_available_physical_list.append(total_available_physical)
    used_logical_list.append(used_logical)
    used_physical_list.append(used_physical)
    used_physical_out_bbox_list.append(used_physical_out_bbox)
    used_physical_in_bbox_list.append(used_physical_in_bbox)
    used_physical_tapped_list.append(used_physical_tapped)
    used_physical_tapped_out_bbox_list.append(used_physical_tapped_out_bbox)
    used_physical_tapped_in_bbox_list.append(used_physical_tapped_in_bbox)
    sneak_connections_list.append(sneak_connections)
# parse_report

# get the report files of all testbenches
def get_reports_content(root_dir):
    matching_dirs = glob.glob(root_dir)

    for dir_path in matching_dirs:
        for dirpath, dirnames, filenames in os.walk(dir_path):
            # Iterate over each file in the directory
            for filename in filenames:
                # Check if the file ends with .report
                if filename.endswith('.report'):
                    # get the name of the testbench
                    print(f'found the .report file: {dirpath}/{filename}')
                    testbenches.append(filename.split('.')[0])
                    # Open the file
                    with open(os.path.join(dirpath, filename), 'r', encoding='ascii') as f:
                        # Read the file contents and pass them to parse_report() 
                        file_contents = f.readlines()
                        parse_report(file_contents)
    
    # for testbench in testbenches:
    #     testbenches[testbenches.index(testbench)] += '\n Available logical wires:\n' + str(total_available_logical[testbenches.index(testbench)])
    # for testbench in testbenches:
    #     testbenches[testbenches.index(testbench)] += '\n Available physical wires:\n' + str(total_available_physical[testbenches.index(testbench)])
    for testbench in testbenches:
        testbench += '\n Grids Touched:\n' + str(num_of_used_grids[testbenches.index(testbench)])
    for testbench in testbenches:
        testbench += '\n Routing Array Size:\n' + routing_array_size[testbenches.index(testbench)].split(":")[1]
        log(f"testbench: {testbench}\n")
# get_reports_content

# plot a chart for all testbenches
def plot_all():

    
    # all bars of each testbench
    bars = {}
    num_of_wires = 0
    for length in used_logical_list[0]:
        bars[f"Logical L{length}"] = [d[length] for d in used_logical_list if length in d]
        bars[f"Physical L{length}"] = [d[length] for d in used_physical_list if length in d]
        bars[f"Physical Tapped L{length}"] = [d[length] for d in used_physical_tapped_list if length in d]
        num_of_wires += 3
    bars[f"Sneak Paths Wirelength"] = [d for d in sneak_connections_list]
    num_of_wires += 1
    log(f"plot_all - bars = {bars}\n")
    
    total_available_logical_all_wires = []
    for element in total_available_logical_list:
        sum_of_all_wires = 0
        for value in element.values():
            sum_of_all_wires += value
        total_available_logical_all_wires.append(sum_of_all_wires)
    log(f"plot_all - total_available_logical_all_wires = {total_available_logical_all_wires}\n")

    total_available_physical_all_wires = []
    for element in total_available_physical_list:
        sum_of_all_wires = 0
        for value in element.values():
            sum_of_all_wires += value
        total_available_physical_all_wires.append(sum_of_all_wires)
    log(f"plot_all - total_available_physical_all_wires = {total_available_physical_all_wires}\n")
    ################################## Raw numbers ##################################
    width = 0.5  # the width of the bars
    x = np.arange(0, num_of_wires*len(testbenches), num_of_wires)  # the labels location
    multiplier = 0
    fig, ax = plt.subplots(figsize=((len(testbenches)*3), 12))

    for attribute, measurement in bars.items():
        offset = width * multiplier # move x lable after ploting each bar
        percentage = 0
        percentage_formatted = 0.00

        if attribute.startswith("Logical"):
            percentage = [m / total_available_logical_all_wires[index] * 100 for index, m in enumerate(measurement)]
        elif attribute.startswith("Physical"):
            percentage = [m / total_available_physical_all_wires[index] * 100 for index, m in enumerate(measurement)]
        elif attribute.startswith("Sneak"):
            percentage = [m / total_available_physical_all_wires[index] * 100 for index, m in enumerate(measurement)]
        percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]

        rects = ax.bar(x + offset, percentage_formatted, width, label=attribute)
        ax.bar_label(rects, label_type='center', fontsize=4)
        multiplier += 1



    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Length usage (%)')
    ax.set_xticks(x + ((num_of_wires/2) - width)*width, testbenches)
    ax.set_title("Node statistics")
    ax.legend()

    # resize the figure to have enough space for info
    fig.set_size_inches((len(testbenches)*3), 14)

    # resize the plot so that it does not enlarge as the figure does
    fig.subplots_adjust(left=0.1, bottom=0.15, right=0.95, top=0.9)

    #plt.show()
    fig.savefig("node_based_all_raw.pdf")
    ################################## Raw numbers ##################################

    ################################## Scaled numbers ##################################
    width = 0.5  # the width of the bars
    x = np.arange(0, num_of_wires*len(testbenches), num_of_wires)  # the labels location
    multiplier = 0
    fig, ax = plt.subplots(figsize=((len(testbenches)*3), 12))

    for attribute, measurement in bars.items():
        offset = width * multiplier # move x lable after ploting each bar
        percentage = 0
        percentage_formatted = 0.00

        if attribute.startswith("Logical"):
            percentage = [m / (total_available_logical_all_wires[index] * (num_of_used_grids[index]/ (int(routing_array_size[index].split(' ')[3])*int(routing_array_size[index].split(' ')[5])))) * 100 for index, m in enumerate(measurement)]
        elif attribute.startswith("Physical"):
            percentage = [m / (total_available_physical_all_wires[index] * (num_of_used_grids[index]/ (int(routing_array_size[index].split(' ')[3])*int(routing_array_size[index].split(' ')[5])))) * 100 for index, m in enumerate(measurement)]
        elif attribute.startswith("Sneak"):
            percentage = [m / (total_available_physical_all_wires[index] * (num_of_used_grids[index]/ (int(routing_array_size[index].split(' ')[3])*int(routing_array_size[index].split(' ')[5])))) * 100 for index, m in enumerate(measurement)]
        percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]
        

        rects = ax.bar(x + offset, percentage_formatted, width, label=attribute)
        ax.bar_label(rects, label_type='center', fontsize=4)
        multiplier += 1

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Length usage (%)')
    ax.set_xticks(x + ((num_of_wires/2) - width)*width, testbenches)
    ax.set_title("Node statistics (Total avaialable wires are scaled)")
    ax.legend()

    # resize the figure to have enough space for info
    fig.set_size_inches((len(testbenches)*3), 14)

    # resize the plot so that it does not enlarge as the figure does
    fig.subplots_adjust(left=0.1, bottom=0.15, right=0.95, top=0.9)

    #plt.show()
    fig.savefig("node_based_all_scaled.pdf")
    ################################## Scaled numbers ##################################
#plot_all

# plot a chart for each testbench
def plot_this():
    for testbench in testbenches:
        total_available_logical = total_available_logical_list[testbenches.index(testbench)]
        total_available_physical = total_available_physical_list[testbenches.index(testbench)]
        used_logical = used_logical_list[testbenches.index(testbench)]
        used_physical = used_physical_list[testbenches.index(testbench)]
        used_physical_out_bbox = used_physical_out_bbox_list[testbenches.index(testbench)]
        used_physical_in_bbox = used_physical_in_bbox_list[testbenches.index(testbench)]
        used_physical_tapped = used_physical_tapped_list[testbenches.index(testbench)]
        used_physical_tapped_out_bbox = used_physical_tapped_out_bbox_list[testbenches.index(testbench)]
        used_physical_tapped_in_bbox = used_physical_tapped_in_bbox_list[testbenches.index(testbench)]
        sneak_connections = sneak_connections_list[testbenches.index(testbench)]

        logical_bars = []
        logical_wires = []

        physical_bars = []
        physical_wires = {}

        for length, value in used_logical.items():
            logical_bars.append(f"Used Logical L{length}\n ({value})")
            logical_wires.append(value)

        for length, value in used_physical.items():
            physical_bars.append(f"Used Physical L{length}\n ({value})")
            physical_bars.append(f"Used Tapped Physical L{length}\n ({used_physical_tapped[length]})")
        
        combined_dict = {} #  merging two dictionaries into one.
        for key, value in used_physical_out_bbox.items():
            combined_dict[f"{key}"] = value
            combined_dict[f"{key}_tapped"] = used_physical_tapped_out_bbox[key]
        physical_wires["Physical wires outside bbox"] = np.array(list(combined_dict.values()))

        combined_dict2 = {} #  merging two dictionaries into one.
        for key, value in used_physical_in_bbox.items():
            combined_dict2[f"{key}"] = value
            combined_dict2[f"{key}_tapped"] = used_physical_tapped_in_bbox[key]
        physical_wires["Physical wires inside bbox"] = np.array(list(combined_dict2.values()))
        
        log(f"logical_bars: {logical_bars}\n")
        log(f"logical_wires: {logical_wires}\n")
        log(f"physical_bars: {physical_bars}\n")
        log(f"physical_wires: {physical_wires}\n")
        ################################## Raw numbers ##################################
        width = 0.4  # the width of the bars
        bottom = np.zeros(len(physical_bars)) # for stacked bars
        # multiplier = 0
        # offset = width * multiplier
        #multiplier += 1
        
        fig, ax = plt.subplots(figsize=(24, 12), dpi=300) # size of the subplot and its dpi (different from the size of figure)

        total_available_logical_all_wires = 0
        for value in total_available_logical.values():
            total_available_logical_all_wires += value
        
        percentage = [m / total_available_logical_all_wires * 100 for m in logical_wires]
        percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]
        rects = ax.bar(logical_bars, percentage_formatted, width, label="Logical wires") # plot the logical bars first
        ax.bar_label(rects, label_type='center')

        # plot the pysical bars after
        
        total_available_physical_all_wires = 0
        for value in total_available_physical.values():
            total_available_physical_all_wires += value

        for attribute, measurement in physical_wires.items():
            percentage = [m / total_available_physical_all_wires * 100 for m in measurement]
            percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]
            rects = ax.bar(physical_bars, percentage_formatted, width, label=attribute, bottom=bottom) # for each pysical bar and its value plot a new bar
            ax.bar_label(rects, label_type='center')
            bottom += percentage_formatted

        # plot sneak connections bar
        bar_name = f"sneak wires\n({sneak_connections})"
        percentage = (sneak_connections / total_available_physical_all_wires) * 100
        percentage_formatted = float("{:.2f}".format(percentage))
        rects = ax.bar(bar_name, percentage_formatted, width, label="Total sneak wirelength")
        ax.bar_label(rects, label_type='center')

        #log(f"sneak wires - bar_name: {bar_name}, percentage_formatted: {percentage_formatted}\n")
        
        # set y lable
        ax.set_ylabel('Length Usage (%)')
        
        # add extra info to the chart and figure
        testbench_name = testbench.split('\n')[0] # the name of the testbench

        # design usage
        used_logical_all_wires = 0
        for value in used_logical.values():
            used_logical_all_wires += value

        used_physical_all_wires = 0
        for value in used_physical.values():
            used_physical_all_wires += value

        used_physical_tapped_all_wires = 0
        for value in used_physical_tapped.values():
            used_physical_tapped_all_wires += value

        logical_usage = (used_logical_all_wires/total_available_logical_all_wires)*100
        physical_usage = (used_physical_all_wires/total_available_physical_all_wires)*100
        physical_tapped_usage = (used_physical_tapped_all_wires/total_available_physical_all_wires)*100
        physical_usage_include_sneak = ((used_physical_all_wires + sneak_connections)/total_available_physical_all_wires)*100

        # format as "5.33"
        logical_usage_formatted_result = '{:.2f}'.format(logical_usage)
        physical_usage_formatted_result = '{:.2f}'.format(physical_usage)
        physical_tapped_usage_formatted_result = '{:.2f}'.format(physical_tapped_usage)
        physical_usage_include_sneak_formatted_result = '{:.2f}'.format(physical_usage_include_sneak)

        # set the title
        title = f"Node statistics for: {testbench_name}\n"
        title += f"Total Avaiable Logical Wires: {total_available_logical_all_wires}\n"
        title += f" Total Avaiable Physical Wires: {total_available_physical_all_wires}\n"
        title += f" Design Logical Usage: {used_logical_all_wires}  ({logical_usage_formatted_result}%)\n"
        title += f" Design Physical Usage: {used_physical_all_wires} ({physical_usage_formatted_result}%)\n"
        title += f" Design Physical Usage Including Sneak Paths: {(used_physical_all_wires + sneak_connections)} ({physical_usage_include_sneak_formatted_result}%)\n"
        title += f" Design Physical Tapped Usage: {used_physical_tapped_all_wires} ({physical_tapped_usage_formatted_result}%)"
        ax.set_title(title,fontsize=16)

        # set the figure information
        info = f"Touched Grids: {num_of_used_grids[testbenches.index(testbench)]}\n"
        info += f"{routing_array_size[testbenches.index(testbench)]}\n"
        
        info += f"{architecture_file_addr[testbenches.index(testbench)]}"
        info += f"{architecture_last_modified[testbenches.index(testbench)]}\n"

        info += f"{rrg_file_addr[testbenches.index(testbench)]}"
        info += f"{rrg_last_modified[testbenches.index(testbench)]}\n"

        info += f"{routing_file_addr[testbenches.index(testbench)]}"
        info += f"{routing_last_modified[testbenches.index(testbench)]}"
        info += f"{routing_placement_file[testbenches.index(testbench)]}"
        info += f"{routing_placement_id[testbenches.index(testbench)]}\n"

        # add the info to the figure - the first two arguments are padding from x and y
        fig.text(0.5, 0, info, fontsize=10, ha='center')

        # resize the figure to have enough space for info
        fig.set_size_inches(24, 18)

        # resize the plot so that it does not enlarge as the figure does
        fig.subplots_adjust(left=0.1, bottom=0.3, right=0.9, top=0.9)

        ax.legend()

        #plt.show()

        # save the plot as a pdf file
        filename = testbench.split('\n')[0]
        fig.savefig(f"{filename}.pdf")
        ################################## Raw numbers ##################################

        ################################## Scaled numbers ##################################
        width = 0.4  # the width of the bars
        bottom = np.zeros(len(physical_bars)) # for stacked bars
        # multiplier = 0
        # offset = width * multiplier
        #multiplier += 1
        
        fig, ax = plt.subplots(figsize=(24, 12), dpi=300) # size of the subplot and its dpi (different from the size of figure)

        percentage = [m / (total_available_logical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))) * 100 for m in logical_wires]
        percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]
        rects = ax.bar(logical_bars, percentage_formatted, width, label="Logical wires") # plot the logical bars first
        ax.bar_label(rects, label_type='center')

        # plot the pysical bars after
        
        for attribute, measurement in physical_wires.items():
            percentage = [m / (total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))) * 100 for m in measurement]
            percentage_formatted = [float("{:.2f}".format(p)) for p in percentage]
            rects = ax.bar(physical_bars, percentage_formatted, width, label=attribute, bottom=bottom) # for each pysical bar and its value plot a new bar
            ax.bar_label(rects, label_type='center')
            bottom += percentage_formatted

        #(num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))
        
        # plot sneak connections bar
        bar_name = f"sneak wires\n({sneak_connections})"
        percentage = (sneak_connections / (total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))) ) * 100
        percentage_formatted = float("{:.2f}".format(percentage))
        rects = ax.bar(bar_name, percentage_formatted, width, label="Total sneak wirelength")
        ax.bar_label(rects, label_type='center')
        
        #log(f"scaled sneak wires - bar_name: {bar_name}, percentage_formatted: {percentage_formatted}\n")

        # set y lable
        ax.set_ylabel('Length Usage (%)')
        
        # add extra info to the chart and figure
        testbench_name = testbench.split('\n')[0] # the name of the testbench

        # design usage
        logical_usage = (used_logical_all_wires/ (total_available_logical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))))*100
        physical_usage = (used_physical_all_wires/ (total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))))*100
        physical_tapped_usage = (used_physical_tapped_all_wires/ (total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))))*100
        physical_usage_include_sneak = ((used_physical_all_wires + sneak_connections)/(total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5])))))*100
        # format as "5.33"
        logical_usage_formatted_result = '{:.2f}'.format(logical_usage)
        physical_usage_formatted_result = '{:.2f}'.format(physical_usage)
        physical_tapped_usage_formatted_result = '{:.2f}'.format(physical_tapped_usage)
        physical_usage_include_sneak_formatted_result = '{:.2f}'.format(physical_usage_include_sneak)

        # set the title
        title = f"Node statistics for: {testbench_name}\n"
        title += f"Total Avaiable Logical Wires: {(total_available_logical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5]))))}\n"
        title += f" Total Avaiable Physical Wires: {(total_available_physical_all_wires * (num_of_used_grids[testbenches.index(testbench)]/ (int(routing_array_size[testbenches.index(testbench)].split(' ')[3])*int(routing_array_size[testbenches.index(testbench)].split(' ')[5]))))}\n"
        title += f" Design Logical Usage: {used_logical_all_wires}  ({logical_usage_formatted_result}%)\n"
        title += f" Design Physical Usage: {used_physical_all_wires} ({physical_usage_formatted_result}%)\n"
        title += f" Design Physical Usage Including Sneak Paths: {(used_physical_all_wires + sneak_connections)} ({physical_usage_include_sneak_formatted_result}%)\n"
        title += f" Design Physical Tapped Usage: {used_physical_tapped_all_wires} ({physical_tapped_usage_formatted_result}%)"
        ax.set_title(title,fontsize=16)

        # set the figure information
        info = f"Touched Grids: {num_of_used_grids[testbenches.index(testbench)]}\n"
        info += f"{routing_array_size[testbenches.index(testbench)]}\n"
        
        info += f"{architecture_file_addr[testbenches.index(testbench)]}"
        info += f"{architecture_last_modified[testbenches.index(testbench)]}\n"

        info += f"{rrg_file_addr[testbenches.index(testbench)]}"
        info += f"{rrg_last_modified[testbenches.index(testbench)]}\n"

        info += f"{routing_file_addr[testbenches.index(testbench)]}"
        info += f"{routing_last_modified[testbenches.index(testbench)]}"
        info += f"{routing_placement_file[testbenches.index(testbench)]}"
        info += f"{routing_placement_id[testbenches.index(testbench)]}\n"

        # add the info to the figure - the first two arguments are padding from x and y
        fig.text(0.5, 0, info, fontsize=10, ha='center')

        # resize the figure to have enough space for info
        fig.set_size_inches(24, 18)

        # resize the plot so that it does not enlarge as the figure does
        fig.subplots_adjust(left=0.1, bottom=0.3, right=0.9, top=0.9)

        ax.legend()

        #plt.show()

        # save the plot as a pdf file
        filename = testbench.split('\n')[0]
        fig.savefig(f"{filename}_scaled.pdf")
        ################################## Scaled numbers ##################################
# plot_this

arguments = sys.argv
designs_dir = ''
if len(arguments) > 1:
    designs_dir = arguments[1]
    get_reports_content(designs_dir) #first, get the reports content
    
    if len(arguments) > 2: # if it requires to plot a chart for all testbenches
        if arguments[2] == "all":
            plot_all()
        else:
            sys.stderr.write("bad argument\n")
    else: # if only requires to plot a chart for this testbench
        plot_this()
else:
    sys.stderr.write("Please indicate the design directory\n")
