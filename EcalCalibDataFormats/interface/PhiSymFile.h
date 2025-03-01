#ifndef PHISYMFILE_H
#define PHISYMFILE_H

#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"

using namespace std;

//**********EB TREE***********************************************************************

class EBTree
{
public: 

    //---ctors---
    EBTree();
    //---dtor---
    ~EBTree() {};
    //---wrappers
    inline void Fill() {tree_->Fill();};
    inline void SetMaxVirtualSize(uint64_t size) {tree_->SetMaxVirtualSize(size);};
    inline void Write(const char* name) {tree_->Write(name);};
    inline void Write(string name) {tree_->Write(name.c_str());};
    
    //---branches variables---
    uint32_t run;
    uint32_t lumi;
    int ieta;
    int iphi;
    float et;
    
private:

    TTree* tree_;    
};

EBTree::EBTree()
{
    tree_ = new TTree();
    //---init
    run=0;
    lumi=0;
    ieta=0;
    iphi=0;
    et=0;
    
    //---create branches
    tree_->Branch("run", &run, "run/i");
    tree_->Branch("lumi", &lumi, "lumi/i");
    tree_->Branch("ieta", &ieta, "ieta/I");
    tree_->Branch("iphi", &iphi, "iphi/I");
    tree_->Branch("et", &et, "et/F");
}

//**********EE TREE***********************************************************************

class EETree
{
public: 

    //---ctors---
    EETree();
    //---dtor---
    ~EETree() {};
    //---wrappers
    inline void Fill() {tree_->Fill();};
    inline void SetMaxVirtualSize(uint64_t size) {tree_->SetMaxVirtualSize(size);};
    inline void Write(const char* name) {tree_->Write(name);};
    inline void Write(string name) {tree_->Write(name.c_str());};
    
    //---branches variables---
    uint32_t run;
    uint32_t lumi;
    int iring;
    int ix;
    int iy;
    float et;
    
private:

    TTree* tree_;    
};

EETree::EETree()
{
    tree_ = new TTree();
    //---init
    run=0;
    lumi=0;
    iring=0;
    ix=0;
    iy=0;
    et=0;
    
    //---create branches
    tree_->Branch("run", &run, "run/i");
    tree_->Branch("lumi", &lumi, "lumi/i");
    tree_->Branch("iring", &iring, "iring/I");
    tree_->Branch("ix", &ix, "ix/I");
    tree_->Branch("iy", &iy, "iy/I");
    tree_->Branch("et", &et, "et/F");
}

//**********FILE**************************************************************************

class PhiSymFile
{
public:    
    
    PhiSymFile();
    PhiSymFile(TFile* file);

    inline void Close() {file_->Close();};
    inline void cd() {file_->cd();};

    EBTree ebTree;
    EETree eeTree;
    
private:
    
    TFile* file_;
};

PhiSymFile::PhiSymFile()
{}

PhiSymFile::PhiSymFile(TFile* file)
{
    file_ = file;
    file_->cd();
    ebTree.SetMaxVirtualSize(100);
    eeTree.SetMaxVirtualSize(100);
}

#endif 
