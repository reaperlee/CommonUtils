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
		struct cycle_queue* m_cycleQueue;
		CSysVShm m_shm;
		CSysVSem m_readLock;
		CSysVSem m_writeLock;
		bool m_enableReadLock;
		bool m_enableWriteLock;
		int m_errno;

	public:
		const static int ERRNO_SUCCESS=0;
		const static int ENQUEUE_ERROR_TRY_LOCK=-100;
		const static int ENQUEUE_ERROR_FULL=-101;
		const static int ENQUEUE_ERROR_UNKNOWN=-102;

		const static int DEQUEUE_ERROR_TRY_LOCK=-103;
		const static int DEQUEUE_ERROR_EMPTY=-104;
		const static int DEQUEUE_ERROR_INSUFBUF=-105;
		const static int DEQUEUE_ERROR_UNKNOWN=-106;

		const static int INIT_ERROR_SHM_FAILED=-107;
		const static int INIT_ERROR_RLOCK_SEM_FAILED=-108;
		const static int INIT_ERROR_WLOCK_SEM_FAILED=-109;
		const static int INIT_ERROR_ATTACH_SHM_QUEUE=-110;

		const static int OPEN_ERROR_SHM_FAILED=-111;
		const static int OPEN_ERROR_RLOCK_SEM_FAILED=-112;
		const static int OPEN_ERROR_WLOCK_SEM_FAILED=-113;
		const static int OPEN_ERROR_ATTACH_SHM_QUEUE=-114;
		const static int SHM_MAGIC=0x01;
		const static int SEM_READ_LOCK_MAGIC=0x02;
		const static int SEM_WRITE_LOCK_MAGIC=0x03;

		CShmQueue();
		~CShmQueue();

		int init(const char* ipcPath,size_t queueSize,bool enableReadLock,bool enableWriteLock);
		int open(const char* ipcPath,bool enableReadLock,bool enableWriteLock);
		int enqueue(const char* data,uint32_t len);
		int dequeue(char* dataBuffer,uint32_t& bufferLen);
		void pop();

		inline int getLastErrorCode()const{
			return m_errno;
		}
		const char* errorMsg(int errCode)const;

		unsigned long queueTotalLen()const{
			return cycle_queue_total_len(m_cycleQueue);
		}

		unsigned long queueUsedLen()const{
			return cycle_queue_used_len(m_cycleQueue);
		}
	};
}
#endif //COMMONUTILS_SHMQUEUE_H
