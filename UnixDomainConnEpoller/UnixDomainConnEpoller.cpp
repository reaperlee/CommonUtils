//
// Created by 李兴 on 16/7/29.
//

#include "UnixDomainConnEpoller.h"

#include <iostream>

#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

#define ENTER_DEBUG_OUT fprintf(stderr,"[ENTER <%-16s><%-12s><%-5d>]\n",__FILE__,__FUNCTION__,__LINE__);
#define EXIT_DEBUG_OUT fprintf(stderr,"[EXIT <%-16s><%-12s><%-5d>]\n",__FILE__,__FUNCTION__,__LINE__);

using namespace std;
using namespace francis_module;

const char* printHexData(const char* data,uint32_t dataSize){
	static char printBuffer[8192]={0};
	int printBufferIndex=0;
	for(uint32_t i=0;i<dataSize;i++){
		printBufferIndex+=snprintf(printBuffer+printBufferIndex,sizeof(printBuffer)-1-printBufferIndex,"%02x ",(unsigned char)data[i]);
	}
	printBuffer[printBufferIndex]='\0';
	return printBuffer;
}
/*
 * CUnixDomainConnEpoller
 */
CUnixDomainConnHandler* CUnixDomainConnHandler::m_instance=0;

CUnixDomainConnHandler::CUnixDomainConnHandler():m_connPool(NULL),m_connPoolSize(0),m_connPoolBaseAddr(0),m_connPoolMaxAddr(0),m_connTimeout(15),m_epollFD(-1),m_unixDomainListenFD(-1),m_msgHandler(NULL),m_completeHandler(NULL) {
	INIT_LIST_HEAD(&m_freeList);
	INIT_LIST_HEAD(&m_usedList);
}

CUnixDomainConnHandler* CUnixDomainConnHandler::getInstance() {
	if(m_instance==0){
		m_instance=new (nothrow) CUnixDomainConnHandler();
		assert(m_instance!=0);
	}
	return m_instance;
}

int CUnixDomainConnHandler::initUnixDomainListener(const char *unixDomainPath) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	if(m_epollFD<0){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		return -1;
	}

	m_unixDomainListenFD=socket(AF_UNIX,SOCK_STREAM,0);
	if(m_unixDomainListenFD<0){
#ifdef _MODULE_DEBUG
		cerr<<"Create UnixSockFD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -1;
	}

	struct sockaddr_un unixSockAddr;
	memset(&unixSockAddr,0,sizeof(unixSockAddr));
	unixSockAddr.sun_family=AF_UNIX;
	strncpy(unixSockAddr.sun_path,unixDomainPath,sizeof(unixSockAddr.sun_path)-1);

	unlink(unixSockAddr.sun_path);
	if(bind(m_unixDomainListenFD,(struct sockaddr*)&unixSockAddr,sizeof(struct sockaddr_un))<0){
#ifdef _MODULE_DEBUG
		cerr<<"Bind UnixSockFD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -2;
	}

	if(fcntl(m_unixDomainListenFD, F_SETFL, O_NONBLOCK)<0){
#ifdef _MODULE_DEBUG
		cerr<<"Setnoblock UnixSockFD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -2;
	}

	if(listen(m_unixDomainListenFD,1024)<0){
#ifdef _MODULE_DEBUG
		cerr<<"Listen UnixSockFD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -3;
	}

	struct epoll_event epollEvent;
	epollEvent.events=EPOLLIN|EPOLLERR;
	epollEvent.data.ptr=&m_unixDomainListenFD;
	if(epoll_ctl(m_epollFD,EPOLL_CTL_ADD,m_unixDomainListenFD,&epollEvent)<0){

#ifdef _MODULE_DEBUG
		cerr<<"Add ListenFD To Epoll Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -4;
	}
#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif

	return 0;
}

int CUnixDomainConnHandler::initEpoll() {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	m_epollFD=epoll_create1(EPOLL_CLOEXEC);
	if(m_epollFD<0){
#ifdef _MODULE_DEBUG
		cerr<<"Create EpollFD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return -1;
	}
#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
	return 0;
}

int CUnixDomainConnHandler::init(uint32_t initPoolSize,uint32_t recvBufferInitSize,const char* unixPath,int epollFD) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	int ret;

	if(epollFD<0){
		ret=initEpoll();
		if(ret<0){
#ifdef _MODULE_DEBUG
			EXIT_DEBUG_OUT
#endif
			return -3;
		}
	}
	else{
		m_epollFD=epollFD;
	}

	ret=initUnixDomainListener(unixPath);
	if(ret<0){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		return -2;
	}
	m_connPoolSize=initPoolSize;

	//This will call constructor of StUnixDomainConn
	m_connPool=new (nothrow) StUnixDomainConn[m_connPoolSize];
	if(m_connPool==0){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		return -1;
	}

	for(uint32_t i=0;i<m_connPoolSize;i++){
		m_connPool[i]._read_buffer._buffer_init_size=recvBufferInitSize;
		list_add_tail(&m_connPool[i]._next,&m_freeList);
	}

	m_connPoolBaseAddr=(void*)&m_connPool[0];
	m_connPoolMaxAddr=(void*)&m_connPool[initPoolSize-1];

#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
	return 0;
}

void CUnixDomainConnHandler::handleUnixDomainHandlerAccept() {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	struct sockaddr_un cliAddr;
	socklen_t cliAddrLen=sizeof(struct sockaddr_un);
	memset(&cliAddr,0,sizeof(struct sockaddr_un));
	int cliFD=accept(m_unixDomainListenFD,(struct sockaddr*)&cliAddr,&cliAddrLen);
	if(cliFD<0){
#ifdef _MODULE_DEBUG
		cerr<<"UnixSockFD Accept Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return;
	}

	if(fcntl(cliFD, F_SETFL, O_RDWR | O_NONBLOCK)<0){
#ifdef _MODULE_DEBUG
		cerr<<"Setnoblock Client FD Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		close(cliFD);
		return;
	}


	void* conn=this->addConn(cliFD);
	if(conn==NULL){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
		cerr<<"Add New Connection Failed!"<<endl;
#endif
		close(cliFD);
		return;
	}
	struct epoll_event ev;
	ev.events=EPOLLIN|EPOLLERR;
	ev.data.ptr=conn;
	if(epoll_ctl(m_epollFD,EPOLL_CTL_ADD,cliFD,&ev)<0){
		close(cliFD);
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
		cerr<<"Add Client FD To Epoll Failed:"<<strerror(errno)<<endl;
#endif
		return;
	}
	//cerr<<"cliFD="<<cliFD<<endl;
#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
	return;
}


void* CUnixDomainConnHandler::addConn(int fd) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif

	if(fd<0){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		return NULL;
	}

	if(list_empty(&m_freeList)){
		//TODO:LOG Here!
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		m_stat.connOverloadCnt++;
		return NULL;
	}

	StUnixDomainConn* newConn=list_entry(m_freeList.next,struct unix_domain_conn,_next);
	newConn->_fd=fd;
	newConn->_last_access=time(NULL);

	list_move_tail(&newConn->_next,&m_usedList);

#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
	return (void*)newConn;

}

void CUnixDomainConnHandler::closeConn(StUnixDomainConn *conn) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	if(conn->_fd>0){
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		close(conn->_fd);
	}

	conn->_last_access=0;
	conn->_read_buffer.reinit();
	list_move_tail(&conn->_next,&m_freeList);
#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
}

int CUnixDomainConnHandler::handleConn(void *ptr, uint32_t epollEvent) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif

	static char recvBuffer[MAX_RECV_BUFFER_SIZE]={0};
	struct timeval currentTV;
	StUnixDomainConn* conn= reinterpret_cast<StUnixDomainConn*>(ptr);

	gettimeofday(&currentTV,NULL);

	conn->_last_access=currentTV.tv_sec;

	if(epollEvent & EPOLLERR){
		//TODO:LOG Here
#ifdef _MODULE_DEBUG
		EXIT_DEBUG_OUT
#endif
		m_stat.epollErrCnt++;
		closeConn(conn);
		return -1;
	}

	if(epollEvent & EPOLLIN){
		int loopCnt=0;
		while(loopCnt<100){
			ssize_t readRet=0;
			readRet=read(conn->_fd,recvBuffer,MAX_RECV_BUFFER_SIZE);
			if(readRet>0){
#ifdef _MODULE_DEBUG
				fprintf(stderr,"[<%-16s><%-12s><%-5d>]readRet=%d\n",__FILE__,__FUNCTION__,__LINE__,readRet);
#endif
				if(conn->_read_buffer.append(recvBuffer,readRet)<0){
					//TODO:LOG Here!
#ifdef _MODULE_DEBUG
					EXIT_DEBUG_OUT
#endif
					closeConn(conn);
					return -1;
				}
			}
			else if(readRet==0){
#ifdef _MODULE_DEBUG
				fprintf(stderr,"[<%-16s><%-12s><%-5d>]readRet=%d\n",__FILE__,__FUNCTION__,__LINE__,readRet);
				fprintf(stderr,"[<%-16s><%-12s><%-5d>]_read_buffer.getCurrentDataLen()=%u\n",__FILE__,__FUNCTION__,__LINE__,conn->_read_buffer.getCurrentDataLen());
#endif
				if(conn->_read_buffer.getCurrentDataLen()>0){
					conn->_delay_close=1;
					break;
				}
				else{
					closeConn(conn);
					break;
				}
			}
			else{
#ifdef _MODULE_DEBUG
				fprintf(stderr,"[<%-16s><%-12s><%-5d>]readRet=%d errno=%d(%s)\n",__FILE__,__FUNCTION__,__LINE__,readRet,errno,strerror(errno));
#endif
				if(errno!=EAGAIN){
					if(conn->_read_buffer.getCurrentDataLen()>0){
						conn->_delay_close=1;
						break;
					}
					//TODO:LOG HERE
					m_stat.recvErrCnt++;
					closeConn(conn);
#ifdef _MODULE_DEBUG
					EXIT_DEBUG_OUT
#endif
					return -1;
				}
				else{
					break;
				}
			}
			loopCnt++;
		}//while(loopCnt<100)
	}

	getMessage(conn);

	if(conn->_delay_close){
		closeConn(conn);
	}
#ifdef _MODULE_DEBUG
	EXIT_DEBUG_OUT
#endif
	return 0;
}

void CUnixDomainConnHandler::getMessage(StUnixDomainConn *conn) {
#ifdef _MODULE_DEBUG
	ENTER_DEBUG_OUT
#endif
	char _tmp_buffer[8192];
	ssize_t _tmp_buffer_size=0;

	while(conn->_read_buffer.getCurrentDataLen()){
		ssize_t completeCheckRet=0;
		if(m_completeHandler){
			completeCheckRet=m_completeHandler((void*)conn->_read_buffer.getCurrentData(),conn->_read_buffer.getCurrentDataLen());
		}
		if(completeCheckRet>0){
			m_stat.completePkgCnt++;
			memcpy(_tmp_buffer,conn->_read_buffer.getCurrentData(),completeCheckRet);
			_tmp_buffer_size=completeCheckRet;
			if(m_msgHandler){
				this->m_msgHandler(conn->_read_buffer.getCurrentData(),completeCheckRet);
			}
			conn->_read_buffer.skip(completeCheckRet);
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Receive Package:%s\n",printHexData(_tmp_buffer,_tmp_buffer_size));
#endif
		}
		else if(completeCheckRet==0){
#ifdef _MODULE_DEBUG
			fprintf(stderr,"Not Complete:%s\n",printHexData(conn->_read_buffer.getCurrentData(),conn->_read_buffer.getCurrentDataLen()));
#endif
			break;
		}
		else{
			m_stat.errPkgCnt++;
			conn->_delay_close=1;
			break;
		}
	}
}

void CUnixDomainConnHandler::timeTick(const struct timeval& tm) {
	static time_t lastTimeoutCheck=time(NULL);

	if(tm.tv_sec-lastTimeoutCheck>m_connTimeout/5){
		lastTimeoutCheck=tm.tv_sec;
		//TODO:Check conn expire here!

		list_head_t *tmp;
		StUnixDomainConn* conn;
		time_t accessDeadLine=tm.tv_sec-m_connTimeout;
		list_for_each_entry_safe_l(conn,tmp,&m_usedList,_next){
			if(conn->_last_access<accessDeadLine){
				closeConn(conn);
			}
		}
	}
}

void CUnixDomainConnHandler::epollOnce() {
	struct epoll_event waitEvents[1024];
	int epollEventsCnt=epoll_wait(m_epollFD,waitEvents,1024,1);
	if(epollEventsCnt<0){
#ifdef _MODULE_DEBUG
		cerr<<"Epoll Wait Failed:"<<strerror(errno)<<endl;
		EXIT_DEBUG_OUT
#endif
		return;
	}
	for(int i=0;i<epollEventsCnt;i++){
		if(waitEvents[i].data.ptr==&m_unixDomainListenFD){
			//listen fd
			this->handleUnixDomainHandlerAccept();
		}
		else if(isConnObject(waitEvents[i].data.ptr)){
			this->handleConn(waitEvents[i].data.ptr,waitEvents[i].events);
		}
		else{

		}
	}
}

int CUnixDomainConnHandler::tryHandleEpoll(void *ptr,uint32_t events) {
	if(ptr==(void*)&m_unixDomainListenFD){
		this->handleUnixDomainHandlerAccept();
		return 1;
	}
	else if(isConnObject(ptr)){
		this->handleConn(ptr,events);
		return 1;
	}
	else{
		return 0;
	}
	return 0;
}


