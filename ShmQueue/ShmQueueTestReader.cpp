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
	CShmQueue shmQueue;

	char cwd[256]={0};

	if(getcwd(cwd,sizeof(cwd)-1)<0){
		cerr<<"getcwd failed:"<<strerror(errno)<<endl;
		return 1;
	}

	int ret=shmQueue.init(cwd,1024*1024*128,false,true);
	if(ret<0){
		return 1;
	}

	char buffer[4096]={0};
	size_t bufferLen=sizeof(buffer);
	unsigned int dequeueTotalCnt=0;
	unsigned int dequeueEmptyCnt=0;
	unsigned int dequeueSuccessCnt=0;
	unsigned int dequeueFailedCnt=0;
	time_t nextOutputTS=time(NULL);
	while(nextOutputTS%60){
		nextOutputTS++;
	}
	while(1){
		time_t currentTS=time(NULL);
		uint32_t dataLen=(uint32_t)bufferLen;
		for(int i=0;i<1000;i++){
			ret=shmQueue.dequeue(buffer,dataLen);
			dequeueTotalCnt++;
			if(ret==CShmQueue::DEQUEUE_ERROR_EMPTY){
				dequeueEmptyCnt++;
				break;
			}
			else if(ret==0){
				if(dataLen>0){
					dequeueSuccessCnt++;
				}
			}
			else{
				cerr<<"ret="<<ret<<endl;
				dequeueFailedCnt++;
			}
		}
		if(currentTS>=nextOutputTS){
			nextOutputTS+=60;
			cerr<<"[STATISTIC]TotalCnt="<<dequeueTotalCnt<<" DequeueSuccess="<<dequeueSuccessCnt<<" DequeueFail="<<dequeueFailedCnt<<" DequeueEmpty="<<dequeueEmptyCnt<<endl;
			dequeueTotalCnt=0;
			dequeueEmptyCnt=0;
			dequeueSuccessCnt=0;
			dequeueFailedCnt=0;
		}

		usleep(10);
	}

}
