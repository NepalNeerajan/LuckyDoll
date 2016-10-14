#include <iostream>
#include <iomanip>
#include <string>
#include <sys/time.h>
#include <signal.h>
#include "TMath.h"
#include "TFile.h"
#include "TROOT.h"
#include "TTree.h"
#include "TCutG.h"
#include "TKey.h"
#include "TStopwatch.h"
#include "TClonesArray.h"
#include "CommandLineInterface.h"
#include "AIDAUnpacker.h"
#include "BuildAIDAEvents.h"
#include "AIDA.h"
#include "TVectorD.h"

using namespace TMath;
using namespace std;
bool signal_received = false;
void signalhandler(int sig);
double get_time();


typedef struct {
    unsigned long long T; 	 // Calibrated time
    unsigned long long Tfast;
    double E; 	 // Energy
    double EX;
    double EY;
    double x,y,z;// number of pixel for AIDA, or number of tube for BELEN
    unsigned char ID; 	 // Detector type (BigRips, Aida ion, AIDA beta, BELEN, Clovers)
    //** other stuff pending to define **//
} datatype;


const unsigned char IDion = 4;
const unsigned char IDbeta = 5;

int main(int argc, char* argv[]){

  //! Program start time
  double time_start = get_time();
  TStopwatch timer;
  timer.Start();
  //! Add signal handler
  signal(SIGINT,signalhandler);

  cout << "AIDA event builder" << endl;
  int Verbose = 0;
  long long int WindowIon = 2500; //time unit: 10 ns
  long long int WindowBeta = 2500; //time unit: 10 ns
  long long int WindowDiscriminator = 0;

  int FillFlag = 0;
  long long int TransientTime = 20000;

  char* InputAIDA = NULL;
  char* OutFile = NULL;
  char* CalibrationFile = NULL;
  char* ThresholdFile = NULL;
  char* MappingFile = NULL;

  //Read in the command line arguments
  CommandLineInterface* interface = new CommandLineInterface();
  interface->Add("-a", "AIDA input list of files", &InputAIDA);
  interface->Add("-o", "output file", &OutFile);
  interface->Add("-wi", "Ion event building window (default: 2500*10ns)", &WindowIon);
  interface->Add("-wb", "Beta event building window (default: 2500*10ns)", &WindowBeta);
  interface->Add("-wd", "Fast Discriminator Scan window (default: 0 i.e no scan for fast discrimination)", &WindowDiscriminator);
  interface->Add("-v", "verbose level", &Verbose);

  interface->Add("-map", "mapping file", &MappingFile);
  interface->Add("-cal", "calibration file", &CalibrationFile);
  interface->Add("-thr", "threshold file", &ThresholdFile);

  interface->Add("-f", "fill data or not: 1 fill data 0 no fill", &FillFlag);
  interface->Add("-tt", "aida transient time (default: 20000*10ns)", &TransientTime);


  interface->CheckFlags(argc, argv);
  //Complain about missing mandatory arguments

  if(InputAIDA == NULL){
    cout << "No AIDA input list of files given " << endl;
    return 1;
  }
  if(MappingFile == NULL){
    cout << "No Mapping file given " << endl;
    return 1;
  }
  if(ThresholdFile == NULL){
    cout << "No Threshold table given " << endl;
    return 1;
  }
  if(CalibrationFile == NULL){
    cout << "No Calibration table given " << endl;
    //return 1;
  }
  if(OutFile == NULL){
    cout << "No output ROOT file given " << endl;
    return 2;
  }


  cout<<"output file: "<<OutFile<< endl;

  TFile* ofile = new TFile(OutFile,"recreate");
  ofile->cd();

  datatype aida;
  TTree* tree=new TTree("aida","aida tree ion and beta)");
  if (FillFlag){
  //! Book tree and histograms
      tree->Branch("aida",&aida,"T/l:Tfast/l:E/D:EX/D:EY/D:x/D:y/D:z/D:ID/b");
  }


  //! Read list of files
  string inputfiles[1000];
  ifstream inf(InputAIDA);
  Int_t nfiles;
  inf>>nfiles;


  Int_t implantationrate = 0;

  TVectorD runtime(nfiles+1);
  runtime[0] = 0;
  for (Int_t i=0;i<nfiles;i++){
      runtime[i+1] = 0;
      inf>>inputfiles[i];
      cout<<inputfiles[i]<<endl;
  }

  Double_t ecutX[6];
  Double_t ecutY[6];
  for(Int_t i=0;i<NumDSSD;i++){
      ecutX[i]=0.;
      ecutY[i]=0.;
  }

  for (Int_t i=0;i<nfiles;i++){
      BuildAIDAEvents* evts=new BuildAIDAEvents;
      evts->SetVerbose(Verbose);
      evts->SetMappingFile(MappingFile);
      evts->SetThresholdFile(ThresholdFile);
      evts->SetCalibFile(CalibrationFile);
      evts->SetDiscriminatorTimeWindow(WindowDiscriminator);
      evts->SetAIDATransientTime(TransientTime);
      evts->SetEventWindowION(WindowIon);
      evts->SetEventWindowBETA(WindowBeta);
      evts->SetSumEXCut(ecutX);
      evts->SetSumEYCut(ecutY);
      evts->Init((char*)inputfiles[i].c_str());
      double time_last = (double) get_time();

      int ctr=0;
      int total = evts->GetADNblock();
      int ttotal=0;

      long long tstart;
      long long tend;
      int start=0;

      double local_time_start = get_time();

      //!event loop
      while(evts->GetNextEvent()){
          ttotal++;
          ctr=evts->GetCurrentADBlock();
          if(ctr%1000 == 0){
            int nevtbeta = evts->GetCurrentBetaEvent();
            int nevtion = evts->GetCurrentIonEvent();
            double time_end = get_time();
            cout << inputfiles[i] << setw(5) << setiosflags(ios::fixed) << setprecision(1) << (100.*ctr)/total<<" % done\t" <<
              (Float_t)ctr/(time_end - local_time_start) << " blocks/s " <<
              (Float_t)nevtbeta/(time_end - local_time_start) <<" betas/s  "<<
               (Float_t)nevtion/(time_end - local_time_start) <<" ions/s "<<
               (total-ctr)*(time_end - local_time_start)/(Float_t)ctr << "s to go | ";
            if (evts->IsBETA()) cout<<"beta: x"<<evts->GetAIDABeta()->GetCluster(0)->GetHitPositionX()<<"y"<<
                                       evts->GetAIDABeta()->GetCluster(0)->GetHitPositionY()<<"\r"<< flush;
            else cout<<"ion: x"<<evts->GetAIDAIon()->GetCluster(0)->GetHitPositionX()<<"y"<<
                       evts->GetAIDAIon()->GetCluster(0)->GetHitPositionY()<<"\r"<<flush;
            time_last = time_end;
          }
          if (evts->IsBETA()&&evts->GetAIDABeta()->GetMult()<64) {
              //!sort the timestamp
              vector < pair < unsigned long long,int > > tsvector;
              vector < pair < unsigned long long,int > > ::iterator itsvector;
              for (Int_t i = 0;i<evts->GetAIDABeta()->GetNClusters();i++){
                  tsvector.push_back(make_pair(evts->GetAIDABeta()->GetCluster(i)->GetTimestamp() * ClockResolution,i));
                  /*
                  //! No time order
                  Double_t ex = evts->GetAIDABeta()->GetCluster(i)->GetXEnergy();
                  Double_t ey = evts->GetAIDABeta()->GetCluster(i)->GetYEnergy();
                  aida.ID = IDbeta;
                  aida.E = (ex+ey)/2;
                  aida.EX = ex;
                  aida.EY = ey;
                  //!If you need time ordered then we need to modify this
                  aida.T = itsvector->first;
                  aida.Tfast = evts->GetAIDABeta()->GetCluster(i)->GetFastTimestamp() * ClockResolution;
                  aida.x = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionX();
                  aida.y = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionY();
                  aida.z = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionZ();
                  */
              }
              sort(tsvector.begin(),tsvector.end());
              //!fill tree according to the time stamp
              for (itsvector = tsvector.begin();itsvector<tsvector.end();++itsvector){
                  Int_t i = itsvector->second;
                  Double_t ex = evts->GetAIDABeta()->GetCluster(i)->GetXEnergy();
                  Double_t ey = evts->GetAIDABeta()->GetCluster(i)->GetYEnergy();
                  aida.ID = IDbeta;
                  aida.E = (ex+ey)/2;
                  aida.EX = ex;
                  aida.EY = ey;
                  //!If you need time ordered then we need to modify this
                  aida.T = itsvector->first;
                  aida.Tfast = evts->GetAIDABeta()->GetCluster(i)->GetFastTimestamp() * ClockResolution;
                  aida.x = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionX();
                  aida.y = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionY();
                  aida.z = evts->GetAIDABeta()->GetCluster(i)->GetHitPositionZ();
              }

              if (FillFlag) tree->Fill();
          }else if (!evts->IsBETA()){
              int lastclusterID = evts->GetAIDAIon()->GetNClusters()-1;
              if (evts->GetAIDAIon()->GetCluster(lastclusterID)->GetHitPositionZ()==evts->GetAIDAIon()->GetMaxZ()){
                  Double_t ex = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetXEnergy();
                  Double_t ey = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetYEnergy();
                  aida.ID = IDion;
                  aida.E = (ex+ey)/2;
                  aida.EX = ex;
                  aida.EY = ey;
                  aida.T = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetTimestamp() * ClockResolution;
                  aida.Tfast = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetFastTimestamp() * ClockResolution;
                  aida.x = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetHitPositionX();
                  aida.y = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetHitPositionY();
                  aida.z = evts->GetAIDAIon()->GetCluster(lastclusterID)->GetHitPositionZ();
                  //if (evts->GetAIDAIon()->GetCluster(lastclusterID)->GetHitPositionZ()>0) cout<< "eeeed"<<aida.z<<endl;
                  if (FillFlag) tree->Fill();
              }else{
                  cout<<"Somethings wrong with clustering?"<<endl;
              }
          }

          //!Get run time
          if (evts->IsBETA()&&start==0) {
              tstart = evts->GetAIDABeta()->GetHit(0)->GetTimestamp();
              start++;
          }else if (!evts->IsBETA()&&start==0){
              tstart = evts->GetAIDAIon()->GetHit(0)->GetTimestamp();
              start++;
          }
          if(signal_received){
            break;
          }
      }
      if (evts->IsBETA()) {
          tend = evts->GetAIDABeta()->GetHit(0)->GetTimestamp();
      }else if (!evts->IsBETA()){
          tend = evts->GetAIDAIon()->GetHit(0)->GetTimestamp();
      }
      runtime[i+1] = (double)((tend-tstart)*ClockResolution)/(double)1e9;
      runtime[0] += runtime[i+1];
      cout<<evts->GetCurrentBetaEvent()<<" beta events"<<endl;
      cout<<evts->GetCurrentIonEvent()<<" ion events"<<endl;
      implantationrate += evts->GetCurrentIonEvent();
      cout<<ttotal<<" all events (beta+ion)"<<endl;
      cout<<evts->GetCurrentPulserEvent()<<" pulser events"<<endl;
      delete evts;
  }
  if (FillFlag){
      tree->Write();
      runtime.Write("runtime");
  }
  ofile->Close();

  cout<<"\n**********************SUMMARY**********************\n"<<endl;
  cout<<"Total run length = "<<runtime[0]<< " seconds"<<endl;
  cout<<"Sub runs length"<<endl;
  for (Int_t i=0;i<nfiles;i++){
      cout<<inputfiles[i]<<" - "<<runtime[i+1]<< " seconds"<<endl;
  }
  cout<<"\nImplatation rate =  "<<(double)implantationrate/runtime[0]<< " cps"<<endl;
  //runtime.Print();
  //! Finish----------------
  double time_end = get_time();
  cout << "\nProgram Run time: " << time_end - time_start << " s." << endl;
  timer.Stop();
  cout << "CPU time: " << timer.CpuTime() << "\tReal time: " << timer.RealTime() << endl;

  return 0;
}

void signalhandler(int sig){
  if (sig == SIGINT){
    signal_received = true;
  }
}

double get_time(){
    struct timeval t;
    gettimeofday(&t, NULL);
    double d = t.tv_sec + (double) t.tv_usec/1000000;
    return d;
}
