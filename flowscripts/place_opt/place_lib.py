from lxml import etree
import sys
import subprocess

debug = 1

top_net_addr            = '' # e.g., ~/run_1/synth_1_1/impl_1_1/packing/apex2_post_synth.net
top_net_ID              = '' # e.g., d49ce4f1618c1873caed8afd806d7a1668c364ee9b7f11d9131dfaf5137ff581
top_array_size          = '' # 106 x 70 logic blocks
block2info              = {} # block_name ==> (x,y,z,layer,block#)
xyzl2block              = {} # (x,y,z,layer) ==> block_name
blocknum2block          = {} # block# ==> block_name



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


# get the list of (z, block_name, block_num) tuples for an specific (x, y, layer)
def get_sub_blocks(x, y, layer):
    sub_blocks = []

    for block_name, block_info in block2info.items():
        (x_,y_,z_,layer_,block_num_) = block_info
        if x == x_ and y == y_ and layer == layer_:
            sub_blocks.append((z_, block_name, block_num_))

    return sub_blocks
# def get_sub_blocks(x, y, layer)


# parse the place file and fill in global variables
def parse_place_file(address):
    global top_net_addr
    global top_net_ID
    global top_array_size

    with open(address, 'r') as file:
        content = file.readlines()
        i = 0

        # read headers
        info = content[0].split()
        top_net_addr = info[1]
        top_net_ID = info[3].split(':')[1] # "SHA256:" is removed
        top_array_size = content[1].split(':')[1]

        # now skip headers
        while not content[i].startswith('#block name'):
            i += 1
        i += 2

        # read the actual content
        while i < len(content):
            columns = content[i].split()
            if len(columns):
                # Extract values and append to the lists
                block_name = columns[0]
                x                               = int(columns[1])
                y                               = int(columns[2])
                z                               = int(columns[3])
                layer                           = int(columns[4])
                block_number                    = int(columns[5].strip('#'))
                block2info[block_name]          = (x, y, z, layer, block_number)
                xyzl2block[(x, y, z, layer)]    = block_name
                blocknum2block[block_number]    = block_name
                i += 1
    return top_array_size
# def parse_place_file(address)


# write the new place file into "address" using global variables
def write_place_file(place_addr, net_addr):
    global top_net_addr
    global top_net_ID

    # change the address of net file
    top_net_addr = net_addr
    
    # change the sha256 of the new net file
    try:
        result = subprocess.run(["sha256sum", net_addr], capture_output=True, text=True, check=True)
        sha256_checksum = result.stdout.split()[0]
        top_net_ID = sha256_checksum
    except subprocess.CalledProcessError as e:
        print(f"Error calculating SHA-256 checksum: {e}")
        return None

    with open(place_addr, 'w') as output_file:
        output_file.write(f"Netlist_File: {top_net_addr} Netlist_ID: SHA256:{top_net_ID}\n")
        output_file.write(f"Array size:{top_array_size}\n")
        output_file.write("")
        output_file.write("#block name\tx\ty\tsubblk\tlayer\tblock number\n")
        output_file.write("#----------\t--\t--\t------\t-----\t------------\n")
        for (block_name, block_info) in block2info.items():
            (x,y,z,layer,block_num) = block_info
            output_file.write(f"{block_name}\t\t{x}\t{y}\t{z}\t\t{layer}\t\t#{block_num}\n")
# def write_place_file(place_addr, net_addr)


# def top_logic():
#     place_address = "/nfs_project/castor/mabbaszadeh/workarea/ISFPGA/apex2/apex2_golden/apex2/run_1/synth_1_1/impl_1_1/placement/apex2_post_synth.place"
#     parse_place_file(place_address)
#     write_place_file("place_out.place")
# def top_logic()