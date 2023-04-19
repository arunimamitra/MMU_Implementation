//
//  main.cpp
//  MemoryManagementUnit
//
//  Created by Arunima Mitra on 3/30/23.
//  Copyright © 2023 Arunima. All rights reserved.
//

#include <iostream>
#include <vector>
#include <fstream>
#include <vector>
#include <string.h>
#include <istream>
#include <sstream>
#include "math.h"

using namespace std;

std::vector<int> randomValues;
int ofs=0;
ifstream file;
int instructionIndex;
const int PTESize=64;
const int tau=49;
enum counts {UNMAP=0, MAP=1, IN=2, OUT=3, FIN=4, FOUT=5, ZERO=6, SEGV=7, SEGP=8};

void createRandomArray(char* fileName){
    ifstream rfile;
    rfile.open(fileName);
    //commit
    if (rfile.is_open()==false) {
        cout << "The gates of hell will open if I continue to parse with a non-existent file <%s>. Hence I decide to exit this program at this point avoiding insufferable pain\n" << fileName;
        exit(1);
    }
    int elem;
    while(rfile >> elem)
        randomValues.push_back(elem);
    randomValues.erase(randomValues.begin()); //erase first element which just contains size of the file
    //cout<<randomValues.size()<<endl;
}

class vma{
    int beginVPage, endVPage, writeProtected, fileMapped;
public:
    void setValues( int b,  int e,  int w,  int f){
        beginVPage=b;
        endVPage=e;
        writeProtected=w;
        fileMapped=f;
    }
    int getBeginPage(){ return beginVPage; }
    int getEndPage(){ return endVPage; }
    int isWriteProtected(){ return writeProtected; }
    int isFileMapped(){ return fileMapped; }
};

class InstructionPair{
    char type;
    unsigned int ind;
public:
    void setValues(char c, unsigned int i){
        type=c;
        ind=i;
    }
    char getType(){ return type;}
    unsigned int getInd(){ return ind ; }
};

typedef struct {
    unsigned int index:1;
    bool validBit:1;
    bool referenceBit:1;
    bool modifyBit:1;
    bool writeProtectBit:1;
    bool pagedOutBit:1;
    bool fileMappedBit:1;
    unsigned int frameNo:7;
} pte_t;


class Process{
    unsigned int pid;
    unsigned int vmaCount;
    vector<vma> vmaList;
    
public:
    pte_t* pageTable[PTESize];
    int countStats[9];
    void setValues(int a, int b){
        pid=a;
        vmaCount=b;
        for(int i=0;i<PTESize;i++) pageTable[i]=new pte_t;
        for(int i=0;i<9;i++) countStats[i]=0;
        //for(int i=0;i<9;i++) cout<<i<<" "<<countStats[i];
    }
    int* getCountStats(){ return countStats; }
    void addToVMAList( vma v) { vmaList.push_back(v);
        if(v.isWriteProtected()==1){
            for(int i=v.getBeginPage();i<=v.getEndPage();i++){
                pageTable[i]->writeProtectBit=true;
            }
        }
        if(v.isFileMapped()==1){
            for(int i=v.getBeginPage();i<=v.getEndPage();i++){
                pageTable[i]->fileMappedBit=true;
            }
        }
    }
    vector<vma> getVMAList(){ return vmaList; }
    unsigned int getPID(){ return pid; }
};



typedef struct {
    unsigned int pid;
    unsigned int frameNumber, pageNumber=-1;
    bool isVacant=true;
    unsigned age=0;
} frame_t;


frame_t** frameTable;
int frameTableSize;
deque<frame_t*> freeList;
vector<Process> createdProcesses;
vector<InstructionPair> instructions;
int numberOfProcesses=0;

void initialize(char* fileName, int frameTableSize){
    file.open(fileName);
    string lineNow;
    unsigned int numberOfVMA=0;
     
     if (file.is_open()==false) {
         cout << "The gates of hell will open if I continue to parse with a non-existent file <%s>. Hence I decide to exit this program at this point avoiding insufferable pain\n" << fileName;
         exit(1);
     }
    
    while (getline(file, lineNow)) {
        if(lineNow[0]=='#') continue;
        istringstream str(lineNow);
        if(numberOfProcesses==0){
            str >> numberOfProcesses;
            continue;
        }
        
        if(createdProcesses.size()==numberOfProcesses && numberOfVMA==0) {
            // instruction line
            char c;
            int i;
            str >> c >> i;
            InstructionPair inst;
            inst.setValues(c, i);
            instructions.push_back(inst);
            continue;
        }
        
        if(numberOfVMA==0) //vma count is input line
        {
            str >> numberOfVMA;
            // create new process now and subsequenlty add to vmalist
            Process p;
            p.setValues(createdProcesses.size(), numberOfVMA);
            createdProcesses.push_back(p);
        }
        else{   // vma line is input line
            vma X;
            Process p = createdProcesses.back();
            createdProcesses.pop_back();
            int beg,end,wp,fm;
            
            str >> beg >> end >> wp >>fm;
            //cout<<beg<<" "<<end<<" "<<wp<<" "<<fm<<endl;
            X.setValues(beg, end, wp, fm);
            p.addToVMAList(X);
            createdProcesses.push_back(p);
            numberOfVMA--;
        }
    }
    
//    for(int i=0;i<instructions.size();i++){
//        cout<<instructions.at(i).getType()<<"\t"<<instructions.at(i).getInd()<<endl;
//    }
    
    for(int i=0;i<frameTableSize;i++){
        frame_t* f=new frame_t;
        f->frameNumber=i;
        freeList.push_back(f);
        frameTable[i]=f;
    }
}

frame_t* allocateFromFreeList(){
    if(freeList.empty()) return nullptr;
    frame_t* f=freeList.front();
    freeList.pop_front();
    return f;
}

class Pager {
public:
    virtual frame_t* selectVictim() = 0; // virtual base class
};
Pager* THE_PAGER;
int frameHand;

class FIFO:public Pager{
public:
    frame_t* selectVictim(){
        frame_t* f = frameTable[frameHand];
//        frameHand++;
//        if(frameHand==frameTableSize) frameHand=0;
        frameHand=(frameHand+1)%frameTableSize;
        return f;
    }
};

class CLOCK:public Pager{
public:
    frame_t* selectVictim(){
        // do this like fifo, just skip ptes with R bit set
        frame_t* f = frameTable[frameHand];
        Process proc=createdProcesses[f->pid];
        pte_t* pte=proc.pageTable[f->pageNumber];
        
        while(pte->referenceBit){
            pte->referenceBit=false;
//            frameHand++;
//            if(frameHand==frameTableSize) frameHand=0;
            frameHand=(frameHand+1)%frameTableSize;
            frame_t* newFrame = frameTable[frameHand];
            Process newProc=createdProcesses[newFrame->pid];
            pte=newProc.pageTable[newFrame->pageNumber];
        }
        
        frame_t* finalFarme=frameTable[frameHand];
        frameHand=(frameHand+1)%frameTableSize;
        
        return finalFarme;
    }
};


class RANDOM:public Pager{
public:
    frame_t* selectVictim(){
        frameHand=(myrandom());
        frame_t* f = frameTable[frameHand % frameTableSize];
        return f;
    }
    
    int myrandom() {
    return randomValues.at(ofs++%(randomValues.size())); }
};

class AGING: public Pager{
public:
    frame_t* selectVictim() {
        int ptr=frameHand, finalFramePtr=frameHand;
        
        // left shift all frametable entries
        for(int i=0;i<frameTableSize;i++){
            frameTable[i]->age = frameTable[i]->age/2;
        }
        ptr=frameHand;
        
        // reference bit merge and final frame pointer chosen
        for(int i=0;i<frameTableSize;i++){
            pte_t* pte = createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber];
            if(pte->referenceBit==true) frameTable[ptr]->age=frameTable[ptr]->age | 0x80000000;
            if(frameTable[ptr]->age < frameTable[finalFramePtr]->age) finalFramePtr=ptr;
            ptr++;
            if(ptr==frameTableSize) ptr=0;
        }
        
        frameHand=(finalFramePtr+1)%frameTableSize;

        for (int i = 0; i < frameTableSize; i++){
            createdProcesses[frameTable[i]->pid].pageTable[frameTable[i]->pageNumber]->referenceBit = false;
        }
        return frameTable[finalFramePtr];
    }
};

class NRU:public Pager{
    int lastTime=0;
public:
    frame_t * selectVictim(){
        
        int inst=instructionIndex-lastTime+1;
        int ptr=frameHand;
        int class0=-1,class1=-1,class2=-1,class3=-1;
        
        pte_t * pte;
        for(int i=0;i<frameTableSize;i++){
            pte = createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber];
            if(pte->referenceBit && pte->modifyBit==false && class2==-1) class2=ptr;
            else if(pte->referenceBit && pte->modifyBit && class3==-1) class3=ptr;
            else if(pte->referenceBit==false){
                if(pte->modifyBit && class1==-1) class1=ptr;
                else if(pte->modifyBit==false){
                    if(inst<=tau){
                        frameHand=(ptr+1)%frameTableSize;
                        return frameTable[ptr];
                    }
                    else{
                        class0=ptr;
                        break;
                    }
                }
            }
        }
        
        if(inst>tau){
            for(int k=0;k<frameTableSize;k++){
                createdProcesses.at(frameTable[k]->pid).pageTable[frameTable[k]->pageNumber]->referenceBit=false;
            }
            lastTime++;
        }
        
        int temp;
        if (class3 != -1) temp = class3;
        if (class2 != -1) temp = class2;
        if (class1 != -1) temp = class1;
        if (class0 != -1) temp = class0;
        frameHand = (temp+1) % frameTableSize;
        return frameTable[temp];
    }
    
    //proc_arr[frametable[i]->proc_no].page_table[frametable[i]->page_index]->referenced = 0;
};


frame_t *get_frame() {
    frame_t *frame = allocateFromFreeList();
    if (frame == NULL) frame = THE_PAGER->selectVictim();
    return frame;
}

unsigned long ctx_switches=0, process_exits=0;
unsigned long long cost=0;
void Simulation(){
    
    Process currentProc;
    /// for NRU
    for(instructionIndex=0;instructionIndex<instructions.size();instructionIndex++ ){
        InstructionPair inst = instructions.at(instructionIndex);
        char operation = inst.getType();
        unsigned int page = inst.getInd();
        //cout<<"iteration"<<i<<"\toperation : "<<operation<<"\t page : "<<page<<endl;
        cout<<instructionIndex<<": ==> "<<operation<<" "<<page<<endl;
        switch(operation){
            case 'c': {
                ctx_switches++;
                cost+=130;
                currentProc = createdProcesses.at(page);
                break;
            }
            case 'r': {
                cost++;
                
                //check for segv
                bool segv=true;
                for(int j=0;j<currentProc.getVMAList().size();j++){
                    if(page>=currentProc.getVMAList().at(j).getBeginPage() && page<=currentProc.getVMAList().at(j).getEndPage()){
                        segv=false;
                        break;
                    }
                }
                if(segv){
                    cout<<" SEGV"<<endl;
                    currentProc.countStats[SEGV]++;
                    cost+=440;
                    createdProcesses.at(currentProc.getPID())=currentProc;
                    continue;
                }
                
                //check if the vpn already present in frame table
                bool flag=false;
                int frameInd;
                for(frameInd=0;frameInd<frameTableSize;frameInd++){
                    if(frameTable[frameInd]->pageNumber==page && frameTable[frameInd]->pid == currentProc.getPID()){
                        //cout<<frameTable[frameInd]->pageNumber<<"found"<<endl;
                        flag=true;
                        break;
                    }
                }
                pte_t* pte= currentProc.pageTable[page];
                pte->validBit=true;
                pte->referenceBit=true;
                if(flag) {
                    pte->frameNo=frameInd;
                    currentProc.pageTable[page] = pte;
                    break;
                }
                else{
                    frame_t *newframe = get_frame();
                    if(!newframe->isVacant){
                        Process oldProc=createdProcesses[newframe->pid];
                        pte_t* o_pte = oldProc.pageTable[newframe->pageNumber];
                        
                        //UNMAP 0:14
                        cout<<" UNMAP "<<newframe->pid<<":"<<newframe->pageNumber<<endl;
                        oldProc.countStats[UNMAP]++;
                        cost+=410;
                        
                        if(o_pte->modifyBit==true){
                            o_pte->pagedOutBit=true;
                            if(o_pte->fileMappedBit){
                                oldProc.countStats[FOUT]++;
                                cost+=2800;
                                cout<<" FOUT"<<endl;
                            }
                            else{
                                oldProc.countStats[OUT]++;
                                cost+=2750;
                                cout<<" OUT"<<endl;
                            }
                        }
                        
                        o_pte->modifyBit=false;
                        o_pte->referenceBit=false;
                        o_pte->validBit=false;
                        oldProc.pageTable[newframe->pageNumber] = o_pte;
                        createdProcesses[newframe->pid]=oldProc;
                    }
                    
                        newframe->isVacant=false;
                        newframe->pageNumber=page;
                        newframe->age=0;
                        newframe->pid=currentProc.getPID();
                        frameTable[newframe->frameNumber]=newframe;
                    
                        currentProc = createdProcesses.at(newframe->pid);
                        pte=currentProc.pageTable[page];
                    
                        pte->frameNo=newframe->frameNumber;
                        currentProc.pageTable[page] = pte;
                        //ZERO
                        //MAP 3
                    if(pte->fileMappedBit){
                        cout<<" FIN\n";
                        currentProc.countStats[FIN]++;
                        cost+=2350;
                    }
                    else if(!pte->pagedOutBit){
                        cout<<" ZERO\n";
                        currentProc.countStats[ZERO]++;
                        cost+=150;
                    }
                    else {
                        cout<<" IN\n";
                        currentProc.countStats[IN]++;
                        cost+=3200;
                    }
                        cout<<" MAP "<<newframe->frameNumber<<endl;
                        currentProc.countStats[MAP]++;
                        cost+=350;
                        createdProcesses[newframe->pid]=currentProc;
                        break;
                }
                
            }
            
            case 'w':{
                cost++;
                
                //check for segv
                bool segv=true;
                for(int j=0;j<currentProc.getVMAList().size();j++){
                    if(page>=currentProc.getVMAList().at(j).getBeginPage() && page<=currentProc.getVMAList().at(j).getEndPage()){
                        segv=false;
                        break;
                    }
                }
                if(segv){
                    cout<<" SEGV"<<endl;
                    currentProc.countStats[SEGV]++;
                    cost+=440;
                    createdProcesses.at(currentProc.getPID())=currentProc;
                    continue;
                }
                //check if the vpn already present in frame table
                bool flag=false;
                int frameInd;
                for(frameInd=0;frameInd<frameTableSize;frameInd++){
                    if(frameTable[frameInd]->pageNumber==page && frameTable[frameInd]->pid == currentProc.getPID()){
                        flag=true;
                        break;
                    }
                }
                pte_t* pte= currentProc.pageTable[page];
                pte->validBit=true;
                pte->referenceBit=true;
                pte->modifyBit=true;
                
                
                if(flag) {
                    pte->frameNo=frameInd;
                    // check if segprot when frame is already present(and read)
                    if(pte->writeProtectBit){
                        pte->modifyBit=false;
                        cost+=410;
                        currentProc.countStats[SEGP]++;
                        cout<<" SEGPROT"<<endl;
                    }
                    currentProc.pageTable[page] = pte;
                    createdProcesses[currentProc.getPID()]=currentProc;
                    break;
                }
                else{
                    frame_t *newframe = get_frame();
                    if(!newframe->isVacant){
                        Process oldProc=createdProcesses[newframe->pid];
                        pte_t* o_pte = oldProc.pageTable[newframe->pageNumber];
                        
                        //UNMAP 0:14
                        cout<<" UNMAP "<<newframe->pid<<":"<<newframe->pageNumber<<endl;
                        oldProc.countStats[UNMAP]++;
                        cost+=410;
                        
                        if(o_pte->modifyBit==true){
                            o_pte->pagedOutBit=true;
                            if(o_pte->fileMappedBit){
                                oldProc.countStats[FOUT]++;
                                cost+=2800;
                                cout<<" FOUT"<<endl;
                            }
                            else{
                                oldProc.countStats[OUT]++;
                                cost+=2750;
                                cout<<" OUT"<<endl;
                            }
                        }
                        
                        o_pte->modifyBit=false;
                        o_pte->referenceBit=false;
                        o_pte->validBit=false;
                        oldProc.pageTable[newframe->pageNumber] = o_pte;
                        createdProcesses[newframe->pid]=oldProc;
                    }
                        
                        newframe->isVacant=false;
                        newframe->pageNumber=page;
                        newframe->age=0;
                        newframe->pid=currentProc.getPID();
                        frameTable[newframe->frameNumber]=newframe;
                    
                        currentProc = createdProcesses.at(newframe->pid);
                        pte=currentProc.pageTable[page];
                    
                        pte->frameNo=newframe->frameNumber;
                        
                        if(pte->fileMappedBit){
                            cout<<" FIN\n";
                            currentProc.countStats[FIN]++;
                            cost+=2350;
                        }
                        else if(!pte->pagedOutBit){
                            cout<<" ZERO\n";
                            currentProc.countStats[ZERO]++;
                            cost+=150;
                        }
                        else {
                            cout<<" IN\n";
                            currentProc.countStats[IN]++;
                            cost+=3200;
                        }
                    
                        cout<<" MAP "<<newframe->frameNumber<<endl;
                        currentProc.countStats[MAP]++;
                        cost+=350;
                        if(pte->writeProtectBit){
                            pte->modifyBit=false;
                            cost+=410;
                            currentProc.countStats[SEGP]++;
                            cout<<" SEGPROT"<<endl;
                        }
                        currentProc.pageTable[page] = pte;
                        createdProcesses[newframe->pid]=currentProc;
                
                        break;
                }
            }
            
            case 'e':{
                cost+=1230;
                process_exits++;
                cout<<"EXIT current process "<<currentProc.getPID()<<endl;

                for (int j = 0; j<currentProc.getVMAList().size();j++){
                    for (int k = currentProc.getVMAList().at(j).getBeginPage(); k<=currentProc.getVMAList().at(j).getEndPage(); k++){
                        
                        if (currentProc.pageTable[k]->validBit) {// unmap thing
                            currentProc.pageTable[k]->validBit=false;
                            currentProc.countStats[UNMAP]++;
                            cost+= 410;
                            cout<<" UNMAP "<<currentProc.getPID()<< ":"<<k<<endl;
                            if (currentProc.pageTable[k]->modifyBit && currentProc.pageTable[k]->fileMappedBit==true) {
                                currentProc.countStats[FOUT]++;
                                cost+=2800;
                                cout << " FOUT"<<endl;
                            }
                            frameTable[currentProc.pageTable[k]->frameNo]->isVacant=true;
                            freeList.push_back(frameTable[currentProc.pageTable[k]->frameNo]);
                        }
                        currentProc.pageTable[k]->pagedOutBit=false;
                        createdProcesses.at(currentProc.getPID())=currentProc;
                    }

                }
                
                
            }
        }
    }
}

void printPageTable(){
    for(int i=0;i<createdProcesses.size();i++){
        cout<<"PT["<<i<<"]:";
        for(int j=0;j<PTESize;j++){
            pte_t* pte=createdProcesses.at(i).pageTable[j];
            if(pte->validBit==false){
                if(pte->pagedOutBit) cout<<" #";
                else cout<<" *";
            }
            else{
                cout<<" "<<j<<":";
                if(pte->referenceBit)cout<<"R";
                else cout<<"-";
                if(pte->modifyBit)cout<<"M";
                else cout<<"-";
                if(pte->pagedOutBit)cout<<"S";
                else cout<<"-";
            }
        }
        cout<<endl;
    }
    
}

void printFrameTable(){
    cout<<"FT:";
    for(int i=0;i<frameTableSize;i++){
        frame_t * f=frameTable[i];
        if(f->isVacant){
            cout<<" *";
            continue;
        }
        cout<<" "<<f->pid<<":"<<f->pageNumber;
    }
    cout<<endl;
}

void printCountStats(){
    for(int i=0;i<createdProcesses.size();i++){
        Process proc = createdProcesses.at(i);
        //for(int i=0;i<9;i++) cout<<i<<" "<<proc.countStats[i];
        printf("PROC[%d]: U=%d M=%d I=%d O=%d FI=%d FO=%d Z=%d SV=%d SP=%d\n",
               proc.getPID(), proc.countStats[UNMAP], proc.countStats[MAP], proc.countStats[IN], proc.countStats[OUT], proc.countStats[FIN], proc.countStats[FOUT], proc.countStats[ZERO], proc.countStats[SEGV], proc.countStats[SEGP]);
    }
    
}

//maps=350, unmaps=410, ins=3200, outs=2750, fins=2350, fouts=2800, zeros=150, segv=440, segprot=410
void printCostStats(){
    
    cost =
    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
           instructions.size(), ctx_switches, process_exits, cost, sizeof(pte_t));
}


int main(int argc, const char * argv[]) {
    // insert code here...
    char* rfile= "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/rfile";
    createRandomArray(rfile);
    cout<<"rfile created"<<endl;
    char* inputFile = "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/in11";
    frameTableSize=32;
    frameTable = new frame_t*[frameTableSize];
    initialize(inputFile, frameTableSize);
    //cout<<"size of my pte_t is "<<sizeof(pte_t)<<endl;
    THE_PAGER=new NRU();
    Simulation();
    printPageTable();
    printFrameTable();
    printCountStats();
    printCostStats();
    return 0;
}
