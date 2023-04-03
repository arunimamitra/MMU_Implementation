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

using namespace std;

std::vector<int> randomValues;
int ofs=1;
ifstream file;
const int PTESize=64;

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
int myrandom(int burst) { return  randomValues.at(ofs++%(randomValues.size()-1)) ; }

class vma{
    unsigned int beginVPage, endVPage, writeProtected, fileMapped;
public:
    void setValues(int b, int e, int w, int f){
        beginVPage=b;
        endVPage=e;
        writeProtected=w;
        fileMapped=f;
    }
    int getBeginPage(){ return beginVPage; }
    int getEndPage(){ return beginVPage; }
    int isWriteProtected(){ return writeProtected; }
    int isFileMapped(){ return fileMapped; }
    
    void setBeginVPage(int b){beginVPage=b;}
    void setEndVPage(int e){endVPage=e;}
    void setWriteProtected(int w){ writeProtected=w; }
    void setFileMapped(int f){ fileMapped=f;}
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

class Process{
    unsigned int pid;
    unsigned int vmaCount;
    vector<vma*> vmaList;
    unsigned int* pageTable;
    
public:
    Process(int a, int b){
        pid=a;
        vmaCount=b;
        pageTable = new unsigned int[PTESize];
    }
    
    void addToVMAList( vma * v) { vmaList.push_back(v);}
    vector<vma*> getVMAList(){ return vmaList; }
    unsigned int getPID(){ return pid; }
    unsigned int* getPageTable(){ return pageTable; }
};

std::vector<Process*> createdProcesses;
std::vector<InstructionPair> instructions;
int numberOfProcesses=0;

void initialize(char* fileName){
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
        
        if(numberOfProcesses==createdProcesses.size()) {
            // instruction line
            char c;
            unsigned int i;
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
            Process* p =new Process(createdProcesses.size(), numberOfVMA);
            createdProcesses.push_back(p);
        }
        else{   // vma line is input line
            vma* X;
            Process* p = createdProcesses.back();
            int beg,end,wp,fm;
            str >> beg >> end >> wp >>fm;
            X->setValues(beg, end, wp, fm);
            p->addToVMAList(X);
            numberOfVMA--;
        }
    }
    
    for(int i=0;i<instructions.size();i++){
        cout<<instructions.at(i).getType()+"\t"+instructions.at(i).getInd()<<endl;
    }
}




int main(int argc, const char * argv[]) {
    // insert code here...
    char* rfile= "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/rfile";
    createRandomArray(rfile);
    cout<<"rfile created"<<endl;
    char* inputFile = "/Users/asmitamitra/Desktop/Spring2023/OS/Lab3/lab3_assign/in1";
    initialize(inputFile);
    return 0;
}
