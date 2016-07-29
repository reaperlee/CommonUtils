//
// Created by 李兴 on 16/7/28.
//

#ifndef COMMONUTILS_SHMQUEUE_H
#define COMMONUTILS_SHMQUEUE_H

#include "cycle_queue.h"
#include "SysVSem/SysVSem.h"
#include "SysVShareMem/SysVShm.h"


namespace francis_module{
	class CShmQueue{
	private:
		const static int SHM_MAGIC=0x01;
		const static int SEM_READ_LOCK_MAGIC=0x02;
		const static int SEM_WRITE_LOCK_MAGIC=0x03;
		struct cycle_queue* m_cycleQueue;
		CSysVShm m_shm;
		CSysVSem m_readLock;
		CSysVSem m_writeLock;
		bool m_enableReadLock;
		bool m_enableWriteLock;

	public:
		const static int ENQUEUE_ERROR_TRY_LOCK=-1;
		const static int ENQUEUE_ERROR_FULL=-2;
		const static int ENQUEUE_ERROR_UNKNOWN=-3;

		const static int DEQUEUE_ERROR_TRY_LOCK=-1;
		const static int DEQUEUE_ERROR_EMPTY=-2;
		const static int DEQUEUE_ERROR_INSUFBUF=-3;
		const static int DEQUEUE_ERROR_UNKNOWN=-4;
		CShmQueue();
		~CShmQueue();

		int init(const char* ipcPath,size_t queueSize,bool enableReadLock,bool enableWriteLock);
		int open(const char* ipcPath,bool enableReadLock,bool enableWriteLock);
		int enqueue(const char* data,size_t len);
		int dequeue(char* dataBuffer,size_t& bufferLen);
	};
}
#endif //COMMONUTILS_SHMQUEUE_H
