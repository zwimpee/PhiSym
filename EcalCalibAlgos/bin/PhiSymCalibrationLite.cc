#ifndef __PHISYM_CALIBRATION_LITE__
#define __PHISYM_CALIBRATION_LITE__

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm> 

#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TGraphErrors.h"

#include "FWCore/FWLite/interface/AutoLibraryLoader.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/PythonParameterSet/interface/MakeParameterSets.h"

#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalDetId/interface/EEDetId.h"

#include "Calibration/Tools/interface/EcalRingCalibrationTools.h"

#include "PhiSym/EcalCalibDataFormats/interface/PhiSymRecHit.h"
#include "PhiSym/EcalCalibDataFormats/interface/CalibrationFile.h"
#include "PhiSym/EcalCalibAlgos/interface/utils.h"

using namespace std;

//****************************************************************************************
//-----gloabal variables definition------
int nLumis_=0;
int nEvents_=0;
int nMisCalib_=-1;
vector<float>* misCalibValuesEB_;
vector<float>* misCalibValuesEE_;

static const short kNRingsEB = EcalRingCalibrationTools::N_RING_BARREL;
static const short kNRingsEE = EcalRingCalibrationTools::N_RING_ENDCAP;
float ebOldICs_[kNRingsEB][361];
float eeOldICs_[EEDetId::IX_MAX+1][EEDetId::IY_MAX+1][2];
double ebOldICsErr_[kNRingsEB][361];
double eeOldICsErr_[EEDetId::IX_MAX+1][EEDetId::IY_MAX+1][2];
float ebAbsICs_[kNRingsEB][361];
float eeAbsICs_[EEDetId::IX_MAX+1][EEDetId::IY_MAX+1][2];
//---ring based
//---EB
double ebRingsSumEt_[kNRingsEB][11];
double ebRingsSumEt2_[kNRingsEB]={0};
double kFactorsEB_[kNRingsEB]={0};
double kFactorsErrEB_[kNRingsEB]={0};
float  icRMeanEB_[kNRingsEB];
double ebICRingErr_[EEDetId::kSizeForDenseIndexing]={0};
float  ebSumEtCuts_[kNRingsEB][2];
//---EE
double eeRingsSumEt_[kNRingsEE][11];
double eeRingsSumEt2_[kNRingsEE]={0};
double kFactorsEE_[kNRingsEE]={0};
double kFactorsErrEE_[kNRingsEE]={0};
float  icRMeanEE_[kNRingsEE];
double eeICRingErr_[EEDetId::kSizeForDenseIndexing]={0};
float  eeSumEtCuts_[kNRingsEE][2];
//---crystal based
//---EB
map<int, int> ebRingsMap_;
PhiSymRecHit ebXstals_[EBDetId::kSizeForDenseIndexing];
bool   goodXstalsEB_[kNRingsEB][361][11];
float  nGoodInRingEB_[kNRingsEB][11];
double kFactorsChEB_[EBDetId::kSizeForDenseIndexing];
double kFactorsChErrEB_[EBDetId::kSizeForDenseIndexing];
double ebICChErr_[EBDetId::kSizeForDenseIndexing]={0};
float  icChMeanEB_[kNRingsEB];
//---EE
map<int, int> eeRingsMap_;
PhiSymRecHit eeXstals_[EEDetId::kSizeForDenseIndexing];
bool   goodXstalsEE_[kNRingsEE][EEDetId::IX_MAX+1][EEDetId::IY_MAX+1][11];
float  nGoodInRingEE_[kNRingsEE][11];
double kFactorsChEE_[EEDetId::kSizeForDenseIndexing]={0};
double kFactorsChErrEE_[EEDetId::kSizeForDenseIndexing]={0};
double eeICChErr_[EEDetId::kSizeForDenseIndexing]={0};
float  icChMeanEE_[kNRingsEE];

bool kFactorsComputed_;

//---outputs
auto_ptr<CalibrationFile> outFile_;

//**********FUNCTIONS*********************************************************************
//----------compute the ring-dependent k-factors for both EB and EE-----------------------
// + errors for different iMis are assumed to be ugual to the error on the nominal <sumEt>
// + for the averages the error is the RMS of the channels sumEt distribution inside a ring
// (sum over N blocks)
// + for the single channel k-factor otherwise the error is the RMS of the sumEt distribution
// (sum over 1 block)
// + this implies that the error on the single channel k-factor is larger.
void ComputeKfactors()
{
    TF1* kFactFitFunc = new TF1("kFFF", "[0]*x", -0.5, 0.5);
    kFactFitFunc->SetParameter(0, 1);
    TGraphErrors* kFactorGraph = new TGraphErrors();
    //---EB---
    //---ring-averaged k-factors
    for(int iRing=0; iRing<kNRingsEB; ++iRing)
    {
        if(ebRingsSumEt_[iRing][0] == 0)
            continue;
        ebICRingErr_[iRing] = sqrt(ebRingsSumEt2_[iRing]/nGoodInRingEB_[iRing][0]-
                                   pow(ebRingsSumEt_[iRing][0]/nGoodInRingEB_[iRing][0], 2));
        float error = ebICRingErr_[iRing]/ebRingsSumEt_[iRing][0];
        for(int iMis=0; iMis<=nMisCalib_; ++iMis)
        {
            float point = ebRingsSumEt_[iRing][iMis]/ebRingsSumEt_[iRing][0]-1;
            float p_error = error*sqrt(pow(point, 2)+1);
            kFactorGraph->SetPoint(iMis, misCalibValuesEB_->at(iMis)-1, point);
            kFactorGraph->SetPointError(iMis, 0, p_error);
        }
        kFactorGraph->Fit(kFactFitFunc, "Q");
        kFactorsEB_[iRing]=kFactorGraph->GetFunction("kFFF")->GetParameter(0);
        kFactorsErrEB_[iRing]=kFactorGraph->GetFunction("kFFF")->GetParError(0);
    }
    //---channel-based k-factors
    for(uint32_t index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
    {
        ebICChErr_[index]=sqrt(ebXstals_[index].GetSumEt2()/ebXstals_[index].GetNhits()-
                             pow(ebXstals_[index].GetSumEt()/ebXstals_[index].GetNhits(), 2));
        float error = ebICChErr_[index]/ebXstals_[index].GetSumEt(0);
        for(int iMis=0; iMis<nMisCalib_; ++iMis)
        {
            float point = ebXstals_[index].GetSumEt(iMis)/ebXstals_[index].GetSumEt(0) - 1;
            float p_error = error*sqrt(pow(point, 2)+1);
            kFactorGraph->SetPoint(iMis, misCalibValuesEB_->at(iMis)-1, point);
            kFactorGraph->SetPointError(iMis, 0, p_error);
        }
        kFactorGraph->Fit(kFactFitFunc, "Q");
        kFactorsChEB_[index]=kFactorGraph->GetFunction("kFFF")->GetParameter(0);
        kFactorsChErrEB_[index]=kFactorGraph->GetFunction("kFFF")->GetParError(0);
    }
    //---EE---
    for(int iRing=0; iRing<kNRingsEE; ++iRing)
    {
        eeICRingErr_[iRing] = sqrt(eeRingsSumEt2_[iRing]/nGoodInRingEE_[iRing][0]-
                                   pow(eeRingsSumEt_[iRing][0]/nGoodInRingEE_[iRing][0], 2));
        float error = eeICRingErr_[iRing]/eeRingsSumEt_[iRing][0];
        for(int iMis=0; iMis<=nMisCalib_; ++iMis)
        {
            float point = eeRingsSumEt_[iRing][iMis]/eeRingsSumEt_[iRing][0]-1;
            float p_error = error*sqrt(pow(point, 2)+1);
            kFactorGraph->SetPoint(iMis, misCalibValuesEE_->at(iMis)-1, point);
            kFactorGraph->SetPointError(iMis, 0, p_error);
        }
        kFactorGraph->Fit(kFactFitFunc, "Q");
        kFactorsEE_[iRing]=kFactorGraph->GetFunction("kFFF")->GetParameter(0);
        kFactorsErrEE_[iRing]=kFactorGraph->GetFunction("kFFF")->GetParError(0);
    }
    //---channel-based k-factors
    for(uint32_t index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
    {
        if(eeXstals_[index].GetNhits() == 0)
            continue;
        eeICChErr_[index]=sqrt(eeXstals_[index].GetSumEt2()/eeXstals_[index].GetNhits()-
                             pow(eeXstals_[index].GetSumEt()/eeXstals_[index].GetNhits(), 2));
        float error = eeICChErr_[index]/eeXstals_[index].GetSumEt(0);
        for(int iMis=0; iMis<=nMisCalib_; ++iMis)
        {
            float point = eeXstals_[index].GetSumEt(iMis)/eeXstals_[index].GetSumEt(0) - 1;
            float p_error = error*sqrt(pow(point, 2)+1);
            kFactorGraph->SetPoint(iMis, misCalibValuesEE_->at(iMis)-1, point);
            kFactorGraph->SetPointError(iMis, 0, p_error);
        }
        kFactorGraph->Fit(kFactFitFunc, "Q");
        kFactorsChEE_[index]=kFactorGraph->GetFunction("kFFF")->GetParameter(0);
        kFactorsChErrEE_[index]=kFactorGraph->GetFunction("kFFF")->GetParError(0);
    }
    kFactFitFunc->Delete();
    kFactorGraph->Delete();
    kFactorsComputed_=true;
}

//----------return the ring-based k-factor --- sub_det: 0->EB, 1->EE----------------------
pair<float, float> GetRingKfactor(int ring, int sub_det)
{
    if(!kFactorsComputed_)
        ComputeKfactors();
    if(sub_det == 0)
        return make_pair(kFactorsEB_[ring], kFactorsErrEB_[ring]);
    else
        return make_pair(kFactorsEE_[ring], kFactorsErrEE_[ring]);
}

//----------return the channel-based k-factor --- sub_det: 0->EB, 1->EE-------------------
pair<float, float> GetChannelKfactor(int index, int sub_det)
{
    if(!kFactorsComputed_)
        ComputeKfactors();
    if(sub_det == 0)
        return make_pair(kFactorsChEB_[index], kFactorsChErrEB_[index]);
    else
        return make_pair(kFactorsChEE_[index], kFactorsChErrEE_[index]);
}

//----------cumpute phisym ICs for both EB and EE and fill the output tree----------------
void ComputeICs()
{
    //---increment block output trees counters
    outFile_->eb_xstals.block++;
    outFile_->eb_xstals.n_lumis = nLumis_;
    outFile_->ee_xstals.block++;
    outFile_->ee_xstals.n_lumis = nLumis_;

    //---compute EB rings averages
    for(int iRing=0; iRing<kNRingsEB; ++iRing)
    {        
        for(int iMis=0; iMis<=nMisCalib_; ++iMis)
        {
            for(int iPhi=0; iPhi<=360; ++iPhi)
                nGoodInRingEB_[iRing][iMis] += goodXstalsEB_[iRing][iPhi][iMis];
            if(nGoodInRingEB_[iRing][iMis] > 0)
            {
                if(iMis==0)
                    ebRingsSumEt2_[iRing] = ebRingsSumEt2_[iRing] / nGoodInRingEB_[iRing][iMis];
                ebRingsSumEt_[iRing][iMis] = ebRingsSumEt_[iRing][iMis] / nGoodInRingEB_[iRing][iMis];
            }
        }
    }
    //---compute EE rings averages
    for(int iRing=0; iRing<kNRingsEE; ++iRing)
    {
        for(int iMis=0; iMis<=nMisCalib_; ++iMis)
        {
            for(int iX=0; iX<EEDetId::IX_MAX; ++iX)
                for(int iY=0; iY<EEDetId::IY_MAX; ++iY)
                    nGoodInRingEE_[iRing][iMis] += goodXstalsEE_[iRing][iX][iY][iMis];
            if(nGoodInRingEE_[iRing][iMis] > 0)
            {
                if(iMis==0)
                    eeRingsSumEt2_[iRing] = eeRingsSumEt2_[iRing] / nGoodInRingEE_[iRing][iMis];
                eeRingsSumEt_[iRing][iMis] = eeRingsSumEt_[iRing][iMis] / nGoodInRingEE_[iRing][iMis];
            }
        }
    }
    ComputeKfactors();

    //---normalize ICs ring by ring---
    //---compute averages
    for(uint32_t index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
    {
        EBDetId ebXstal = EBDetId::detIdFromDenseIndex(index);
        int currentRing = ebRingsMap_[index];
        if(currentRing == -1 || !goodXstalsEB_[currentRing][ebXstal.iphi()][0])
            continue;
        {
            icChMeanEB_[currentRing] += (ebXstals_[index].GetSumEt(0)/
                                         ebRingsSumEt_[currentRing][0]-1)/GetChannelKfactor(index, 0).first+1;
            icRMeanEB_[currentRing] += (ebXstals_[index].GetSumEt(0)/
                                        ebRingsSumEt_[currentRing][0]-1)/GetRingKfactor(currentRing, 0).first+1;
        }
    }
    //---compute normalization EB 
    for(int iRing=0; iRing<kNRingsEB; ++iRing)
    {
        icChMeanEB_[iRing] = icChMeanEB_[iRing]/nGoodInRingEB_[iRing][0];
        icRMeanEB_[iRing] = icRMeanEB_[iRing]/nGoodInRingEB_[iRing][0];            
    }    
    //---compute averages EE
    for(uint32_t index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
    {
        EEDetId eeXstal = EEDetId::detIdFromDenseIndex(index);
        int currentRing = eeRingsMap_[index];
        if(currentRing == -1 || !goodXstalsEE_[currentRing][eeXstal.ix()][eeXstal.iy()][0])
            continue;
        {
            icChMeanEE_[currentRing] += (eeXstals_[index].GetSumEt(0)/
                                         eeRingsSumEt_[currentRing][0]-1)/GetChannelKfactor(index, 1).first+1;
            icRMeanEE_[currentRing] += (eeXstals_[index].GetSumEt(0)/
                                        eeRingsSumEt_[currentRing][0]-1)/GetRingKfactor(currentRing, 1).first+1;
        }
    }    
    //---compute normalization EE
    for(int iRing=0; iRing<kNRingsEE; ++iRing)
    {
        icChMeanEE_[iRing] = icChMeanEE_[iRing]/nGoodInRingEE_[iRing][0];
        icRMeanEE_[iRing] = icRMeanEE_[iRing]/nGoodInRingEE_[iRing][0];            
    }    

    //---loop over the EB channels and compute the IC
    for(uint32_t index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
    {
        EBDetId ebXstal = EBDetId::detIdFromDenseIndex(index);
        int currentRing = ebRingsMap_[index];
        if(currentRing > -1)
        {
            //---fill the output tree
            outFile_->eb_xstals.n_events = nEvents_;
            outFile_->eb_xstals.bounds[0] = ebSumEtCuts_[currentRing][0];
            outFile_->eb_xstals.bounds[1] = ebSumEtCuts_[currentRing][1];
            outFile_->eb_xstals.rec_hit = &ebXstals_[index];
            outFile_->eb_xstals.ieta = ebXstal.ieta();
            outFile_->eb_xstals.iphi = ebXstal.iphi();
            outFile_->eb_xstals.k_ring = GetRingKfactor(currentRing, 0).first;
            outFile_->eb_xstals.k_ring_err = GetRingKfactor(currentRing, 0).second;
            outFile_->eb_xstals.k_ch = GetChannelKfactor(index, 0).first;
            outFile_->eb_xstals.k_ch_err = GetChannelKfactor(index, 0).second;
            outFile_->eb_xstals.ic_ring = 1/(((ebXstals_[index].GetSumEt(0)/ebRingsSumEt_[currentRing][0]-1)
                                              /outFile_->eb_xstals.k_ring+1)/icRMeanEB_[currentRing]);
            outFile_->eb_xstals.ic_ch = 1/(((ebXstals_[index].GetSumEt(0)/ebRingsSumEt_[currentRing][0]-1)
                                            /outFile_->eb_xstals.k_ch+1)/icChMeanEB_[currentRing]);
            outFile_->eb_xstals.ic_old = ebOldICs_[currentRing][ebXstal.iphi()];
            outFile_->eb_xstals.ic_abs = ebAbsICs_[currentRing][ebXstal.iphi()];
            outFile_->eb_xstals.ic_ring_err = ebICRingErr_[currentRing]/(ebRingsSumEt_[currentRing][0]*outFile_->eb_xstals.k_ring);
            outFile_->eb_xstals.ic_ring_err = outFile_->eb_xstals.ic_ring_err/pow(outFile_->eb_xstals.ic_ring, 2);
            outFile_->eb_xstals.ic_ch_err = ebICChErr_[index]/(ebRingsSumEt_[currentRing][0]*outFile_->eb_xstals.k_ch);
            outFile_->eb_xstals.ic_ch_err = outFile_->eb_xstals.ic_ch_err/pow(outFile_->eb_xstals.ic_ch, 2);
            outFile_->eb_xstals.ic_err_sys = ebOldICsErr_[currentRing][ebXstal.iphi()];
            outFile_->eb_xstals.Fill();
        }
    }
    //---loop over the EE channels and compute the IC
    for(uint32_t index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
    {
        EEDetId eeXstal = EEDetId::detIdFromDenseIndex(index);
        int currentRing = eeRingsMap_[index];
        if(currentRing > -1)
        {
            //---fill the output tree
            outFile_->ee_xstals.n_events = nEvents_;
            outFile_->ee_xstals.rec_hit = &eeXstals_[index];
            outFile_->ee_xstals.iring = currentRing<kNRingsEE/2 ? currentRing-kNRingsEE/2 : currentRing-kNRingsEE/2 + 1;
            outFile_->ee_xstals.ix = eeXstal.ix();
            outFile_->ee_xstals.iy = eeXstal.iy();
            outFile_->ee_xstals.k_ring = GetRingKfactor(currentRing, 1).first;
            outFile_->ee_xstals.k_ring_err = GetRingKfactor(currentRing, 1).second;
            outFile_->ee_xstals.k_ch = GetChannelKfactor(index, 1).first;
            outFile_->ee_xstals.k_ch_err = GetChannelKfactor(index, 1).second;
            outFile_->ee_xstals.ic_ring = 1/(((eeXstals_[index].GetSumEt(0)/eeRingsSumEt_[currentRing][0]-1)
                                              /outFile_->ee_xstals.k_ring+1)/icRMeanEE_[currentRing]);
            outFile_->ee_xstals.ic_ch = 1/(((eeXstals_[index].GetSumEt(0)/eeRingsSumEt_[currentRing][0]-1)
                                            /outFile_->ee_xstals.k_ch+1)/icChMeanEE_[currentRing]);
            outFile_->ee_xstals.ic_old = eeOldICs_[eeXstal.ix()][eeXstal.iy()][eeXstal.zside()<0 ? 0 : 1];
            outFile_->ee_xstals.ic_abs = eeAbsICs_[eeXstal.ix()][eeXstal.iy()][eeXstal.zside()<0 ? 0 : 1];            
            outFile_->ee_xstals.ic_ring_err = eeICRingErr_[index]/(eeRingsSumEt_[currentRing][0]*outFile_->ee_xstals.k_ring);
            outFile_->ee_xstals.ic_ring_err = outFile_->ee_xstals.ic_ring_err/pow(outFile_->ee_xstals.ic_ring, 2);
            outFile_->ee_xstals.ic_ch_err = eeICChErr_[index]/(eeRingsSumEt_[currentRing][0]*outFile_->ee_xstals.k_ch);
            outFile_->ee_xstals.ic_ch_err = outFile_->ee_xstals.ic_ch_err/pow(outFile_->ee_xstals.ic_ch, 2);
            outFile_->ee_xstals.ic_err_sys = eeOldICsErr_[eeXstal.ix()][eeXstal.iy()][eeXstal.zside()<0 ? 0 : 1];
            outFile_->ee_xstals.Fill();
        }
    }
}

//----------if a file is specified read the old ICs, otherwise set them to -1-------------
void Read2012ICs(string name)
{
    //---no file specified
    if(name == "")
    {
        //---EB
        for(int ieta=0; ieta<kNRingsEB; ++ieta)
            for(int iphi=0; iphi<=360; ++iphi)
            {
                ebOldICs_[ieta][iphi]=-1;
                ebOldICsErr_[ieta][iphi]=-1;
            }
        //---EE
        for(int ix=0; ix<=100; ++ix)
        {
            for(int iy=0; iy<=100; ++iy)
            {
                eeOldICs_[ix][iy][0]=-1;
                eeOldICs_[ix][iy][1]=-1;
                eeOldICsErr_[ix][iy][0]=-1;
                eeOldICsErr_[ix][iy][1]=-1;
            }
        }
    }
    
    //---help variables
    int x,y,subdet;
    float ic, err;
        
    ifstream oldICs(name.c_str(), ios::in);
    while(oldICs.good())
    {
        oldICs >> x >> y >> subdet >> ic >> err;
        if(subdet==0)
        {
            ebOldICs_[x<0 ? x+85 : x+84][y]=ic;
            ebOldICsErr_[x<0 ? x+85 : x+84][y]=err;
        }
        else
        {
            eeOldICs_[x][y][subdet<0 ? 0 : 1]=ic;
            eeOldICsErr_[x][y][subdet<0 ? 0 : 1]=err;
        }
    }
    oldICs.close();

    return;
}

//----------if a file is specified read the abs ICs, otherwise set them to -1-------------
void ReadAbsICs(string name)
{
    //---no file specified
    if(name == "")
    {
        //---EB
        for(int ieta=0; ieta<kNRingsEB; ++ieta)
            for(int iphi=0; iphi<=360; ++iphi)
                ebAbsICs_[ieta][iphi]=-1;
        //---EE
        for(int ix=0; ix<=100; ++ix)
        {
            for(int iy=0; iy<=100; ++iy)
            {
                eeAbsICs_[ix][iy][0]=-1;
                eeAbsICs_[ix][iy][1]=-1;
            }
        }
    }
    
    //---help variables
    int x,y,subdet;
    float ic, fake;
        
    ifstream absICs(name.c_str(), ios::in);
    while(absICs.good())
    {
        absICs >> x >> y >> subdet >> ic >> fake;
        if(subdet==0)
            ebAbsICs_[x<0 ? x+85 : x+84][y]=ic;
        else
            eeAbsICs_[x][y][subdet<0 ? 0 : 1]=ic;
    }
    absICs.close();

    return;
}

//**********MAIN**************************************************************************
int main( int argc, char *argv[] )
{
    gSystem->Load("libFWCoreFWLite");
    gSystem->Load("libPhiSymEcalCalibDataFormats.so");
    AutoLibraryLoader::enable();

    if(argc < 2)
    {
        cout << "Usage : " << argv[0] << " [parameters.py]" << endl;
        return 0;
    }
    if(!edm::readPSetsFrom(argv[1])->existsAs<edm::ParameterSet>("process"))
    {
        cout << " ERROR: ParametersSet 'process' is missing in your configuration file"
             << endl;
        return 0;
    }

    //---inputs    
    vector<string> inputFiles;
    string outputFileBase;
    vector<string> oldICsFiles;
    kFactorsComputed_=false;
    nLumis_=0;
    nEvents_=0;
    
    //-----get the python configuration-----
    const edm::ParameterSet &process = edm::readPSetsFrom(argv[1])->getParameter<edm::ParameterSet>("process");
    const edm::ParameterSet &filesOpt = process.getParameter<edm::ParameterSet>("ioFilesOpt");
    const edm::ParameterSet &IOVBounds = process.getParameter<edm::ParameterSet>("IOVBounds");
        
    //---get IOV boundaries
    vector<int> IOVBeginRuns = IOVBounds.getParameter<vector<int> >("beginRuns");
    vector<int> IOVEndRuns = IOVBounds.getParameter<vector<int> >("endRuns");
    vector<int> IOVBeginLumis = IOVBounds.getParameter<vector<int> >("beginLumis");
    vector<int> IOVEndLumis = IOVBounds.getParameter<vector<int> >("endLumis");    
    int startingIOV = IOVBounds.getParameter<int>("startingIOV");
    int nIOVs = IOVBounds.getParameter<int>("nIOVs");
    
    //---get input/output files
    outputFileBase = filesOpt.getParameter<string>("outputFile");
    inputFiles = filesOpt.getParameter<vector<string> >("inputFiles");
    map<int, vector<string> > iovInputFiles;
    for(auto& fileName : inputFiles)
    {
        //---open the next file
        TFile* file = TFile::Open(fileName.c_str());
        CrystalsEBTree ebTree((TTree*)file->Get("eb_xstals"));

        //---assign file to the correct IOVs 
        //---(requires only that the file last run is > the IOV first run)
        //ebTree.NextEntry();
        for(int iIOV=startingIOV; iIOV<startingIOV+nIOVs; ++iIOV)
        {
            for(int iBlk=0; iBlk<nIOVs; ++iBlk)
            {
                if(ebTree.NextEntry(61200*iBlk+1))
                {
                    if(ebTree.begin[0] >= IOVBeginRuns[iIOV] && ebTree.end[0] <= IOVEndRuns[iIOV])
                        iovInputFiles[iIOV].push_back(fileName);
                }
            }
        }        
        file->Close();
    }

    //---get ICs (the old ones for comparison, and reco ones to compute the absolute ICs)
    oldICsFiles = filesOpt.getParameter<vector<string> >("oldConstantsFiles");
    ReadAbsICs(filesOpt.getParameter<string>("recoConstantsFile"));

    for(int iIOV=startingIOV; iIOV<startingIOV+nIOVs; ++iIOV)
    {
        if(iovInputFiles[iIOV].size() == 0)
            continue;

        //---reset
        nLumis_ = 0;
        nEvents_ = 0;
        kFactorsComputed_ = false;
        //---EB
        for(int iRing=0; iRing<kNRingsEB; ++iRing)
        {
            icChMeanEB_[iRing] = 0;
            icRMeanEB_[iRing] = 0;
            ebRingsSumEt2_[iRing] = 0;
            for(int iMis=0; iMis<=nMisCalib_; ++iMis)
            {
                ebRingsSumEt_[iRing][iMis] = 0;
                nGoodInRingEB_[iRing][iMis] = 0;
                for(int iPhi=0; iPhi<=360; ++iPhi)
                    goodXstalsEB_[iRing][iPhi][iMis]=0;
            }
        }
        for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
        {
            ebXstals_[index] = PhiSymRecHit();
            ebRingsMap_[index] = -1;
        }
        //---EE
        for(int iRing=0; iRing<kNRingsEE; ++iRing)
        {
            eeRingsSumEt2_[iRing] = 0;
            icChMeanEE_[iRing] = 0;
            icRMeanEE_[iRing] = 0;
            for(int iMis=0; iMis<=nMisCalib_; ++iMis)
            {
                eeRingsSumEt_[iRing][iMis] = 0;
                nGoodInRingEE_[iRing][iMis] = 0;
                for(int iX=0; iX<=100; ++iX)
                    for(int iY=0; iY<=100; ++iY)
                        goodXstalsEE_[iRing][iX][iY][iMis]=0;
            }
        }
        for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
        {
            eeXstals_[index] = PhiSymRecHit();
            eeRingsMap_[index] = -1;
        }

        nMisCalib_ = -1;
        
        //---output file        
        TFile* out = TFile::Open((outputFileBase+
                                  to_string(IOVBeginRuns[iIOV])+"_"+
                                  to_string(IOVEndRuns[iIOV])+".root").c_str(),
                                 "RECREATE");
        outFile_ = auto_ptr<CalibrationFile>(new CalibrationFile(out));

        //---read old ICs (maybe IOV dependent)
        Read2012ICs(oldICsFiles[0]);

        //---vectors for sumEt cuts computation
        vector<float> ebRingsSumEts[kNRingsEB];
        for(int iRing=0; iRing<kNRingsEB; ++iRing)
            ebRingsSumEts[iRing].reserve(EBDetId::kSizeForDenseIndexing);
        vector<float> eeRingsSumEts[kNRingsEE];
        for(int iRing=0; iRing<kNRingsEE; ++iRing)
            eeRingsSumEts[iRing].reserve(EEDetId::kSizeForDenseIndexing);
        //---files loop 1
        for(auto& fileName : iovInputFiles[iIOV])
        {
            //---open the next file
            TFile* file = TFile::Open(fileName.c_str());
            CrystalsEBTree ebTree((TTree*)file->Get("eb_xstals"));
            CrystalsEETree eeTree((TTree*)file->Get("ee_xstals"));

            cout << "IOV: " << iIOV << endl;
            cout << "Reading file: " << fileName.c_str() << "..." << endl;
        
            //---get miscalib values
            if(nMisCalib_ == -1)
            {
                misCalibValuesEB_ = new vector<float>;
                misCalibValuesEE_ = new vector<float>;
                TH1F* eb = (TH1F*)file->Get("eb_miscalib");
                TH1F* ee = (TH1F*)file->Get("ee_miscalib");
                if(eb)
                    nMisCalib_ = eb->GetNbinsX();

                nMisCalib_ = 11;
                for(int index=1; index<=nMisCalib_; ++index)
                {
                    int iMis=index-1;
                    if(!eb)
                        iMis = iMis>5 ? iMis-5 : iMis>0 ? iMis-6 : 0;
                    if(eb)
                        misCalibValuesEB_->push_back(eb->GetBinContent(index));
                    else
                        misCalibValuesEB_->push_back(0.01*(float)iMis + 1);
                    if(ee)
                        misCalibValuesEE_->push_back(ee->GetBinContent(index));
                    else
                        misCalibValuesEE_->push_back(0.02*(float)iMis + 1);
                }
            }
            nMisCalib_=10;

            int currentBlock=-1;
            //---EB
            while(ebTree.NextEntry())
            {
                //---skip unwanted blocks
                if(ebTree.begin[0] < IOVBeginRuns[iIOV] || ebTree.end[0] > IOVEndRuns[iIOV])
                    continue;
                
                //---counts summed lumis
                if(currentBlock != ebTree.block)
                {
                    nLumis_ += ebTree.n_lumis;
                    nEvents_ += ebTree.n_events;
                    currentBlock = ebTree.block;
                }
            
                int index = EBDetId(ebTree.ieta, ebTree.iphi).denseIndex();
                int currentRing = ebTree.ieta<0 ? ebTree.ieta + 85 : ebTree.ieta + 84;
                ebXstals_[index] += *ebTree.rec_hit;
                //---no geometry available, thus fill a hashedIndex->ring map
                if(ebRingsMap_[index] == -1)
                    ebRingsMap_[index] = currentRing;

                //---increase all the sums
                ebRingsSumEt2_[currentRing] += ebTree.rec_hit->GetSumEt2(); 
                for(int iMis=0; iMis<=nMisCalib_; ++iMis)
                {
                    if(ebTree.rec_hit->GetSumEt(iMis) > 0)
                    {
                        ebRingsSumEt_[currentRing][iMis] += ebTree.rec_hit->GetSumEt(iMis);
                        goodXstalsEB_[currentRing][ebTree.iphi][iMis]=1;
                    }
                }
            }
            //---EE
            while(eeTree.NextEntry())
            {
                //---skip unwanted blocks
                if(eeTree.begin[0] < IOVBeginRuns[iIOV] || eeTree.end[0] > IOVEndRuns[iIOV])
                    continue;
                
                int currentRing = eeTree.iring;
                currentRing = currentRing<0 ? currentRing+kNRingsEE/2 : currentRing-1+kNRingsEE/2;
                int index = EEDetId(eeTree.ix, eeTree.iy, eeTree.iring>0?1:-1).denseIndex();
                eeXstals_[index] += *eeTree.rec_hit;
                //---no geometry available, thus fill a hashedIndex->ring map
                if(eeRingsMap_[index] == -1)
                    eeRingsMap_[index] = currentRing;

                //---increase all the sums
                eeRingsSumEt2_[currentRing] += eeTree.rec_hit->GetSumEt2();
                for(int iMis=0; iMis<=nMisCalib_; ++iMis)
                {
                    if(eeTree.rec_hit->GetSumEt(iMis) > 0)
                    {
                        eeRingsSumEt_[currentRing][iMis] += eeTree.rec_hit->GetSumEt(iMis);
                        goodXstalsEE_[currentRing][eeTree.ix][eeTree.iy][iMis]=1;
                    }
                }
            }
            file->Close();
        }

        //---compute sumEt cuts by ring
        //---EB
        for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
        {
            int currentRing = ebRingsMap_[index];
            EBDetId ebXstal = EBDetId::detIdFromDenseIndex(index);
            if(currentRing > -1 && goodXstalsEB_[currentRing][ebXstal.iphi()][0])
                ebRingsSumEts[currentRing].push_back(ebXstals_[index].GetSumEt());
        }
        for(int iRing=0; iRing<kNRingsEB; ++iRing)
        {
            sort(ebRingsSumEts[iRing].begin(), ebRingsSumEts[iRing].end());
            float mean=-1;
            float rms=-1;
            for(int iRm=0; iRm<(int)ebRingsSumEts[iRing].size()/2; ++iRm)
            {
                pair<float, float> tmp = PhiSym::VectorMeanRMS(ebRingsSumEts[iRing],
                                                               iRm, ebRingsSumEts[iRing].size()-1-iRm);
                if(mean == -1 || fabs(mean-tmp.first)/mean>0.0005)
                {
                    mean = tmp.first;
                    rms = tmp.second;
                }
                else
                    break;
            }
            //---sumEt cuts
            ebSumEtCuts_[iRing][0] = mean-2*rms;
            ebSumEtCuts_[iRing][1] = mean+2*rms;
        }
        //---EE
        for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
        {            
            int currentRing = eeRingsMap_[index];
            EEDetId eeXstal = EEDetId::detIdFromDenseIndex(index);
            if(currentRing > -1 && goodXstalsEE_[currentRing][eeXstal.ix()][eeXstal.iy()][0])
                eeRingsSumEts[currentRing].push_back(eeXstals_[index].GetSumEt());
        }
        for(int iRing=0; iRing<kNRingsEE; ++iRing)
        {
            sort(eeRingsSumEts[iRing].begin(), eeRingsSumEts[iRing].end());
            float mean=-1;
            float rms=-1;
            for(int iRm=0; iRm<(int)eeRingsSumEts[iRing].size()/2; ++iRm)
            {
                pair<float, float> tmp = PhiSym::VectorMeanRMS(eeRingsSumEts[iRing],
                                                               iRm, eeRingsSumEts[iRing].size()-1-iRm);
                if(mean == -1 || fabs(mean-tmp.first)/mean>0.001)
                {
                    mean = tmp.first;
                    rms = tmp.second;
                }
                else
                    break;
            }
            //---sumEt cuts
            eeSumEtCuts_[iRing][0] = mean-2*rms;
            eeSumEtCuts_[iRing][1] = mean+2*rms;
        }

        //---remove bad crystals from the ring sums
        //---EB
        for(int index=0; index<EBDetId::kSizeForDenseIndexing; ++index)
        {
            int currentRing = ebRingsMap_[index];
            if(currentRing > -1 &&
               (ebXstals_[index].GetSumEt() < ebSumEtCuts_[currentRing][0] ||
                ebXstals_[index].GetSumEt() > ebSumEtCuts_[currentRing][1]))
            {
                EBDetId ebXstal = EBDetId::detIdFromDenseIndex(index);
                ebRingsSumEt2_[currentRing] -= ebXstals_[index].GetSumEt2();
                for(int iMis=0; iMis<=nMisCalib_; ++iMis)
                {
                    if(ebXstals_[index].GetSumEt(iMis) > 0)
                    {
                        ebRingsSumEt_[currentRing][iMis] -= ebXstals_[index].GetSumEt(iMis);
                        goodXstalsEB_[currentRing][ebXstal.iphi()][iMis]=0;
                    }
                }
            }
        }
        //---EE
        for(int index=0; index<EEDetId::kSizeForDenseIndexing; ++index)
        {
            int currentRing = eeRingsMap_[index];
            if(currentRing > -1 &&
               (eeXstals_[index].GetSumEt() < eeSumEtCuts_[currentRing][0] ||
                eeXstals_[index].GetSumEt() > eeSumEtCuts_[currentRing][1]))
            {
                EEDetId eeXstal = EEDetId::detIdFromDenseIndex(index);
                eeRingsSumEt2_[currentRing] -= eeXstals_[index].GetSumEt2();
                for(int iMis=0; iMis<=nMisCalib_; ++iMis)
                {
                    if(eeXstals_[index].GetSumEt(iMis) > 0)
                    {
                        eeRingsSumEt_[currentRing][iMis] -= eeXstals_[index].GetSumEt(iMis);
                        goodXstalsEE_[currentRing][eeXstal.ix()][eeXstal.iy()][iMis]=0;
                    }
                }
            }
        }
        
        //---compute k-fact and ICs for this IOV
        ComputeICs();

        //---finalize outputs
        outFile_->cd();
        outFile_->eb_xstals.Write("eb_xstals");
        outFile_->ee_xstals.Write("ee_xstals");
        out->Close();
    }    
}

#endif
