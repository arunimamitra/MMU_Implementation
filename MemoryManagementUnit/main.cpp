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
#include <queue>
#include <string.h>
#include <istream>
#include <sstream>
#include "math.h"
#include <stdlib.h>
#include <unistd.h>
#include<getopt.h>
#include <iomanip>
#include <cstring>
#include <string>
#include <map>
#define stringify( name ) #name

using namespace std;

std::vector<int> randomValues;
bool oO = false, oP = false, oF = false, oS = false;

int ofs=0;
ifstream file;
int instructionIndex;
const int PTESize=64;
const int tau=49;
enum counts {UNMAP=0, MAP=1, IN=2, OUT=3, FIN=4, FOUT=5, ZERO=6, SEGV=7, SEGP=8};

void createRandomArray(string fileName){
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
    unsigned long tau;
} frame_t;


frame_t** frameTable;
int frameTableSize;
deque<frame_t*> freeList;
//std::queue<frame_t*> freeList;
vector<Process> createdProcesses;
vector<InstructionPair> instructions;
int numberOfProcesses=0;

void initialize(string fileName, int frameTableSize){
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
            
            if(!pte->referenceBit && !pte->modifyBit && inst<=tau){
                frameHand=(ptr+1)%frameTableSize;
                return frameTable[ptr];
            }
            /*NRU CLASSES
            0:- Not Referenced, Not Modified
            1:- Not Referenced, Modified
            2:- Referenced, Not Modified
            3:- Referenced, Modified*/
            
            if(!pte->referenceBit && !pte->modifyBit){
                class0=ptr;
                break;
            }
            else if(class1==-1 && !pte->referenceBit && pte->modifyBit){
                class1=ptr;
            }
            else if(class2==-1 && pte->referenceBit && !pte->modifyBit){
                class2=ptr;
            }
            else if(class3==-1 && pte->referenceBit && pte->modifyBit){
                class3=ptr;
            }
            
            
//            if(pte->referenceBit && pte->modifyBit==false && class2==-1) class2=ptr;
//            else if(pte->referenceBit && pte->modifyBit && class3==-1) class3=ptr;
//            else if(pte->referenceBit==false){
//                if(pte->modifyBit && class1==-1) class1=ptr;
//                else if(pte->modifyBit==false){
//                    if(inst<=tau){
//                        frameHand=(ptr+1)%frameTableSize;
//                        return frameTable[ptr];
//                    }
//                    else{
//                        class0=ptr;
//                        break;
//                    }
//                }
//            }
            ptr=(ptr+1)%frameTableSize;
        }
        
        if(inst>tau){
            for(int k=0;k<frameTableSize;k++){
                createdProcesses.at(frameTable[k]->pid).pageTable[frameTable[k]->pageNumber]->referenceBit=false;
            }
            lastTime=instructionIndex+1;
        }
        
        int x;
        if (class0!=-1) x = class0;
        else if (class1!=-1) x = class1;
        else if (class2!=-1) x = class2;
        else if (class3!=-1) x = class3;
        frameHand=(x+1)%frameTableSize;
        return frameTable[x];
    }
    //proc_arr[frametable[i]->proc_no].page_table[frametable[i]->page_index]->referenced = 0;
};

class WSET:public Pager{
public:
    frame_t* selectVictim(){
        int ptr=frameHand,temp=-1,oldF=-1,old2=-1,reset=0;
        int max=0,small1=0,small2=0;
        for(int i=0;i<frameTableSize;i++){
            pte_t* pte=createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber];
            if(pte->referenceBit){
                reset++;
                if(oldF==-1){
                    small1=frameTable[ptr]->tau;
                    oldF=ptr;
                }
            }
            else if(instructionIndex-frameTable[ptr]->tau>tau){
                temp=ptr;
                max=i;
                break;
            }
            else if(instructionIndex<=tau+frameTable[ptr]->tau && old2==-1){
                old2=ptr;
                small2=frameTable[ptr]->tau;
            }
            ptr=(ptr+1)%frameTableSize;
        }
        
        
        ptr=frameHand;
        
        if(temp==-1){
            if(reset==frameTableSize){
                for (int i = 0; i < frameTableSize; i++) {
                    createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber]->referenceBit=false;
                    if(reset==frameTableSize) frameTable[ptr]->tau=instructionIndex;
                    if(frameTable[ptr]->tau<small1){
                        oldF=ptr;
                        small1=frameTable[ptr]->tau;
                    }
                    ptr=(ptr+1)%frameTableSize;
                }
                temp = oldF;
            }
            else {
                for (int i = 0;i<frameTableSize; i++) { //reset the referenced frames
                    pte_t* pte = createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber];
                    if (pte->referenceBit) {
                        frameTable[ptr]->tau = instructionIndex;
                        pte->referenceBit=false;
                    }
                    else if (!pte->referenceBit && frameTable[ptr]->tau<small2) {
                        old2=ptr;
                        small2 = frameTable[ptr]->tau;
                    }
                    ptr=(ptr+1)%frameTableSize;
                }
                temp=old2;
            }
        }
        else{
            for (int i = 0; i<max; i++) {
                pte_t* pte = createdProcesses.at(frameTable[ptr]->pid).pageTable[frameTable[ptr]->pageNumber];
                if (reset<= frameTableSize && pte->referenceBit) {
                    frameTable[ptr]->tau = instructionIndex;
                    pte->referenceBit = false;
                }
                ptr=(ptr+1)%frameTableSize;
            }
        }
        frameHand=(temp+1)%frameTableSize;
        return frameTable[temp];
    }
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
        cout<<instructionIndex<<": ==> "<<operation<<" "<<page<<"\n";
        switch(operation){
            case 'c': {
                ctx_switches++;
                cost+=130;
                currentProc = createdProcesses.at(page);
                break;
            }
            case 'r': {
                cost++;
                pte_t* pte= currentProc.pageTable[page];
                
                //check for segv
                bool segv=true;
                for(int j=0;j<currentProc.getVMAList().size();j++){
                    if(page>=currentProc.getVMAList().at(j).getBeginPage() && page<=currentProc.getVMAList().at(j).getEndPage()){
                        segv=false;
                        break;
                    }
                }
                if(segv){
                    cout<<" SEGV"<<"\n";
                    currentProc.countStats[SEGV]++;
                    cost+=440;
                    createdProcesses.at(currentProc.getPID())=currentProc;
                    continue;
                }
                
                //check if the vma already present in frame table
                bool flag=false;
                int frameInd;
                for(frameInd=0;frameInd<frameTableSize;frameInd++){
                    if(frameTable[frameInd]->pageNumber==page && frameTable[frameInd]->pid == currentProc.getPID()){
                        //cout<<frameTable[frameInd]->pageNumber<<"found"<<endl;
                        flag=true;
                        break;
                    }
                }
                
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
                        cout<<" UNMAP "<<newframe->pid<<":"<<newframe->pageNumber<<"\n";
                        oldProc.countStats[UNMAP]++;
                        cost+=410;
                        
                        if(o_pte->modifyBit==true){
                            
                            if(o_pte->fileMappedBit){
                                oldProc.countStats[FOUT]++;
                                cost+=2800;
                                cout<<" FOUT"<<"\n";
                            }
                            else{
                                o_pte->pagedOutBit=true;
                                oldProc.countStats[OUT]++;
                                cost+=2750;
                                cout<<" OUT"<<"\n";
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
                        newframe->tau=instructionIndex;
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
                    else if(pte->pagedOutBit){
                        cout<<" IN\n";
                        currentProc.countStats[IN]++;
                        cost+=3200;
                        
                    }
                    else {
                        cout<<" ZERO\n";
                        currentProc.countStats[ZERO]++;
                        cost+=150;
                    }
                        cout<<" MAP "<<newframe->frameNumber<<"\n";
                        currentProc.countStats[MAP]++;
                        cost+=350;
                        createdProcesses[newframe->pid]=currentProc;
                        break;
                }
            
                
            }
            
            case 'w':{
                cost++;
                pte_t* pte= currentProc.pageTable[page];
                
                //check for segv
                bool segv=true;
                for(int j=0;j<currentProc.getVMAList().size();j++){
                    if(page>=currentProc.getVMAList().at(j).getBeginPage() && page<=currentProc.getVMAList().at(j).getEndPage()){
                        segv=false;
                        break;
                    }
                }
                if(segv){
                    cout<<" SEGV"<<"\n";
                    currentProc.countStats[SEGV]++;
                    cost+=440;
                    createdProcesses.at(currentProc.getPID())=currentProc;
                    continue;
                }
                //check if the vma already present in frame table
                bool flag=false;
                int frameInd;
                for(frameInd=0;frameInd<frameTableSize;frameInd++){
                    if(frameTable[frameInd]->pageNumber==page && frameTable[frameInd]->pid == currentProc.getPID()){
                        flag=true;
                        break;
                    }
                }
                //pte_t* pte= currentProc.pageTable[page];
                pte->validBit=true;
                pte->referenceBit=true;
                //pte->modifyBit=true;
                
                if(flag) {
                    pte->frameNo=frameInd;
                    // check if segprot when frame is already present(and read)
                    if(pte->writeProtectBit){
                        //pte->modifyBit=false;
                        cost+=410;
                        currentProc.countStats[SEGP]++;
                        cout<<" SEGPROT"<<"\n";
                    }
                    else pte->modifyBit=true;
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
                        cout<<" UNMAP "<<newframe->pid<<":"<<newframe->pageNumber<<"\n";
                        oldProc.countStats[UNMAP]++;
                        cost+=410;
                        
                        if(o_pte->modifyBit==true){
                            
                            if(o_pte->fileMappedBit){
                                oldProc.countStats[FOUT]++;
                                cost+=2800;
                                cout<<" FOUT"<<"\n";
                            }
                            else{
                                o_pte->pagedOutBit=true;
                                oldProc.countStats[OUT]++;
                                cost+=2750;
                                cout<<" OUT"<<"\n";
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
                        newframe->tau=instructionIndex;
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
                        else if(pte->pagedOutBit){
                            cout<<" IN\n";
                            currentProc.countStats[IN]++;
                            cost+=3200;
                        }
                        else {
                            cout<<" ZERO\n";
                            currentProc.countStats[ZERO]++;
                            cost+=150;
                        }
                    
                        cout<<" MAP "<<newframe->frameNumber<<"\n";
                        currentProc.countStats[MAP]++;
                        cost+=350;
                        if(pte->writeProtectBit){
                            //pte->modifyBit=false;
                            cost+=410;
                            currentProc.countStats[SEGP]++;
                            cout<<" SEGPROT"<<"\n";
                        }
                        else pte->modifyBit=true;
                        currentProc.pageTable[page] = pte;
                        createdProcesses[newframe->pid]=currentProc;
                
                        break;
                }
                    
            }
            
            case 'e':{
                cost+=1230;
                process_exits++;
                cout<<"EXIT current process "<<currentProc.getPID()<<"\n";

                for (int j = 0; j<currentProc.getVMAList().size();j++){
                    for (int k = currentProc.getVMAList().at(j).getBeginPage(); k<=currentProc.getVMAList().at(j).getEndPage(); k++){
                        
                        if (currentProc.pageTable[k]->validBit) {// unmap thing
                            currentProc.pageTable[k]->validBit=false;
                            currentProc.countStats[UNMAP]++;
                            cost+= 410;
                            cout<<" UNMAP "<<currentProc.getPID()<< ":"<<k<<"\n";
                            if (currentProc.pageTable[k]->modifyBit && currentProc.pageTable[k]->fileMappedBit==true) {
                                currentProc.countStats[FOUT]++;
                                cost+=2800;
                                cout << " FOUT"<<"\n";
                            }
                            frameTable[currentProc.pageTable[k]->frameNo]->isVacant=true;
                            freeList.push_back(frameTable[currentProc.pageTable[k]->frameNo]);
                        }
                        currentProc.pageTable[k]->pagedOutBit=false;
                        currentProc.pageTable[k]->referenceBit=false;
                        currentProc.pageTable[k]->modifyBit=false;
                        currentProc.pageTable[k]->fileMappedBit=false;
                        currentProc.pageTable[k]->validBit=false;
                        currentProc.pageTable[k]->frameNo=0;
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
        cout<<"\n";
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
    cout<<"\n";
}

void printCountStats(){
    for(int i=0;i<createdProcesses.size();i++){
        Process proc = createdProcesses.at(i);
        //for(int i=0;i<9;i++) cout<<i<<" "<<proc.countStats[i];
        printf("PROC[%d]: U=%d M=%d I=%d O=%d FI=%d FO=%d Z=%d SV=%d SP=%d\n",
               proc.getPID(), proc.countStats[UNMAP], proc.countStats[MAP], proc.countStats[IN], proc.countStats[OUT], proc.countStats[FIN], proc.countStats[FOUT], proc.countStats[ZERO], proc.countStats[SEGV], proc.countStats[SEGP]);
    }
    
}

void printCostStats(){
    
    cost =
    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
           instructions.size(), ctx_switches, process_exits, cost, sizeof(pte_t));
}

char* pagingAlgo=NULL;
char *options=NULL;
//char *optarg;

int main(int argc, char ** argv) {
    // insert code here...
    //char* rfile= "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/rfile";
   
    //cout<<"rfile created"<<endl;
    //char* inputFile = "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/in11";
    //frameTableSize=32;
    
    
    //cout<<"size of my pte_t is "<<sizeof(pte_t)<<endl;
    frameTableSize=0;
    
    
    //cout<<"beginning"<<endl;
    int c;
    while ((c = getopt(argc, argv, "f::a::o::")) != -1) {
        switch (c)
        {
        case 'f':
            frameTableSize = stoi(optarg);
            //cout<<frameTableSize<<endl;
            break;
        case 'a':
            {
                switch (optarg[0])
                {
                case 'f':
                    THE_PAGER=new FIFO();
                    break;
                case 'r':
                        THE_PAGER=new RANDOM();
                    break;
                case 'c':
                    THE_PAGER=new CLOCK();
                    break;
                case 'e':
                        THE_PAGER=new NRU();
                    break;
                case 'a':
                        THE_PAGER=new AGING();
                    break;
                case 'w':
                        THE_PAGER=new WSET();
                    break;
                default:
                    break;
                }
                break;}
        case 'o':
            options = optarg;
            break;
        case '?':
            if (optopt == 'f' || optopt == 'a' || optopt == 'o')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
            std::exit(1);
        default:
            abort();
        }
    }
    //frameTableSize=32;
    //options[0]='P';
    frameTable = new frame_t*[frameTableSize];
    
    
    for(int i=0; i<strlen(options); i++) {
        //cout<<"options : "<<options[i];
        if (options[i]=='O'||options[i]=='o') oO = true;
        else if (options[i]=='P' || options[i]=='p') oP = true;
        else if (options[i]=='F'|| options[i]=='f') oF = true;
        else if (options[i]=='S'|| options[i]=='s') oS = true;
    }
    
    //cout<<"optind="<<optind<<"  "<<argv[optind]<<endl;
    string rfile = argv[optind + 1];
    createRandomArray(rfile);
    //cout<<"rfile size : "<<randomValues.size();
    string inputFile=argv[optind];
    initialize(inputFile, frameTableSize);
    Simulation();
    if(oP) printPageTable();
    if(oF) printFrameTable();
    if(oS){
        printCountStats();
        printCostStats();
    }
    return 0;
    
}

