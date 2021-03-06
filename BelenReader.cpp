#include "BelenReader.h"

void genRndCircle(Double_t &x,Double_t &y,Double_t a,Double_t b,Double_t xpos,Double_t ypos,Double_t R){
    if (b<a){
        Double_t temp=a;
        a=b;
        b=temp;
    }
    x=xpos+b*R*TMath::Cos(2*TMath::Pi()*a/b);
    y=ypos+b*R*TMath::Sin(2*TMath::Pi()*a/b);
}

BelenReader::BelenReader():rr()
{

    for (Int_t i=0;i<MaxID;i++){
        fHe3Id2posX[i]=0;
        fHe3Id2posY[i]=0;
        fHe3Id2posZ[i]=0;
        fHe3Id2diameter[i]=0;
        fHe3Id2ring[i]=0;
        fHe3Id2length[i]=0;
    }

    for (Int_t i=0;i<MaxIndex1;i++){
        for (Int_t j=0;j<MaxIndex2;j++){
            fCrystalId2posX[i][j]=0;
            fCrystalId2posY[i][j]=0;
            fCrystalId2posZ[i][j]=0;
        }
    }

    ftreedataNeuron = NULL;
    ftreedataGamma = NULL;
    ftreedataAnc = NULL;
    fflag_filldata = false;
}

BelenReader::~BelenReader()
{
    delete flocalNeutron;
    delete flocalGamma;
}


void BelenReader::Init(char* belenfile){
    fBLAncEntry = 0;
    fBLGamEntry = 0;
    fBLAncEntry = 0;

    flocalNeutron = new BELENHit;
    flocalGamma = new CloverHit;

    flocalAnc = new BELENHit;

    finfile = new TFile(belenfile);
    ftree = (TTree*) finfile->Get("BelenTree");

    fnentries = ftree->GetEntries();
    cout<<"There are "<<fnentries<<" entries in Belen: "<< belenfile<<endl;

    //! brach tree
    ftree->SetBranchAddress("Neutrons",&ftreedataNeuron);
    ftree->SetBranchAddress("Gamma",&ftreedataGamma);
    ftree->SetBranchAddress("Ancillary",&ftreedataAnc);

    fcurentry = 0;
    fBLNeuEntry = 0;
    fBLGamEntry = 0;
    fBLAncEntry = 0;
    GetGeoMapping();
    //if (!GetNextEvent()) exit(1);
}

void BelenReader::ClearAncHits(){
    /*s
    for (unsigned int idx=0;idx<flocalAncUpstreamPL.size();idx++){
        delete flocalAncUpstreamPL[idx];
    }
    for (unsigned int idx=0;idx<flocalAncAIDAPL.size();idx++){
        delete flocalAncAIDAPL[idx];
    }
    for (unsigned int idx=0;idx<flocalAncdE.size();idx++){
        delete flocalAncdE[idx];
    }
    for (unsigned int idx=0;idx<flocalAncF11PL.size();idx++){
        delete flocalAncF11PL[idx];
    }
    flocalAncUpstreamPL.clear();
    flocalAncAIDAPL.clear();
    flocalAncdE.clear();
    flocalAncF11PL.clear();
    */
    flocalAnc->Clear();
}

void BelenReader::GetGeoMapping(){
    std::ifstream inpf(fmappingfile);
    if (inpf.fail()){
        cout<<"No BELEN Mapping file is given"<<endl;
        return;
    }
    cout<<"Start reading BELEN Mapping file: "<<fmappingfile<<endl;

    Int_t id,index1,index2;
    UShort_t ring;
    Double_t x,y,z;
    Double_t d,length;
    Int_t mm=0;

    while (inpf.good()){
        inpf>>id>>index1>>index2>>d>>x>>y>>z>>ring>>length;
        if (id<=500){//for he3
            fHe3Id2posX[id]=x;
            fHe3Id2posY[id]=y;
            fHe3Id2posZ[id]=z;
            fHe3Id2diameter[id]=d;
            fHe3Id2ring[id]=ring;
            fHe3Id2length[id]=length;
        }else if(id>500){ //for clover
            fCrystalId2posX[index1][index2]=x;
            fCrystalId2posY[index1][index2]=y;
            fCrystalId2posZ[index1][index2]=z;
        }
        //cout<<He3id<<"-"<<daqId<<"-"<<d<<"-"<<x<<"-"<<y<<"-"<<z<<endl;
        mm++;
    }
    cout<<"Read "<<mm<<" line"<<endl;
    inpf.close();
}

void BelenReader::BookTree(TTree* treeNeutron, TTree *treeGamma, TTree *treeAnc, Int_t bufsize){
    //! initilize output
    fmtrNeutron = treeNeutron;
    //fmtrNeutron->Branch("blentry",&fBLNeuEntry,bufsize); //320000
    //fmtrNeutron->Branch("blTS",&fBLtsNeutron,bufsize);
    //fmtrNeutron->Branch("neutron",&flocalNeutron,bufsize);
    fmtrNeutron->Branch("neutron",&flocalNeutron,bufsize);
    fmtrNeutron->BranchRef();


    fmtrGamma = treeGamma;
    //fmtrGamma->Branch("blentry",&fBLGamEntry,bufsize);
    //fmtrGamma->Branch("blTS",&fBLtsGamma,bufsize);
    fmtrGamma->Branch("gamma",&flocalGamma,bufsize);
    fmtrGamma->BranchRef();

    fmtrAnc = treeAnc;
    //fmtrAnc->Branch("blentry",&fBLAncEntry,bufsize);
    //fmtrAnc->Branch("blTS",&fBLtsAnc,bufsize);
    /*
    fmtrAnc->Branch("f11",&flocalAncF11PL,bufsize);
    fmtrAnc->Branch("uveto",&flocalAncUpstreamPL,bufsize);
    fmtrAnc->Branch("dE",&flocalAncdE,bufsize);
    fmtrAnc->Branch("dveto",&flocalAncAIDAPL,bufsize);
    */
    fmtrAnc->Branch("anc",&flocalAnc,bufsize);
    fmtrAnc->BranchRef();

    fflag_filldata=true;
}


bool BelenReader::GetNextEvent(){
    ftree->GetEntry(fcurentry);

    fE = ftreedataNeuron->E + ftreedataGamma->E + ftreedataAnc->E;
    fT = ftreedataNeuron->T + ftreedataGamma->T + ftreedataAnc->T;
    fId = ftreedataNeuron->Id + ftreedataGamma->Id + ftreedataAnc->Id;
    ftype = ftreedataNeuron->type + ftreedataGamma->type + ftreedataAnc->type;
    fIndex1 = ftreedataNeuron->Index1 + ftreedataGamma->Index1 + ftreedataAnc->Index1;
    fIndex2 = ftreedataNeuron->Index2 + ftreedataGamma->Index2 + ftreedataAnc->Index2;

    fInfoFlag = ftreedataNeuron->InfoFlag + ftreedataGamma->InfoFlag + ftreedataAnc->InfoFlag;
    fName = ftreedataNeuron->Name + ftreedataGamma->Name + ftreedataAnc->Name;

    fcurentry++;
    //! fill data if needed
    if (ftype==1){
        flocalNeutron->SetEnergy(fE);
        flocalNeutron->SetTimestamp(fT);
        flocalNeutron->SetDaqID(fId);
        Int_t id = atoi(fName.substr(2,3).c_str());
        flocalNeutron->SetID(id);
        flocalNeutron->SetRing(fIndex1);
        flocalNeutron->SetType(fIndex2);
        PerturbateHe3(id);
        flocalNeutron->SetRndPos(fposX,fposY,fposZ);
        if (fflag_filldata) fmtrNeutron->Fill();
        fBLNeuEntry++;
    }else if (ftype==2){
        flocalGamma->SetEnergy(fE);
        flocalGamma->SetTimestamp(fT);
        flocalGamma->SetDaqID(fId);
        if (fIndex1==Index1Clover1) flocalGamma->SetClover(1);
        else if (fIndex1==Index1Clover2) flocalGamma->SetClover(2);
        flocalGamma->SetCloverLeaf(fIndex2);
        flocalGamma->SetID((flocalGamma->GetClover()-1)*4 + flocalGamma->GetCloverLeaf());
        PerturbateClover(fIndex1,fIndex2);
        flocalGamma->SetPos(fposX,fposY,fposZ);
        if (fflag_filldata) fmtrGamma->Fill();
        fBLGamEntry++;
    }
    else if (ftype==3){
        //clear hits first!
        //ClearAncHits();
        /*
        BELENHit* hit = new BELENHit();
        hit->SetEnergy(fE);
        hit->SetTimestamp(fT);
        hit->SetDaqID(fId);
        hit->SetID(fIndex2);
        hit->SetRing(0);
        hit->SetPos(0,0,0);
        */
        flocalAnc->SetEnergy(fE);
        flocalAnc->SetTimestamp(fT);
        flocalAnc->SetDaqID(fId);
        flocalAnc->SetID(fIndex2);
        if (fIndex1==Index1UPlastic){
            flocalAnc->SetPos(0,0,1);
            flocalAnc->SetRing(2);
            flocalAnc->SetType(ScintillatorType);
            //hit->SetType(ScintillatorType);
            //flocalAncUpstreamPL.push_back(hit);
        }else if(fIndex1==Index1F11){
            flocalAnc->SetRing(1);
            flocalAnc->SetType(ScintillatorType);
            flocalAnc->SetPos(0,0,1);
            //hit->SetType(ScintillatorType);
            //flocalAncF11PL.push_back(hit);
        }else if (fIndex1==Index1AIDAPL){
            flocalAnc->SetRing(4);
            flocalAnc->SetType(ScintillatorType);
            flocalAnc->SetPos(0,0,4);
            //hit->SetType(ScintillatorType);
            //flocalAncAIDAPL.push_back(hit);
        }else if (fIndex1==Index1dE){
            flocalAnc->SetRing(3);
            flocalAnc->SetType(SilliconType);
            flocalAnc->SetPos(0,0,3);
            //hit->SetType(SilliconType);
            //flocalAncdE.push_back(hit);
        }
        if (fflag_filldata) fmtrAnc->Fill();
        fBLAncEntry++;
    }
    if (fcurentry>fnentries) return false;
    return true;
}

bool BelenReader::GetNextNeutronEvent(){
    if (!GetNextEvent()) return false;
    while (ftype!=1){
        if (!GetNextEvent()) return false;
    }
    fBLNeuEntry++;
    return true;
}

bool BelenReader::GetNextGammaEvent(){
    if (!GetNextEvent()) return false;
    while (ftype!=2){
        if (!GetNextEvent()) return false;
    }
    fBLGamEntry++;
    return true;
}

bool BelenReader::GetNextAncEvent(){
    if (!GetNextEvent()) return false;
    while (ftype!=3){
        if (!GetNextEvent()) return false;
    }
    fBLAncEntry++;
    return true;
}

void BelenReader::PerturbateHe3(UShort_t He3Id){
    //!there is nothing here for the moment!
    fposX = fHe3Id2posX[He3Id];
    fposY = fHe3Id2posY[He3Id];
    fposZ = fHe3Id2posZ[He3Id];
    flocalNeutron->SetPos(fposX,fposY,fposZ);
    //! pertubating
    Double_t a,b,x,y,r;
    r=fHe3Id2diameter[He3Id]/2;
    a=rr.Rndm();
    b=rr.Rndm();
    genRndCircle(x,y,a,b,fposX,fposY,r);
    fposX = x;
    fposY = y;
    fposZ = rr.Rndm()*fHe3Id2length[He3Id]+fposZ-fHe3Id2length[He3Id]/2;
}

void BelenReader::PerturbateClover(UShort_t Index1, UShort_t Index2){
    //!there is nothing here for the moment!
    fposX = fCrystalId2posX[Index1][Index2];
    fposY = fCrystalId2posZ[Index1][Index2];
    fposZ = fCrystalId2posZ[Index1][Index2];
    //! pertubating
}
