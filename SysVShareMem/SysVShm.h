//
// Created by 李兴 on 16/7/28.
//

#ifndef COMMONUTILS_SYSVSHM_H
#define COMMONUTILS_SYSVSHM_H

#include <string>
#include <sys/ipc.h>

namespace francis_module{
	const static int DEFAULT_SHM_OPEN_PERM=0666;
	class CSysVShmException{
	private:
		int m_code;
		std::string m_reason;
	public:
		const static int CODE_NOT_INITED=-1;
		const static int CODE_SHM_EXISTS=-2;
		const static int CODE_CREATE_SHM_FAIL=-3;
		const static int CODE_OPEN_SHM_FAIL=-4;
		const static int CODE_ATTACH_SHM_FAIL=-5;
		const static int CODE_DETACH_SHM_FAIL=-6;
		const static int CODE_SHMCTL_GETINFO_FAIL=-7;
		const static int CODE_FTOK_FAIL=-8;
		const static int CODE_CTL_MEMLOCK_FAIL=-9;
		const static int CODE_CTL_RMID_FAIL=-10;

		CSysVShmException(int code,const std::string& reason);

		const std::string& why()const{
			return m_reason;
		}
		int errCode()const{
			return m_code;
		}
	};
	class CSysVShm{
	private:
		int m_shmID;
		void* m_mem;
		key_t m_shmKey;
		size_t m_shmSize;

		int createShm(key_t shmKey,size_t size) throw(CSysVShmException);
		int openShm(key_t shmKey) throw(CSysVShmException);
		int attach() throw(CSysVShmException);
		int detach() throw(CSysVShmException);
		int remove() throw(CSysVShmException);
	public:
		CSysVShm();
		CSysVShm(const char* ipcPath,int magic,size_t size) throw(CSysVShmException);
		~CSysVShm();

		//0:success(just open share memory)
		//1:success(create share memory)
		int init(const char* ipcPath,int magic,size_t size) throw(CSysVShmException);
		int onlyOpen(const char* ipcPath,int magic) throw(CSysVShmException);
		int deleteShm(const char* ipcPath,int magic) throw(CSysVShmException);
		int setMemLock();

		void* getShm(){
			return m_mem;
		}

		size_t getShmSize()const{
			return m_shmSize;
		}

		int getShmID()const{
			return m_shmID;
		}

		key_t getShmKey()const{
			return m_shmKey;
		}


	};
}
#endif //COMMONUTILS_SYSVSHM_H
