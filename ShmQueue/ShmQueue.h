//
// Created by 李兴 on 16/7/28.
//

#ifndef COMMONUTILS_SHMQUEUE_H
#define COMMONUTILS_SHMQUEUE_H

#include "cycle_queue.h"
#include "SysVShareMem/SysVShm.h"
#include <pthread.h>
#include <assert.h>


namespace francis_module{
	class CShmQueue{
	private:
#pragma pack(1)
		typedef struct shm_queue_header_s{
			pthread_mutex_t robustReadMutex;
			pthread_mutex_t robustWriteMutex;
			char data[0];
		}StShmQueueHeader;

		typedef struct shm_queue_entry_s{
			StShmQueueHeader* shmHeader;
			struct cycle_queue* cycleQueue;
		}StShmQueueEntry;
#pragma pack()

		StShmQueueEntry* m_shmQueueEntry;
		CSysVShm m_shm;
		bool m_enableReadLock;
		bool m_enableWriteLock;
		int m_errno;
		bool m_isInited;

		int initRobustMutex(pthread_mutex_t* mutex);
		inline void lockRobustMutex(pthread_mutex_t* mutex);
		inline void unlockRobustMutex(pthread_mutex_t* mutex);
	public:
		const static int ERRNO_SUCCESS=0;
		const static int ENQUEUE_ERROR_TRY_LOCK=-100;
		const static int ENQUEUE_ERROR_FULL=-101;
		const static int ENQUEUE_ERROR_UNKNOWN=-102;

		const static int DEQUEUE_ERROR_EMPTY=-104;
		const static int DEQUEUE_ERROR_INSUFBUF=-105;
		const static int DEQUEUE_ERROR_UNKNOWN=-106;

		const static int INIT_ERROR_SHM_FAILED=-107;
		const static int INIT_ERROR_ATTACH_SHM_QUEUE=-110;

		const static int OPEN_ERROR_SHM_FAILED=-111;
		const static int OPEN_ERROR_ATTACH_SHM_QUEUE=-114;

		const static int INIT_ERROR_RLOCK_MUTEX_FAILED=-115;
		const static int INIT_ERROR_WLOCK_MUTEX_FAILED=-116;

		const static int INIT_ROBUST_MUTEX_FAILED=-117;
		const static int INIT_ERROR_OFM=-118;

		const static int SHM_MAGIC=0x01;

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

		inline unsigned long queueTotalLen()const{
			assert(m_isInited);
			return cycle_queue_total_len(m_shmQueueEntry->cycleQueue);
		}

		inline unsigned long queueUsedLen()const{
			assert(m_isInited);
			return cycle_queue_used_len(m_shmQueueEntry->cycleQueue);
		}

	};
}
#endif //COMMONUTILS_SHMQUEUE_H
