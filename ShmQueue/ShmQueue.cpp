//
// Created by 李兴 on 16/7/28.
//
#include "ShmQueue.h"

#include <iostream>
#include <string>

#include <stdio.h>
#include <string.h>

using namespace std;
using namespace francis_module;

#define _MODULE_DEBUG

CShmQueue::CShmQueue():m_cycleQueue(NULL),m_shm(),m_readLock(),m_writeLock(),m_enableReadLock(false),m_enableWriteLock(false) {

}

CShmQueue::~CShmQueue() {

}

int CShmQueue::init(const char *ipcPath, size_t queueSize,bool enableReadLock,bool enableWriteLock) {
	m_enableReadLock=enableReadLock;
	m_enableWriteLock=enableWriteLock;
	try{
		m_shm.init(ipcPath,SHM_MAGIC,queueSize);
	}catch(CSysVShmException& e){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Init Shm Failed:%s\n",e.why().c_str());
#endif
		return -1;
	}

	if(enableReadLock){
		try{
			m_readLock.init(ipcPath,SEM_READ_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Init Read Sem Failed:%s\n",e.why().c_str());
#endif
			return -2;
		}
	}

	if(enableWriteLock){
		try{
			m_writeLock.init(ipcPath,SEM_WRITE_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Init Write Sem Failed:%s\n",e.why().c_str());
#endif
			return -3;
		}
	}

	memset(m_shm.getShm(),0,m_shm.getShmSize());
	m_cycleQueue=cycle_queue_init((unsigned char*)m_shm.getShm(),m_shm.getShmSize()-sizeof(struct cycle_queue));
	if(m_cycleQueue==0){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Attach Cycle Queue Failed!\n");
#endif
		return -3;
	}

	return 0;
}

int CShmQueue::open(const char *ipcPath, bool enableReadLock, bool enableWriteLock) {
	m_enableReadLock=enableReadLock;
	m_enableWriteLock=enableWriteLock;

	try{
		m_shm.onlyOpen(ipcPath,SHM_MAGIC);
	}catch(CSysVShmException& e){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Open ShareMem Failed:%s\n",e.why().c_str());
#endif
		return -1;
	}

	if(m_enableReadLock){
		try{
			m_readLock.open(ipcPath,SEM_READ_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Open ReadLock Sem Failed:%s\n",e.why().c_str());
#endif
			return -2;

		}
	}

	if(m_enableWriteLock){
		try{
			m_writeLock.open(ipcPath,SEM_WRITE_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Open ReadLock Sem Failed:%s\n",e.why().c_str());
#endif
			return -2;

		}
	}

	m_cycleQueue=cycle_queue_attach((unsigned char*)m_shm.getShm(),m_shm.getShmSize()-sizeof(struct cycle_queue));
	if(m_cycleQueue==0){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Attach Cycle Queue Failed!\n");
#endif
		return -3;
	}

	return 0;
}

int CShmQueue::enqueue(const char *data, size_t len) {
	if(m_enableWriteLock){
		if(m_writeLock.semP(false)<0){
			return ENQUEUE_ERROR_TRY_LOCK;
		}
	}

	int ret=cycle_enqueue(m_cycleQueue,data,len);
	if(m_enableWriteLock){
		m_writeLock.semV();
	}
	if(ret>=0){
		return 0;
	}
	else if(ret==ERR_Q_FULL){
		return ENQUEUE_ERROR_FULL;
	}
	else{
		return ENQUEUE_ERROR_UNKNOWN;
	}
	return ENQUEUE_ERROR_UNKNOWN;
}

int CShmQueue::dequeue(char *dataBuffer, size_t &bufferLen) {
	if(m_enableReadLock){
		if(m_readLock.semP()<0){
			return DEQUEUE_ERROR_TRY_LOCK;
		}
	}

	int len=(int)bufferLen;
	int ret=cycle_dequeue(m_cycleQueue,dataBuffer,&len);
	if(m_enableReadLock){
		m_readLock.semV();
	}

	if(ret>=0){
		return 0;
	}
	else if(ret==ERR_Q_EMPTY){
		bufferLen=0;
		return DEQUEUE_ERROR_EMPTY;
	}
	else if(ret==ERR_BUF_INSUFF){
		bufferLen=(size_t)len;
		return DEQUEUE_ERROR_INSUFBUF;
	}
	else{
		bufferLen=0;
		return DEQUEUE_ERROR_UNKNOWN;
	}
	return DEQUEUE_ERROR_UNKNOWN;
}


