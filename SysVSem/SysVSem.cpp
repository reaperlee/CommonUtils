//
// Created by 李兴 on 16/7/28.
//
#include "SysVSem.h"

#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

using namespace francis_module;

/*
 * CSysVSemExcetpion
 */

CSysVSemException::CSysVSemException(int code,const std::string& msg) {
	/*
	switch(code){
		case
	}
	 */
	m_code=code;
	m_reason=msg;
}
/*
 * CSysVSem
 */
CSysVSem::CSysVSem():m_semID(-1),m_maxTrialTimes(1) {

}

CSysVSem::CSysVSem(const char *ipcPath, int magic) {

}

CSysVSem::~CSysVSem() {

}

int CSysVSem::createSem(key_t semKey) throw(CSysVSemException){
	char msgBuffer[1024]={0};

	int semID=semget(semKey,1,DEFAULT_IPC_PERM|IPC_CREAT|IPC_EXCL);
	if(semID<0){
		if(errno==EEXIST){
			throw CSysVSemException(CSysVSemException::CODE_SEM_EXISTS,"Semphore already exists.");
		}
		else{
			snprintf(msgBuffer,sizeof(msgBuffer)-1,"Create semphore failed:%s(errno=%d)",strerror(errno),errno);
			throw CSysVSemException(CSysVSemException::CODE_SEM_CREATE_FAIL,msgBuffer);
		}
	}

	union semun{
		int val;
		struct semid_ds *buf;
		unsigned short *array;
		struct seminfo *__buf;
	};

	union semun arg;
	struct semid_ds seminfo;
	arg.buf=&seminfo;
	int ret=semctl(semID,0,IPC_STAT,arg);
	if(ret<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"Get semphore information failed:%s(errno=%d)",strerror(errno),errno);
		throw CSysVSemException(CSysVSemException::CODE_SEM_GET_INFO_FAIL,msgBuffer);
	}

	if(seminfo.sem_nsems==0){
		throw CSysVSemException(CSysVSemException::CODE_SEM_ILLEGAL_NSEMS,"Illegal semphore count.");
	}

	unsigned short* setValArray=(unsigned short*)malloc(sizeof(unsigned short)*seminfo.sem_nsems);
	if(setValArray==NULL){
		throw CSysVSemException(CSysVSemException::CODE_SEM_SET_INITVAL_FAIL_OFM,"Set semphore initial value failed:Out Of Memory!");
	}

	setValArray[0]=1;
	arg.array=setValArray;
	ret=semctl(semID,0,SETALL,arg);
	if(ret<0){
		free(setValArray);
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"Set semphore init value failed:%s(errno=%d)",strerror(errno),errno);
		throw CSysVSemException(CSysVSemException::CODE_SEM_SET_INITVAL_FAIL,msgBuffer);
	}

	free(setValArray);
	return semID;
}

int CSysVSem::openSem(key_t semKey) throw(CSysVSemException){
	char msgBuffer[1024]={0};
	int semID=semget(semKey,0,O_RDWR);
	if(semID<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"Open semphore failed:%s(errno=%d)",strerror(errno),errno);
		throw CSysVSemException(CSysVSemException::CODE_SEM_OPEN_FAIL,msgBuffer);
	}

	return semID;
}

int CSysVSem::init(const char *ipcPath, int magic) throw(CSysVSemException){
	char msgBuffer[1024]={0};
	if(m_semID>0){
		throw CSysVSemException(CSysVSemException::CODE_INITED,"Already inited!");
	}

	key_t semKey=ftok(ipcPath,magic);
	if(semKey<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"Init ipc key failed:%s(errno=%d)",strerror(errno),errno);
		throw CSysVSemException(CSysVSemException::CODE_FTOK_FAILED,msgBuffer);
	}

	try{
		m_semID=createSem(semKey);
	}catch(CSysVSemException& e){
		if(e.errCode()!=CSysVSemException::CODE_SEM_EXISTS){
			throw e;
		}
		try{
			m_semID=openSem(semKey);
		}catch(CSysVSemException& e){
			m_semID=-1;
			throw e;
		}
	}

	return 0;

}

int CSysVSem::open(const char *ipcPath, int magic) throw(CSysVSemException){
	char msgBuffer[1024]={0};
	if(m_semID>0){
		throw CSysVSemException(CSysVSemException::CODE_INITED,"Already inited!");
	}

	key_t semKey=ftok(ipcPath,magic);
	if(semKey<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"Init ipc key failed:%s(errno=%d)",strerror(errno),errno);
		throw CSysVSemException(CSysVSemException::CODE_FTOK_FAILED,msgBuffer);
	}

	try{

		m_semID=openSem(semKey);
	}catch(CSysVSemException& e){
		m_semID=-1;
		throw e;
	}
	return 0;
}

void CSysVSem::setMaxTrial(unsigned int maxTrial) {
	this->m_maxTrialTimes=maxTrial;
}

int CSysVSem::semP(bool noWait) {
	if(m_semID<0){
		return -3;
	}
	struct sembuf opBuf;
	memset(&opBuf,0,sizeof(opBuf));

	opBuf.sem_num=0;
	opBuf.sem_op=-1;

	if(noWait){
		opBuf.sem_flg|=IPC_NOWAIT;
	}
	opBuf.sem_flg|=SEM_UNDO;

	unsigned int i=0;
	for(i=0;i<m_maxTrialTimes;i++){
		if(semop(m_semID,&opBuf,1)<0){
			if(errno==EAGAIN){
				continue;
			}
			return -1;
		}
		break;
	}

	if(i>=m_maxTrialTimes){
		return -2;
	}

	return 0;
}

int CSysVSem::semV(bool noWait) {
	if(m_semID<0){
		return -3;
	}
	struct sembuf opBuf;
	memset(&opBuf,0,sizeof(opBuf));

	opBuf.sem_num=0;
	opBuf.sem_op=1;
	if(noWait){
		opBuf.sem_flg|=IPC_NOWAIT;
	}
	opBuf.sem_flg|=SEM_UNDO;

	return semop(m_semID,&opBuf,1);
}


