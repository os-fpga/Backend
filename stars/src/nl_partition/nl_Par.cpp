#include "nl_partition/nl_Par.h"
#include "file_readers/pinc_Fio.h"
#include "globals.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <thread>

namespace pln {

using namespace fio;

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

uint Par::countMolecules(t_pack_molecule* mol_head) {
  assert(mol_head);
  uint cnt = 0;
  for (const t_pack_molecule* mol = mol_head; mol; mol = mol->next) {
    cnt++;
  }
  return cnt;
}

bool Par::init(t_pack_molecule* mol_head) {
  assert(mol_head);
  numMolecules_ = numNets_ = numAtoms_ = 0;
  molecules_.clear();
  molNames_.clear();
  solverInputs_.clear();
  solverOutputs_.clear();
  partitions_.clear();
  pp_array_.clear();
  MtKaHyPar_path_.clear();
  if (!mol_head)
    return false;

  saved_ltrace_ = ltrace();
  {
  const char* ts = ::getenv("nl_par_trace");
  if (ts)
    set_ltrace(::atoi(ts));
  else
    set_ltrace(3);
  }
  uint16_t tr = ltrace();

  numMolecules_ = countMolecules(mol_head);

  if (tr >= 4)
    lprintf("\n pln::Par::init()   trace= %u   numMolecules_= %u\n", tr, numMolecules_);

  if (!numMolecules_)
    return false;

  MtKaHyPar_path_ = get_MtKaHyPar_path();
  if (tr >= 3)
    lprintf("MtKaHPar PATH: %s\n", MtKaHyPar_path_.c_str());
  if (MtKaHyPar_path_.empty()) {
    VTR_LOG("Bi-Partition init FAILED: MtKaHyPar executable not found\n");
    cerr << "[Error] Bi-Partition init FAILED:  MtKaHyPar executable not found\n" << endl;
    partitions_.clear();
    return false;
  }

  molecules_.reserve(numMolecules_);
  for (t_pack_molecule* mol = mol_head; mol; mol = mol->next) {
    molecules_.push_back(mol);
  }

  const AtomNetlist& aNtl = g_vpr_ctx.atom().nlist;

  if (tr >= 9)
    lputs("==== nets:");
  for (auto netId : aNtl.nets()) {
    if (tr >= 9)
      lprintf("\t  netId= %zu\n", size_t(netId));
    numNets_++;
  }

  if (tr >= 9)
    lputs("\n==== atom-blocks:");
  for (auto blockId : aNtl.blocks()) {
    if (tr >= 9)
      lprintf("\t  blockId= %zu\n", size_t(blockId));
    numAtoms_++;
  }
  if (tr >= 9) lputs();

  if (tr >= 3) {
    lprintf(" pln::Par::init   numMolecules_= %u  numNets_= %u   numBlocks === numAtoms_= %u\n",
              numMolecules_, numNets_, numAtoms_);
  }
  if (tr >= 9) lputs();

  atomBlockIdToMolId_ = new int[numAtoms_];
  molNames_.clear();
  molNames_.resize(numMolecules_);

  partition_array_ = (uint*) ::calloc(numMolecules_ + 1, sizeof(uint));
  for (uint i = 0; i < numMolecules_; i++) {
    partition_array_[i] = 1;
  }

  for (uint i = 0; i < numAtoms_; i++) {
    atomBlockIdToMolId_[i] = -1;
  }

  assert(molecules_.size() == numMolecules_);
  assert(molNames_.size() == numMolecules_);

  for (uint i = 0; i < numMolecules_; i++) {
    const t_pack_molecule& mol = *molecules_[i];
    string& molName = molNames_[i];
    molName.clear();
    for (auto abid : mol.atom_block_ids) {
      molName += aNtl.block_name(abid);
      uint bid = size_t(abid);
      if (bid >= numAtoms_)
        continue;
      assert(atomBlockIdToMolId_[bid] == -1);
      atomBlockIdToMolId_[bid] = i;
    }
  }

  if (tr >= 9) {
    lputs("\n==== molNames_:");
    for (uint i = 0; i < molNames_.size(); i++) {
      const t_pack_molecule& mol = *molecules_[i];
      uint deg = mol.atom_block_ids.size();
      const string& nm = molNames_[i];
      lprintf("    |%u|  %s   #atoms= %u\n", i, nm.c_str(), deg);
    }
  }

  for (uint i = 0; i < numAtoms_; i++) {
    assert(atomBlockIdToMolId_[i] != -1);
  }
  return true;
}

Par::~Par() {
  // p_free(partition_array_);
  if (saved_ltrace_)
    set_ltrace(saved_ltrace_);
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

string Par::get_MtKaHyPar_path() {
  int pid = ::getpid();
  string selfPath, exe_link, result;

  const char* es = ::getenv("PLN_KHP_EXE");
  if (es && ::strlen(es) > 1) {
    result = es;
    if (ltrace() >= 2)
      lprintf("got KHP_EXE=  %s  from env variable PLN_KHP_EXE\n", es);
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
    lprintf("\nKHP_EXE: result= %s\n", result.c_str());

  bool ok = Fio::regularFileExists(result);
  if (not ok) {
    lprintf2("\n[Error] KHP_EXE=  %s  does not exist\n", result.c_str());
    cerr <<  "\n[Error] no such file: " << result << endl << endl;
    result.clear();
  }

  return result;
}

bool Par::split(uint partition_index) {
  splitCnt_++;
  string splitS = std::to_string(splitCnt_);
  uint16_t tr = ltrace();
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
  const AtomNetlist& aNtl = g_vpr_ctx.atom().nlist;
  vector<string> uniqueLines;
  vector<int> lineCounts;

  //// uint nextIndexForMolecule = numMolecules_ + 1;
  //// bool addNewNodeAndUseWeightsInHmetis = false;

  vector<int> molecule_size;
  int sum_malecule_size = 0;
  for (auto netId : aNtl.nets()) {
    //VTR_LOG("NET ID: %d\n", size_t(netId));
    vector<int> newLine;
    vector<int> criticality;
    //// int sum_criticality = 0;
    AtomBlockId driverBlockId = aNtl.net_driver_block(netId);
    int moleculeId = atomBlockIdToMolId_[size_t(driverBlockId)];
    if (partition_array_[moleculeId] == partition_index) {
      newLine.push_back(MoleculesToIntermediate[moleculeId] + 1);
    }
    //// double maxCriticality = 0;
    for (auto pin_id : aNtl.net_pins(netId)) {
      //VTR_LOG("pin criticality: %f\n", pinCriticality);

      auto port_id = aNtl.pin_port(pin_id);
      auto blk_id = aNtl.port_block(port_id);
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

  string hmetis_inp_fn = str::concat("hmetis_inp_", splitS, "_.txt");
  solverInputs_.push_back(hmetis_inp_fn);
  if (tr >= 3) {
    lprintf("hmetis_file_name= %s  solver-input# %u\n",
             hmetis_inp_fn.c_str(), splitCnt_);
  }

  std::ofstream hmetisFile(hmetis_inp_fn);
  if (!hmetisFile) {
    const char* fn = hmetis_inp_fn.c_str();
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

  //constexpr int K = 2;
  CStr Kstr = "2";

  // uint num_cpus = std::thread::hardware_concurrency();
  // int num_threads = 1; // (int)(num_cpus / 2) > 1 ? (int)(num_cpus / 2) : 1;
  CStr Tstr = "1";

  if (tr >= 3)
    lprintf("MtKaHPar PATH: %s\n", MtKaHyPar_path_.c_str());
  if (MtKaHyPar_path_.empty()) {
    VTR_LOG("Bi-Partition with MtKaHPar FAILED: MtKaHyPar executable not found\n");
    cerr << "[Error] Bi-Partition with MtKaHPar FAILED:  MtKaHyPar executable not found\n" << endl;
    partitions_.clear();
    return false;
  }

  string cmd = str::concat(MtKaHyPar_path_,
                      string(" -h "), hmetis_inp_fn,
                      " --preset-type=quality -t ", Tstr,
                      " -k ", Kstr);

  cmd += " -e 0.01 -o soed --enable-progress-bar=true --show-detailed-timings=false --verbose=true --write-partition-file=true";

  if (tr >= 3) {
    VTR_LOG("MtKaHPar COMMAND: %s\n", cmd.c_str());
  }

  int code = ::system(cmd.c_str());
  if (code == 0) {
    VTR_LOG("MtKaHPar succeeded.\n");
  } else {
    VTR_LOG("MtKaHPar FAILED: exit code = %i\n", code);
    cerr << "[Error] MtKaHPar FAILED: exit code = " << code << endl;
    partitions_.clear();
    return false;
  }

  string raw_out = str::concat(hmetis_inp_fn, ".part", Kstr, ".epsilon0.01.seed0.KaHyPar");

  bool ok = Fio::regularFileExists(raw_out);
  if (not ok) {
    lprintf2("\n[Error] expected KHP output =  %s  does not exist\n", raw_out.c_str());
    cerr <<  "\n[Error] expected KHP output: no such file: " << raw_out << endl << endl;
    partitions_.clear();
    return false;
  }

  string hmetis_out_fn = str::concat(string("hmetis_out_"), splitS, "_K", Kstr, "_.txt");
  try {
    std::error_code ec;
    std::filesystem::rename(raw_out, hmetis_out_fn, ec);
    if (ec)
      hmetis_out_fn = raw_out;
  } catch (...) {
    hmetis_out_fn = raw_out;
  }
  solverOutputs_.push_back(hmetis_out_fn);

  CStr hmetis_out_cs = hmetis_out_fn.c_str();
  if (tr >= 4)
    lprintf("reading KHP output from file %s\n", hmetis_out_cs);

  ok = Fio::regularFileExists(hmetis_out_cs);
  if (not ok) {
    lprintf2("\n[Error] expected KHP output =  %s  does not exist\n", hmetis_out_cs);
    cerr <<  "\n[Error] expected KHP output: no such file: " << hmetis_out_cs << endl << endl;
    partitions_.clear();
    return false;
  }

  std::ifstream hmetisOutFile;
  hmetisOutFile.open(hmetis_out_cs);
  if (!hmetisOutFile.is_open()) {
    partitions_.clear();
    return false;
  }

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

bool Par::do_part(int mol_per_partition) {
  uint16_t tr = ltrace();
  if (tr >= 3) {
    lprintf("+Par::do_part( Cpart === mol_per_partition= %i )\n", mol_per_partition);
    lprintf(" numMolecules_= %u\n", numMolecules_);
  }
  //const AtomNetlist& aNtl = g_vpr_ctx.atom().nlist;

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
    if (sum_partition > mol_per_partition) {
      VTR_LOG("\nstart Bipartitioning\n");
      if (!split(cnt)) {
        VTR_LOG("Bipartitioning FAILED. cnt= %u\n", cnt);
        partitions_.clear();
        return false;
      }
      VTR_LOG("end Bipartitioning\n");
      max_iter = 2 * cnt + 1;
    } else {
      VTR_LOG("Bipartitioning NOP.  sum_partition= %i  mol_per_partition= %i\n",
              sum_partition, mol_per_partition);
      partitions_.push_back(cnt);
      //partitions_.clear(); // NOP handling is the same as error handling for the caller
      //return false;
    }
    cnt++;
  }

  partition_position* pp = new partition_position;
  pp->x1 = 2;
  pp->x2 = 103;
  pp->y1 = 2;
  pp->y2 = 67;
  // VTR_LOG("FPGA sized to %zu x %zu (%s)\n", grid.width(), grid.height(), grid.name().c_str());
  // exit(0);
  // VTR_LOG("\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",pp->x1,pp->y1,pp->x2,pp->y2);
  pp_array_.push_back(nullptr);
  pp_array_.push_back(pp);
  for (uint i = 2; i <= max_iter; i++) {
    pp = new partition_position;
    pp_array_.push_back(pp);
    if (partion_size[i] == 0) {
      continue;
    }
    uint parent = i / 2;
    // VTR_LOG("parent  = %d,child =  %d\n",partion_size[parent],partion_size[2*parent]);
    float division = float(partion_size[parent]) / float(partion_size[2 * parent]);
    // float division = 2.0;
    // VTR_LOG("before parent %d %d\n",i,parent);
    partition_position* pp_parent = pp_array_[parent];
    // VTR_LOG("after parent\n");
    if (pp_parent->isHorizontal()) {
      pp->is_vert_ = true;
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
      pp->is_vert_ = false;
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

  bool wr_ok = write_constraints_xml();
  if (!wr_ok) {
    lprintf2("\n    Par::write_constraints_xml() FAILED\n");
    partitions_.clear();
    return false;
  }

  cleanup_tmp_files();

  return true;
}

bool Par::write_constraints_xml() const {

  const AtomNetlist& aNtl = g_vpr_ctx.atom().nlist;

  // for (int i=1; i<=max_iter; i++) {
  //     VTR_LOG("\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",
  //        pp_array_[i]->x1,pp_array_[i]->y1,pp_array_[i]->x2,pp_array_[i]->y2);
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
          if (size_t(abid) >= numAtoms_ or aNtl.block_type(abid) != AtomBlockType::BLOCK)
            continue;
          //atgs[clusterId].group_atoms.push_back(abid);
          if (size_t(abid) >= numAtoms_) {
            cout << '\n' << numAtoms_ << "\t" << size_t(abid) << endl;
            lprintf2("internal [Error] in Par::do_part()\n");
            return false;
          }
          fprintf(file, "\t\t<add_atom name_pattern=\"%s\"/>\n", aNtl.block_name(abid).c_str());
        }
      }
    }
    fprintf(file, "\t\t<add_region x_low=\"%d\" y_low=\"%d\" x_high=\"%d\" y_high=\"%d\"/>\n",
            pp_array_[partitions_[i]]->x1 - 1, pp_array_[partitions_[i]]->y1, pp_array_[partitions_[i]]->x2 + 1,
            pp_array_[partitions_[i]]->y2);
    fprintf(file, "\t</partition>\n");
  }

  fprintf(file, "\t</partition_list>\n");
  fprintf(file, "</vpr_constraints>\n");

  ::fclose(file);
  return true;
}

void Par::cleanup_tmp_files() const {
  uint16_t tr = ltrace();
  if (tr >= 4) {
    lprintf("    solverInputs_.size()= %zu  solverOutputs_.size()= %zu\n",
            solverInputs_.size(), solverOutputs_.size());
  }
  if (tr >= 7) {
    lprintf("  Par:: keeping tmp hmetis files for debugging\n");
    return;
  }

  if (tr >= 3)
    lprintf("  Par:: removing tmp hmetis inputs..\n");

  std::error_code ec;
  if (solverInputs_.size() > 3) {
    uint max_i = solverInputs_.size() - 2;
    for (uint i = 1; i <= max_i; i++) {
      const string& fn = solverInputs_[i];
      ec.clear();
      std::filesystem::remove(fn, ec);
    }
  }

  if (tr >= 3)
    lprintf("  Par:: removing tmp hmetis outputs..\n");

  if (solverOutputs_.size() > 3) {
    uint max_i = solverOutputs_.size() - 2;
    for (uint i = 1; i <= max_i; i++) {
      const string& fn = solverOutputs_[i];
      ec.clear();
      std::filesystem::remove(fn, ec);
    }
  }
}

}

