//
// Created by 李兴 on 16/7/28.
//
#include "ShmQueue.h"

#include <iostream>
#include <string>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

using namespace std;
using namespace francis_module;
int main(int argc,char* argv[]){
	if(argc!=2){
		cerr<<"Usage:"<<argv[0]<<" enqueue-content"<<endl;
		return 1;
	}
	char buffer[1024]={0};
	strncpy(buffer,argv[1],sizeof(buffer)-1);
	size_t dataLen=strlen(argv[1]);

	CShmQueue shmQueue;

	char cwd[256]={0};

	if(getcwd(cwd,sizeof(cwd)-1)<0){
		cerr<<"getcwd failed:"<<strerror(errno)<<endl;
		return 1;
	}

	int ret=shmQueue.open(cwd,false,true);
	if(ret<0){
		return 1;
	}

	time_t nextOutputTS=time(NULL);
	while(nextOutputTS%60){
		nextOutputTS++;
	}
	unsigned int enqueueCnt=0;
	unsigned int enqueueSuccessCnt=0;
	unsigned int enqueueFailFullCnt=0;
	unsigned int enqueueFailLockCnt=0;
	unsigned int enqueueFailUnknowCnt=0;

	while(1){
		time_t currentTS=time(NULL);
		for(int i=0;i<5;i++){
			ret=shmQueue.enqueue(buffer,dataLen);
			enqueueCnt++;
			if(ret==CShmQueue::ENQUEUE_ERROR_FULL){
				enqueueFailFullCnt++;
			}
			else if(ret==CShmQueue::ENQUEUE_ERROR_TRY_LOCK){
				enqueueFailLockCnt++;
			}
			else if(ret==CShmQueue::ENQUEUE_ERROR_UNKNOWN){
				enqueueFailUnknowCnt++;
			}
			else{
				enqueueSuccessCnt++;
			}

		}

		if(currentTS>=nextOutputTS){
			nextOutputTS+=60;
			cerr<<"[STATISTICS]EnqueueTotal="<<enqueueCnt<<" EnqueueSuccess="<<enqueueSuccessCnt<<" EnqueueFailFull="<<enqueueFailFullCnt
			<<" EnqueueFailLockCnt="<<enqueueFailLockCnt<<" EnqueueFailUnknownCnt="<<enqueueFailUnknowCnt<<endl;
			enqueueCnt=0;
			enqueueSuccessCnt=0;
			enqueueFailFullCnt=0;
			enqueueFailLockCnt=0;
			enqueueFailUnknowCnt=0;
		}
		usleep(1);
	}

	return 0;

}
