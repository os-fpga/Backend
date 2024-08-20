from pack_lib import *
from place_lib import *
from optimization import *
import pickle

max_x = 0
max_y = 0

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
        print(f"{obj_name} object file does not exist.\n")
    return loaded_obj
# load_object

# read the new .net file and build a new graph --> Then compare the built graph with the previous optimized graph to make sure everything is same.
def verify_out_pack(old_graph, old_xyplane2fle_name, old_net2edges, pack_addr, place_addr):
    global max_x, max_y
    log('Verifying the generated .net file by running the flow again:\n')
    
    top_array_size = parse_place_file(place_addr)

    pattern = r'\b\d+\b' # Define a regular expression pattern to extract max_x and max_y from placement file
    matches = re.findall(pattern, top_array_size)
    
    if len(matches) >= 2:
        max_x = int(matches[0])
        max_y = int(matches[1])
    else:
        print(f"ERROR: Could not find max_x, max_y in the {top_array_size}")
        exit()
    
    parse_net_file(pack_addr)
    make_pins()
    make_nets()

    print("net2edges_pin_obj\n\n")
    for (net_name,edges) in net2edges_pin_obj.items():
        if net_name not in old_net2edges:
            log(f'ERROR: net_name: {net_name} not in optimized net2edges_pin_obj\n')
            exit()
        optimized_edges = old_net2edges[net_name]
        print(f"net_name:\t{net_name}")
        for edge in edges:
            (plane, src_x, src_y, input_object, dst_x, dst_y) = edge
            FOUND = False
            for optimized_edge in optimized_edges:
                (plane_, src_x_, src_y_, input_object_, dst_x_, dst_y_) = optimized_edge
                if (src_x_, src_y_, dst_x_, dst_y_) == (src_x, src_y, dst_x, dst_y):
                    FOUND = True
            if not FOUND:
                log(f'ERROR: net_name: {net_name}, edge: {edge} not in optimized_edges: {optimized_edges}\n')
                exit()
            print(f"OPIN ({src_x},{src_y}) --> IPIN ({dst_x},{dst_y})")
            print(f"OPIN plane#({plane}) --> IPIN ({input_object.block.name}, {input_object.name}, {input_object.assigned_name})\t plane#({input_object.sub_ys})\n")
        print("\n")
    print("\n")


    set_movable_objects()
    build_graph(max_x, max_y)
    
    
    
    # the actual verification
    VERIFIED = 1
    for node in old_graph.nodes():
        x, y, old_plane, new_plane = Graph.nodes[node]['x'], Graph.nodes[node]['y'], old_graph.nodes[node]['plane'], Graph.nodes[node]['plane']
        if (x,y,old_plane) in old_xyplane2fle_name:
            if old_xyplane2fle_name[x,y,old_plane] != xyplane2fle_name[x,y,old_plane]:
                log(f"\tERROR: for node: {node}, with optimizied_plane = {old_graph.nodes[node]['plane']}, and current_plane = {Graph.nodes[node]['plane']}, FLEs are different:\n")
                log(f"\t\t old_xyplane2fle_name[{x,y,old_plane}] = {old_xyplane2fle_name[x,y,old_plane]}\n")
                log(f"\t\t xyplane2fle_name[{x,y,new_plane}] = {xyplane2fle_name[x,y,new_plane]}\n")
                VERIFIED = -1
                exit()
            # if
        # if
    # for
    return VERIFIED
# def verify_out_pack(old_graph, old_xyplane2fle_name, pack_addr)


def run_optimization(pack_addr, place_addr, pack_out, place_out):
    global max_x, max_y
    top_array_size = parse_place_file(place_addr)

    pattern = r'\b\d+\b' # Define a regular expression pattern to extract max_x and max_y from placement file
    matches = re.findall(pattern, top_array_size)
    
    if len(matches) >= 2:
        max_x = int(matches[0])
        max_y = int(matches[1])
    else:
        print(f"ERROR: Could not find max_x, max_y in the {top_array_size}")
        exit()

    parse_net_file(pack_addr)
    make_pins()
    make_nets()
    find_internal_FLE_connection()

    print("net2edges_pin_obj\n\n")
    for (net_name,edges) in net2edges_pin_obj.items():
        print(f"net_name:\t{net_name}")
        for edge in edges:
            (plane, src_x, src_y, input_object, dst_x, dst_y) = edge
            print(f"OPIN ({src_x},{src_y}) --> IPIN ({dst_x},{dst_y})")
            print(f"OPIN plane#({plane}) --> IPIN ({input_object.block.name}, {input_object.name}, {input_object.assigned_name})\t plane#({input_object.sub_ys})\n")
        print("\n")
    print("\n")

    save_object(net2edges_pin_obj, 'optimizied_net2edges_pin_obj')
    set_movable_objects()
    build_graph(max_x, max_y)
    
    ################### if you want to run the KL algorithm --> uncomment the following: ###################
    kl_partitioning()
    
    ################### if you want to run the CG algorithm --> uncomment the following: ################### 
    # CGD()

    # Note that if you run both KL and CG alogorithms, the latter will use the optimized results of the former. Everything is kept in Graph
    # you can also run CG first and then run KL. There is no restriction and KL will use the results of CG. 

    # manual test of re-ordering FLE planes --> If running KL or CG, comment these
    # node0 = '53,2,0'
    # node1 = '53,2,1'
    # node2 = '53,2,2'
    # node3 = '53,2,3'
    # node4 = '53,2,4'
    # node5 = '53,2,5'
    # node6 = '53,2,6'
    # node7 = '53,2,7'
    # Graph.nodes[node0]['plane'] = 7
    # Graph.nodes[node1]['plane'] = 6
    # Graph.nodes[node2]['plane'] = 5
    # Graph.nodes[node3]['plane'] = 4
    # Graph.nodes[node4]['plane'] = 3
    # Graph.nodes[node5]['plane'] = 2
    # Graph.nodes[node6]['plane'] = 1
    # Graph.nodes[node7]['plane'] = 0

    # print("\n\nnet2edges_coor\n\n")
    # for (net_name,edges) in net2edges_coor.items():
    #     print(f"net_name:\t{net_name}\n")
    #     for edge in edges:
    #         (src_x, src_y, dst_x, dst_y) = edge
    #         print(f"OPIN ({src_x},{src_y}) --> IPIN ({dst_x},{dst_y})")
    #     print("\n")

    # actual re-ordering the FLEs based on updated values in Graph
    reorder_FLEs(graph=Graph)
    # create a new .net file
    # packer_output_addr = '/nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/pack_out.net'
    packer_output_addr = pack_out
    write_net_file(packer_output_addr)

    graph_copy = Graph.copy()
    save_object(graph_copy, 'optimized_graph')
    old_xyplane2fle_name = xyplane2fle_name.copy()
    save_object(old_xyplane2fle_name, 'optimized_xyplane2fle_name')
    
    # create the new placement file 
    # placer_output_addr = place_addr
    # placer_output_addr = '/nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/place_out.place'
    placer_output_addr = place_out
    write_place_file(placer_output_addr, packer_output_addr)
# def run_optimization(pack_addr, place_addr)


def top_logic():
    pack_addr = ''
    place_addr = ''
    pack_out = ''
    place_out = ''
    arguments = sys.argv
    if len(arguments) > 1:
        flag = arguments[1]
        pack_addr = arguments[2]
        place_addr = arguments[3]
        if flag == '-o':
            if len(arguments) > 3:
                pack_out = arguments[4]
                place_out = arguments[5]
            else:
                log(f"ERROR: bad arguments for optimization. Expect (-o/-v), packer and placer addresses as well as pack/place output addresses.\narguments: {arguments}\n")
                exit()
    else:
        log(f"ERROR: bad arguments. Expect (-o/-v), packer and placer addresses.\narguments: {arguments}\n")
        exit()
    
    # optimization flow:
    if flag == '-o':
        run_optimization(pack_addr, place_addr, pack_out, place_out)
        print('End of optimization')
    
    elif flag == '-v':
        optimized_graph             = load_object('optimized_graph')
        optimized_xyplane2fle_name  = load_object('optimized_xyplane2fle_name')
        optimizied_net2edges_pin_obj= load_object('optimizied_net2edges_pin_obj')
        VERIFIED = verify_out_pack(optimized_graph, optimized_xyplane2fle_name, optimizied_net2edges_pin_obj, pack_addr, place_addr)
        if VERIFIED:
            print('new .net file is verified against optimized graph results.\nEnd of program')
# def top_logic()

top_logic()


    
# There are two ways to run this code: 
    # 1. for the purpose of optimization. 
    # 2. (Only after optimization) for the purpose of verification to make sure new generated .net file is consistent with the optimized graph

# optimization:
# python3 top.py -o <ORIGINAL .net FILE> <ORIGINAL .place FILE> <OUTPUT OPTIMIZED .net FILE> <OUTPUT OPTIMIZED .place FILE> 
# note that the only thing that changes in .net file is the SHA256SUM of .net file. Nothing else should be changed becase we do not want to run placement stage again
# Example (py3 is an alias in my machine):
# py3 top.py -o /nfs_project/castor/mabbaszadeh/workarea/ArchBench_MCNC_VTR/Testcases/apex4/apex4_golden/apex4/run_1/synth_1_1/impl_1_1/packing/apex4_post_synth.net /nfs_project/castor/mabbaszadeh/workarea/ArchBench_MCNC_VTR/Testcases/apex4/apex4_golden/apex4/run_1/synth_1_1/impl_1_1/placement/apex4_post_synth.place /nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/pack_out.net /nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/place_out.place 2> test_o.log > test_o.out

# verification:
# python3 top.py -v <OUTPUT OPTIMIZED .net FILE> <OUTPUT OPTIMIZED .place FILE>
# in the verification mode, the program will be using .pkl files stored in /objects directory. These objects are produced at the end of optimization flow.
# Example:
# py3 top.py -v /nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/pack_out.net /nfs_project/castor/mabbaszadeh/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/place_opt/place_out.place 2> test_v.log > test_v.out