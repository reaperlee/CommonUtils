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


CShmQueue::CShmQueue():m_cycleQueue(NULL),m_shm(),m_readLock(),m_writeLock(),m_enableReadLock(false),m_enableWriteLock(false),m_errno(ERRNO_SUCCESS)
{

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
		m_errno=INIT_ERROR_SHM_FAILED;
		return -1;
	}

	if(enableReadLock){
		try{
			m_readLock.init(ipcPath,SEM_READ_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Init Read Sem Failed:%s\n",e.why().c_str());
#endif
			m_errno=INIT_ERROR_RLOCK_SEM_FAILED;
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
			m_errno=INIT_ERROR_WLOCK_SEM_FAILED;
			return -3;
		}
	}

	memset(m_shm.getShm(),0,m_shm.getShmSize());
	m_cycleQueue=cycle_queue_init((unsigned char*)m_shm.getShm(),m_shm.getShmSize()-sizeof(struct cycle_queue));
	if(m_cycleQueue==0){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Attach Cycle Queue Failed!\n");
#endif
		m_errno=INIT_ERROR_ATTACH_SHM_QUEUE;
		return -3;
	}

	m_errno=ERRNO_SUCCESS;
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
		m_errno=OPEN_ERROR_SHM_FAILED;
		return -1;
	}

	if(m_enableReadLock){
		try{
			m_readLock.open(ipcPath,SEM_READ_LOCK_MAGIC);
		}catch(CSysVSemException& e){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Open ReadLock Sem Failed:%s\n",e.why().c_str());
#endif
			m_errno=OPEN_ERROR_RLOCK_SEM_FAILED;
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
			m_errno=OPEN_ERROR_WLOCK_SEM_FAILED;
			return -2;

		}
	}

	m_cycleQueue=cycle_queue_attach((unsigned char*)m_shm.getShm(),m_shm.getShmSize()-sizeof(struct cycle_queue));
	if(m_cycleQueue==0){
#ifdef _MODULE_DEBUG
		fprintf(stderr,"Attach Cycle Queue Failed!\n");
#endif
		m_errno=OPEN_ERROR_ATTACH_SHM_QUEUE;
		return -3;
	}

	m_errno=ERRNO_SUCCESS;
	return 0;
}

int CShmQueue::enqueue(const char *data, uint32_t len) {
	if(m_enableWriteLock){
		if(m_writeLock.semP(false)<0){
			m_errno=ENQUEUE_ERROR_TRY_LOCK;
			return ENQUEUE_ERROR_TRY_LOCK;
		}
	}

	int ret=cycle_enqueue(m_cycleQueue,data,len);
	if(m_enableWriteLock){
		m_writeLock.semV();
	}
	if(ret>=0){
		m_errno=ERRNO_SUCCESS;
		return ERRNO_SUCCESS;
	}
	else if(ret==ERR_Q_FULL){
		m_errno=ENQUEUE_ERROR_FULL;
		return ENQUEUE_ERROR_FULL;
	}
	else{
	}
	m_errno=ENQUEUE_ERROR_UNKNOWN;
	return ENQUEUE_ERROR_UNKNOWN;
}

int CShmQueue::dequeue(char *dataBuffer, uint32_t &bufferLen) {
	if(m_enableReadLock){
		if(m_readLock.semP()<0){
			m_errno=DEQUEUE_ERROR_TRY_LOCK;
			return DEQUEUE_ERROR_TRY_LOCK;
		}
	}

	int ret=cycle_dequeue(m_cycleQueue,dataBuffer,&bufferLen);
	if(m_enableReadLock){
		m_readLock.semV();
	}

	if(ret==0){
		m_errno=ERRNO_SUCCESS;
		return ERRNO_SUCCESS;
	}
	else if(ret==ERR_Q_EMPTY){
		bufferLen=0;
		m_errno=DEQUEUE_ERROR_EMPTY;
		return DEQUEUE_ERROR_EMPTY;
	}
	else if(ret==ERR_BUF_INSUFF){
		m_errno=DEQUEUE_ERROR_INSUFBUF;
		return DEQUEUE_ERROR_INSUFBUF;
	}
	else{
		bufferLen=0;
	}
	m_errno=DEQUEUE_ERROR_UNKNOWN;
	return DEQUEUE_ERROR_UNKNOWN;
}

void CShmQueue::pop() {
	cycle_queue_pop(m_cycleQueue);
}


