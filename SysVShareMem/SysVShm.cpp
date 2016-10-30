//
// Created by 李兴 on 16/7/28.
//

#include "SysVShm.h"

#include <iostream>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;
using namespace francis_module;

CSysVShmException::CSysVShmException(int code,const string& reason):m_code(code),m_reason(reason){

}

/*
 * CSysVShm
 */

CSysVShm::CSysVShm():m_shmID(-1),m_mem(NULL),m_shmKey(-1),m_shmSize(0) {

}

CSysVShm::CSysVShm(const char *ipcPath, int magic,size_t size) throw(CSysVShmException){
	this->init(ipcPath,magic,size);
}

CSysVShm::~CSysVShm() {
	try{

		this->detach();
	}catch(...){

	}
}

int CSysVShm::createShm(key_t shmKey,size_t size) throw(CSysVShmException){
	char msgBuffer[512]={0};
	int shmID=shmget(shmKey,size,DEFAULT_SHM_OPEN_PERM|IPC_CREAT|IPC_EXCL);
	if(shmID<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmget failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		if(errno==EEXIST){
			throw CSysVShmException(CSysVShmException::CODE_SHM_EXISTS,msgBuffer);
		}
		throw CSysVShmException(CSysVShmException::CODE_CREATE_SHM_FAIL,msgBuffer);
	}


	m_shmID=shmID;
	this->attach();


	m_shmSize=size;
	m_shmKey=shmKey;
	return 0;
}

int CSysVShm::openShm(key_t shmKey) throw(CSysVShmException){
	char msgBuffer[512]={0};
	int shmID=shmget(shmKey,0,SHM_R|SHM_W);
	if(shmID<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmget failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_OPEN_SHM_FAIL,msgBuffer);
	}

	struct shmid_ds shmInfo;
	if(shmctl(shmID,IPC_STAT,&shmInfo)<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmctl(get_size) failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_SHMCTL_GETINFO_FAIL,msgBuffer);
	}

	m_shmID=shmID;
	this->attach();

	m_shmSize=shmInfo.shm_segsz;
	m_shmKey=shmKey;
	return 0;
}

int CSysVShm::attach() throw(CSysVShmException){
	char msgBuffer[512]={0};
	if(m_mem!=NULL){
		detach();
	}

	m_mem=shmat(m_shmID,NULL,0);
	if(m_mem==(void*)-1){
		m_shmID=-1;
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmat failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_ATTACH_SHM_FAIL,msgBuffer);
	}
	return 0;
}

int CSysVShm::detach() throw(CSysVShmException){
	char msgBuffer[512]={0};
	if(m_mem==NULL){
		return -1;
	}

	if(shmdt(m_mem)<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmdt failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_DETACH_SHM_FAIL,msgBuffer);
	}

	return 0;
}

int CSysVShm::remove() throw(CSysVShmException){
	char msgBuffer[512]={0};
	if(m_shmID<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shm not inited yet!",__FUNCTION__);
		throw CSysVShmException(CSysVShmException::CODE_NOT_INITED,msgBuffer);
	}

	if(shmctl(m_shmID,IPC_RMID,NULL)<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmctl(IPC_RMID) failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_CTL_RMID_FAIL,msgBuffer);
	}
	return 0;
}

int CSysVShm::init(const char *ipcPath, int magic, size_t size) throw(CSysVShmException){
	int isCreate=0;
	char msgBuffer[512]={0};
	key_t shmKey=ftok(ipcPath,magic);
	if(shmKey<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:ftok failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_FTOK_FAIL,msgBuffer);
	}

	try{
		this->createShm(shmKey,size);
		isCreate=1;
	}catch(CSysVShmException& e){
		if(e.errCode()!=CSysVShmException::CODE_SHM_EXISTS){
			throw e;
		}
		this->openShm(shmKey);
	}

	return isCreate;
}

int CSysVShm::onlyOpen(const char *ipcPath, int magic) throw(CSysVShmException){
	char msgBuffer[512]={0};
	key_t shmKey=ftok(ipcPath,magic);
	if(shmKey<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:ftok failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_FTOK_FAIL,msgBuffer);
	}

	this->openShm(shmKey);
	return 0;
}

int CSysVShm::deleteShm(const char *ipcPath, int magic) throw(CSysVShmException){
	try{
		this->onlyOpen(ipcPath,magic);
	}catch(CSysVShmException& e){
		throw e;
	}

	this->remove();
	return 0;
}

int CSysVShm::setMemLock() {
	assert(m_shmID>0);
	char msgBuffer[512]={0};

	struct rlimit limit;
	limit.rlim_cur=RLIM_INFINITY;
	limit.rlim_max=limit.rlim_cur;
	if(setrlimit(RLIMIT_MEMLOCK,&limit)<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:setrlimit(memlock) failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_CTL_MEMLOCK_FAIL,msgBuffer);
	}

	if(shmctl(m_shmID,SHM_LOCK,0)<0){
		snprintf(msgBuffer,sizeof(msgBuffer)-1,"%s:shmctl(memlock) failed:%s(errno=%d)",__FUNCTION__,strerror(errno),errno);
		throw CSysVShmException(CSysVShmException::CODE_CTL_MEMLOCK_FAIL,msgBuffer);
	}

	return 0;
}
