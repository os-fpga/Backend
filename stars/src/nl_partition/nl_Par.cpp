#include "nl_Par.h"
#include "globals.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <thread>

namespace nlp {

Par::Par(t_pack_molecule* molecule_head) {
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  for (auto cur_molecule = molecule_head; cur_molecule; cur_molecule = cur_molecule->next) {
    molecules.push_back(cur_molecule);
    numberOfMolecules++;
  }

  for (auto netId : myNetlist.nets()) {
    numberOfNets++;
  }

  for (auto blockId : myNetlist.blocks()) {
    numberOfAtoms++;
  }

  atomBlockIdToMoleculeId = new int[numberOfAtoms];
  moleculeIdToName = new std::string[numberOfMolecules + 1];
  partition_array = new int[numberOfMolecules + 1];
  for (int i = 0; i < numberOfMolecules; i++) {
    partition_array[i] = 1;
  }

  for (int i = 0; i < numberOfAtoms; i++) {
    atomBlockIdToMoleculeId[i] = -1;
  }

  {
    int i = 0;
    for (auto molecule : molecules) {
      std::string name_block = "";
      for (auto abid : molecule->atom_block_ids) {
        name_block += myNetlist.block_name(abid);
        int bid = size_t(abid);
        if (bid < 0 || bid >= numberOfAtoms) continue;
        VTR_ASSERT(atomBlockIdToMoleculeId[bid] == -1);
        atomBlockIdToMoleculeId[bid] = i;
      }
      moleculeIdToName[i] = name_block;
      i++;
    }
  }

  for (int i = 0; i < numberOfAtoms; i++) {
    VTR_ASSERT(atomBlockIdToMoleculeId[i] != -1);
  }
}

static bool read_exe_link(const std::string& path, std::string& out) {
  out.clear();
  if (path.empty()) return false;

  const char* cs = path.c_str();
  struct stat sb;
  if (::stat(cs, &sb)) return false;
  if (not(S_IFREG & sb.st_mode)) return false;
  if (::lstat(cs, &sb)) return false;
  if (not(S_ISLNK(sb.st_mode))) {
    out = "(* NOT A LINK : !S_ISLNK *)";
    return false;
  }

  char buf[8192];
  buf[0] = 0;
  buf[1] = 0;
  buf[8190] = 0;
  buf[8191] = 0;

  int64_t numBytes = ::readlink(cs, buf, 8190);
  if (numBytes == -1) {
    perror("readlink()");
    return false;
  }
  buf[numBytes] = 0;
  out = buf;

  return true;
}

static std::string get_MtKaHyPar_path() {
  int pid = ::getpid();
  std::string selfPath, exe_link;

  exe_link.reserve(512);
  exe_link = "/proc/";
  exe_link += std::to_string(pid);
  exe_link += "/exe";
  if (!read_exe_link(exe_link, selfPath)) return {};

  // replace 'vpr' by 'MtKaHyPar' in selfPath
  if (selfPath == "vpr") return "MtKaHyPar";
  size_t len = selfPath.length();
  if (len < 4 || len > 10000) return {};
  const char* cs = selfPath.c_str();
  int i = len - 1;
  for (; i >= 0; i--) {
    if (cs[i] == '/') break;
  }
  if (i < 1 || i == int(len) - 1) return {};

  std::string result{selfPath.begin(), selfPath.begin() + i + 1};
  result += "MtKaHyPar";
  return result;
}

void Par::Bi_Partion(int partition_index) {
  //making the intermediate nodes
  std::vector<int> MoleculesToIntermediate;
  std::vector<int> IntermediateToMolecules;
  int num_intermediate = 0;
  for (int i = 0; i < numberOfMolecules; i++) {
    if (partition_array[i] == partition_index) {
      MoleculesToIntermediate.push_back(num_intermediate++);
      IntermediateToMolecules.push_back(i);
    } else {
      MoleculesToIntermediate.push_back(-1);
    }
  }
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  std::vector<std::string> uniqueLines;
  std::vector<int> lineCounts;

  int nextIndexForMolecule = numberOfMolecules + 1;

  bool addNewNodeAndUseWeightsInHmetis = false;

  std::vector<int> molecule_size;
  int sum_malecule_size = 0;
  for (auto netId : myNetlist.nets()) {
    //VTR_LOG("NET ID: %d\n", size_t(netId));
    std::vector<int> newLine;
    std::vector<int> criticality;
    int sum_criticality = 0;
    AtomBlockId driverBlockId = myNetlist.net_driver_block(netId);
    int moleculeId = atomBlockIdToMoleculeId[size_t(driverBlockId)];
    if (partition_array[moleculeId] == partition_index) {
      newLine.push_back(MoleculesToIntermediate[moleculeId] + 1);
    }
    double maxCriticality = 0;
    for (auto pin_id : myNetlist.net_pins(netId)) {
      //VTR_LOG("pin criticality: %f\n", pinCriticality);

      auto port_id = myNetlist.pin_port(pin_id);
      auto blk_id = myNetlist.port_block(port_id);
      if (blk_id == driverBlockId) continue;
      //VTR_LOG("%d ", size_t(blk_id));
      //hmetisFile << size_t(blk_id)+1 << " ";
      moleculeId = atomBlockIdToMoleculeId[size_t(blk_id)];
      if (partition_array[moleculeId] != partition_index) {
        continue;
      }
      if (std::find(newLine.begin(), newLine.end(), moleculeId + 1) == newLine.end()) {
        newLine.push_back(MoleculesToIntermediate[moleculeId] + 1);
      }
    }
    if (newLine.size() <= 1) {
      continue;
    }
    int lineWeight = 1;
    molecule_size.push_back(newLine.size() - 1);
    sum_malecule_size += newLine.size() - 1;
    if (newLine.size() > 1) {
      std::sort(newLine.begin(), newLine.end());
      std::string s =
          std::accumulate(newLine.begin() + 1, newLine.end(), std::to_string(newLine[0]),
                          [](const std::string& a, int b) { return a + " " + std::to_string(b); });
      auto pointer = std::find(uniqueLines.begin(), uniqueLines.end(), s);
      if (pointer == uniqueLines.end()) {
        //netLines.push_back(newLine);
        lineCounts.push_back(lineWeight);
        uniqueLines.push_back(s);
      } else {
        lineCounts[pointer - uniqueLines.begin()] += lineWeight;
      }
    }
  }

  std::ofstream hmetisFile;
  hmetisFile.open("hmetis.txt", std::ofstream::out);
  hmetisFile << uniqueLines.size() << " " << num_intermediate << " " << 11 << std::endl;
  {
    int i = 0;
    for (auto line : uniqueLines) {
      hmetisFile << lineCounts[i] << " " << line << std::endl;
      i++;
    }
    for (int j = 0; j < IntermediateToMolecules.size(); j++) {
      // for (auto molecule : molecules) {
      hmetisFile << molecules[IntermediateToMolecules[j]]->atom_block_ids.size() << std::endl;
      // }
    }
  }
  hmetisFile.close();
  int numberOfClusters = 2;
  if (numberOfClusters == 1) numberOfClusters = 2;

  //numberOfClusters = 3;
  char commandToExecute[6000] = {};  // unix PATH_MAX is 4096
  unsigned num_cpus = std::thread::hardware_concurrency();
  int num_threads = (int)(num_cpus / 2) > 1 ? (int)(num_cpus / 2) : 1;

  std::string MtKaHyPar_path = get_MtKaHyPar_path();
  VTR_LOG("MtKaHPar PATH: %s\n", MtKaHyPar_path.c_str());

  sprintf(
      commandToExecute,
      "%s -h hmetis.txt --preset-type=quality -t %d -k %d -e 0.01 -o soed --enable-progress-bar=true --show-detailed-timings=true --verbose=true --write-partition-file=true",
      MtKaHyPar_path.c_str(), num_threads, numberOfClusters);
  VTR_LOG("MtKaHPar COMMAND: %s\n", commandToExecute);

  int code = system(commandToExecute);
  VTR_ASSERT_MSG(code == 0, "Running MtKaHyPar failed with non-zero code");

  // delete commandToExecute;

  char newFilename[1024] = {};
  // sprintf(newFilename, "hmetis.txt.part.%d", numberOfClusters);
  sprintf(newFilename, "hmetis.txt.part%d.epsilon0.01.seed0.KaHyPar", numberOfClusters);
  // char* commandToExecute = new char[1000];
  // sprintf(commandToExecute, "~/hmetis hmetis.txt %d 1 20 4 1 3 0 0 ", numberOfClusters);

  // VTR_LOG("HMETIS COMMAND: %s\n", commandToExecute);
  // system(commandToExecute);
  //exit(0);
  // char* newFilename = new char[100];
  // sprintf(newFilename, "hmetis.txt.part.%d", numberOfClusters);
  // sprintf(newFilename, "hmetis_96_1.txt", numberOfClusters);
  std::ifstream hmetisOutFile;
  hmetisOutFile.open(newFilename);

  // int* clusterOfMolecule = new int[numberOfMolecules];

  for (int i = 0; i < num_intermediate; i++) {
    int clusterId;
    hmetisOutFile >> clusterId;
    if (clusterId == 0) {
      partition_array[IntermediateToMolecules[i]] = partition_array[IntermediateToMolecules[i]] * 2;
    } else {
      partition_array[IntermediateToMolecules[i]] = partition_array[IntermediateToMolecules[i]] * 2 + 1;
    }

    //invertedIndexOfClustersAndAtomBlockIds[clusterId].push_back(i);
    // for (AtomBlockId abid : molecules[i]->atom_block_ids) {
    //     if (size_t(abid) < 0 || size_t(abid) >= numberOfAtoms) continue;
    //     //atgs[clusterId].group_atoms.push_back(abid);
    //     if (size_t(abid) >= numberOfAtoms) {
    //         std::cout << numberOfAtoms << "\t" << size_t(abid) << std::endl;
    //         exit(0);
    //     }
    //     atomBlockIdToCluster[size_t(abid)] = clusterId;
    // }
    //AtomBlockId _aid(i);
    //atgs[clusterId].group_atoms.push_back(_aid);
  }
  hmetisOutFile.close();
}

struct partition_position {
  int x1 = 0;
  int x2 = 0;
  int y1 = 0;
  int y2 = 0;
  bool direction = true;  //false means from left to right and true means from top to bottom
};


void Par::recursive_partitioning(int molecule_per_partition) {
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  auto& device_ctx = g_vpr_ctx.device();
  auto& grid = device_ctx.grid;
  std::vector<int> partion_size;
  partion_size.push_back(-1);
  int cnt = 1;
  int max_itration = 1;
  while (cnt <= max_itration) {
    int sum_partition = 0;
    for (int j = 0; j < numberOfMolecules; j++) {
      if (partition_array[j] == cnt) {
        sum_partition++;
      }
    }
    // if(sum_partition==0){
    //     break;
    // }
    partion_size.push_back(sum_partition);
    if (sum_partition > molecule_per_partition) {
      VTR_LOG("start Bipartitioning\n");
      Bi_Partion(cnt);
      VTR_LOG("end Bipartitioning\n");
      max_itration = 2 * cnt + 1;
    } else {
      partitions.push_back(cnt);
    }
    cnt++;
  }
  std::vector<partition_position*> pp_array;
  partition_position* pp = new partition_position;
  pp->x1 = 2;
  pp->x2 = 103;
  pp->y1 = 2;
  pp->y2 = 67;
  // VTR_LOG("FPGA sized to %zu x %zu (%s)\n", grid.width(), grid.height(), grid.name().c_str());
  // exit(0);
  // VTR_LOG("\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",pp->x1,pp->y1,pp->x2,pp->y2);
  pp_array.push_back(nullptr);
  pp_array.push_back(pp);
  for (int i = 2; i <= max_itration; i++) {
    pp = new partition_position;
    pp_array.push_back(pp);
    if (partion_size[i] == 0) {
      continue;
    }
    int parent = i / 2;
    // VTR_LOG("parent  = %d,child =  %d\n",partion_size[parent],partion_size[2*parent]);
    float division = float(partion_size[parent]) / float(partion_size[2 * parent]);
    // float division = 2.0;
    // VTR_LOG("before parent %d %d\n",i,parent);
    partition_position* pp_parent = pp_array[parent];
    // VTR_LOG("after parent\n");
    if (pp_parent->direction == false) {
      pp->direction = true;
      pp->x1 = pp_parent->x1;
      pp->x2 = pp_parent->x2;
      if (i == parent * 2) {
        pp->y1 = pp_parent->y1;
        pp->y2 = int((float((pp_parent->y2 - pp_parent->y1)) / division) + pp_parent->y1);
      } else {
        pp->y1 = int((float((pp_parent->y2 - pp_parent->y1)) / division) + pp_parent->y1);
        pp->y2 = pp_parent->y2;
      }

    } else {
      pp->direction = false;
      pp->y1 = pp_parent->y1;
      pp->y2 = pp_parent->y2;
      if (i == parent * 2) {
        pp->x1 = pp_parent->x1;
        pp->x2 = int((float((pp_parent->x2 - pp_parent->x1)) / division) + pp_parent->x1);
      } else {
        pp->x1 = int((float((pp_parent->x2 - pp_parent->x1)) / division) + pp_parent->x1);
        pp->x2 = pp_parent->x2;
      }
    }
  }

  // for(int i=1;i<=max_itration;i++){
  //     VTR_LOG("\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",pp_array[i]->x1,pp_array[i]->y1,pp_array[i]->x2,pp_array[i]->y2);
  // }
  FILE* file;
  file = fopen("vpr_constraints.xml", "w");  // Open the file for writing

  fprintf(file, "<vpr_constraints tool_name=\"vpr\">\n");
  fprintf(file, "\t<partition_list>\n");
  for (unsigned int i = 0; i < partitions.size(); i++) {
    // VTR_LOG("%d %d %d\n",i,int(pow(2,num_partitions)),i-int(pow(2,num_partitions)));
    fprintf(file, "\t<partition name=\"Part%d\">\n", i);
    for (int j = 0; j < numberOfMolecules; j++) {
      if (partition_array[j] == partitions[i]) {
        for (AtomBlockId abid : molecules[j]->atom_block_ids) {
          if (size_t(abid) < 0 || size_t(abid) >= numberOfAtoms ||
              myNetlist.block_type(abid) != AtomBlockType::BLOCK)
            continue;
          //atgs[clusterId].group_atoms.push_back(abid);
          if (size_t(abid) >= numberOfAtoms) {
            std::cout << numberOfAtoms << "\t" << size_t(abid) << std::endl;
            exit(0);
          }
          fprintf(file, "\t\t<add_atom name_pattern=\"%s\"/>\n", myNetlist.block_name(abid).c_str());
        }
      }
    }
    fprintf(file, "\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",
            pp_array[partitions[i]]->x1 - 1, pp_array[partitions[i]]->y1, pp_array[partitions[i]]->x2 + 1,
            pp_array[partitions[i]]->y2);
    fprintf(file, "\t</partition>\n");
  }

  fprintf(file, "\t</partition_list>\n");
  fprintf(file, "</vpr_constraints>\n");
  if (file == NULL) {
    perror("Error opening the file");
    exit(0);
  }

  // Write some text to the file

  // Close the file
  fclose(file);
}

}

