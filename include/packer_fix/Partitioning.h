#ifndef PARTITIONING_H
#define PARTITIONING_H
#include<vector>
#include<iostream>
#include "vpr_types.h"

class Partitioning{
public: 
    Partitioning(t_pack_molecule* molecule_head);
    void Bi_Partion(int partion_index);
    void recursive_partitioning(int num_partitions);
// private:
    std::vector<t_pack_molecule*> molecules;
    int numberOfMolecules = 0;
    int numberOfNets = 0;
    int numberOfAtoms = 0;
    int* atomBlockIdToMoleculeId;
    std::string* moleculeIdToName;
    int* partition_array;
    std::vector<int> partitions;
};

class Person {
public:
    // Constructor
    Person(std::string name, int age);

    // Member functions
    std::string getName() const;
    int getAge() const;
    void setName(std::string name);
    void setAge(int age);

private:
    std::string name;
    int age;
};
#endif