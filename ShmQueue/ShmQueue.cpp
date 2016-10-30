//
// Created by 李兴 on 16/7/28.
//
#include "ShmQueue.h"

#include <iostream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

using namespace std;
using namespace francis_module;

#define _MODULE_DEBUG

#ifdef _MODULE_DEBUG
#define DEBUG_OUTPUT(format,args...) fprintf(stderr,"[%s][%s@%d]"format"\n",__FILE__,__FUNCTION__,__LINE__,##args)
#else
#define DEBUG_OUTPUT(format,args...)
#endif

CShmQueue::CShmQueue():m_shmQueueEntry(NULL),m_shm(),m_enableReadLock(false),m_enableWriteLock(false),m_errno(ERRNO_SUCCESS),m_isInited(false)
{

}

CShmQueue::~CShmQueue() {
	if(m_shmQueueEntry){
		free(m_shmQueueEntry);
	}
}

int CShmQueue::initRobustMutex(pthread_mutex_t *mutex) {
	int ret;
	pthread_mutexattr_t mutexattr;

	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_setpshared(&mutexattr,PTHREAD_PROCESS_SHARED);

	ret=pthread_mutexattr_setprotocol(&mutexattr,PTHREAD_PRIO_INHERIT);
	if(ret<0){
		DEBUG_OUTPUT("Set mutex protocol failed(%d).",ret);
		return -1;
	}

	ret=pthread_mutexattr_setrobust_np(&mutexattr,PTHREAD_MUTEX_ROBUST_NP);
	if(ret<0){
		DEBUG_OUTPUT("Set mutex robust_np failed(%d).",ret);
		return -1;
	}

	ret=pthread_mutex_init(mutex,&mutexattr);
	if(ret<0){
		DEBUG_OUTPUT("Init mutex failed(%d).",ret);
		return -1;
	}
	return 0;
}

void CShmQueue::lockRobustMutex(pthread_mutex_t *mutex) {
	int ret=pthread_mutex_lock(mutex);
	if(ret==EOWNERDEAD){
		pthread_mutex_consistent_np(mutex);
	}
}

void CShmQueue::unlockRobustMutex(pthread_mutex_t *mutex) {
	pthread_mutex_unlock(mutex);
}

int CShmQueue::init(const char *ipcPath, size_t queueSize,bool enableReadLock,bool enableWriteLock) {
	assert(!m_isInited);
	m_enableReadLock=enableReadLock;
	m_enableWriteLock=enableWriteLock;
	int shmCreateFlag=0;

	m_shmQueueEntry=(StShmQueueEntry*)malloc(sizeof(StShmQueueEntry));
	if(m_shmQueueEntry==NULL){
		m_errno=INIT_ERROR_OFM;
		return -1;
	}

	size_t shmTotalSize=queueSize+sizeof(pthread_mutex_t)*2;
	//queueSize+=sizeof(pthread_mutex_t)*2;
	try{
		shmCreateFlag=m_shm.init(ipcPath,SHM_MAGIC,shmTotalSize);
	}catch(CSysVShmException& e){
		DEBUG_OUTPUT("Init Shm Failed:%s",e.why().c_str());
		m_errno=INIT_ERROR_SHM_FAILED;
		return -1;
	}

	if(shmCreateFlag){
		memset(m_shm.getShm(),0,m_shm.getShmSize());
	}
	m_shmQueueEntry->shmHeader=(StShmQueueHeader*)m_shm.getShm();

	if(enableReadLock&&shmCreateFlag){
		if(initRobustMutex(&m_shmQueueEntry->shmHeader->robustReadMutex)<0){
			m_errno=INIT_ERROR_RLOCK_MUTEX_FAILED;
			return -1;
		}
	}

	if(enableWriteLock&&shmCreateFlag){
		if(initRobustMutex(&m_shmQueueEntry->shmHeader->robustWriteMutex)<0){
			m_errno=INIT_ERROR_WLOCK_MUTEX_FAILED;
			return -1;
		}
	}

	if(shmCreateFlag){
		m_shmQueueEntry->cycleQueue=cycle_queue_init((unsigned char*)m_shmQueueEntry->shmHeader+sizeof(StShmQueueHeader),queueSize-sizeof(struct cycle_queue));
	}
	else{
		m_shmQueueEntry->cycleQueue=cycle_queue_attach((unsigned char*)m_shmQueueEntry->shmHeader+sizeof(StShmQueueHeader),queueSize-sizeof(struct cycle_queue));
	}
	//m_cycleQueue=cycle_queue_init((unsigned char*)m_shm.getShm()+sizeof(pthread_mutex_t)*2,m_shm.getShmSize()-sizeof(struct cycle_queue)-sizeof(pthread_mutex_t)*2);
	if(m_shmQueueEntry->cycleQueue==0){
		DEBUG_OUTPUT("Attach Cycle Queue Failed!");
		m_errno=INIT_ERROR_ATTACH_SHM_QUEUE;
		return -3;
	}

	m_errno=ERRNO_SUCCESS;
	m_isInited=true;
	return 0;
}

int CShmQueue::open(const char *ipcPath, bool enableReadLock, bool enableWriteLock) {
	assert(!m_isInited);

	m_enableReadLock=enableReadLock;
	m_enableWriteLock=enableWriteLock;

	try{
		m_shm.onlyOpen(ipcPath,SHM_MAGIC);
	}catch(CSysVShmException& e){
		DEBUG_OUTPUT("Open ShareMem Failed:%s",e.why().c_str());
		m_errno=OPEN_ERROR_SHM_FAILED;
		return -1;
	}

	m_shmQueueEntry=(StShmQueueEntry*)malloc(sizeof(StShmQueueEntry));
	if(m_shmQueueEntry==NULL){
		m_errno=INIT_ERROR_OFM;
		return -1;
	}

	m_shmQueueEntry->shmHeader=(StShmQueueHeader*)m_shm.getShm();
	//m_cycleQueue=cycle_queue_attach((unsigned char*)m_shm.getShm(),m_shm.getShmSize()-sizeof(struct cycle_queue));
	m_shmQueueEntry->cycleQueue=cycle_queue_attach((unsigned char*)m_shmQueueEntry->shmHeader+sizeof(StShmQueueHeader),m_shm.getShmSize()-sizeof(StShmQueueHeader)-sizeof(struct cycle_queue));
	if(m_shmQueueEntry->cycleQueue==0){
		DEBUG_OUTPUT("Attach Cycle Queue Failed!");
		m_errno=OPEN_ERROR_ATTACH_SHM_QUEUE;
		return -3;
	}

	m_errno=ERRNO_SUCCESS;
	m_isInited=true;
	return 0;
}

int CShmQueue::enqueue(const char *data, uint32_t len) {
	assert(m_isInited);
	if(m_enableWriteLock){
		lockRobustMutex(&m_shmQueueEntry->shmHeader->robustWriteMutex);
	}

	int ret=cycle_enqueue(m_shmQueueEntry->cycleQueue,data,len);
	if(m_enableWriteLock){
		unlockRobustMutex(&m_shmQueueEntry->shmHeader->robustWriteMutex);
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
	assert(m_isInited);
	if(m_enableReadLock){
		lockRobustMutex(&m_shmQueueEntry->shmHeader->robustReadMutex);
	}

	int ret=cycle_dequeue(m_shmQueueEntry->cycleQueue,dataBuffer,&bufferLen);
	if(m_enableReadLock){
		unlockRobustMutex(&m_shmQueueEntry->shmHeader->robustReadMutex);
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
	assert(m_isInited);
	cycle_queue_pop(m_shmQueueEntry->cycleQueue);
}


