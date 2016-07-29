//
// Created by 李兴 on 16/7/28.
//
#include <iostream>
#include <string>

#include <string.h>
#include <stdlib.h>

#include "SysVSem.h"

using namespace std;
using namespace francis_module;
int main(int argc,char* argv[]){
	CSysVSem sem;

	try{
		sem.init("/data/francisxli/CommonUtils/",1);
	}catch(CSysVSemException& e){
		cerr<<e.why()<<endl;
		return 1;
	}

	int ret=sem.semP();
	if(ret<0){
		cerr<<"SemP Failed:"<<ret<<endl;
	}

	sleep(20);

	ret=sem.semV();
	if(ret<0){
		cerr<<"SemV Failed:"<<ret<<endl;
	}

	return 0;

}

