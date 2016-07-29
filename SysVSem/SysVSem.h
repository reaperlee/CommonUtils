//
// Created by 李兴 on 16/7/28.
//

#ifndef COMMONTOOLS_SYSVSEM_H
#define COMMONTOOLS_SYSVSEM_H

#include <iostream>
#include <string>
#include <sys/ipc.h>

namespace francis_module{

	class CSysVSem;

	const static int DEFAULT_IPC_PERM=0660;
	class CSysVSemException{
	private:
		std::string m_reason;
		int m_code;
	public:
		const static int CODE_INITED=-1;
		const static int CODE_FTOK_FAILED=-2;
		const static int CODE_SEM_EXISTS=-3;
		const static int CODE_SEM_CREATE_FAIL=-4;
		const static int CODE_SEM_GET_INFO_FAIL=-5;
		const static int CODE_SEM_ILLEGAL_NSEMS=-6;
		const static int CODE_SEM_SET_INITVAL_FAIL_OFM=-7;
		const static int CODE_SEM_SET_INITVAL_FAIL=-8;
		const static int CODE_SEM_OPEN_FAIL=-9;

		CSysVSemException(int code,const std::string& reason);

		const std::string& why()const{
			return m_reason;
		}
		int errCode()const{
			return m_code;
		}
	};
	class CSysVSem{
	private:
		int m_semID;
		unsigned int m_maxTrialTimes;

		int createSem(key_t semKey) throw(CSysVSemException);
		int openSem(key_t semKey) throw(CSysVSemException);
	public:
		CSysVSem();
		CSysVSem(const char* ipcPath,int magic);
		~CSysVSem();

		int init(const char* ipcPath,int magic) throw(CSysVSemException);
		int open(const char* ipcPath,int magic) throw(CSysVSemException);
		void setMaxTrial(unsigned int maxTrial);
		int semP(bool noWait=true);
		int semV(bool noWait=true);
	};
}

#endif //COMMONTOOLS_SYSVSEM_H
