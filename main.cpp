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

using namespace std;

std::vector<int> randomValues;
int ofs=1;
ifstream file;

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
    int beginVPage, endVPage, writeProtected, fileMapped;
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

class Process{
    int pid;
    int vmaCount;
    int vma[][4];
public:
    Process(int a, int b){
        pid=a;
        vmaCount=b;
        
    }
};


void initialize(char* fileName){
    file.open(fileName);
    string lineNow;
     
     if (file.is_open()==false) {
         cout << "The gates of hell will open if I continue to parse with a non-existent file <%s>. Hence I decide to exit this program at this point avoiding insufferable pain\n" << fileName;
         exit(1);
     }
    
}
int main(int argc, const char * argv[]) {
    // insert code here...
    char* rfile= "/Users/asmitamitra/Desktop/Spring2023/OS/Lab2/lab2_assign/rfile";
    createRandomArray(rfile);
    return 0;
}
