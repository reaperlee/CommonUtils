//
// Created by 李兴 on 16/7/28.
//

#include "SysVShm.h"

#include <iostream>
#include <string>

#include <unistd.h>
#include <string.h>
#include <errno.h>

using namespace std;
using namespace francis_module;
int main(int argc,char* argv[]){
	char cwd[256]={0};

	if(getcwd(cwd,sizeof(cwd)-1)<0){
		cerr<<"Getcwd Failed:"<<strerror(errno)<<endl;
		return 1;
	}
	CSysVShm sysVShm;


	try{
		sysVShm.init(cwd,1,1234);
	}catch(CSysVShmException& e){
		cerr<<"Init Failed:"<<e.why()<<endl;
	}

	cerr<<"SHMID="<<sysVShm.getShmID()<<" SHMKEY="<<sysVShm.getShmKey()<<" SHMSIZE="<<sysVShm.getShmSize()<<endl;
	return 0;

}

