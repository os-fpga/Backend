#include "nl_partition/nl_Par.h"
#include "file_readers/pinc_Fio.h"
#include "globals.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <thread>

namespace pln {

using namespace fio;

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

uint Par::countMolecules(t_pack_molecule* molecule_head) {
  uint cnt = 0;
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  for (auto cur_mol = molecule_head; cur_mol; cur_mol = cur_mol->next) {
    cnt++;
  }
  return cnt;
}

bool Par::init(t_pack_molecule* molecule_head) {
  assert(molecule_head);
  numMolecules_ = numNets_ = numAtoms_ = 0;
  set_ltrace(3);
  auto tr = ltrace();

  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  for (auto cur_mol = molecule_head; cur_mol; cur_mol = cur_mol->next) {
    molecules_.push_back(cur_mol);
    numMolecules_++;
  }
  for (auto netId : myNetlist.nets()) {
    if (tr >= 9)
      lprintf("\t    netId= %zu\n", size_t(netId));
    numNets_++;
  }
  for (auto blockId : myNetlist.blocks()) {
    if (tr >= 9)
      lprintf("\t    blockId= %zu\n", size_t(blockId));
    numAtoms_++;
  }

  if (tr >= 2) {
    lprintf("pln::Par::init  numMolecules_= %u  numNets_= %u  numAtoms_= %u\n",
              numMolecules_, numNets_, numAtoms_);
  }

  atomBlockIdToMolId_ = new int[numAtoms_];
  molIdToName_ = new string[numMolecules_ + 1];
  partition_array_ = (uint*) ::calloc(numMolecules_ + 1, sizeof(uint));
  for (uint i = 0; i < numMolecules_; i++) {
    partition_array_[i] = 1;
  }

  for (uint i = 0; i < numAtoms_; i++) {
    atomBlockIdToMolId_[i] = -1;
  }

  {
    int i = 0;
    for (auto molecule : molecules_) {
      string name_block = "";
      for (auto abid : molecule->atom_block_ids) {
        name_block += myNetlist.block_name(abid);
        uint bid = size_t(abid);
        if (bid >= numAtoms_) continue;
        VTR_ASSERT(atomBlockIdToMolId_[bid] == -1);
        atomBlockIdToMolId_[bid] = i;
      }
      molIdToName_[i] = name_block;
      i++;
    }
  }

  for (uint i = 0; i < numAtoms_; i++) {
    VTR_ASSERT(atomBlockIdToMolId_[i] != -1);
  }
  return true;
}

Par::~Par() {
  // p_free(partition_array_);
}

static bool read_exe_link(const string& path, string& out) {
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

static string get_MtKaHyPar_path() {
  int pid = ::getpid();
  string selfPath, exe_link, result;

  const char* es = ::getenv("RSBE_KHP_EXE");
  if (es && ::strlen(es) > 1) {
    result = es;
    if (ltrace() >= 2)
      lprintf("got KHP_EXE=  %s  from env variable RSBE_KHP_EXE\n", es);
  }
  else {
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

    string sp{selfPath.begin(), selfPath.begin() + i + 1};
    result += sp;
    result += "MtKaHyPar";
  }

  if (ltrace() >= 2)
    lprintf("KHP_EXE: result= %s\n", result.c_str());

  bool ok = Fio::regularFileExists(result);
  if (not ok) {
    lprintf("\n[Error] KHP_EXE=  %s  does not exist\n", result.c_str());
    cerr << "\n[Error] no such file: " << result << endl;
    result.clear();
  }

  return result;
}

bool Par::split(uint partition_index) {
  splitCnt_++;
  auto tr = ltrace();
  if (tr >= 3)
    lprintf("+Par::split( partition_index= %u ) #%u\n", partition_index, splitCnt_);

  //making the intermediate nodes
  vector<int> MoleculesToIntermediate;
  vector<int> IntermediateToMolecules;
  int num_intermediate = 0;
  for (uint i = 0; i < numMolecules_; i++) {
    if (partition_array_[i] == partition_index) {
      MoleculesToIntermediate.push_back(num_intermediate++);
      IntermediateToMolecules.push_back(i);
    } else {
      MoleculesToIntermediate.push_back(-1);
    }
  }
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  vector<string> uniqueLines;
  vector<int> lineCounts;

  //// uint nextIndexForMolecule = numMolecules_ + 1;
  //// bool addNewNodeAndUseWeightsInHmetis = false;

  vector<int> molecule_size;
  int sum_malecule_size = 0;
  for (auto netId : myNetlist.nets()) {
    //VTR_LOG("NET ID: %d\n", size_t(netId));
    vector<int> newLine;
    vector<int> criticality;
    //// int sum_criticality = 0;
    AtomBlockId driverBlockId = myNetlist.net_driver_block(netId);
    int moleculeId = atomBlockIdToMolId_[size_t(driverBlockId)];
    if (partition_array_[moleculeId] == partition_index) {
      newLine.push_back(MoleculesToIntermediate[moleculeId] + 1);
    }
    //// double maxCriticality = 0;
    for (auto pin_id : myNetlist.net_pins(netId)) {
      //VTR_LOG("pin criticality: %f\n", pinCriticality);

      auto port_id = myNetlist.pin_port(pin_id);
      auto blk_id = myNetlist.port_block(port_id);
      if (blk_id == driverBlockId) continue;
      moleculeId = atomBlockIdToMolId_[size_t(blk_id)];
      if (partition_array_[moleculeId] != partition_index) {
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
      string s =
          std::accumulate(newLine.begin() + 1, newLine.end(), std::to_string(newLine[0]),
                          [](const string& a, int b) { return a + " " + std::to_string(b); });
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

  string hmetis_fn = str::concat("hmetis_", std::to_string(splitCnt_), ".txt");
  if (tr >= 3)
    lprintf("hmetis_file_name= %s\n", hmetis_fn.c_str());
  std::ofstream hmetisFile(hmetis_fn);
  if (!hmetisFile) {
    const char* fn = hmetis_fn.c_str();
    VTR_LOG("Bi-Partition with MtKaHPar FAILED: could not open file for writing: %s\n", fn);
    fprintf(stderr,
        "[Error] Bi-Partition with MtKaHPar FAILED: could not open file for writing: %s\n", fn);
    partitions_.clear();
    return false;
  }
  if (tr >= 3) {
    lprintf("hmetis header: %zu %i 11\n", uniqueLines.size(), num_intermediate);
  }
  hmetisFile << uniqueLines.size() << ' ' << num_intermediate << ' ' << 11 << endl;
  for (size_t i = 0; i < uniqueLines.size(); i++) {
    const string& line = uniqueLines[i];
    hmetisFile << lineCounts[i] << " " << line << endl;
  }
  for (uint j = 0; j < IntermediateToMolecules.size(); j++) {
    // for (auto molecule : molecules_) {
    hmetisFile << molecules_[IntermediateToMolecules[j]]->atom_block_ids.size() << endl;
    // }
  }
  hmetisFile.close();

  int K = 2;

  char commandToExecute[6000] = {};  // unix PATH_MAX is 4096
  uint num_cpus = std::thread::hardware_concurrency();
  int num_threads = (int)(num_cpus / 2) > 1 ? (int)(num_cpus / 2) : 1;

  string MtKaHyPar_path = get_MtKaHyPar_path();
  VTR_LOG("MtKaHPar PATH: %s\n", MtKaHyPar_path.c_str());
  if (MtKaHyPar_path.empty()) {
    VTR_LOG("Bi-Partition with MtKaHPar FAILED: MtKaHyPar executable not found\n");
    cerr << "[Error] Bi-Partition with MtKaHPar FAILED:  MtKaHyPar executable not found" << endl;
    partitions_.clear();
    return false;
  }

  sprintf(
      commandToExecute,
      "%s -h %s --preset-type=quality -t %d -k %d -e 0.01 -o soed --enable-progress-bar=true --show-detailed-timings=false --verbose=true --write-partition-file=true",
      MtKaHyPar_path.c_str(), hmetis_fn.c_str(), num_threads, K);
  VTR_LOG("MtKaHPar COMMAND: %s\n", commandToExecute);

  int code = system(commandToExecute);
  if (code == 0) {
    VTR_LOG("MtKaHPar succeeded.\n");
  } else {
    VTR_LOG("MtKaHPar FAILED: exit code = %i\n", code);
    cerr << "[Error] MtKaHPar FAILED: exit code = " << code << endl;
    partitions_.clear(); // empty partitions_ mean failure for the caller
    return false;
  }

  char newFilename[4100] = {};
  sprintf(newFilename, "%s.part%d.epsilon0.01.seed0.KaHyPar", hmetis_fn.c_str(), K);

  // VTR_LOG("HMETIS COMMAND: %s\n", commandToExecute);
  // system(commandToExecute);
  //exit(0);
  // char* newFilename = new char[100];
  // sprintf(newFilename, "hmetis.txt.part.%d", K);
  // sprintf(newFilename, "hmetis_96_1.txt", K);
  std::ifstream hmetisOutFile;
  hmetisOutFile.open(newFilename);

  // int* clusterOfMolecule = new int[numMolecules_];

  for (int i = 0; i < num_intermediate; i++) {
    int clusterId;
    hmetisOutFile >> clusterId;
    if (clusterId == 0) {
      partition_array_[IntermediateToMolecules[i]] = partition_array_[IntermediateToMolecules[i]] * 2;
    } else {
      partition_array_[IntermediateToMolecules[i]] = partition_array_[IntermediateToMolecules[i]] * 2 + 1;
    }

    //invertedIndexOfClustersAndAtomBlockIds[clusterId].push_back(i);
    // for (AtomBlockId abid : molecules_[i]->atom_block_ids) {
    //     if (size_t(abid) < 0 || size_t(abid) >= numAtoms_) continue;
    //     //atgs[clusterId].group_atoms.push_back(abid);
    //     if (size_t(abid) >= numAtoms_) {
    //         cout << numAtoms_ << "\t" << size_t(abid) << endl;
    //         exit(0);
    //     }
    //     atomBlockIdToCluster[size_t(abid)] = clusterId;
    // }
    //AtomBlockId _aid(i);
    //atgs[clusterId].group_atoms.push_back(_aid);
  }
  hmetisOutFile.close();
  return true;
}

namespace {

struct partition_position {
  int x1 = 0;
  int x2 = 0;
  int y1 = 0;
  int y2 = 0;
  bool direction = true;  //false means from left to right and true means from top to bottom

  partition_position() noexcept = default;
};

}

bool Par::do_partitioning(int molecule_per_partition) {
  AtomNetlist myNetlist = g_vpr_ctx.atom().nlist;
  //// auto& device_ctx = g_vpr_ctx.device();
  //// auto& grid = device_ctx.grid;
  vector<int> partion_size;
  partion_size.push_back(-1);
  uint cnt = 1;
  uint max_iter = 1;
  while (cnt <= max_iter) {
    int sum_partition = 0;
    for (uint j = 0; j < numMolecules_; j++) {
      if (partition_array_[j] == cnt) {
        sum_partition++;
      }
    }
    // if(sum_partition==0){
    //     break;
    // }
    partion_size.push_back(sum_partition);
    if (sum_partition > molecule_per_partition) {
      VTR_LOG("start Bipartitioning\n");
      if (!split(cnt)) {
        VTR_LOG("Bipartitioning FAILED. cnt= %u\n", cnt);
        partitions_.clear();
        return false;
      }
      VTR_LOG("end Bipartitioning\n");
      max_iter = 2 * cnt + 1;
    } else {
      VTR_LOG("Bipartitioning NOP.  sum_partition= %i  molecule_per_partition= %i\n",
              sum_partition, molecule_per_partition);
      partitions_.push_back(cnt);
      //partitions_.clear(); // NOP handling is the same as error handling for the caller
      //return false;
    }
    cnt++;
  }
  vector<partition_position*> pp_array;
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
  for (uint i = 2; i <= max_iter; i++) {
    pp = new partition_position;
    pp_array.push_back(pp);
    if (partion_size[i] == 0) {
      continue;
    }
    uint parent = i / 2;
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

  // for (int i=1; i<=max_iter; i++) {
  //     VTR_LOG("\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",
  //        pp_array[i]->x1,pp_array[i]->y1,pp_array[i]->x2,pp_array[i]->y2);
  // }

  const char* vpr_constr_fn = "vpr_constraints.xml";
  FILE* file = ::fopen(vpr_constr_fn, "w");
  if (!file) {
    cerr << '\n'
         << "[Error] netlist partitioning: could not open file for writing: "
         << vpr_constr_fn << endl;
    lout() << '\n'
         << "[Error] netlist partitioning: could not open file for writing: "
         << vpr_constr_fn << endl;
    return false;
  }

  fprintf(file, "<vpr_constraints tool_name=\"vpr\">\n");
  fprintf(file, "\t<partition_list>\n");
  for (uint i = 0; i < partitions_.size(); i++) {
    // VTR_LOG("%d %d %d\n",i,int(pow(2,num_partitions)),i-int(pow(2,num_partitions)));
    fprintf(file, "\t<partition name=\"Part%d\">\n", i);
    for (uint j = 0; j < numMolecules_; j++) {
      if (partition_array_[j] == partitions_[i]) {
        for (AtomBlockId abid : molecules_[j]->atom_block_ids) {
          if (size_t(abid) >= numAtoms_ or myNetlist.block_type(abid) != AtomBlockType::BLOCK)
            continue;
          //atgs[clusterId].group_atoms.push_back(abid);
          if (size_t(abid) >= numAtoms_) {
            cout << '\n' << numAtoms_ << "\t" << size_t(abid) << endl;
            //// exit(0);
            lout() << "internal [Error] in Par::do_partitioning()" << endl;
            cerr   << "internal [Error] in Par::do_partitioning()" << endl;
            return false;
          }
          fprintf(file, "\t\t<add_atom name_pattern=\"%s\"/>\n", myNetlist.block_name(abid).c_str());
        }
      }
    }
    fprintf(file, "\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",
            pp_array[partitions_[i]]->x1 - 1, pp_array[partitions_[i]]->y1, pp_array[partitions_[i]]->x2 + 1,
            pp_array[partitions_[i]]->y2);
    fprintf(file, "\t</partition>\n");
  }

  fprintf(file, "\t</partition_list>\n");
  fprintf(file, "</vpr_constraints>\n");

  // Write some text to the file

  // Close the file
  fclose(file);
  return true;
}

}

