//
// Created by 李兴 on 16/7/29.
//

#ifndef COMMONUTILS_UNIXDOMAINCONNEPOLLER_H
#define COMMONUTILS_UNIXDOMAINCONNEPOLLER_H

#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "NetBuffer/NetBuffer.h"
#include "Common/list.h"

namespace francis_module{
	typedef struct conn_handler_statistic{
		uint32_t completePkgCnt;
		uint32_t errPkgCnt;
		uint32_t connOverloadCnt;
		uint32_t epollErrCnt;
		uint32_t recvErrCnt;
	}StConnHandlerStat;

	typedef void (*msg_handler_func)(const char* data,size_t dataLen);
	typedef ssize_t (*check_complete_func)(void* data,size_t dataLen);
	class CUnixDomainConnHandler{
	private:
		const static uint32_t MAX_RECV_BUFFER_SIZE=8192;

		static CUnixDomainConnHandler* m_instance;

		typedef struct unix_domain_conn{
			int _fd;
			time_t _last_access;
			francis_module::StNetBuffer _read_buffer;
			list_head_t _next;
			union{
				struct{
					unsigned int _delay_close:1;
				};
				uint32_t _flags;
			};

			unix_domain_conn():_fd(-1),_last_access(0),_read_buffer(512),_flags(0){
				INIT_LIST_HEAD(&_next);
			}
		}StUnixDomainConn;

		StUnixDomainConn* m_connPool;
		uint32_t m_connPoolSize;
		list_head_t m_usedList;
		list_head_t m_freeList;
		void* m_connPoolBaseAddr;
		void* m_connPoolMaxAddr;
		uint32_t m_connTimeout;
		StConnHandlerStat m_stat;
		int m_epollFD;
		int m_unixDomainListenFD;
		msg_handler_func m_msgHandler;
		check_complete_func m_completeHandler;

		void getMessage(StUnixDomainConn* conn);
		int initUnixDomainListener(const char* unixDomainPath);
		int initEpoll();
		void handleUnixDomainHandlerAccept();
	protected:
		CUnixDomainConnHandler();
		CUnixDomainConnHandler(const CUnixDomainConnHandler&);
		const CUnixDomainConnHandler& operator=(const CUnixDomainConnHandler&);
	public:
		//TODO:增加清理接口
		static CUnixDomainConnHandler* getInstance();

		int init(uint32_t initPoolSize,uint32_t recvBufferInitSize,const char* unixPath,int epollFD=-1);
		void* addConn(int fd);
		void closeConn(StUnixDomainConn* conn);
		inline bool isConnObject(void* ptr){
			return ptr>=m_connPoolBaseAddr&&ptr<=m_connPoolMaxAddr;
		}
		inline bool isListenFD(void* ptr){
			return ptr==(void*)&m_unixDomainListenFD;
		}
		int handleConn(void* ptr,uint32_t epollEvent);
		void timeTick(const struct timeval& tm);
		int tryHandleEpoll(void* ptr,uint32_t events);
		void epollOnce();

		void setCompleteHandler(check_complete_func completeFunc){
			this->m_completeHandler=completeFunc;
		}

		void setDataHandler(msg_handler_func msgHandler){
			m_msgHandler=msgHandler;
		}

		void resetStatistic(){
			memset(&m_stat,0,sizeof(m_stat));
		}

		void printStatistic()const{
			fprintf(stderr,"[STATISTIC]CompletePkgCnt=%u ErrPkgCnt=%u RecvErrCnt=%u EpollErrCnt=%u ConnOverloadCnt=%u\n",m_stat.completePkgCnt,m_stat.errPkgCnt,m_stat.recvErrCnt,m_stat.epollErrCnt,m_stat.connOverloadCnt);
		}

	};

}
#endif //COMMONUTILS_UNIXDOMAINCONNEPOLLER_H
