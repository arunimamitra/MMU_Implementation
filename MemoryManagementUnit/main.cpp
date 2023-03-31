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

class Process{
    int pid;
    int vmaCount;
    int vma[][4];
public:
    Process(int a, int b, int c[][4]){
        pid=a;
        vmaCount=b;
        for(int i=0;i<vmaCount;i++){
            for(int j=0;j<4;j++){
                vma[i][j] = c[i][j];
            }
        }
    }
};

int main(int argc, const char * argv[]) {
    // insert code here...
    char* rfile= "/Users/asmitamitra/Desktop/Spring2023/OS/Lab2/lab2_assign/rfile";
    createRandomArray(rfile);
    return 0;
}
