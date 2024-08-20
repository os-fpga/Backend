import sys
import os
import subprocess
import networkx as nx
import matplotlib.pyplot as plt
from pack_lib import *
from place_lib import *
import re
import copy
import numpy as np
from scipy.optimize import minimize


debug = 1
net2graph       = {}    # net_name --> graph object
net2sub_graphs  = {}    # net_name --> list of sub_graphs
net2xy_set      = {}    # net_name --> (x,y) set
net2fixed_nodes = {}    # net_name --> nodes with fixed planes
SKIP_SP         = True  # Skip steiner points when building the graph
MAX_NODES       = 100
MAX_DISTANCE    = 20
MAX_PLANE       = 8
# coor2tile       = {} # (x,y) --> tile

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


# set how important it is to optimize the connection b/w two pins based on their distance (d = [x1 - x2] + [y1 - y2]) and plane difference
def taper(distance, plane_diff):
    distance_effect = MAX_DISTANCE - distance if distance < MAX_DISTANCE else 0
    # if distance == 0:
    #     distance_effect = 0
    # else:
    #    distance_effect = 1 / (distance // 4 + distance % 4) if distance < MAX_DISTANCE else 0
    plane_effect = plane_diff
    return distance_effect + plane_effect
# def taper(distance, plane_diff)


# find the movable objects of each tile (could be 1 or more FLEs in CLBs or nothing in other tiles)
def set_movable_objects():
    # log('inside set_movable_objects\n')
    for tile in top_blocks:
        
        # coor2tile[(tile.base_x, tile.base_y)] = tile
        # if tile.instance_name.startswith('dsp') or tile.instance_name.startswith('bram'):
        #     coor2tile[(tile.base_x+1, tile.base_y+1)] = tile.instance_name.split('[')[0]
        #     coor2tile[(tile.base_x+2, tile.base_y+2)] = tile.instance_name.split('[')[0]
        
        FIXED_CLB = False
        if tile.instance_name.startswith('clb'):
            # log(f'\ttile = {tile.instance_name}\n')
            # check clb's cin, cout first
            for input_port in tile.inputs:
                (_, port_name, pin_list) = input_port
                if port_name == 'cin':
                    if not pin_list[0] == 'open':
                        # log(f'\t\tcin is not open: {pin_list[0]} --> FIXED_CLB = True\n')
                        FIXED_CLB = True
                        continue
            for output_port in tile.outputs:
                (_, port_name, pin_list) = output_port
                if port_name == 'cout':
                    if not pin_list[0] == 'open':
                        # log(f'\t\tcout is not open: {pin_list[0]} --> FIXED_CLB = True\n')
                        FIXED_CLB = True
                        continue
            if FIXED_CLB:
                # log(f'\tFIXED_CLB = True --> continue\n')
                continue


            # now check FLEs chains
            # find CLB_LR first
            clb_lr = None
            if tile.sub_blocks:
                clb_lr = tile.sub_blocks[0]
                if not clb_lr.instance_name.startswith('clb_lr'):
                    log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
                    exit()
            else:
                log(f"ERROR: {tile} does not have clb_lr\n")
                exit()
            # log(f"\tclb_lr = {clb_lr.instance_name}\n")

            # now find FLEs
            fles = [] # [(cin, cout)]
            for fle in clb_lr.sub_blocks:
                # log(f"\tfle = {fle.name}, {fle.instance_name}\n")
                cin = cout = False
                for input_port in fle.inputs:
                    (_, port_name, pin_list) = input_port
                    if port_name == 'cin':
                        # log(f'\t\tcin name: {pin_list[0]}\n')
                        if not pin_list[0] == 'open':
                            # log(f'\t\tfle cin is not open: {pin_list[0]} --> cin = True\n')
                            cin = True
                for output_port in fle.outputs:
                    (_, port_name, pin_list) = output_port
                    if port_name == 'cout':
                        # log(f'\t\tcout name: {pin_list[0]}\n')
                        if not pin_list[0] == 'open':
                            # log(f'\t\tfle cout is not open: {pin_list[0]} --> cout = True\n')
                            cout = True
                index = int(fle.instance_name.split(']')[0].split('[')[-1])
                # log(f'\t\tindex = {index}\n')
                # fles.add((index, cin, cout))
                fles.insert(index, (cin, cout))
            
            count = 0
            for i, fle in enumerate(fles):
                if count > 0:
                    count -= 1
                    continue
                (cin, cout) = fle
                # log(f"\t(cin, cout, index)= {cin, cout, i}\n")
                start = i
                count = 0
                
                if i == 0 and cin:
                    # log(f"\tindex == 0 and cin = True --> FIXED_CLB = True --> break\n")
                    FIXED_CLB = True
                    break
                if i == 7 and cout:
                    # log(f"\tindex == 7 and cout = True --> FIXED_CLB = True --> break\n")
                    FIXED_CLB = True
                    break
                
                if cout:
                    for j in range(i+1, len(fles)):
                        (cin_2, cout_2) = fles[j]
                        # log(f"\t\t(cin_2, cout_2, index_2)= {cin_2, cout_2, j}\n")
                        if cin_2:
                            count += 1
                            # log(f"\t\tcount = {count}\n")
                        if cout_2:
                            continue
                        else:
                            break
                
                # log(f"\ttile.add_movable_objects(({start}, {start + count}))\n")
                tile.add_movable_objects((start, (start + count)))
            
            if FIXED_CLB:
                # log(f'\tFIXED_CLB = True --> continue\n')
                continue         
# def set_movable_objects()


def get_FLUTE_info(net_name, xy_set):
    script_directory = os.path.dirname(os.path.abspath(__file__))
    program_directory = f"{script_directory}/../evalroute/flute-3.1"
    executable_path = f"{program_directory}/flute-net"
    coordinates = ""
    edges = list()

    for (x,y) in xy_set:
        coordinates += f"{x} {y} \n"

    w1 = w2 = 0
    if len(xy_set) == 1:
        return w1, w2, edges
    try:
        command = [executable_path]
        result = subprocess.run(command, input=coordinates, text=True, cwd=program_directory, capture_output=True, check=True)
        
        # Parse the output to extract wirelength values
        output_lines = result.stdout.splitlines()
        # log(f"output_lines = {output_lines}\n")
        w1 = int(output_lines[0].split()[-1])
        w2 = int(output_lines[1].split()[-1])

        i = 0
        for line in output_lines:
            if i == 0 or i == 1:
                i += 1
                continue
            if i < (len(output_lines) - 1) and output_lines[i]:
                (x1, y1, x2, y2) = int(output_lines[i].split()[0]), int(output_lines[i].split()[1]), int(output_lines[i+1].split()[0]), int(output_lines[i+1].split()[1])
                edges.append((x1, y1, x2, y2))
                i += 2
            else:
                i += 1
            
        
        # log(f"For Net {net_name}:")
        # log(f"\tFLUTE wirelength = {w1}\n")
        # log(f"\tFLUTE wirelength (without RSMT construction) = {w2}\n")
        # log(f"\tEdges = {edges}\n")
        
    except subprocess.CalledProcessError as e:
        log(f"Error for Net {net_name}: {e}")
        log(f"Command output: {e.output}")
        exit()

    return w1, w2, edges
# def get_FLUTE_info(net_name, xy_set)


def plot_tree(net, all_edges, original_points, src_coordinate):

    if not os.path.exists('RSMT_trees'):
        # If not, create it
        os.makedirs('RSMT_trees')
    # Initialize min and max values
    min_x = min_y = int(sys.maxsize)
    max_x = max_y = 0

    # Iterate through the list and update min and max values
    for (x1, y1, x2, y2) in all_edges:
        min_x = min(min_x, x1, x2)
        max_x = max(max_x, x1, x2)
        min_y = min(min_y, y1, y2)
        max_y = max(max_y, y1, y2)

    for (x, y) in original_points:
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
    for (x, y) in original_points:
        if (x, y) == src_coordinate:
            plt.scatter(x, y, color='green')
        else:
            plt.scatter(x, y, color='red')
    
    # draw steiner points and then edges
    for (x1, y1, x2, y2) in all_edges:
        
        # steiner points
        if (x1, y1) not in original_points and (x1, y1) not in original_points:
            plt.scatter(x1, y1, color='blue')
        if (x2, y2) not in original_points and (x2, y2) not in original_points:
            plt.scatter(x2, y2, color='blue')
        # edges
        x_values = [x1, x2, x2]
        y_values = [y1, y1, y2]
        plt.plot(x_values, y_values, color='black')
    
    
    # Set the x-axis and y-axis ticks with a step of 1
    plt.savefig(f"RSMT_trees/net-{net}.pdf")
# def plot_tree(net_id, all_edges, original_points)


# crate edge b/w nodes using flute results
def build_edges(graph, xy2input_obj, is_parent_sp, parent_coor, actual_parent_coor, src_plane, FLUTE_edges, sp_set, visited_edges):
    # log('inside build_edges\n')
    # log(f'is_parent_sp = {is_parent_sp}\n')
    # log(f'parent_coor = {parent_coor}\n')
    # log(f'actual_parent_coor = {actual_parent_coor}\n')
    # log(f'src_plane = {src_plane}\n')
    # log(f'visited_edges = {visited_edges}\n')
    
    for edge in FLUTE_edges:
        (x1, y1, x2, y2) = edge
        # log(f'\t(x1, y1, x2, y2) = {(x1, y1, x2, y2)}\n')
        src_x = src_y = dst_x = dst_y = -1
        same_coor = False
        if edge not in visited_edges: # only if the edge is not already visited
            
            if (x1, y1) == parent_coor: # if x1,y1 is the parent
                # log(f'\t\t(x1, y1) == parent_coor\n')
                src_x = x1
                src_y = y1
                dst_x = x2
                dst_y = y2
                if is_parent_sp and SKIP_SP:
                    (src_x, src_y) = actual_parent_coor
                    # log(f'\t\t(src_x, src_y) = actual_parent_coor\n')
            
            elif (x2, y2) == parent_coor: # if x2,y2 is the parent
                # log(f'\t\t(x2, y2) == parent_coor\n')
                src_x = x2
                src_y = y2
                dst_x = x1
                dst_y = y1
                if is_parent_sp and SKIP_SP:
                    (src_x, src_y) = actual_parent_coor
                    # log(f'\t\t(src_x, src_y) = actual_parent_coor\n')
            
            else: # none of the coordinates are the parent_coor
                # log(f'\t\tnone of the coordinates are the parent_coor\n')
                continue
            
            if src_x == dst_x and src_y == dst_y: # when both coordinates are the same
                # log(f'\t\tsrc_x({src_x}) == dst_x({dst_x}) and src_y({src_y}) == dst_y({dst_y})\n')
                same_coor = True
                # visited_edges.add(edge)
                # continue
            
            src_name = f'{src_x},{src_y},{src_plane}'
            # log(f'\t\tadding src node {src_name}\n')
            # graph.add_node(src_name, x=src_x, y=src_y, plane=src_plane) # add the parent first
            
            if (dst_x, dst_y) not in sp_set: # if (dst_x, dst_y) is not a steiner point then add this node and create an edge
                # log(f'\t\t(dst_x({dst_x}), dst_y({dst_y})) is not a steiner point --> add all input objects at this coordinate\n')
                if not (dst_x, dst_y) in xy2input_obj:
                    log(f'\t\tERROR: ({dst_x}, {dst_y}) is not in xy2input_obj:\n\t\t {xy2input_obj}\n')
                    exit()
                for input_object in xy2input_obj[(dst_x, dst_y)]:
                    for dst_plane in input_object.sub_ys:
                        if same_coor and src_plane == dst_plane:
                            # log(f'\t\t\tsrc and dst are in same coordinate with the same plane# --> skip this dst node: {dst_x},{dst_y},{dst_plane}\n')
                            continue
                        dst_name = f'{dst_x},{dst_y},{dst_plane}'
                        # log(f'\t\tadding dst node {dst_name}\n')
                        # graph.add_node(dst_name, x=dst_x, y=dst_y, plane=dst_plane) # add the child node
                        edge_weight = taper(distance=(abs(src_x - dst_x) + abs(src_y - dst_y)), plane_diff=abs(src_plane - dst_plane))
                        # log(f'\t\tedge_weight = {edge_weight}\n')
                        # log(f'\t\tadding edge src_name({src_name}) --> dst_name({dst_name}) with weight = {edge_weight}\n')
                        graph.add_edge(src_name, dst_name, weight=edge_weight)
                        # log(f'\t\tcalling build_edges() for dst_plane = {dst_plane}\n')
                        if edge not in visited_edges:
                            visited_edges.add(edge)
                        build_edges(graph=graph,
                                    xy2input_obj=xy2input_obj,
                                    is_parent_sp=False, 
                                    parent_coor=(dst_x, dst_y), 
                                    actual_parent_coor=(dst_x, dst_y), 
                                    src_plane=dst_plane,
                                    FLUTE_edges=FLUTE_edges, 
                                    sp_set=sp_set, 
                                    visited_edges=visited_edges,
                        ) # call the function recursively to build edges from (dst_x, dst_y) to its children
                visited_edges.add(edge)
                
            
            elif (x2, y2) in sp_set and SKIP_SP: # if (dst_x, dst_y) is in steiner points and we want to skip it, then the node won't be added and edge won't be created
                visited_edges.add(edge)
                # log(f'\t\tcalling build_edges() when (x2, y2) in sp_set and SKIP_SP is True\n')
                build_edges(graph=graph,
                            xy2input_obj=xy2input_obj,
                            is_parent_sp=True,
                            parent_coor=(dst_x, dst_y),
                            actual_parent_coor=(src_x, src_y),
                            src_plane=src_plane,
                            FLUTE_edges=FLUTE_edges,
                            sp_set=sp_set,
                            visited_edges=visited_edges,
                ) # call the function recursively to build edges from x2,y2 to its children (but the actual parent is (x1,y1))
# def build_edges(graph, xy2input_obj, is_parent_sp, parent_coor, actual_parent_coor, src_plane, FLUTE_edges, sp_set, visited_edges)


def cal_raduis(graph):
    # log(f"inside cal_raduis:\n")
    for node in graph.nodes():
        # log(f"\tcalculating radius for node: {node}\n")
        (node_x, node_y) = (graph.nodes[node]['x'], graph.nodes[node]['y'])
        # log(f"\t(node_x, node_y) = ({node_x, node_y})\n")
        
        # initial value of maximum_radius
        maximum_radius = 0

        # initilize radius attribute
        graph.nodes[node]['radius'] = maximum_radius

        for neighbor in graph.neighbors(node):
            # log(f"\t\tlooking at neighbor: {neighbor}\n")
            (neighbor_x, neighbor_y) = (graph.nodes[neighbor]['x'], graph.nodes[neighbor]['y'])
            # log(f"\t\t(neighbor_x, neighbor_y) = ({neighbor_x, neighbor_y})\n")
            distance = abs(node_x - neighbor_x) + abs(node_y - neighbor_y)
            # log(f"\t\tdistance = {distance}\n")
            if distance > maximum_radius:
                # log(f"\t\tdistance ({distance}) > maximum_radius ({maximum_radius}) --> updating node[radius]\n")
                graph.nodes[node]['radius'] = distance
        # log(f"\tfinal radius = {graph.nodes[node]['radius']}\n")
# def cal_raduis(graph)


# add fuzzy edges
def add_fuzzy_edges(graph):
    # log(f"inside add_fuzzy_edges:\n")
    for node_i in graph.nodes():
        i_x, i_y, i_plane = (graph.nodes[node_i]['x'], graph.nodes[node_i]['y'], graph.nodes[node_i]['plane'])
        i_radius = graph.nodes[node_i]['radius']
        if i_radius == 0:
            continue
        for node_j in graph.nodes():
            # log(f'\t\tnode j: {node_j}\n')
            if node_j == node_i:
                # log(f'\t\tnode_j == node_i --> skip\n')
                continue
            if node_j in graph.neighbors(node_i):
                # log(f'\t\tnode_j in graph.neighbors(node_i) = {list(graph.neighbors(node_i))} --> skip\n')
                continue
            j_x, j_y, j_plane = (graph.nodes[node_j]['x'], graph.nodes[node_j]['y'], graph.nodes[node_i]['plane'])
            # log(f"\t\tj_x, j_y, j_plane = ({j_x, j_y, j_plane})\n")
            distance = abs(i_x - j_x) + abs(i_y - j_y)
            if distance <= i_radius:
                # log(f"\tnode i: {node_i}\n")
                # log(f"\ti_x, i_y, i_plane = ({i_x, i_y, i_plane})\n")
                # log(f"\ti_radius = {i_radius}\n")
                # log(f'\t\tfor node j: {node_j}\n')
                # log(f'\t\tdistance ({distance}) <= radius ({i_radius})\n')
                edge_weight = taper(distance=distance, plane_diff=abs(i_plane - j_plane))
                # log(f'\t\tedge_weight = {edge_weight}\n')
                # log(f'\t\tadding edge from node_i --> node_j\n')
                graph.add_edge(node_i, node_j, weight=edge_weight)
            # else:
            #     log(f'\t\tdistance ({distance}) > radius ({i_radius}) --> skip\n')
# def add_fuzzy_edges(graph)

# build the graph
def build_graph(max_x, max_y):
    global net2graph
    global net2sub_graphs
    global net2xy_set
    global net2fixed_nodes

    for (net_name,edges) in net2edges_pin_obj.items():
        log(f"net_name:\t{net_name}\n")
        xy_set = set()
        sp_set = set()
        same_coor_set = set()
        G = nx.Graph(net_name=net_name)
        # G.graph['net_name']
        src_coordinate = None
        plane_num = None
        FLUTE_edges = None
        visited_edges = set()
        xy2input_obj = dict()
        sub_graphs = [list() for _ in range(MAX_PLANE)]

        # build (x,y) set for FLUTE
        for edge in edges:
            (plane_num, src_x, src_y, input_object, dst_x, dst_y) = edge
            # log(f"\tsrc ({src_x},{src_y}) with plane# ({plane_num}) --> dst ({dst_x},{dst_y}) with input_object ({input_object.name})\n")

            if src_x == dst_x and src_y == dst_y:
                # log(f"\tsame coordinate for src and dst --> add to same_coor_set\n")
                same_coor_set.add((src_x,src_y))
            
            xy_set.add((src_x, src_y)) # Only one src
            xy_set.add((dst_x, dst_y)) # might be multiple dst(s)
            src_coordinate = (src_x, src_y)
            xy2input_obj.setdefault((dst_x, dst_y), []).append(input_object)
        # log(f"xy_set built:\t{xy_set}\n")
        # log(f"src_coordinate =\t{src_coordinate}\n")
        # log(f"xy2input_obj:\t{xy2input_obj}\n")
        # log(f"same_coor_set:\t{same_coor_set}\n")

        net2xy_set[net_name] = xy_set
        if len(xy_set) > MAX_NODES:
            log(f"number of nodes ({len(xy_set)}) > {MAX_NODES} --> skip this net\n")
            continue
        
        # build fixed_nodes dict:
        fixed_nodes = {} # node --> fixed plane#

        # initilize the graph:
        for xy in xy_set:
            (x, y) = xy
            for plane in range (0, MAX_PLANE):
                node_name = f'{x},{y},{plane}'
                G.add_node(node_name, x=x, y=y, plane=plane)
                
                sub_graphs[plane].append(node_name)
                
                # add node to fixed nodes if it is not part of CLB movable objects
                if (not coor2tile[(x,y)].movable_objects) or (not coor2tile[(x,y)].instance_name.startswith('clb')):
                    fixed_nodes[node_name] = plane
                
        net2sub_graphs[net_name] = sub_graphs
        net2fixed_nodes[net_name] = fixed_nodes

        # run FLUTE and get the edges
        (w1, w2, FLUTE_edges) = get_FLUTE_info(net_name, xy_set)

        
        new_FLUTE_edges = []
        for edge in FLUTE_edges:
            (x1, y1, x2, y2) = edge
            # add steiner points to sp_set
            if (x1, y1) not in xy_set:
                sp_set.add((x1,y1))
            # add steiner points to sp_set
            if (x2, y2) not in xy_set:
                sp_set.add((x2,y2))
            
            # only add edges that are not in the same coordinate
            if (x1 == x2) and ( y1 == y2):
                pass
                # log(f"\t{x1} == {x2} and {y1} == {y2}\n")
            else:
                new_FLUTE_edges.append(edge)
        # log(f"sp_set built:\t{sp_set}\n")

        # assign new list to the previous one
        FLUTE_edges = new_FLUTE_edges
        # log(f"New FLUTE_edges:\t{FLUTE_edges}\n")

        # log(f"adding the orignal same-coordinate src & dst to FLUTE_edges\n")
        for (x,y) in same_coor_set:
            # log(f"\tadding ({x,y}) --> ({x,y})\n")
            FLUTE_edges.append((x, y, x, y))

        # log(f"Final FLUTE_edges:\t{FLUTE_edges}\n")

        
        build_edges(graph=G,
                    xy2input_obj=xy2input_obj,
                    is_parent_sp=False, 
                    parent_coor=src_coordinate,
                    actual_parent_coor=src_coordinate,
                    src_plane=int(plane_num),
                    FLUTE_edges=FLUTE_edges, 
                    sp_set=sp_set,
                    visited_edges=visited_edges,
                    )
        
        # log(f"graph is {G.graph}, {G}, {G.nodes()}, {G.edges()}\n")
        # log(f"Adjacency information for net: {net_name}\n")
        # for node in G.nodes:
        #     neighbors = list(G.neighbors(node))
        #     log(f"\tNode {node} --> {neighbors}\n")
        
        # calculate radius
        # cal_raduis(G)

        # add fuzzy edges
        # add_fuzzy_edges(G)


        net2graph[net_name] = G
        
        ################################################################################################
        # sub0 = nx.subgraph(G,sub_graphs[0])
        # sub4 = nx.subgraph(G,sub_graphs[4])
        # sub_0_4 = nx.subgraph(G,(sub_graphs[0] + sub_graphs[4]))
        # print(sub_0_4)
        # print(sub_0_4.edges)
        #################################################################################################

        # log(f"Adjacency information for net: {net_name}\n")
        # for node in G.nodes:
        #     neighbors = list(G.neighbors(node))
        #     log(f"\tNode {node} --> {neighbors}\n")
    
        # plot_tree(net_name, FLUTE_edges, xy_set, src_coordinate)
# def build_graph(max_x, max_y)


# Define a custom sorting key based on x, y, plane
def custom_sort_key(node_name):
    x, y, z = map(int, node_name.split(','))
    return x, y, z
# def custom_sort_key(node_name)


def cal_cutcost_Dvalues(graph):
    cut_cost = 0
    external_costs = dict() # node_name --> external cost
    internal_costs = dict() # node_name --> internal cost
    D_values = dict()       # node_name --> (External cost - Internal cost)
    for edge in graph.edges(data=True):
        node1 = edge[0]
        node2 = edge[1]
        node1_plane = graph.nodes[node1]['plane']
        node2_plane = graph.nodes[node2]['plane']
        edge_weight = graph[node1][node2]['weight']
        if node1_plane != node2_plane:
            cut_cost += edge_weight
            external_costs[node1] = external_costs.setdefault(node1, 0) + edge_weight
            external_costs[node2] = external_costs.setdefault(node2, 0) + edge_weight
        elif node1_plane == node2_plane:
            internal_costs[node1] = internal_costs.setdefault(node1, 0) + edge_weight
            internal_costs[node2] = internal_costs.setdefault(node2, 0) + edge_weight
    # log(f'\texternal_costs = {external_costs}\n\tinternal_costs = {internal_costs}\n')
    
    for node in graph.nodes():
        external_cost = internal_cost = 0
        if node in external_costs:
            external_cost = external_costs[node]
        if node in internal_costs:
            internal_cost = internal_costs[node]
        D_value = external_cost - internal_cost
        D_values[node] = D_value
    return cut_cost, D_values
# def cal_cutcost_Dvalues(graph)


def update_net2sub_graphs(net_name, graph):
    #TODO: update in graphs of all nets
    global net2sub_graphs
    sub_graphs = [list() for _ in range(MAX_PLANE)]
    net2sub_graphs[net_name] = None
    for plane in range (0, MAX_PLANE):
        for node in graph:
            if graph.nodes[node]['plane'] == plane:
                sub_graphs[plane].append(node)
    net2sub_graphs[net_name] = sub_graphs
# def update_net2sub_graphs(net_name, graph)


# determine whether the swap is valid or not. If it is a valid swap, the function returns a map b/w swapped nodes
def is_valid_swap(graph, node_a, node_b):
    swap_map = {}
    (x_a, y_a, plane_a) = (graph.nodes[node_a]['x'], graph.nodes[node_a]['y'], graph.nodes[node_a]['plane'])
    (x_b, y_b, plane_b) = (graph.nodes[node_b]['x'], graph.nodes[node_b]['y'], graph.nodes[node_b]['plane'])
    
    ############### Limits the valid swaps ###############

    if x_a != x_b or y_a != y_b: # not in the same tile
        return False, swap_map
    if not coor2tile[(x_a, y_a)].movable_objects: # empty list of movable objects
        return False, swap_map
    if not coor2tile[(x_b, y_b)].movable_objects: # empty list of movable objects
        return False, swap_map
    if not coor2tile[(x_a, y_a)].instance_name.startswith('clb'): # It is not even a CLB
        return False, swap_map
    if not coor2tile[(x_b, y_b)].instance_name.startswith('clb'): # It is not even a CLB
        return False, swap_map
    
    x = x_a
    y = y_a

    a_is_in_movable_objects = False # a in in the movable objects
    a_is_in_chain           = False # a is part of a chain
    a_is_beg_chain          = False # a in the beginning of a chain
    a_movable_object        = None  # start and end of a movable object containing a

    b_is_in_movable_objects = False # b in in the movable objects
    b_is_in_chain           = False # b is part of a chain
    b_is_beg_chain          = False # b in the beginning of a chain 
    b_movable_object        = None  # start and end of a movable object containing b

    for movable_object in coor2tile[(x, y)].movable_objects:
        (start, end) = movable_object
        if plane_a >= start and plane_a <= end:
            a_is_in_movable_objects = True
            a_movable_object = (start, end)
            if start != end: # if it is part of a chain 
                # log(f'\t\ta: start({start}) != end({end})\n')
                a_is_in_chain = True
                if plane_a != start: # if a is not the beginning of the chain
                    a_is_beg_chain = False
                    # log(f'\t\ta is not the beginning of the chain\n')
                else:
                    a_is_beg_chain = True
                    # log(f'\t\ta is the beginning of the chain\n')
        elif plane_b >= start and plane_b <= end:
            b_is_in_movable_objects = True
            b_movable_object = (start, end)
            if start != end: # if it is part of a chain
                # log(f'\t\tb: start({start}) != end({end})\n')
                b_is_in_chain = True
                if plane_b != start: # if a is not the beginning of the chain
                    b_is_beg_chain = False
                    # log(f'\t\tb is not the beginning of the chain\n')
                else:
                    b_is_beg_chain = True
                    # log(f'\t\tb is the beginning of the chain\n')  
    if not a_is_in_movable_objects: # if a not found
        return False, swap_map
    if not b_is_in_movable_objects: # if b not found
        return False, swap_map
    
    
    # if either of them are in the chain but not the beginning of a chain --> no swap
    if (a_is_in_chain and not a_is_beg_chain) or (b_is_in_chain and not b_is_beg_chain):
        return False, swap_map
    
    (a_start, a_end) = a_movable_object
    (b_start, b_end) = b_movable_object
    a_len = abs(a_end - a_start + 1)
    b_len = abs(b_end - b_start + 1)
    x_len = abs(a_start - b_end - 1) if a_start > b_start else abs(b_start - a_end - 1) # length of objects in between a and b
    
    # if both of them are part of the same chain --> no need to swap
    if a_start == b_start and a_end == b_end:
        return False, swap_map
    
    # log(f'\t\t node_a: {node_a} ({a_start}, {a_end}), node_b: {node_b} ({b_start}, {b_end})\n')
    # log(f'\t\t x_len = {x_len}\n')
    # log(f'\t\t a_start, a_end, b_start, b_end, a_len, b_len = {a_start, a_end, b_start, b_end, a_len, b_len}\n')
    
    movable_objects = list(range(MAX_PLANE))
    new_movable_objects = list(range(MAX_PLANE))

    for index, obj in enumerate(movable_objects):
        for node in graph.nodes():
            if graph.nodes[node]['x'] == x and graph.nodes[node]['y'] == y and graph.nodes[node]['plane'] == index:
                movable_objects[index] = node
    # log(f'\t\t current nodes plane mapping:\n')
    # for index, obj in enumerate(movable_objects):
        # log(f'\t\t\t({obj}) --> plane: ({index})\n')
    # if a is above b:
    if a_start > b_start:
        for index, obj in enumerate(new_movable_objects):
            if index >= a_start and index <= a_end:
                new_movable_objects[index] = index - (b_len + x_len)
            elif index >= b_start and index <= b_end:
                new_movable_objects[index] = index + (a_len + x_len)
            elif index > b_end and index < a_start:
                new_movable_objects[index] = index + (a_len - b_len)
    else:
        for index, obj in enumerate(new_movable_objects):
            if index >= b_start and index <= b_end:
                new_movable_objects[index] = index - (a_len + x_len)
            elif index >= a_start and index <= a_end:
                new_movable_objects[index] = index + (b_len + x_len)
            elif index > a_end and index < b_start:
                new_movable_objects[index] = index + (b_len - a_len)
    
    # log(f'\t\t planes to be changed:\n')
    for prev_plane, curr_plane in enumerate(new_movable_objects):
        # log(f'\t\t\t({prev_plane}) --> ({curr_plane})\n')
        if prev_plane != curr_plane:
            a = None
            b = None
            for node in graph.nodes():
                if graph.nodes[node]['x'] == x and graph.nodes[node]['y'] == y:
                    if graph.nodes[node]['plane'] == prev_plane:
                        a = node
                    elif graph.nodes[node]['plane'] == curr_plane:
                        b = node
            if a == None:
                log(f'\t\tERROR: a == None. prev_plane = {prev_plane}, obj = {curr_plane}, a_start = {a_start}, a_end = {a_end}, x = {x}, y = {y}\n')
                exit()
            if b == None:
                log(f'\t\tERROR: b == None. prev_plane = {prev_plane}, obj = {curr_plane}, b_start = {b_start}, b_end = {b_end}, x = {x}, y = {y}\n')
                exit()
            
            swap_map[a] = b
    # log(f'\t\t return True, swap_map: {swap_map}\n')
    return True, swap_map
# def is_valid_swap(node_a, node_b)


# swap a and b. If they are part of a chain --> swap all planes
def swap(graph, P_AB, swap_map):
    i = 1
    a_old_plane = {}
    a_new_plane = {}
    new_movable_objects = []
    x = -1 
    y = -1
    for a,b in swap_map.items():
        x = graph.nodes[a]['x']
        y = graph.nodes[a]['y']

        if i == 1: # initialize the new_movable_objects
            new_movable_objects = {item: item for item in coor2tile[(x, y)].movable_objects}
            # log(f"\t\tmovable_objects for ({x, y}) initialized: {new_movable_objects}\n")
        # log(f'\t\t({i}/{len(swap_map)} pair in swap_map: {a,b}\n')
        # log(f"\t\tnode a: {a} with plane = {graph.nodes[a]['plane']}\n")
        # log(f"\t\tnode b: {b} with plane = {graph.nodes[b]['plane']}\n")
        
        a_old_plane[(a,b)] = graph.nodes[a]['plane']
        a_new_plane[(a,b)] = graph.nodes[b]['plane']

        
        for movable_object in new_movable_objects:
            (start, end) = movable_object
            if graph.nodes[a]['plane'] == start:
                new_movable_objects[movable_object] = (start - (graph.nodes[a]['plane'] - graph.nodes[b]['plane']), end - (graph.nodes[a]['plane'] - graph.nodes[b]['plane']))      
        i += 1
    
    i = 1
    for (a,b) in swap_map.items():
        if a in P_AB.nodes():
            P_AB.nodes[a]['plane'] = a_new_plane[(a,b)]
        #TODO: update in graphs of all nets
        graph.nodes[a]['plane'] = a_new_plane[(a,b)]
        
        # log(f'\t\t({i}/{len(swap_map)} pair in swap_map: {a,b}\n')
        # log(f"\t\tnode a: {a} now has plane = {graph.nodes[a]['plane']}\n")
        # log(f"\t\tnode b: {b} now has plane = {graph.nodes[b]['plane']}\n")
        i += 1
        
    coor2tile[(x, y)].movable_objects = list(new_movable_objects.values())

    # log(f"\t\tmovable_objects for ({x, y}) after swap: {coor2tile[(x, y)].movable_objects}\n")
# def swap(graph, P_AB, swap_map)


# KL Partitioning
def kl_partitioning():
    global net2graph
    global net2sub_graphs
    log(f'inside kl_partitioning:\n\n')
    # KL algorithm works on two partitions. We have 8 partitions.
    # Thus, we should run the algorithm (8 choose 2 = 28) times for different pairs of partitions. 
    # we can run the algorithm as long as the total design spread reduces.  
    ii = 1
    last_graph = {}
    last_sub_graphs = {}
    while True:
        initial_design_spread = 0
        final_design_spread = 0
        design_gains = 0
        
        # log(f'running KL partitioning. iteration: {ii}\n\n')
        for (net_name,graph) in net2graph.items():
            # log(f'net_name = {net_name}\n')
            net_gains = 0
            initial_net_spread = 0
            for edge in graph.edges(data=True):
                node1 = edge[0]
                node2 = edge[1]
                node1_plane = graph.nodes[node1]['plane']
                node2_plane = graph.nodes[node2]['plane']
                initial_net_spread += abs(node1_plane - node2_plane)
            initial_design_spread += initial_net_spread
            log(f'for net_name: {net_name} --> initial net spread value = {initial_net_spread}\n')
            for i in range (0,MAX_PLANE):
                for j in range (i+1,MAX_PLANE):
                    # log(f'\ti = {i}, j = {j}\n')

                    p = 0 # pass
                    total_gain = 0
                    MAX_PASS = 1000
                    # repeat until g_max <= 0
                    while True:
                        if p >= MAX_PASS:
                            break
                        S_A = net2sub_graphs[net_name][i]
                        S_B = net2sub_graphs[net_name][j]
                        P_AB = nx.subgraph(graph,(S_A + S_B))   # Partition A+B including nodes in A&B plus edges in A, B, *and between them*
                        # log(f'\tS_A = {S_A}\n\tS_B = {S_B}\n')
                        # log(f'\tP_AB = {P_AB}\n')
                        # log(f'\tP_AB nodes = {P_AB.nodes(data=True)}\n')
                        # log(f'\tP_AB Edges = {P_AB.edges(data=True)}\n')
                        group_a = []
                        group_b = []
                        gains = [] # [ ([a, b], gain), ... ]
                        
                        for node in P_AB.nodes():
                            if P_AB.nodes[node]['plane'] == i:
                                group_a.append(node)
                            elif P_AB.nodes[node]['plane'] == j:
                                group_b.append(node)
                        # log(f'\tgroup_a:{group_a}\n')
                        # log(f'\tgroup_b:{group_b}\n')

                        init_cut_cost, D_values = cal_cutcost_Dvalues(P_AB)
                        # log(f'\tInitial cut cost of Graph (A+B) = {init_cut_cost}\n')
                        # log(f'\tD_values = {D_values}\n')
                        if init_cut_cost == 0:
                            break
                        
                        # while there are unvisited vertices
                        for iteration in range(int(len(P_AB.nodes())/2)):
                            # log(f'\t\tpair number: {iteration}\n')
                            # choose a pair that maximizes gain 
                            max_gain = -1 * float("inf") # -infinity
                            pair = []
                            swap_map_max = {}
                            for a in group_a:
                                for b in group_b:
                                    valid_swap, swap_map = is_valid_swap(graph, a, b)
                                    if not valid_swap:
                                        continue
                                    if P_AB.has_edge(a, b):
                                        c_ab = P_AB[a][b]['weight']
                                    else:
                                        c_ab = 0
                                    gain = D_values[a] + D_values[b] - (2 * c_ab)
                                
                                    if gain > max_gain:
                                        max_gain = gain
                                        pair = [a, b]
                                        swap_map_max = swap_map
                        
                            # mark that pair as visited
                            if max_gain == -1 * float("inf"):
                                continue
                            a = pair[0]
                            b = pair[1]
                            # log(f'\t\tremoving a = {a} and b = {b} from groups with max_gain = {max_gain}\n')
                            group_a.remove(a)
                            group_b.remove(b)
                            gains.append([[a, b], max_gain, swap_map_max])
                    
                            # update D_values of other unvisited nodes connected to a and b, as if a and b are swapped
                            for x in group_a:
                                c_xa = P_AB[x][a]['weight'] if P_AB.has_edge(x, a) else 0
                                c_xb = P_AB[x][b]['weight'] if P_AB.has_edge(x, b) else 0
                                D_values[x] += 2 * (c_xa) - 2 * (c_xb)
                        
                            for y in group_b:
                                c_yb = P_AB[y][b]['weight'] if P_AB.has_edge(y, b) else 0
                                c_ya = P_AB[y][a]['weight'] if P_AB.has_edge(y, a) else 0
                                D_values[y] += 2 * (c_yb) - 2 * (c_ya)
                            # log(f'\t\tD_values updated: {D_values}\n')
                        # log(f'\t\tgains: {gains}\n')
                        # find k that maximizes the sum g_max
                        g_max = -1 * float("inf")
                        kmax = 0
                        for k in range(1, len(gains) + 1):
                            g_sum = 0
                            for l in range(k):
                                g_sum += gains[l][1]
                        
                            if g_sum > g_max:
                                g_max = g_sum
                                kmax = k
                    
                        if g_max > 0:
                            # log(f'\t\ttaking the first {kmax+1} swaps:\n')
                            # swap in graph
                            for m in range(kmax):
                                # find nodes and change their partition
                                swapped_nodes = set()
                                for n in P_AB.nodes():
                                    if n in swapped_nodes:
                                        continue
                                    if n == gains[m][0][0]:
                                        a = n
                                        b = gains[m][0][1]
                                        swap_map = gains[m][2]
                                        swap(graph, P_AB, swap_map)
                                        swapped_nodes.add(a)
                                        swapped_nodes.add(b)
                                    
                                    elif n == gains[m][0][1]:
                                        b = n
                                        a = gains[m][0][0]
                                        swap_map = gains[m][2]
                                        swap(graph, P_AB, swap_map)
                                        swapped_nodes.add(a)
                                        swapped_nodes.add(b)
                            p += 1
                            total_gain += g_max
                            # log(f"\t\tPass: {str(p)} \t\tGain: {str(g_max)}\n")
                            # update net2sub_graphs dict
                            update_net2sub_graphs(net_name=net_name, graph=graph)
                        else:
                            break
                    
                    final_cut_cost = 0
                    for edge in P_AB.edges():
                        node1_plane = P_AB.nodes[edge[0]]['plane']
                        node2_plane = P_AB.nodes[edge[1]]['plane']
                        if node1_plane != node2_plane:
                            final_cut_cost += P_AB[edge[0]][edge[1]]['weight']
                    
                    log(f"\t\tTotal passes: {p} \t\tTotal Gain: {total_gain}\t\tFinal partition cut cost: {final_cut_cost}\n")
                    net_gains += total_gain

                    
            log(f'for net_name: {net_name} --> the total gains for all partition pairs: {net_gains}\n')
            
            final_net_spread = 0
            for edge in graph.edges(data=True):
                node1 = edge[0]
                node2 = edge[1]
                node1_plane = graph.nodes[node1]['plane']
                node2_plane = graph.nodes[node2]['plane']
                final_net_spread += abs(node1_plane - node2_plane)
            final_design_spread += final_net_spread
            log(f'for net_name: {net_name} --> final net spread value = {final_net_spread}\n')
            design_gains += net_gains
        
        log(f'design_gains: {design_gains} iteration: {ii}\n')
        log(f'(KL) initial design spread: {initial_design_spread} iteration: {ii}\n')
        log(f'(KL) final design spread: {final_design_spread} iteration: {ii}\n')
        
        ii += 1
        
        if final_design_spread == initial_design_spread:
            log(f'Last iteration has the same total design spread as the current one. End of the algorithm.\n')
            break
        elif final_design_spread > initial_design_spread:
            log(f'Last iteration made the total design spread worse --> restore to the previous iteration result and end of the algorithm\n')
            for (net_name,graph) in last_graph.items():
                net2sub_graphs[net_name] = last_sub_graphs[net_name].copy()
                net2graph[net_name] = last_graph[net_name].copy()
            break
        else:
            log(f'Last iteration made the total design spread better --> store the results and continue the algorithm\n')
            for (net_name,graph) in net2graph.items():
                last_sub_graphs[net_name] = net2sub_graphs[net_name].copy()
                last_graph[net_name] = net2graph[net_name].copy()
    
    log(f'After running the KL algorithm:\n')
    design_spread = 0
    for (net_name,graph) in net2graph.items():
        net_spread = 0
        for edge in graph.edges(data=True):
            node1 = edge[0]
            node2 = edge[1]
            node1_plane = graph.nodes[node1]['plane']
            node2_plane = graph.nodes[node2]['plane']
            net_spread += abs(node1_plane - node2_plane)
        design_spread += net_spread
    log(f'Design Spread reduced to {design_spread}\n')
# def kl_partitioning()


# compute the objective (sum of squared plane differences)
def objective_function(planes, node_indices, graph):
    total_difference = 0.0
    for edge in graph.edges():
        u, v = edge
        if u in node_indices and v in node_indices:
            u_index, v_index = node_indices[u], node_indices[v]
            total_difference += (planes[u_index] - planes[v_index]) ** 2
    return total_difference
# def objective_function(planes, node_indices, graph)


# compute the gradient of the objective function
def gradient(planes, node_indices, graph):
    grad = {index: 0.0 for index in node_indices.values()}
    for edge in graph.edges():
        u, v = edge
        if u in node_indices and v in node_indices:
            u_index, v_index = node_indices[u], node_indices[v]
            grad[u_index] += 2 * (planes[u_index] - planes[v_index])
            grad[v_index] -= 2 * (planes[u_index] - planes[v_index])
    return list(grad.values())
# def gradient(planes, node_indices, graph)


def fixed_planes_constraint(planes, fixed_nodes, node_indices):
    # Ensure that certain nodes have fixed 'plane' values
    return np.array([planes[node_indices[node]] - fixed_nodes[node] for node in fixed_nodes])


# update the plane numbers only if it is allowed
def update_planes(net_name, graph, optimized_planes):
    # log('\tinside update_planes\n')
    xy_set = net2xy_set[net_name]
    node_planes = dict()
    for xy in xy_set:
        (x, y) = xy
        planes = list()
        for plane in range (0, MAX_PLANE):
            node_name = f'{x},{y},{plane}'
            prev_plane = graph.nodes[node_name]['plane']
            if node_name in optimized_planes:
                next_plane = optimized_planes[node_name]['plane']
            else: # it is part of a chain
                for movable_object in coor2tile[(x, y)].movable_objects:
                    (start, end) = movable_object
                    if prev_plane > start and prev_plane <= end:
                        for item in planes:
                            if item[1] == start: # if prev_plane is the start (we already added the start of the movable object)
                                beg_node = item[0]
                        next_plane = optimized_planes[beg_node]['plane']
            planes.append((node_name, prev_plane, next_plane))
        node_planes[(x, y)] = planes
    log(f'\t\t(1) node_planes = {node_planes}\n')
    
    for key, value in node_planes.items():
        # Sort the list based on the next plane number
        value.sort(key=lambda x: (x[2], x[1]))
        for i, (_, _, next_plane) in enumerate(value):
            # Normalize the next plane numbers between 0 and 7
            normalized_plane = i
            node_planes[key][i] = (node_planes[key][i][0], node_planes[key][i][1], normalized_plane)

    # Now node_planes contains the adjusted next plane numbers
    log(f'\t\t(2) node_planes = {node_planes}\n')

    log(f'\t\tassigning new values to nodes plane# and update movable objects\n')
    for xy, planes in node_planes.items():
        (x, y) = xy
        
        # check if it is not CLB --> do not update
        # if not coor2tile[(x, y)].instance_name.startswith('clb'): # It is not a CLB
        #     continue

        # check for movable objects --> if zero object, skip
        # if not coor2tile[(x, y)].movable_objects:
        #     continue

        #TODO: assign new plane# to nodes
        for item in planes:
            node_name, prev_plane, next_plane = item
            log(f'\t\t\tnode_name: {node_name}, prev_plane: {prev_plane}, next_plane: {next_plane}\n')
            log(f"\t\t\t(1) graph.nodes[{node_name}]['plane'] = {graph.nodes[node_name]['plane']}\n")
            
            # graph.nodes[node_name]['plane'] = next_plane
            # log(f"\t\t\t(2) graph.nodes[{node_name}]['plane'] = {graph.nodes[node_name]['plane']}\n")
            
            #TODO: update this node in graph of all nets
            for graph_ in net2graph.values():
                if node_name in graph_.nodes():
                    graph_.nodes[node_name]['plane'] = next_plane

            log(f"\t\t\t(2) graph.nodes[{node_name}]['plane'] = {graph.nodes[node_name]['plane']}\n")

        log(f'\t\t\t(1) movable_objects = {coor2tile[(x, y)].movable_objects}\n')
        new_movable_objects = []
        for movable_object in coor2tile[(x,y)].movable_objects:
            (start, end) = movable_object
            for item in planes:
                node_name, prev_plane, next_plane = item
                if prev_plane == start:
                    new_start = next_plane
                    new_end = (end - start) + next_plane
                    new_movable_objects.append((new_start,new_end))
        coor2tile[(x,y)].movable_objects = new_movable_objects
        log(f'\t\t\t(2) movable_objects = {coor2tile[(x,y)].movable_objects}\n')
# def update_planes(net_name, graph, optimized_planes)


# Minimize the objective function using conjugate gradient descent
def CGD():
    log(f'inside CGD:\n\n')
    initial_design_spread = 0
    final_design_spread = 0
    
    for (net_name,graph) in net2graph.items():
        initial_net_spread = 0
        for edge in graph.edges(data=True):
            node1 = edge[0]
            node2 = edge[1]
            node1_plane = graph.nodes[node1]['plane']
            node2_plane = graph.nodes[node2]['plane']
            initial_net_spread += abs(node1_plane - node2_plane)
        initial_design_spread += initial_net_spread
        log(f'\tfor net_name: {net_name} --> initial net spread value = {initial_net_spread}\n')
    log(f'(CGD) initial design spread = {initial_design_spread}\n')

    REPEAT = 2
    for repeat in range(REPEAT):
        for (net_name,graph) in net2graph.items():
            # log(f'\tnet_name = {net_name}\n')
            # Create a mapping between node names and indices
            # node_indices = {node: i for i, node in enumerate(graph.nodes)}
            node_indices = {}
            # Extract the initial 'plane' attribute values **** Excluding nodes not in the beginning of a chain
            initial_planes = []
            i = 0
            for node in graph.nodes:
                x, y = graph.nodes[node]['x'], graph.nodes[node]['y']
                do_not_add = False
                if coor2tile[(x, y)].movable_objects and len(coor2tile[(x, y)].movable_objects) != 8:
                    for movable_object in coor2tile[(x, y)].movable_objects:
                        (start, end) = movable_object
                        if start != end and graph.nodes[node]['plane'] > start and graph.nodes[node]['plane'] <= end:
                            log(f'node not added. node = {node}, i = {i}\n')
                            do_not_add = True
                            break
                if not do_not_add:
                    initial_planes.append(graph.nodes[node]['plane'])
                    node_indices[node] = i
                    i +=1
            initial_planes = np.array(initial_planes)

            fixed_nodes = net2fixed_nodes[net_name]

            # Minimize the objective function using conjugate gradient descent
            result = minimize(objective_function, 
                                x0=initial_planes, args=(node_indices, graph),
                                jac=gradient, 
                                constraints={'type': 'eq', 'fun': fixed_planes_constraint, 'args': (fixed_nodes,node_indices)},
                                method='SLSQP')
            # Extract the optimized plane attributes
            optimized_planes = result.x
            optimized_planes_dict = {}
            i = 0
            for node in graph.nodes:
                x, y = graph.nodes[node]['x'], graph.nodes[node]['y']
                do_not_add = False
                if coor2tile[(x, y)].movable_objects and len(coor2tile[(x, y)].movable_objects) != 8:
                    for movable_object in coor2tile[(x, y)].movable_objects:
                        (start, end) = movable_object
                        if start != end and graph.nodes[node]['plane'] > start and graph.nodes[node]['plane'] <= end:
                            log(f'node should not be in optimized_planes. node = {node}, i = {i}\n')
                            do_not_add = True
                            break
                if not do_not_add:
                    optimized_planes_dict[node] = {'plane': optimized_planes[i]}
                    i += 1
            # optimized_planes_dict = {node: {'plane': optimized_planes[i]} for i, node in enumerate(graph.nodes)}
            # log(f"\tInitial Planes: {dict(graph.nodes(data='plane'))}")
            

            update_planes(net_name=net_name, graph=graph, optimized_planes=optimized_planes_dict)

    for (net_name,graph) in net2graph.items():
        final_net_spread = 0
        for edge in graph.edges(data=True):
            node1 = edge[0]
            node2 = edge[1]
            node1_plane = graph.nodes[node1]['plane']
            node2_plane = graph.nodes[node2]['plane']
            final_net_spread += abs(node1_plane - node2_plane)
        final_design_spread += final_net_spread
        log(f'\tfor net_name: {net_name} --> final net spread value = {final_net_spread}\n')
    log(f'(CGD) final design spread = {final_design_spread}\n')
# def CGD(graph)


# replace FLEs inside a CLB and return the previous object of FLE at dst_num
def FLE_replacement(CLB_block, src_num, dst_num):
    # find the CLB_LR first
    clb_lr = None
    if CLB_block.sub_blocks:
        clb_lr = CLB_block.sub_blocks[0]
        if not clb_lr.instance_name.startswith('clb_lr'):
            log(f"ERROR: this is not a clb_lr block: {clb_lr.name}, {clb_lr.instance_name}\n")
            exit()
    else:
        log(f"ERROR: {CLB_block} does not have clb_lr\n")
        exit()
    # log(f"\tclb_lr = {clb_lr.instance_name}\n")

    src_fle = None
    dst_fle = None

    # log(f"FLEs before replacement:\n")
    for fle in clb_lr.sub_blocks:
        # log(f"\t{fle.instance_name}: {fle.name}\n")
        if fle.instance_name == f'fle[{src_num}]': # find the src fle and clean it in the tile
            src_fle = copy.deepcopy(fle)
            fle.name = 'open'
        elif fle.instance_name == f'fle[{dst_num}]': # find the dst fle and clean it in the tile
            dst_fle = copy.deepcopy(fle)
            fle.name = 'open'
    # log(f"CLB_LR pins:\n")
    for output_port in clb_lr.outputs:
        (port_tag, port_name, pin_list) = output_port
        # for pin in pin_list:
            # log(f"port_name: {port_name},\t pin: {pin}\n")
    # log(f"FLEs after replacement:\n")
    
    # remove dst fle outputs from clb_lr and change src fle output names to dst fle number
    removed_src_pins = [] # list of (port_tag, port_name, pin, pin_index) tuple
    removed_dst_pins = [] # list of (port_tag, port_name, pin, pin_index) tuple
    for output_port in clb_lr.outputs:
        (port_tag, port_name, pin_list) = output_port
        for pin in pin_list:
            if f'fle[{dst_num}]' in pin:
                removed_dst_pins.append((port_tag, port_name, pin, pin_list.index(pin)))
                pin = 'open'
        for pin in pin_list:    
            if f'fle[{src_num}]' in pin:
                removed_src_pins.append((port_tag, port_name, pin, pin_list.index(pin)))
                start_index = pin.find('fle[') + len('fle[')
                end_index = pin.find(']', start_index)
                pin = pin[:start_index] + f'fle[{dst_num}]' + pin[end_index:]
    
    
    # now we can assign src fle to dst fle keeping the fle number unchanged
    for fle in clb_lr.sub_blocks:
        if fle.instance_name == f'fle[{dst_num}]':
            fle = src_fle
            fle.instance_name = f'fle[{dst_num}]'
            fle.name = src_fle.name
    
    # log(f"FLEs after replacement:\n")
    # for fle in clb_lr.sub_blocks:
        # log(f"\t{fle.instance_name}: {fle.name}\n")
    # log(f"CLB_LR pins:\n")
    for output_port in clb_lr.outputs:
        (port_tag, port_name, pin_list) = output_port
        # for pin in pin_list:
            # log(f"port_name: {port_name},\t pin: {pin}\n")
    # return what we removed from the dst fle
    return dst_fle, removed_dst_pins
# def FLE_replacement(CLB_block, src_num, dst_num)


# Flow:
    # set_movable_objects
    # build_graph
        # get_FLUTE_info
        # build_edges
            # taper
        # xxxx cal_raduis       xxxx not used in current implementation
        # xxxx add_fuzzy_edges  xxxx not used in current implementation
    # kl_partitioning
        # cal_cutcost_Dvalues
        # is_valid_swap
        # update_net2sub_graphs
        # swap
    # CGD
        # objective_function
        # gradient
        # update_planes
