//
// Created by 李兴 on 16/7/29.
//

#include <iostream>
#include <string>

#include "UnixDomainConnEpoller.h"

using namespace std;
using namespace francis_module;

int main(int argc,char* argv[]){
	int ret=CUnixDomainConnHandler::getInstance()->init(200,4096,"/tmp/test_unix_domain.sock");
	if(ret<0){
		cerr<<"Init Poller Failed!"<<endl;
		return 1;
	}

	while(1){
		CUnixDomainConnHandler::getInstance()->epollOnce();
	}

	return 0;
}

