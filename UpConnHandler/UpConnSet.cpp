//
// Created by 李兴 on 16/5/8.
//
#include "UpConnSet.h"
#include "ccd/tfc_net_socket_tcp.h"
#include "ccd/tfc_net_socket_udp.h"

#ifdef _USE_FASTMEM
#include "fastmem.h"
#endif
#include <iostream>
#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
using namespace tfc::net;
using namespace std;
using namespace tfc::new_module;


//#define _DEBUG_MODULE

#define ENTER_DEBUG_OUT fprintf(stderr,"[ENTER <%-16s><%-12s><%-5d>]\n",__FILE__,__FUNCTION__,__LINE__);
#define EXIT_DEBUG_OUT fprintf(stderr,"[EXIT <%-16s><%-12s><%-5d>]\n",__FILE__,__FUNCTION__,__LINE__);
//CRawCache
int CRawCache::append(const char* data0, unsigned data_len0, const char* data1, unsigned data_len1)
{
    unsigned data_len = data_len0 + data_len1;
    if (0 == data_len)
    {
        printf("empty content to append!\n");
        return -1;
    }

    if(NULL == _data)
    {
        unsigned size = (data_len < _buff_size ? _buff_size : data_len);
#ifdef _USE_FASTMEM
        _data = (char*)fastmem_get(size, &_size);
#else
        _data=(char*)malloc(sizeof(char)*size);
        _size=size;
#endif
        if(NULL == _data)
        {
            //分配内存失败，丢弃当前操作
            printf("out of memory[size:%u], %s:%d\n", size, __FILE__, __LINE__);
            return -1;
        }

        _len    = 0;
        _offset = 0;
    }

    if(_size - _len - _offset >= data_len)
    {
        memcpy(_data + _offset + _len, data0, data_len0);
        memcpy(_data + _offset + _len + data_len0, data1, data_len1);
        _len += data_len;
    }
    else if(_len + data_len <= _size)
    {
        memmove(_data, _data + _offset, _len);
        memcpy(_data + _len, data0, data_len0);
        memcpy(_data + _len + data_len0, data1, data_len1);

        _offset = 0;
        _len += data_len;
    }
    else
    {
        unsigned tmp_len = calculate_new_buffer_size(data_len);
        unsigned tmp_size;

#ifdef _USE_FASTMEM
        char* tmp = (char*)fastmem_get(tmp_len, &tmp_size);
#else
        char* tmp=(char*)malloc(sizeof(char)*tmp_len);
        tmp_size=tmp_len;
#endif
        if(NULL == tmp)
        {
            //分配内存失败，丢弃当前操作
            printf("out of memory[size:%u], %s:%d\n", tmp_len, __FILE__, __LINE__);
            return -1;
        }

        memcpy(tmp, _data + _offset, _len);
        memcpy(tmp + _len, data0, data_len0);
        memcpy(tmp + _len + data_len0, data1, data_len1);

#ifdef _USE_FASTMEM
        fastmem_put(_data, _size);
#else
        free(_data);
#endif

        _data   = tmp;
        _size   = tmp_size;
        _len    = _len + data_len;
        _offset = 0;
    }

    return 0;
}

int CRawCache::append(const char* data, unsigned data_len)
{
    if(NULL == _data)
    {
        unsigned size = (data_len < _buff_size ? _buff_size : data_len);
#ifdef _USE_FASTMEM
        _data = (char*)fastmem_get(size, &_size);
#else
        _data=(char*)malloc(sizeof(char)*size);
        _size=size;
#endif

        if(NULL == _data)
        {
            //分配内存失败，丢弃当前操作
            printf("out of memory[size:%u], %s:%d\n", size, __FILE__, __LINE__);
            return -1;
        }

        _len    = 0;
        _offset = 0;
    }

    if(_size - _len - _offset >= data_len)
    {
        memcpy(_data + _offset + _len, data, data_len);
        _len += data_len;
    }
    else if(_len + data_len <= _size)
    {
        memmove(_data, _data + _offset, _len);
        memcpy(_data + _len, data, data_len);

        _offset = 0;
        _len   += data_len;
    }
    else
    {
        unsigned tmp_len = calculate_new_buffer_size(data_len);
        unsigned tmp_size;
#ifdef _USE_FASTMEM
        char* tmp = (char*)fastmem_get(tmp_len, &tmp_size);
#else
        char* tmp=(char*)malloc(tmp_len);
        tmp_size=tmp_len;
#endif
        if(!tmp)
        {
            //分配内存失败，丢弃当前操作
            printf("out of memory[size:%u], %s:%d\n", tmp_len, __FILE__, __LINE__);
            return -1;
        }

        memcpy(tmp, _data + _offset, _len);
        memcpy(tmp + _len, data, data_len);

#ifdef _USE_FASTMEM
        fastmem_put(_data, _size);
#else
        free(_data);
#endif

        _data   = tmp;
        _size   = tmp_size;
        _len    = _len + data_len;
        _offset = 0;
    }

    return 0;
}

int CRawCache::calculate_new_buffer_size(unsigned append_size) {
    unsigned linear_increment_len = _len + append_size;

    // current buffer is not too long, use liner increment first
    if (_len < (unsigned)LINEAR_MALLOC_THRESHOLD) {
        return linear_increment_len;
    }

    // current buffer is long enough, try exponent increment, and return the
    // bigger one
    unsigned exponent_increment_len = _len + _len / EXPONENT_INCREMENT_PERCENT;
    if (exponent_increment_len > linear_increment_len) {
        return exponent_increment_len;
    } else {
        return linear_increment_len;
    }
}

void CRawCache::skip(unsigned length)
{
    if(NULL == _data)
    {
        return;
    }

    if(length >= _len)
    {
        if ( _size > _buff_size )
        {
            reinit();
        }
        else
        {
            clean_data();
        }
    }
    else
    {
        _len    -= length;
        _offset += length;
    }
}

void CRawCache::reinit()
{
#ifdef _USE_FASTMEM
    fastmem_put(_data, _size);
#else
    free(_data);
#endif

    _data   = NULL;
    _size   = 0;
    _len    = 0;
    _offset = 0;
}

//CUpStreamConnSet

int CUpStreamConnSet::addConnTCP(const char *ip, unsigned short port, uint64_t sessionSeq,StUpStreamConn** conn) {
#ifdef _DEBUG_MODULE
	ENTER_DEBUG_OUT
#endif
    int ret;
    static CSocketTCP socket(-1, false);
    socket.create();
    socket.set_nonblock();
    ret = socket.connect(ip, port);
    if ((ret != 0) && (ret != -EWOULDBLOCK) && (ret != -EINPROGRESS)) {
        close(socket.fd());
#ifdef _DEBUG_MODULE
		EXIT_DEBUG_OUT
#endif
        return ADDCONNTCP_ERROR_CONNECT;
    }

    StUpStreamConn* cc=NULL;

    if (list_empty(&m_freeConnList)) {
        //TODO:Report To Controller HERE!
        close(socket.fd());
#ifdef _DEBUG_MODULE
		EXIT_DEBUG_OUT
#endif
        return ADDCONNTCP_ERROR_CONN_OVERLOAD;
    }

    cc = list_entry(m_freeConnList.next, StUpStreamConn, _next);

    cc->_remoteIp=inet_addr(ip);
    cc->_remotePort=port;
    cc->_access_time= time(NULL);
    cc->_sessionNum=sessionSeq;
    cc->_fd     = socket.fd();
    cc->_flag   = 0;
    cc->_type   = CONN_TYPE_TCP;
    cc->_start_time.tv_sec=0;
    cc->_start_time.tv_usec=0;

    cc->_epoll_flag = (EPOLLIN | EPOLLOUT);

    list_move_tail(&cc->_next, &m_usedConnList);

    *conn=cc;
    m_statInfo.tcpConnCnt++;
#ifdef _DEBUG_MODULE
	EXIT_DEBUG_OUT
#endif
    return ADDCONNTCP_ERROR_NONE;
}

void CUpStreamConnSet::closeConn(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
	ENTER_DEBUG_OUT
#endif
	if(upStreamConn->_type==CONN_TYPE_TCP){
        m_statInfo.tcpConnCnt--;
    }
    else if(upStreamConn->_type==CONN_TYPE_UDP){
        m_statInfo.udpConnCnt--;
    }
    StConnKey connKey;
    connKey.ip=upStreamConn->_remoteIp;
    connKey.port=upStreamConn->_remotePort;
    connKey.flow=upStreamConn->_sessionNum;
    connKey.protocol=upStreamConn->_type;
    m_connTable.del(connKey);
    if(upStreamConn->_fd>0){
#ifdef _DEBUG_MODULE
		struct in_addr _tmp_addr;
        _tmp_addr.s_addr=upStreamConn->_remoteIp;
        fprintf(stderr,"Close Remote (%s:%d)\n",inet_ntoa(_tmp_addr),upStreamConn->_remotePort);
#endif
        close(upStreamConn->_fd);
        upStreamConn->_fd=-1;
    }

    upStreamConn->_writeCache->reinit();
    upStreamConn->_readCache->reinit();
    upStreamConn->_type=CONN_TYPE_TCP;
    upStreamConn->_flag=0;
    upStreamConn->_start_time.tv_sec=0;
    upStreamConn->_start_time.tv_usec=0;

    list_move(&upStreamConn->_next,&m_freeConnList);
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
}

int CUpStreamConnSet::addConnUDP(const char* ip,unsigned short port,StUpStreamConn** upStreamConn){
	int ret;
	static CSocketUDP socket(-1, false);

    if ((ret = socket.create()) < 0) {
        return ADDCONNUDP_ERROR_SOCKET;
    }
    if ((ret = socket.set_nonblock()) < 0) {
        return ADDCONNUDP_ERROR_SOCKET;
    }

    StUpStreamConn* cc=NULL;

    if (list_empty(&m_freeConnList)) {
        //TODO:Report To Controller HERE!
        close(socket.fd());
#ifdef _DEBUG_MODULE
		EXIT_DEBUG_OUT
#endif
        return ADDCONNUDP_ERROR_CONN_OVERLOAD;
    }

    cc = list_entry(m_freeConnList.next, StUpStreamConn, _next);

    cc->_remoteIp=inet_addr(ip);
    cc->_remotePort=port;
    cc->_access_time= time(NULL);
    cc->_sessionNum=0;
    cc->_fd     = socket.fd();
    cc->_flag   = 0;
    cc->_type   = CONN_TYPE_UDP;

    cc->_epoll_flag = (EPOLLIN | EPOLLOUT);

    list_move_tail(&cc->_next, &m_usedConnList);

    *upStreamConn=cc;
    m_statInfo.udpConnCnt++;
#ifdef _DEBUG_MODULE
	EXIT_DEBUG_OUT
#endif
    return ADDCONNTCP_ERROR_NONE;
}



int CUpStreamConnSet::sendOnlyCacheTCP(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
	ENTER_DEBUG_OUT
#endif
    if((m_sndBuffMax)&&(upStreamConn->_writeCache->data_len()>m_sndBuffMax)){
        //TODO:REPORT TO CONTROLLER HERE
#ifdef _DEBUG_MODULE
		EXIT_DEBUG_OUT
#endif
        return -ERROR_NEED_CLOSE;
    }

    if(upStreamConn->_writeCache->data_len()==0){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }


    size_t sentLen=0;
    int ret=sendTCP(upStreamConn,upStreamConn->_writeCache->data(),upStreamConn->_writeCache->data_len(),sentLen);
    if((ret==0)&&(sentLen==upStreamConn->_writeCache->data_len())){
        m_statInfo.tcpSendBytesCnt+=sentLen;
        upStreamConn->_writeCache->skip(sentLen);
        if(upStreamConn->_finclose){
            closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_FORCE_CLOSE;
        }
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }
    else if((ret==0)&&(sentLen<upStreamConn->_writeCache->data_len())){
        m_statInfo.tcpSendBytesCnt+=sentLen;
        upStreamConn->_writeCache->skip(sentLen);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -ERROR_NEED_SEND;
    }
    else if(ret<0){
        if(ret==-EAGAIN){
            //继续发送
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_SEND;
        }
        else{
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
    }
    else{
        fprintf(stderr,"[%s][%d]Should Not Be Here!\n",__FILE__,__LINE__);
        assert(false);
    }

#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return 0;
}
int CUpStreamConnSet::sendWithoutCacheTCP(StUpStreamConn *upStreamConn, const char *data, size_t dataLen) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    size_t sentLen=0;
    int ret=sendTCP(upStreamConn,data,dataLen,sentLen);
    if(ret==0&&sentLen==dataLen){
        m_statInfo.tcpSendBytesCnt+=sentLen;
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 1;
    }
    else if(ret==0&&sentLen<dataLen){
        m_statInfo.tcpSendBytesCnt+=sentLen;
        if(upStreamConn->_writeCache->append(data+sentLen,dataLen-sentLen)){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_MEM_ALLOC;
        }
    }
    else{
        if(ret==-EAGAIN){
            if(upStreamConn->_writeCache->append(data,dataLen)<0){
                //Mem Alloc Failed
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_MEM_ALLOC;
            }
        }
        else{
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
    }

#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return 0;
}

int CUpStreamConnSet::sendWithCacheTCP(StUpStreamConn *upStreamConn, const char *data, size_t dataLen) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    int ret;

    if((m_sndBuffMax)&&(upStreamConn->_writeCache->data_len()>m_sndBuffMax)){
        //TODO:REPORT TO CONTROLLER HERE
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -ERROR_NEED_CLOSE;
    }

    size_t sentLen=0;

    //FIRST SEND WRITE CACHE CONTENT
    if(upStreamConn->_writeCache->data_len()>0){
        ret=sendTCP(upStreamConn,upStreamConn->_writeCache->data(),upStreamConn->_writeCache->data_len(),sentLen);
        if((ret==0)&&(sentLen==upStreamConn->_writeCache->data_len())){
            m_statInfo.tcpSendBytesCnt+=sentLen;
            upStreamConn->_writeCache->skip(upStreamConn->_writeCache->data_len());
        }
        else if((ret==0)&&(sentLen<upStreamConn->_writeCache->data_len())){
            m_statInfo.tcpSendBytesCnt+=sentLen;
            upStreamConn->_writeCache->skip(sentLen);
            if(upStreamConn->_writeCache->append(data,dataLen)<0){
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_MEM_ALLOC;
            }
            return 0;
        }
        else if (ret<0){
            if(ret==-EAGAIN){
                if(upStreamConn->_writeCache->append(data,dataLen)<0){
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -ERROR_MEM_ALLOC;
                }
                return 0;
            }
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
    }

    sentLen=0;
    //SECOND SEND BUFFER DATA
    ret=sendTCP(upStreamConn,data,dataLen,sentLen);
    if((ret==0)&&(sentLen==dataLen)){
        m_statInfo.tcpSendBytesCnt+=sentLen;
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 1;
    }
    else if(ret==0&&sentLen<dataLen){
        m_statInfo.tcpSendBytesCnt+=sentLen;
        if(upStreamConn->_writeCache->append(data+sentLen,dataLen-sentLen)<0){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_MEM_ALLOC;
        }
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }
    else{
        if(ret==-EAGAIN){
            if(upStreamConn->_writeCache->append(data,dataLen)<0){
                //Mem Alloc Failed
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_MEM_ALLOC;
            }
        }
        else{
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
    }

#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return 0;
}

int CUpStreamConnSet::sendTCP(StUpStreamConn *upStreamConn, const char *data, size_t dataLen,size_t& sentLen) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    static CSocketTCP sock(-1, false);
    sock.attach(upStreamConn->_fd);
    int ret;

    upStreamConn->_access_time= time(NULL);
    list_move_tail(&upStreamConn->_next, &m_usedConnList);
    ret = sock.send(data, dataLen, sentLen);
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return ret;
}

int CUpStreamConnSet::recvConnDataTCP(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    static char recvBuffer[RECV_BUFF_LEN];
    static unsigned int LOOP_CNT_MAX=100;
    size_t recvLen=0;
    unsigned int loopCnt=0;
    int ret=0;
    while(1){
        if(m_recvBuffMax&&upStreamConn->_readCache->data_len()>m_recvBuffMax){
            //TODO:REPORT CONTROLLER HERE
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }

        if(loopCnt>LOOP_CNT_MAX){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_RECV;
        }

        recvLen=0;
        ret=recvTCP(upStreamConn,recvBuffer,RECV_BUFF_LEN,recvLen);
        loopCnt++;
        if(ret==0){
            //receive success
            if(recvLen==0){
                if(upStreamConn->_readCache->data_len()){
                    upStreamConn->_finclose=1;
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return 0;
                }
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_NEED_CLOSE;
            }
            else{
                m_statInfo.tcpRecvBytesCnt+=recvLen;
                if(upStreamConn->_readCache->append(recvBuffer,recvLen)<0){
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -ERROR_MEM_ALLOC;
                }
            }
        }else{
            if(ret==-EAGAIN){
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_NEED_RECV;
            }else{
                if(upStreamConn->_readCache->data_len()){
                    upStreamConn->_finclose=1;
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return 0;
                }
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -ERROR_NEED_CLOSE;
            }
        }
    }
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
}

int CUpStreamConnSet::recvTCP(StUpStreamConn *upStreamConn, char *dataBuf, size_t buffSize, size_t &recvLen) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    static CSocketTCP sock(-1,false);
    sock.attach(upStreamConn->_fd);

    upStreamConn->_access_time=time(NULL);
    list_move_tail(&upStreamConn->_next,&m_usedConnList);

    //TODO:MAYBE ADD SOME STATISTIC HERE!
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return sock.receive(dataBuf,buffSize,recvLen);
}

int CUpStreamConnSet::sendUDP(StUpStreamConn *upStreamConn, const char *data, size_t dataLen, size_t &sentLen,const struct sockaddr &to, socklen_t tolen) {
    static CSocketUDP udpSocket(-1,false);
    udpSocket.attach(upStreamConn->_fd);

    upStreamConn->_access_time=time(NULL);
    list_move_tail(&upStreamConn->_next,&this->m_usedConnList);
    return udpSocket.sendto(data,dataLen,sentLen,0,to,tolen);
}

int CUpStreamConnSet::recvUDP(StUpStreamConn *upStreamConn, char *dataBuf, size_t buffSize, size_t &recvLen,
                              struct sockaddr &from, socklen_t &fromLen) {
    static CSocketUDP udpSocket(-1,false);
    udpSocket.attach(upStreamConn->_fd);

    upStreamConn->_access_time=time(NULL);
    list_move_tail(&upStreamConn->_next,&this->m_usedConnList);

    return udpSocket.recvfrom(dataBuf,buffSize,recvLen,0,from,fromLen);
}

int CUpStreamConnSet::sendOnlyCacheUDP(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    StUDPHeader *udpHeaderPtr=NULL;
    unsigned int i=0;
    while(i<m_udpSndCnt){
        if(upStreamConn->_writeCache->data_len()==0){
            //Nothing more in the cache to send
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_SEND_COMP;
        }
        if(upStreamConn->_writeCache->data_len()<sizeof(StUDPHeader)){
            upStreamConn->_writeCache->reinit();
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
        udpHeaderPtr=(StUDPHeader*)upStreamConn->_writeCache->data();
        size_t msgLen=udpHeaderPtr->udpMsgLen;
        if(upStreamConn->_writeCache->data_len()<msgLen+sizeof(StUDPHeader)){
            upStreamConn->_writeCache->reinit();
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -ERROR_NEED_CLOSE;
        }
        time_t msgTime=udpHeaderPtr->udpStartTime.tv_sec*1000+udpHeaderPtr->udpStartTime.tv_usec/1000;
        struct timeval currentTimeVal;
        gettimeofday(&currentTimeVal,NULL);
        time_t currentTime=currentTimeVal.tv_sec*1000+currentTimeVal.tv_usec/1000;
        if(currentTime-msgTime>m_udpMsgTimeout){
            upStreamConn->_writeCache->skip(msgLen+sizeof(StUDPHeader));
            continue;
        }
        size_t sentLen;
        int ret=this->sendUDP(upStreamConn,
                              upStreamConn->_writeCache->data()+sizeof(StUDPHeader),
                              msgLen,
                              sentLen,
                              *((struct sockaddr *)(&udpHeaderPtr->udpAddr)),
                              sizeof(struct sockaddr_in));
        if(ret==0){
            upStreamConn->_writeCache->skip(msgLen);
            m_statInfo.udpSendBytesCnt+=sentLen;
            if(sentLen!=msgLen){
                if(upStreamConn->_writeCache->data_len()==0){
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -ERROR_SEND_COMP;
                }
                else{
                    i++;
                    continue;
                }
            }

			if(upStreamConn->_writeCache->data_len()==0){
				if(upStreamConn->_finclose){
					this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
					return -ERROR_NEED_CLOSE;
				}
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
				return -ERROR_SEND_COMP;
			}	
        }else if(ret==-EAGAIN){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -ERROR_NEED_SEND;
		}
		else{
			upStreamConn->_writeCache->skip(msgLen);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -ERROR_NEED_CLOSE;
		}
		i++;
    }

#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return -ERROR_NEED_SEND;
}

int CUpStreamConnSet::sendWithCacheUDP(StUpStreamConn *upStreamConn, const char *data, size_t dataLen, size_t &sentLen,const struct sockaddr &to, socklen_t tolen) {
#ifdef _DEBUG_MODULE
	ENTER_DEBUG_OUT
#endif
	
	int ret;

	ret=this->sendOnlyCacheUDP(upStreamConn);
	if(ret==-ERROR_SEND_COMP){
		int iret=this->sendUDP(upStreamConn,data,dataLen,sentLen,to,tolen);
		if(iret==0){
            m_statInfo.udpSendBytesCnt+=sentLen;
#ifdef _DEBUG_MODULE
	        EXIT_DEBUG_OUT
#endif
			return -ERROR_SEND_COMP;
		}else if(iret==-EAGAIN){
			StUDPHeader udpHeader;
			udpHeader.udpMsgLen=dataLen;
			memcpy(&udpHeader.udpAddr,&to,sizeof(struct sockaddr_in));
			gettimeofday(&udpHeader.udpStartTime,NULL);

			if(upStreamConn->_writeCache->append((char*)&udpHeader,sizeof(StUDPHeader),data,dataLen)<0){
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
				return -ERROR_MEM_ALLOC;
			}
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -ERROR_NEED_SEND;
		}else{
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -ERROR_NEED_CLOSE;
		}
	}
	else if(ret==-ERROR_NEED_CLOSE){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_NEED_CLOSE;
	}
	else if(ret==-ERROR_NEED_SEND){
		StUDPHeader udpHeader;
		udpHeader.udpMsgLen=dataLen;
		memcpy(&udpHeader.udpAddr,&to,sizeof(struct sockaddr_in));
		gettimeofday(&udpHeader.udpStartTime,NULL);

		if(upStreamConn->_writeCache->append((char*)&udpHeader,sizeof(StUDPHeader),data,dataLen)<0){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
				return -ERROR_MEM_ALLOC;
		}
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_NEED_SEND;
	}
	else{
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_NEED_CLOSE;
	}
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return -ERROR_NEED_CLOSE;
}

int CUpStreamConnSet::recvConnDataUDP(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
	ENTER_DEBUG_OUT
#endif
	static char _udpPkgBuf[UDP_PKG_BUFF_LEN];	
	size_t recvLen=0;
	StUDPHeader udpHeader;
	//udpHeader.udpMsgLen=recvLen;
	//gettimeofday(&(udpHeader.udpStartTime),NULL);
	struct sockaddr *paddr=(struct sockaddr*)(&udpHeader.udpAddr);
	socklen_t addrLen=0;

	int ret=this->recvUDP(upStreamConn,_udpPkgBuf,UDP_PKG_BUFF_LEN,recvLen,*paddr,addrLen);
	if(ret==0){
        m_statInfo.udpRecvBytesCnt+=recvLen;
		if(recvLen==UDP_PKG_BUFF_LEN){
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return -ERROR_TRUNC;
		}
        udpHeader.udpMsgLen=recvLen;
        gettimeofday(&(udpHeader.udpStartTime),NULL);

		if(upStreamConn->_readCache->append((const char*)&udpHeader,sizeof(StUDPHeader),_udpPkgBuf,recvLen)<0){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -ERROR_MEM_ALLOC;
		}
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_RECVED;
	}else if(ret==-EAGAIN){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_NEED_RECV;
	}
	else{
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -ERROR_NEED_CLOSE;
	}
#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
	//Should Not Be Here!
    return -ERROR_NEED_CLOSE;
}

int CUpStreamConnSet::init(int epFd) {
    if(epFd<=0){
        return -1;
    }
    if(m_upStreamConnPool==NULL){
        return -3;
    }
    m_epollFd=epFd;


    for (size_t i = 0; i < m_maxConnCnt; ++i) {
        StUpStreamConn* cc=&m_upStreamConnPool[i];

        cc->_readCache= new (nothrow) StRawCache();
        cc->_writeCache=new (nothrow) StRawCache();

        if(cc->_readCache==NULL||cc->_writeCache==NULL){
            return -2;
        }

        cc->_readCache->_data = NULL;
        cc->_readCache->_size = 0;
        cc->_readCache->_len = 0;
        cc->_readCache->_offset = 0;

        cc->_writeCache->_data = NULL;
        cc->_writeCache->_size = 0;
        cc->_writeCache->_len = 0;
        cc->_writeCache->_offset = 0;

        cc->_type = CONN_TYPE_TCP;     /* Default set to TCP. */

        /*
         * 这里只设置收发缓冲区的初始化大小，但是不分配内存，
         * 直到第一次使用到才分配内存
         */
        cc->_readCache->_buff_size = m_recvBuffInitSize;
        cc->_writeCache->_buff_size = m_recvBuffInitSize;
        cc->_start_time.tv_sec = 0;
        cc->_start_time.tv_usec=0;

        INIT_LIST_HEAD(&cc->_next);
        list_add_tail(&cc->_next, &m_freeConnList);
    }
    return 0;
}

void CUpStreamConnSet::checkExpireConn(time_t accessDeadLine) {
    StUpStreamConn* conn=NULL;
    list_head_t* tmp;
    list_for_each_entry_safe_l(conn,tmp,&m_usedConnList,_next){
        if(conn->_type==CONN_TYPE_TCP){
            if(conn->_access_time<accessDeadLine){
#ifdef _DEBUG_MODULE
                cerr<<"["<<__FUNCTION__<<"]conn->_access_time="<<conn->_access_time<<" accessDeadLine="<<accessDeadLine<<" Need To Close!"<<endl;
#endif
                //TODO:ADD STATISTIC HERE
                closeConn(conn);
            }
            else{
                break;
            }
        }
    }
}

int CUpStreamConnSet::handleTCPIn(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    if(upStreamConn->_fd<0){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -1;
    }

    /*
     * 接收消息，如果发现对端断开连接但是读缓冲里还有未处理的数据
     * 则延迟关闭_finclose被设置
     */
    int ret = recvConnDataTCP(upStreamConn);

    /* ret == 0，遇到错误，finclose置位，不再接收 */
    if (ret == 0) {
        upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag & (~EPOLLIN));
        epollMod(m_epollFd, upStreamConn->_fd, upStreamConn, upStreamConn->_epoll_flag);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }

    /* loop_cnt > 100，或者遇到-EAGAIN，监听epoll_in */
    if (ret == -CUpStreamConnSet::ERROR_NEED_RECV) {
        upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag | EPOLLIN);
        epollMod(m_epollFd, upStreamConn->_fd, upStreamConn, upStreamConn->_epoll_flag);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }

    /* 内存分配失败 */
    if (ret == -CUpStreamConnSet::ERROR_MEM_ALLOC) {
        cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Alloc Memory Failed!!"<<endl;

        /*
        fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
        write_mem_log(tools::LOG_RECV_OOM, cc);
        */

        upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag | EPOLLIN);
        epollMod(m_epollFd, upStreamConn->_fd, upStreamConn, upStreamConn->_epoll_flag);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }

    /* 关闭该连接，连接上已经没有缓存的接收数据 */
    if (ret == -CUpStreamConnSet::ERROR_NEED_CLOSE) {
        closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -1;
    }

    cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Should Not Be Here!!"<<endl;
    assert(false);
    return 0;
}

int CUpStreamConnSet::handleTCPOut(StUpStreamConn *upStreamConn) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    if (upStreamConn->_fd < 0) {
#ifdef _DEBUG_MODULE
       EXIT_DEBUG_OUT
#endif
        return -1;
    }

    if (upStreamConn->_connstatus != CONN_STATUS_CONNECTING && upStreamConn->_connstatus != CONN_STATUS_CONNECTED)
    {
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -2;
    }

    /* 之前是connecting状态，现在EPOLL_OUT被置位，socket可写，说明连接建立 */
    if (upStreamConn->_connstatus == CONN_STATUS_CONNECTING) {
        upStreamConn->_connstatus = CONN_STATUS_CONNECTED;
    }

    int ret = sendOnlyCacheTCP(upStreamConn);

    /* 发送完毕，需要发送send_ok通知 */
    if (ret == 0) {
        /* 缓存发送完毕,去除EPOLLOUT */
        upStreamConn->_epoll_flag=(upStreamConn->_epoll_flag&(~EPOLLOUT));
        epollMod(m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }

    if (ret == -CUpStreamConnSet::ERROR_NEED_SEND) {
        upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag | EPOLLOUT);
        epollMod(m_epollFd, upStreamConn->_fd, upStreamConn, upStreamConn->_epoll_flag);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return 0;
    }

    /* 连接已经在SendFromCache中关闭 */
    if (ret == -CUpStreamConnSet::ERROR_FORCE_CLOSE) {
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -3;
    }

    /* 发送失败,关闭连接 */
    if (ret == -CUpStreamConnSet::ERROR_NEED_CLOSE) {
        closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -4;
    }

    cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Should Not Be Here!!"<<endl;
    assert(false);
    return 0;
}

int CUpStreamConnSet::getConnMessage(StUpStreamConn* upStreamConn,char* messageBuffer,size_t messageBufferLen,size_t& messageLen) {
    ssize_t ret;
    struct timeval currentTime;
    gettimeofday(&currentTime,NULL);
    unsigned int recvTimeCost=0;

	if(upStreamConn->_type==CONN_TYPE_TCP){
		ret=upStreamConn->_completeFunc(upStreamConn->_readCache->data(),upStreamConn->_readCache->data_len());
		if(ret>0){
            m_statInfo.tcpPkgRecvCnt++;
            if(ret>(ssize_t)messageBufferLen){
                return -ERROR_NEED_CLOSE;
            }
            memcpy(messageBuffer,upStreamConn->_readCache->data(),ret);
            messageLen=ret;
            upStreamConn->_readCache->skip(ret);

            if(upStreamConn->_start_time.tv_sec||upStreamConn->_start_time.tv_usec){
                recvTimeCost=(currentTime.tv_sec-upStreamConn->_start_time.tv_sec)*1000+(currentTime.tv_usec-upStreamConn->_start_time.tv_usec)/1000;
                upStreamConn->_start_time.tv_sec=0;
                upStreamConn->_start_time.tv_usec=0;
                m_statInfo.tcpPkgRecvTimeTotal+=recvTimeCost;
                if(recvTimeCost>m_statInfo.tcpPkgRecvTimeMax){
                    m_statInfo.tcpPkgRecvTimeMax=recvTimeCost;
                }

                if(recvTimeCost<m_statInfo.tcpPkgRecvTimeMin){
                    m_statInfo.tcpPkgRecvTimeMin=recvTimeCost;
                }
            }

        }
        else if (ret==0){
            return -ERROR_NEED_RECV;
        }
        else{
            m_statInfo.tcpPkgCompErrCnt++;
            return -ERROR_NEED_CLOSE;
		}
	}
	else if(upStreamConn->_type==CONN_TYPE_UDP){
		if(upStreamConn->_readCache->data_len()==0){
			return -ERROR_NEED_RECV;
		}
		if(upStreamConn->_completeFunc){
			StUDPHeader* udpHeaderPtr;
			udpHeaderPtr=(StUDPHeader*)upStreamConn->_readCache->data();
			size_t udpMsgLen=udpHeaderPtr->udpMsgLen;
			ret=upStreamConn->_completeFunc(upStreamConn->_readCache->data()+sizeof(StUDPHeader),udpHeaderPtr->udpMsgLen);
			if(ret>0){
                m_statInfo.udpPkgRecvCnt++;
				if(ret>(ssize_t)messageBufferLen){
					return -ERROR_NEED_CLOSE;
				}
				memcpy(messageBuffer,upStreamConn->_readCache->data()+sizeof(StUDPHeader),ret);
				messageLen=ret;
				//for udp,always skip the whole messsage
				upStreamConn->_readCache->skip(sizeof(StUDPHeader)+udpMsgLen);

                if(upStreamConn->_start_time.tv_sec||upStreamConn->_start_time.tv_usec){
                    recvTimeCost=(currentTime.tv_sec-upStreamConn->_start_time.tv_sec)*1000+(currentTime.tv_usec-upStreamConn->_start_time.tv_usec)/1000;
                    upStreamConn->_start_time.tv_sec=0;
                    upStreamConn->_start_time.tv_usec=0;
                    m_statInfo.udpPkgRecvTimeTotal+=recvTimeCost;
                    if(recvTimeCost>m_statInfo.udpPkgRecvTimeMax){
                        m_statInfo.udpPkgRecvTimeMax=recvTimeCost;
                    }

                    if(recvTimeCost<m_statInfo.udpPkgRecvTimeMin){
                        m_statInfo.udpPkgRecvTimeMin=recvTimeCost;
                    }
                }
				return 0;
			}
			else{
                m_statInfo.udpPkgCompErrCnt++;
				return -ERROR_NEED_CLOSE;
			}
		}
		else{
            m_statInfo.udpPkgRecvCnt++;
			StUDPHeader *udpHeaderPtr;
			udpHeaderPtr=(StUDPHeader*)upStreamConn->_readCache->data();
			size_t udpMsgLen=udpHeaderPtr->udpMsgLen;
			if(udpMsgLen>messageBufferLen){
				return -ERROR_NEED_CLOSE;
			}
			memcpy(messageBuffer,upStreamConn->_readCache->data()+sizeof(StUDPHeader),udpMsgLen);
			messageLen=udpMsgLen;
			upStreamConn->_readCache->skip(sizeof(StUDPHeader)+udpMsgLen);

            if(upStreamConn->_start_time.tv_sec||upStreamConn->_start_time.tv_usec){
                recvTimeCost=(currentTime.tv_sec-upStreamConn->_start_time.tv_sec)*1000+(currentTime.tv_usec-upStreamConn->_start_time.tv_usec)/1000;
                upStreamConn->_start_time.tv_sec=0;
                upStreamConn->_start_time.tv_usec=0;
                m_statInfo.udpPkgRecvTimeTotal+=recvTimeCost;
                if(recvTimeCost>m_statInfo.udpPkgRecvTimeMax){
                    m_statInfo.udpPkgRecvTimeMax=recvTimeCost;
                }

                if(recvTimeCost<m_statInfo.udpPkgRecvTimeMin){
                    m_statInfo.udpPkgRecvTimeMin=recvTimeCost;
                }
            }
			return 0;
		}
	}
	else{

	}
    return 0;
}

int CUpStreamConnSet::handleUDPIn(StUpStreamConn* upStreamConn){
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
	if(upStreamConn->_fd<0){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
		return -1;
	}


	int i=0;	
	do{
		i++;
		if(i>100){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return 0;
		}
		int ret=this->recvConnDataUDP(upStreamConn);
		if(ret==-ERROR_NEED_CLOSE){
			this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -2;
		}
		else if(ret==-ERROR_NEED_RECV){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return 0;
		}
		else if(ret==-ERROR_RECVED){
			continue;
		}
		else if(ret==-ERROR_TRUNC){
			continue;	
		}
		else{
			this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			return -3;
		}

	}while(1);
    cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Should Not Be Here!!"<<endl;
	assert(false);
	return 0;
}

int CUpStreamConnSet::handleUDPOut(StUpStreamConn *upStreamConn){
		
	if(upStreamConn->_fd<0){
		return -1;
	}

	int ret=this->sendOnlyCacheUDP(upStreamConn);
	if(ret==-ERROR_SEND_COMP){
		this->epollMod(this->m_epollFd,upStreamConn->_fd,upStreamConn,EPOLLIN);
		return 0;
	}
	else if(ret==-ERROR_NEED_SEND){
		return 0;
	}
	else if(ret==-ERROR_NEED_CLOSE){
		this->closeConn(upStreamConn);
		return -2;
	}
	else{
		this->closeConn(upStreamConn);
		return -3;
	}

    cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Should Not Be Here!!"<<endl;
	assert(false);
	return 0;
}

void CUpStreamConnSet::printUsedConnInfo() const {
    StUpStreamConn* conn;
    list_head_t* tmp;
    list_for_each_entry_safe_l(conn,tmp,&m_usedConnList,_next){
        struct in_addr addr;
        addr.s_addr=conn->_remoteIp;
        cerr<<"[RemoteIp="<<inet_ntoa(addr)<<" RemotePort="<<conn->_remotePort<<" SessionNum="<<conn->_sessionNum
                <<" Type="<<conn->_type<<" Fd="<<conn->_fd<<" AccessTime="<<conn->_access_time
                <<" Status="<<conn->_connstatus<<" FinCloseFlag="<<conn->_finclose<<endl;
    }
}

int CUpStreamConnSet::netSendTcp(const char *ip, unsigned short port, const char *buff, size_t buffLen,check_complete_func completeFunc,uint64_t flow) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    if(ip==NULL||buff==NULL){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -SEND_FAIL_INVALID_PARAM;
    }
    StConnKey connKey;
    connKey.ip=inet_addr(ip);
    connKey.port=port;
    connKey.flow=flow;
    connKey.protocol=CONN_TYPE_TCP;

    int ret;
    StUpStreamConn* upStreamConn;
    pair<ConsistConnTableIterator,bool> searchIt=m_connTable.search(connKey);
    if(searchIt.second){
        //Connection Already Existed
        upStreamConn=(searchIt.first)->second;

        if(upStreamConn->_start_time.tv_sec==0&&upStreamConn->_start_time.tv_usec==0){
            gettimeofday(&upStreamConn->_start_time,NULL);
        }

        ret=sendWithCacheTCP(upStreamConn,buff,buffLen);
        if(ret==1){
            //SEND COMPLETE
            upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag & (~EPOLLOUT));
            epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
        }
        else if(ret==0){
            //SEND NOT COMPLETE
            upStreamConn->_epoll_flag|=EPOLLOUT;
            epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
        }
        else{
            if(ret==-ERROR_MEM_ALLOC){
#ifdef _DEBUG_MODULE
                EXIT_DEBUG_OUT
#endif
                return -SEND_FAIL_OFM;
            }

            if(ret==-ERROR_NEED_CLOSE){
                closeConn(upStreamConn);
                //m_connTable.del(connKey);

                ret=addConnTCP(ip,port,flow,&upStreamConn);
                if(ret!=ADDCONNTCP_ERROR_NONE){
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -1;
                }
                upStreamConn->_connstatus=CONN_STATUS_CONNECTING;
                ret=sendWithoutCacheTCP(upStreamConn,buff,buffLen);
                if(ret==1){
                    //send complete and success
                    upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag & (~EPOLLOUT));
                    epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
                    upStreamConn->_connstatus=CONN_STATUS_CONNECTED;
                    m_connTable.insert(make_pair(connKey,upStreamConn));
                }
                else if(ret==0){
                    //send not complete
                    upStreamConn->_epoll_flag|=EPOLLOUT;
                    epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
                    m_connTable.insert(make_pair(connKey,upStreamConn));
                }
                else if(ret==-ERROR_MEM_ALLOC){
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -SEND_FAIL_OFM;
                }
                else if(ret==-ERROR_NEED_CLOSE){
                    //TODO:测试对端不可达的时候会否返回到这个分支?还是需要epoll的EPOLLERR返回?
                    //close conn here!
                    closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -SEND_FAIL_CONNECT_FAIL;
                }
                else{
                    //should not be here!
                    fprintf(stderr,"[%s][%d]Should Not Be Here!\n",__FILE__,__LINE__);
                    assert(false);
                }
            }
        }
    }
    else{
        if(completeFunc==NULL){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -SEND_FAIL_INVALID_PARAM;
        }
        ret=addConnTCP(ip,port,flow,&upStreamConn);
        if(ret!=ADDCONNTCP_ERROR_NONE){
            switch(ret){
                case ADDCONNTCP_ERROR_CONN_OVERLOAD:
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -SEND_FAIL_CONN_OVERLOAD;
                case ADDCONNTCP_ERROR_CONNECT:
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -SEND_FAIL_CONNECT_FAIL;
                default:
#ifdef _DEBUG_MODULE
                    EXIT_DEBUG_OUT
#endif
                    return -SEND_FAIL_UNKNOWN;
            }
            fprintf(stderr,"[%s][%d]Should Not Be Here!\n",__FILE__,__LINE__);
            assert(false);
        }
        upStreamConn->_completeFunc=completeFunc;
        upStreamConn->_connstatus=CONN_STATUS_CONNECTING;
        gettimeofday(&upStreamConn->_start_time,NULL);

        ret=sendWithoutCacheTCP(upStreamConn,buff,buffLen);
        if(ret==1){
            //send complete and success
            upStreamConn->_epoll_flag = (upStreamConn->_epoll_flag & (~EPOLLOUT));
            epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
            upStreamConn->_connstatus=CONN_STATUS_CONNECTED;
            m_connTable.insert(make_pair(connKey,upStreamConn));
        }
        else if(ret==0){
            //send not complete
            upStreamConn->_epoll_flag|=EPOLLOUT;
            epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,upStreamConn->_epoll_flag);
            m_connTable.insert(make_pair(connKey,upStreamConn));
        }
        else if(ret==-ERROR_MEM_ALLOC){
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -SEND_FAIL_OFM;
        }
        else if(ret==-ERROR_NEED_CLOSE){
            //TODO:测试对端不可达的时候会否返回到这个分支?还是需要epoll的EPOLLERR返回?
            //close conn here!
            closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
            return -SEND_FAIL_CONNECT_FAIL;
        }
        else{
            //should not be here!
            fprintf(stderr,"[%s][%d]Should Not Be Here!\n",__FILE__,__LINE__);
            assert(false);
        }
    }

#ifdef _DEBUG_MODULE
    EXIT_DEBUG_OUT
#endif
    return 0;
}


int CUpStreamConnSet::netSendUdp(const char *ip, unsigned short port, const char *buff, size_t buffLen,check_complete_func netCompleteFunc) {
#ifdef _DEBUG_MODULE
    ENTER_DEBUG_OUT
#endif
    if(ip==NULL||buff==NULL){
#ifdef _DEBUG_MODULE
        EXIT_DEBUG_OUT
#endif
        return -SEND_FAIL_INVALID_PARAM;
    }
    m_statInfo.udpPkgSendCnt++;

	StConnKey connKey;
    connKey.ip=inet_addr(ip);
    connKey.port=port;
    connKey.flow=0;
    connKey.protocol=CONN_TYPE_UDP;

	int ret;
    StUpStreamConn* upStreamConn;
    pair<ConsistConnTableIterator,bool> searchIt=m_connTable.search(connKey);
	if(searchIt.second){
		//found
		upStreamConn=(searchIt.first)->second;

		struct sockaddr_in addr;
		struct sockaddr *paddr = (struct sockaddr *)(&addr);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);
		addr.sin_port = htons(port);

		size_t sentLen;

        if(upStreamConn->_start_time.tv_sec==0&&upStreamConn->_start_time.tv_usec==0){
            gettimeofday(&upStreamConn->_start_time,NULL);
        }

		ret=this->sendWithCacheUDP(upStreamConn,buff,buffLen,sentLen,*paddr,sizeof(sockaddr_in));
		if(ret==-ERROR_SEND_COMP){
			this->epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,EPOLLIN);
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return 0;
		}
		else if(ret==-ERROR_NEED_SEND){
			this->epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,EPOLLIN|EPOLLOUT);
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return 0;
		}
		else if(ret==-ERROR_MEM_ALLOC){
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return -SEND_FAIL_OFM;
		}
		else if(ret==-ERROR_NEED_CLOSE){
			this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			//this->m_connTable.del(connKey);
			return -SEND_FAIL_INVALID_UDP_PKG;
		}
		else{
			//Should Not Be Here
            //this->m_connTable.del(connKey);
			this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return -SEND_FAIL_UNKNOWN;
		}
	}
	else{
		int iret=this->addConnUDP(ip,port,&upStreamConn);
		if(iret!=ADDCONNUDP_ERROR_NONE){
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return -SEND_FAIL_CONN_OVERLOAD;
		}

		if(netCompleteFunc){
			upStreamConn->_completeFunc=netCompleteFunc;
		}
        gettimeofday(&upStreamConn->_start_time,NULL);

		struct sockaddr_in addr;
		struct sockaddr *paddr = (struct sockaddr *)(&addr);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);
		addr.sin_port = htons(port);


		size_t sentLen=0;
		iret=this->sendUDP(upStreamConn,buff,buffLen,sentLen,*paddr,sizeof(struct sockaddr_in));
		if(iret==0){
			//send success
            m_statInfo.udpSendBytesCnt+=sentLen;
			epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,EPOLLIN);
#ifdef _DEBUG_MODULE
            EXIT_DEBUG_OUT
#endif
			m_connTable.insert(make_pair(connKey,upStreamConn));
			return 0;

		}
		else if(iret==-EAGAIN){
			StUDPHeader udpHeader;
			udpHeader.udpMsgLen=buffLen;
			gettimeofday(&(udpHeader.udpStartTime),NULL);
			memcpy(&(udpHeader.udpAddr),&addr,sizeof(struct sockaddr_in));

			if(upStreamConn->_writeCache->append((const char*)&udpHeader,sizeof(udpHeader),buff,buffLen)){
				closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
				EXIT_DEBUG_OUT
#endif
				return -SEND_FAIL_OFM;
			}
            m_connTable.insert(make_pair(connKey,upStreamConn));
			epollAdd(this->m_epollFd,upStreamConn->_fd,upStreamConn,EPOLLOUT);
            return 0;
		}
		else{
			this->closeConn(upStreamConn);
#ifdef _DEBUG_MODULE
			EXIT_DEBUG_OUT
#endif
			return -SEND_FAIL_UDP_ERROR;	
		}
		//this->send
	}
#ifdef _DEBUG_MODULE
	EXIT_DEBUG_OUT
#endif
    return -SEND_FAIL_UNKNOWN;
}

int CUpStreamConnSet::closeUpstreamConn(const char *ip, unsigned short port, uint64_t flow, unsigned short protocol) {
    StConnKey connKey;
    connKey.ip=inet_addr(ip);
    connKey.port=port;
    connKey.flow=flow;
    connKey.protocol=protocol;

    pair<ConsistConnTableIterator,bool> searchIt=m_connTable.search(connKey);
    if(searchIt.second){
        StUpStreamConn* upStreamConn=(searchIt.first)->second;
        closeConn(upStreamConn);
    }
    else{
        //No Such Conn
        return -1;
    }
    return 0;
}

const StUpStreamConnStatInfo& CUpStreamConnSet::getStatInfo() const {
    return m_statInfo;
}

void CUpStreamConnSet::resetUpStreamConnStatInfo() {
    m_statInfo.reset();
}

