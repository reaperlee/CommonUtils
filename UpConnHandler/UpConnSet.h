//
// Created by 李兴 on 16/5/8.
//

#ifndef MCPP_UPCONNSET_H
#define MCPP_UPCONNSET_H

#include <unistd.h>


#include <iostream>
#include <string>

#include <sys/time.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
//linux list
#include "base/list.h"
#include "new_common/MyContainers.h"
#include "new_common/jhash.h"

//#define _USE_FASTMEM

namespace tfc{
    namespace new_module{
        typedef struct CRawCache
        {
            virtual ~CRawCache() {}

            virtual void reinit();
            virtual void skip(unsigned length);
            virtual int  append(const char *data0, unsigned data_len0, const char *data1, unsigned data_len1);
            virtual int  append(const char* data, unsigned data_len);

            inline virtual void  clean_data() { _offset = 0; _len = 0; }
            inline char* data() {return _data + _offset;}
            inline unsigned data_len() {return _len;}
            inline unsigned size() {return _size;}

            virtual void add_new_msg_time(time_t sec, time_t msec) {}
            virtual bool is_msg_timeout(int expire_time_ms, const struct timeval* now) { return false; }

            int calculate_new_buffer_size(unsigned append_size);

            size_t _buff_size;	//数据buffer的最小长度
            char*    _data;			//数据buffer
            size_t _size;			//数据buffer长度
            size_t _len;			//实际数据长度
            size_t _offset;		//实际数据偏移量
            const static int LINEAR_MALLOC_THRESHOLD = 1024 * 1024;
            const static int EXPONENT_INCREMENT_PERCENT = 10;
        }StRawCache;

        typedef ssize_t (*check_complete_func)(void* data, size_t data_len);

        const static int CONN_TYPE_TCP=0;
        const static int CONN_TYPE_UDP=1;
        const static unsigned int CONN_STATUS_CONNECTING=0;
        const static unsigned int CONN_STATUS_CONNECTED=1;
        typedef struct UpStreamConn{
            list_head_t _next;
            uint64_t _sessionNum;
            int _fd;
            int _type;
            uint32_t _remoteIp;
            uint16_t _remotePort;
            int _epoll_flag;
            time_t _access_time;
            timeval _start_time;//标识一个发送过程开始的时间
            check_complete_func _completeFunc;

            union
            {
                struct
                {
                    unsigned _finclose:1;		//是否在处理完成之后关闭连接，1-关闭，否则不关闭
                    unsigned _connstatus:5;		//连接所处的状态
                };
                unsigned _flag;
            };

            StRawCache* _readCache;
            StRawCache* _writeCache;
        }StUpStreamConn;

        typedef struct upstream_conn_set_stat_t{
            unsigned int udpPkgSendCnt;
            unsigned int udpPkgRecvCnt;
            unsigned int udpPkgCompErrCnt;
            unsigned int udpConnCnt;
            uint64_t udpSendBytesCnt;
            uint64_t udpRecvBytesCnt;
            uint64_t udpPkgRecvTimeTotal;
            unsigned int udpPkgRecvTimeMax;
            unsigned int udpPkgRecvTimeMin;
            unsigned int tcpPkgSendCnt;
            unsigned int tcpPkgRecvCnt;
            unsigned int tcpPkgCompErrCnt;
            unsigned int tcpConnCnt;
            uint64_t tcpSendBytesCnt;
            uint64_t tcpRecvBytesCnt;
            uint64_t tcpPkgRecvTimeTotal;
            unsigned int tcpPkgRecvTimeMax;
            unsigned int tcpPkgRecvTimeMin;

            upstream_conn_set_stat_t():udpPkgSendCnt(0),udpPkgRecvCnt(0),udpPkgCompErrCnt(0),udpConnCnt(0),udpSendBytesCnt(0),udpRecvBytesCnt(0),
                                       udpPkgRecvTimeTotal(0),udpPkgRecvTimeMax(0),udpPkgRecvTimeMin(UINT_MAX),
                                       tcpPkgSendCnt(0),tcpPkgRecvCnt(0),tcpPkgCompErrCnt(0),tcpConnCnt(0),tcpSendBytesCnt(0),tcpRecvBytesCnt(0),
                                       tcpPkgRecvTimeTotal(0),tcpPkgRecvTimeMax(0),tcpPkgRecvTimeMin(UINT_MAX)
            {

            };

            upstream_conn_set_stat_t(const upstream_conn_set_stat_t& otherStat){
                this->udpPkgSendCnt=otherStat.udpPkgSendCnt;
                this->udpPkgRecvCnt=otherStat.udpPkgRecvCnt;
                this->udpPkgCompErrCnt=otherStat.udpPkgCompErrCnt;
                this->udpConnCnt=otherStat.udpConnCnt;
                this->udpSendBytesCnt=otherStat.udpSendBytesCnt;
                this->udpRecvBytesCnt=otherStat.udpRecvBytesCnt;
                this->udpPkgRecvTimeTotal=otherStat.udpPkgRecvTimeTotal;
                this->udpPkgRecvTimeMax=otherStat.udpPkgRecvTimeMax;
                this->udpPkgRecvTimeMin=otherStat.udpPkgRecvTimeMin;

                this->tcpPkgSendCnt=otherStat.tcpPkgSendCnt;
                this->tcpPkgRecvCnt=otherStat.tcpPkgRecvCnt;
                this->tcpPkgCompErrCnt=otherStat.tcpPkgCompErrCnt;
                this->tcpConnCnt=otherStat.tcpConnCnt;
                this->tcpSendBytesCnt=otherStat.tcpSendBytesCnt;
                this->tcpRecvBytesCnt=otherStat.tcpRecvBytesCnt;
                this->tcpPkgRecvTimeTotal=otherStat.tcpPkgRecvTimeTotal;
                this->tcpPkgRecvTimeMax=otherStat.tcpPkgRecvTimeMax;
                this->tcpPkgRecvTimeMin=otherStat.tcpPkgRecvTimeMin;
            }

            const upstream_conn_set_stat_t& operator=(const upstream_conn_set_stat_t& otherStat){
                this->udpPkgSendCnt=otherStat.udpPkgSendCnt;
                this->udpPkgRecvCnt=otherStat.udpPkgRecvCnt;
                this->udpPkgCompErrCnt=otherStat.udpPkgCompErrCnt;
                this->udpConnCnt=otherStat.udpConnCnt;
                this->udpSendBytesCnt=otherStat.udpSendBytesCnt;
                this->udpRecvBytesCnt=otherStat.udpRecvBytesCnt;
                this->udpPkgRecvTimeTotal=otherStat.udpPkgRecvTimeTotal;
                this->udpPkgRecvTimeMax=otherStat.udpPkgRecvTimeMax;
                this->udpPkgRecvTimeMin=otherStat.udpPkgRecvTimeMin;

                this->tcpPkgSendCnt=otherStat.tcpPkgSendCnt;
                this->tcpPkgRecvCnt=otherStat.tcpPkgRecvCnt;
                this->tcpPkgCompErrCnt=otherStat.tcpPkgCompErrCnt;
                this->tcpConnCnt=otherStat.tcpConnCnt;
                this->tcpSendBytesCnt=otherStat.tcpSendBytesCnt;
                this->tcpRecvBytesCnt=otherStat.tcpRecvBytesCnt;
                this->tcpPkgRecvTimeTotal=otherStat.tcpPkgRecvTimeTotal;
                this->tcpPkgRecvTimeMax=otherStat.tcpPkgRecvTimeMax;
                this->tcpPkgRecvTimeMin=otherStat.tcpPkgRecvTimeMin;
                return *this;
            }

            void reset(){
                this->udpPkgSendCnt=0;
                this->udpPkgRecvCnt=0;
                this->udpPkgCompErrCnt=0;
                //this->udpConnCnt=0;
                this->udpSendBytesCnt=0;
                this->udpRecvBytesCnt=0;
                this->udpPkgRecvTimeTotal=0;
                this->udpPkgRecvTimeMax=0;
                this->udpPkgRecvTimeMin=UINT_MAX;

                this->tcpPkgSendCnt=0;
                this->tcpPkgRecvCnt=0;
                this->tcpPkgCompErrCnt=0;
                //this->tcpConnCnt=0;
                this->tcpSendBytesCnt=0;
                this->tcpRecvBytesCnt=0;
                this->tcpPkgRecvTimeTotal=0;
                this->tcpPkgRecvTimeMax=0;
                this->tcpPkgRecvTimeMin=UINT_MAX;
            }

            void print(){
                std::cerr<<"[Upconn-UDP]pkg_snd_cnt="<<udpPkgSendCnt<<" pkg_recv_cnt="<<udpPkgRecvCnt<<" pkgCompErrorCnt="<<udpPkgCompErrCnt
                <<" conn_cnt="<<udpConnCnt<<" send_bytes_cnt="<<udpSendBytesCnt<<" recv_bytes_cnt="<<udpRecvBytesCnt<<" recv_time_total="<<udpPkgRecvTimeTotal
                <<" recv_time_max="<<udpPkgRecvTimeMax<<" recv_time_min="<<udpPkgRecvTimeMin<<std::endl;
            }

        }StUpStreamConnStatInfo;

        class CUpStreamConnSet{
        protected:
            CUpStreamConnSet(const CUpStreamConnSet& otherSet);
            const CUpStreamConnSet& operator=(const CUpStreamConnSet& otherSet);
        private:
            typedef struct ConnKey{
                uint32_t ip;//net byte order
                unsigned short port;
                uint64_t flow;
                unsigned short protocol;

                ConnKey():ip(0),port(0),flow(0),protocol(CONN_TYPE_TCP){

                }

                const ConnKey& operator=(const ConnKey& otherConnKey){
                    this->ip=otherConnKey.ip;
                    this->port=otherConnKey.port;
                    this->flow=otherConnKey.flow;
                    this->protocol=otherConnKey.protocol;

                    return *this;
                }
            }StConnKey;

            struct _StConnKey_Hash{
                uint32_t operator()(const StConnKey& connKey){
                    return jhash(&connKey,sizeof(uint32_t)+sizeof(unsigned short)*2+sizeof(uint64_t),1);
                }
            };

            struct _StConnKey_Equal{
                bool operator()(const StConnKey& key1,const StConnKey& key2){
                    return (key1.ip==key2.ip)&&
                            (key1.port==key2.port) &&
                            (key1.flow==key2.flow)&&
                            (key1.protocol==key2.protocol);
                }
            };

            typedef struct udp_header_t{
                size_t udpMsgLen;
                struct sockaddr_in udpAddr;
                struct timeval udpStartTime;
            }StUDPHeader;

            const static size_t DEFAULT_MAX_CONN=8000;
            const static size_t DEFAULT_RECV_BUFF_INIT_SIZE=4096;
            const static size_t DEFAULT_SEND_BUFF_INIT_SIZE=8192;
            const static size_t DEFAULT_UDP_SEND_MAX=1000;
            const static unsigned DEFAULT_UDP_MSG_TIMEOUT=2000;

            const static size_t RECV_BUFF_LEN=1<<16;
			const static size_t UDP_PKG_BUFF_LEN=1024*1024*64;

            StUpStreamConn* m_upStreamConnPool;
            void* m_upStreamConnPoolBaseAddr;
            void* m_upStreamConnPoolMaxAddr;
            MyContainers::CLinearHashTable<StConnKey,StUpStreamConn*,_StConnKey_Hash,_StConnKey_Equal> m_connTable;
            typedef MyContainers::CLinearHashTable<StConnKey,StUpStreamConn*,_StConnKey_Hash,_StConnKey_Equal>::iterator ConsistConnTableIterator;
            list_head_t m_freeConnList;
            list_head_t m_usedConnList;
            size_t m_maxConnCnt;
            size_t m_recvBuffInitSize;
            size_t m_sndBuffInitSize;
            size_t m_recvBuffMax;//当接收BUF积累到此大小时,认为连接异常,关闭连接(>0时启动)
            size_t m_sndBuffMax;//当发送BUF积累到此大小时,认为连接异常,关闭连接.(>0时启用)
            int m_epollFd;
            unsigned int m_udpSndCnt;//一次最多发送的udp包的个数
            unsigned int m_udpMsgTimeout;//UDP包在发送缓存中可以等待的最大时间(ms)超过此时间之后会被丢弃
            StUpStreamConnStatInfo m_statInfo;


            /* sendTCP:BASIC TCP SENDING OPERATION WITH UPSTREAMCONN's SOCKET,JUST SEND THE DATA BUFFER.FIRST
			 * UPDATE THE RECENT USED UPSTREAMCONN IN m_usedConnList AND THE _access_time IN UPSTREAMCONN
			 * @param upStreamConn:Conn object
			 * @param data:data buffer
			 * @param dataLen:data buffer length need to send
			 * @ret-param sentLen:the data length actually sent
			 * @return: 0:send success <0:send occured an error,return the -errno
			 */
            int sendTCP(StUpStreamConn* upStreamConn,const char* data,size_t dataLen,size_t& sentLen);

            /* receiveTCP:BASIC TCP RECEIVING OPERATION WITH UPSTREAMCONN's SOCKET
			 * @param upStreamConn:Conn Object
			 * @param dataBuf:data buffer to receive the data
			 * @param buffSize:data buffer size(max receiving bytes)
			 * @param recvLen:data actually received
			 * @return:0:receive success <0:receive occured an error,return the -errno
			 */
            int recvTCP(StUpStreamConn* upStreamConn,char* dataBuf,size_t buffSize,size_t& recvLen);

			/* sendUDP:BASIC UDP SEND OPERATION WITH UPSTREAMCONN's SOCKET
			 * @param upStreamConn:Conn object
			 * @param data:data buffer to send
			 * @param dataLen:the data length needs to send
			 * @param sentLen:the length that acturally sent
			 * @param to:the sockaddr that send to
			 * @param tolen:the size of sockaddr 
			 * @return:
			 * 	0:Success	
			 * 	-errno:if ::recvfrom return <0
			 */
            int sendUDP(StUpStreamConn* upStreamConn,const char* data,size_t dataLen,size_t& sentLen, const struct sockaddr &to,socklen_t tolen);

			/* RECVUDP:BASIC UDP RECEIVING OPERATION WITH UPSTREAMCONN's SOCKET
			 * @param upStreamConn:Conn object
			 * @param dataBuf:data buffer to receive the data
			 * @param buffSize:the size of data buffer
			 * @param recvLen:the length actually received
			 * @param from:the sockaddr that received
			 * @param fromLen:the length of from
			 * @return:
			 * 	0:Success	
			 * 	-errno:if ::recvfrom return <0
			 */
            int recvUDP(StUpStreamConn* upStreamConn,char* dataBuf,size_t buffSize,size_t& recvLen,struct sockaddr& from,socklen_t& fromLen);

        public:
            const static int ERROR_NEED_CLOSE=10000;
            const static int ERROR_NOT_FOUND=10001;
            const static int ERROR_NEED_SEND=10002;
            const static int ERROR_NEED_RECV=10003;
            const static int ERROR_FORCE_CLOSE=10004;
            const static int ERROR_NEED_PENDING=10005;
            const static int ERROR_MEM_ALLOC=10006;
            const static int ERROR_NEED_PENDING_NOTIFY=10007;
            const static int ERROR_RECVED=10008;//FOR UDP RECEIVE
            const static int ERROR_TRUNC=10009;//FOR UDP RECEIVE
            const static int ERROR_SEND_COMP=10010;//FOR UDP SEND

            CUpStreamConnSet(size_t maxConn):m_upStreamConnPool(NULL),m_upStreamConnPoolBaseAddr(NULL),m_upStreamConnPoolMaxAddr(NULL),
                                             m_connTable(maxConn*2),m_maxConnCnt(maxConn),
                                             m_recvBuffInitSize(DEFAULT_RECV_BUFF_INIT_SIZE),
                                             m_sndBuffInitSize(DEFAULT_SEND_BUFF_INIT_SIZE),
                                             m_recvBuffMax(0), m_sndBuffMax(0),m_epollFd(-1),m_udpSndCnt(DEFAULT_UDP_SEND_MAX),
                                             m_udpMsgTimeout(DEFAULT_UDP_MSG_TIMEOUT),m_statInfo()
            {
                m_upStreamConnPool=new (std::nothrow) StUpStreamConn[m_maxConnCnt];
                m_upStreamConnPoolBaseAddr=&m_upStreamConnPool[0];
                m_upStreamConnPoolMaxAddr=&m_upStreamConnPool[m_maxConnCnt-1];
                INIT_LIST_HEAD(&m_freeConnList);
                INIT_LIST_HEAD(&m_usedConnList);
            }

            ~CUpStreamConnSet(){
                StUpStreamConn* conn=NULL;
                list_head_t* tmp;
                list_for_each_entry_safe_l(conn,tmp,&m_usedConnList,_next){
                    closeConn(conn);
                }

                delete [] m_upStreamConnPool;
            }

            inline bool isInConnSet(void* ptr)const{
                return (ptr>=m_upStreamConnPoolBaseAddr)&&(ptr<=m_upStreamConnPoolMaxAddr);
            }

            void epollAdd(int epFd,int fd,void* user,int events){
                struct epoll_event ev;
                ev.events   = events | EPOLLERR;
                ev.data.ptr = user;

                epoll_ctl(epFd, EPOLL_CTL_ADD, fd, &ev);
            }
            void epollMod(int epFd,int fd,void* user,int events){
                struct epoll_event ev;
                ev.events   = events | EPOLLERR;
                ev.data.ptr = user;

                epoll_ctl(epFd, EPOLL_CTL_MOD, fd, &ev);
            }
            void epollDel(int epFd,int fd){
                struct epoll_event ev;
                //ev.data.u64 = fd ;

                epoll_ctl(epFd, EPOLL_CTL_DEL, fd, &ev);
            }

            enum{
                ADDCONNTCP_ERROR_NONE=0,
                ADDCONNTCP_ERROR_CONNECT=-1,
                ADDCONNTCP_ERROR_CONN_OVERLOAD=-2
            };
            int addConnTCP(const char* ip,unsigned short port,uint64_t sessionSeq,StUpStreamConn** upStreamConn);
            enum{
                ADDCONNUDP_ERROR_NONE=0,
                ADDCONNUDP_ERROR_SOCKET=-1,
                ADDCONNUDP_ERROR_CONN_OVERLOAD=-2
            };
			int addConnUDP(const char* ip,unsigned short port,StUpStreamConn** upStreamConn);

            void closeConn(StUpStreamConn* upStreamConn);

            /* sendOnlyCacheTCP:ONLY SEND THE WRITE CACHE OF upStreamConn
			 * @param upStreamConn
			 * @return 0:send complete <0:ERROR OCCURED,return one of ERROR_XXX
			 */
            int sendOnlyCacheTCP(StUpStreamConn* upStreamConn);
            /* sendWithCacheTCP:FIRST SEND THE WRITE CACHE IN UPSTREAMCONN,IF SEND COMPLETE,THEN
			 * SEND THE DATA BUFFER CONTENT
			 * @param upStreamConn:Conn Object
			 * @param data:data buffer
			 * @param dataLen:data buffer length need to send
			 * @return 1:send complete 0:send partial <0 ERROR OCCURED,return One Of ERROR_XXX
			 */
            int sendWithCacheTCP(StUpStreamConn* upStreamConn,const char* data,size_t dataLen);

            /* sendWithoutCacheTCP:IGNORE THE WRITE CACHE IN UPSTREAMCONN,JUST SIMPLELY CALL sendTCP,IF
			 * SENDING IS NOT COMPLETE,APPEND THE REST DATA TO UPSTREAMCONN's WRITE CACHE
			 * @param upStreamConn:Conn Object
			 * @param data:data buffer
			 * @param dataLen:data buffer length need to send
			 * @return 1:send complete 0:send partial <0 ERROR OCCURED,return One Of ERROR_XXX
			 */
            int sendWithoutCacheTCP(StUpStreamConn* upStreamConn,const char* data,size_t dataLen);


            /* recvUpstreamConnTCP:RECEIVING THE DATA WITH UPSTREAMCONN'S SOCKET,AND RESERVING
			 * THE DATA IN UPSTREAMCONN'S _readCache
			 * @param upStreamConn:Conn Object
			 * @return:0 receive success <0 ERROR OCCURED,return One of ERROR_XXX
			 */
            int recvConnDataTCP(StUpStreamConn* upStreamConn);

			/* sendOnlyCacheUDP:ONLY SEND THE WRITE CACHE OF upStreamConn VIA UDP
			 * @param upStreamConn:Conn Object
			 * @return: 
			 * 	-E_SEND_COMP:All data in cache sent
			 * 	-E_NEED_CLOSE:Error occured,need to close conn
			 * 	-E_NEED_SEND:EAGAIN occured,need continuing to send
			 */
            int sendOnlyCacheUDP(StUpStreamConn* upStreamConn);

			/* sendWithCacheUDP:FIRST SEND THE WRITE CACHE OF upStreamConn THEN SEND THE data buffer
			 * @param upStreamConn:Conn object
			 * @param data:udp data buffer
			 * @param dataLen:udp data length
			 * @param sentLen:the data length actually sent
			 * @param to:send destination
			 * @param tolen:the size of to
			 * @return:
			 * 	-ERROR_MEM_ALLOC:Malloc when append data to write cache buffer
			 * 	-ERROR_NEED_SEND:EAGAIN Occured,need to send again	
			 * 	-ERROR_NEED_CLOSE:Error occured,need to close conn
			 */
            int sendWithCacheUDP(StUpStreamConn* upStreamConn,const char* data,size_t dataLen,size_t& sentLen, const struct sockaddr &to, socklen_t tolen);

			/* recvConnDataUDP:RECEIVING THE DATA OF UPSTREAMCONN's SOCKET
			 * AND RESERVE DATA IN UPSTREAMCONN's READ CACHE
			 * @param upStreamConn:Conn object
			 * @return:
			 * 	-ERROR_TRUNC:data too large
			 * 	-ERROR_RECVED:received data successfully
			 * 	-ERROR_MEM_ALLOC:alloc _readcache failed
			 * 	
			 */
            int recvConnDataUDP(StUpStreamConn* upStreamConn);


            void setRecvBuffInitSize(size_t recvBuffInitSize){
                this->m_recvBuffInitSize=recvBuffInitSize;
            }
            void setRecvBuffMaxSize(size_t recvBuffMaxSize){
                this->m_recvBuffMax=recvBuffMaxSize;
            }
            void setSendBuffInitSize(size_t sendBuffInitSize){
                this->m_sndBuffInitSize=sendBuffInitSize;
            }
            void setSendBuffMaxSize(size_t sendBuffMaxSize){
                this->m_sndBuffMax=sendBuffMaxSize;
            }
            void setUdpSendMax(unsigned int udpSendMax){
                this->m_udpSndCnt=udpSendMax;
            }

            void setUdpMsgTimeout(unsigned int udpSendTimeout){
                this->m_udpMsgTimeout=udpSendTimeout;
            }
            int handleTCPIn(StUpStreamConn* upStreamConn);
            int handleTCPOut(StUpStreamConn* upStreamConn);
            /* getConnMessage:GET ONE COMPLETE MESSAGE FROM READ CACHE OF upStreamConn
			 * @param upStreamConn:Conn object
			 * @param messageBuffer:Buffer to store the complete message,the caller need to allocate it
			 * @param messageBufferLen:The max length of message buffer
			 * @param messageLen:The length of the complete message
			 * @return 0:get one complete message
			 * @return -ERROR_NEED_RECV:No complete message,need to receive
			 * @return -ERROR_NEED_CLOSE:Check complete failed,need to close this upStreamConn
			 */
            int getConnMessage(StUpStreamConn* upStreamConn,char* messageBuffer,size_t messageBufferLen,size_t& messageLen);
            int handleUDPIn(StUpStreamConn* upStreamConn);
            int handleUDPOut(StUpStreamConn* upStreamConn);
            void checkExpireConn(time_t accessDeadLine);
            int init(int epFd);

            void printUsedConnInfo()const;

            const static int SEND_FAIL_UNKNOWN=1000;
            const static int SEND_FAIL_OFM=1001;
            const static int SEND_FAIL_CONNECT_FAIL=1002;
            const static int SEND_FAIL_CONN_OVERLOAD=1003;
            const static int SEND_FAIL_INVALID_PARAM=1004;
			const static int SEND_FAIL_INVALID_UDP_PKG=1005;
			const static int SEND_FAIL_UDP_ERROR=1006;
            int netSendTcp(const char* ip, unsigned short port,const char* buff,size_t buffLen,check_complete_func completeFunc,uint64_t flow=0);
            int netSendUdp(const char* ip, unsigned short port,const char* buff,size_t buffLen,check_complete_func completeFunc=NULL);
            int closeUpstreamConn(const char* ip,unsigned short port,uint64_t flow,unsigned short protocol);
            const StUpStreamConnStatInfo& getStatInfo()const;
            void resetUpStreamConnStatInfo();

        };

    }
}


#endif //MCPP_UPCONNSET_H
