//
// Created by 李兴 on 16/7/29.
//
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <string.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>


using namespace std;

const char* UNIX_DOMAIN_PATH="/tmp/test_unix_domain.sock";

static int sendViaUnixSocket(const char* buffer,uint32_t bufferLen){
	struct sockaddr_un agentAddr;
	agentAddr.sun_family=AF_UNIX;
	strncpy(agentAddr.sun_path,UNIX_DOMAIN_PATH,sizeof(agentAddr.sun_path));

	int unixSockFd=socket(AF_UNIX,SOCK_STREAM,0);
	if(unixSockFd<0){
		fprintf(stderr,"socket failed:%d(%s)\n",errno,strerror(errno));
		return -1;
	}

	if(connect(unixSockFd,(struct sockaddr*)&agentAddr,sizeof(struct sockaddr_un))<0){
		fprintf(stderr,"connect failed:%d(%s)\n",errno,strerror(errno));
		close(unixSockFd);
		return -2;
	}

	if(fcntl(unixSockFd,F_SETFL,O_NONBLOCK)<0){
		fprintf(stderr,"fcntl failed:%d(%s)\n",errno,strerror(errno));
		close(unixSockFd);
		return -3;
	}

	ssize_t sentRet=write(unixSockFd,buffer,bufferLen);
	if(sentRet==bufferLen){
		fprintf(stderr,"write failed:%d(%s)\n",errno,strerror(errno));
		close(unixSockFd);
		return 0;
	}
	else if(sentRet<bufferLen){
		close(unixSockFd);
		fprintf(stderr,"send not enough!\n");
		return -4;
	}
	else{
		close(unixSockFd);
		fprintf(stderr,"write failed:%d(%s)\n",errno,strerror(errno));
		return -5;
	}

	return 0;
}
int main(int argc,char* argv[]){
	if(argc!=2){
		cerr<<"Usage:"<<argv[0]<<" string-to-send"<<endl;
	}

	sendViaUnixSocket(argv[1],strlen(argv[1]));
	return 0;
}

