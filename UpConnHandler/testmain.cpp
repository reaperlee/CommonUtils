//
// Created by 李兴 on 16/5/8.
//

#include <iostream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "libhttp/http_api.h"
#include "UpConnSet.h"

using namespace std;
using namespace tfc::new_module;

struct DummyMsg_t{
    string ip;
    unsigned short port;
    string extMsg;
}StDummyMsg;

pthread_mutex_t g_queueMutex;
int g_epFd=-1;
pthread_t g_inputThread;
pthread_t g_autoTestThread;
int g_epollFD=-1;
CUpStreamConnSet* g_upStreamConnSet;


char dataBuffer[1024*1024*256];
size_t dataBufferLen=sizeof(dataBuffer);

char udpDataBuffer[1024*1024];
size_t udpDataBufferLen=sizeof(udpDataBuffer);

char lastSndPkg[1024*1024];
int lastSndPkgLen=0;

int httpCheckComplete(void* buffer,unsigned int bufferLen){
    return http_check_complete(buffer,bufferLen,NULL,NULL);
}

void handleUpStreamConnTCP(StUpStreamConn* upStreamConn,struct epoll_event* epollEvent){
    int ret=0;
    uint32_t epollFlag=epollEvent->events;
    if(upStreamConn->_fd<0){
		cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]fd<0!"<<endl;
        return;
    }

    if(!(epollFlag&(EPOLLIN|EPOLLOUT))){
        cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Not EPOLLIN NOR EPOLL OUT:"<<epollEvent<<endl;
		g_upStreamConnSet->closeConn(upStreamConn);
        return;
    }

    if(epollFlag&EPOLLOUT){
        //handle socket send
        ret=g_upStreamConnSet->handleTCPOut(upStreamConn);
        if(ret<0){
            cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]handleTCPOut Failed:ret="<<ret<<endl;
            return;
        }
    }

    if(!(epollFlag&EPOLLIN)){
        //no need to continue
        return;
    }

    //handle socket recv
    if(g_upStreamConnSet->handleTCPIn(upStreamConn)<0){
        return;
    }

    //get message
    while(1){
        size_t packLen=0;
        ret=g_upStreamConnSet->getConnMessage(upStreamConn,dataBuffer,dataBufferLen,packLen);
		dataBuffer[packLen]=0;
        if(ret==0){
            cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Get One Complete Message(package_len="<<packLen<<"):\n"<<dataBuffer<<endl;
        }
        else if(ret==-CUpStreamConnSet::ERROR_NEED_RECV){
            break;
        }
        else if(ret==-CUpStreamConnSet::ERROR_NEED_CLOSE){
            g_upStreamConnSet->closeConn(upStreamConn);
            break;
        }

    }

    if(upStreamConn->_finclose){
        g_upStreamConnSet->closeConn(upStreamConn);
    }

    return;
}

void handleUpStreamConnUDP(StUpStreamConn* conn,struct epoll_event* epollEvent){
	uint32_t epollFlag=epollEvent->events;
	if(!(epollFlag&(EPOLLIN|EPOLLOUT))){
		cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]epoll error!"<<endl;
		g_upStreamConnSet->closeConn(conn);
		return;
	}

	if(epollFlag&EPOLLIN){
		if(g_upStreamConnSet->handleUDPIn(conn)<0){
			return;
		}
	}

	if(epollFlag&EPOLLOUT){
		if(g_upStreamConnSet->handleUDPOut(conn)<0){
			return;
		}
	}

	while(1){
		size_t messageLen=0;
		int ret=g_upStreamConnSet->getConnMessage(conn,udpDataBuffer,udpDataBufferLen,messageLen);
		udpDataBuffer[messageLen]=0;
		if(ret==0){
            cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Get One Complete UDP Message(package_len="<<messageLen<<"):\n"<<udpDataBuffer<<endl;
			if(strncmp(udpDataBuffer,lastSndPkg,messageLen)!=0){
				cerr<<"["<<__FUNCTION__<<":"<<__LINE__<<"]Echo Error!"<<endl;
			}
		}
		else if(ret==-CUpStreamConnSet::ERROR_NEED_RECV){
			break;
		}
		else if(ret==-CUpStreamConnSet::ERROR_NEED_CLOSE){
			g_upStreamConnSet->closeConn(conn);
			break;
		}
		else{
			//should not be here!
			g_upStreamConnSet->closeConn(conn);
			break;
		}
	}
    return;
}

char* randStr(size_t len){
    const static unsigned int BUFFER_LEN=4096;
    static char buffer[BUFFER_LEN]={0};

    if(len>=BUFFER_LEN){
        len=BUFFER_LEN-1;
    }

    size_t i=0;
    for(i=0;i<len;i++){
        buffer[i]='A'+rand()%26;
    }
    buffer[i]='\0';

    return buffer;
}

void* autoTestThread(void* param){
    const char* destIP="10.177.137.176";
    const unsigned destPort=15000;


    static char httpPackBuffer[8192]={0};
    while(true){
        unsigned long int seq=rand()%1000;
        if(seq<=100){
            seq=0;
        }
        int httpPackBufferLen=0;

        char* data=randStr(rand()%4096);

        httpPackBufferLen+=snprintf(httpPackBuffer+httpPackBufferLen,sizeof(httpPackBuffer),"POST / HTTP/1.1\r\n");
        httpPackBufferLen+=snprintf(httpPackBuffer+httpPackBufferLen,sizeof(httpPackBuffer),"User-Agent:UpConnSetDemo\r\n");
        httpPackBufferLen+=snprintf(httpPackBuffer+httpPackBufferLen,sizeof(httpPackBuffer),"Content-Length:%d\r\n",strlen(data));
        httpPackBufferLen+=snprintf(httpPackBuffer+httpPackBufferLen,sizeof(httpPackBuffer),"\r\n%s",data);

        int ret=g_upStreamConnSet->netSendTcp(destIP,destPort,httpPackBuffer,httpPackBufferLen,httpCheckComplete,seq);
        if(ret!=0){
            cerr<<"netSendTcpFailed:ret="<<ret<<endl;
        }
        usleep(500000);
    }

}

void* termInputThread(void *param){
    char lineBuffer[4096]={0};
    while(fgets(lineBuffer,sizeof(lineBuffer)-1,stdin)){
        char* ip=NULL;
        char* port=NULL;
        char* protocolType=NULL;
        char* seq=NULL;
        char* data=NULL;

        size_t lineSize=strlen(lineBuffer);
        lineBuffer[lineSize-1]='\0';//trim \n
        if(lineBuffer[0]=='#'){
            //control msg
            if(strlen(lineBuffer)==1){

            }
            char* command=&lineBuffer[1];
            if(strcasecmp(command,"printconn")==0){
                g_upStreamConnSet->printUsedConnInfo();
            }
            else{
                cerr<<"Unknown Command:"<<command<<endl;
            }
        }
        else{
            //<ip>:<port>:<protocol-type>:<seq>:<data>
            size_t dataSegBegin=0;
            size_t dataSegEnd=0;

            enum{
                BEGIN,
                FOUND_FIRST_COLON,
                FOUND_SECOND_COLON,
                FOUND_THIRD_COLON,
                FOUND_FOUTH_COLON
            };
            int state=BEGIN;
            for(unsigned int i=0;i<lineSize;i++){
                switch (state){
                    case BEGIN:
                        if(lineBuffer[i]==':'){
                            state=FOUND_FIRST_COLON;
                            lineBuffer[i]='\0';
                            ip=&lineBuffer[dataSegBegin];
                            dataSegBegin=i+1;
                            break;
                        }
                        break;
                    case FOUND_FIRST_COLON:
                        if(lineBuffer[i]==':'){
                            state=FOUND_SECOND_COLON;
                            lineBuffer[i]='\0';
                            port=&lineBuffer[dataSegBegin];
                            dataSegBegin=i+1;
                            break;
                        }
                        break;
                    case FOUND_SECOND_COLON:
                        if(lineBuffer[i]==':'){
                            state=FOUND_THIRD_COLON;
                            lineBuffer[i]='\0';
                            protocolType=&lineBuffer[dataSegBegin];
                            dataSegBegin=i+1;
                            break;
                        }
                        break;
                    case FOUND_THIRD_COLON:
                        if(lineBuffer[i]==':'){
                            state=FOUND_FOUTH_COLON;
                            lineBuffer[i]='\0';
                            seq=&lineBuffer[dataSegBegin];
                            dataSegBegin=i+1;
                            break;
                        }
                        break;
                    case FOUND_FOUTH_COLON:
                        if(lineBuffer[i]=='\0'){
                            data=&lineBuffer[dataSegBegin];
                        }
                        break;
                    default:
                        cerr<<"UNKNOWN STATE:"<<state<<endl;
                        break;
                }
            }
            cerr<<"[IP="<<ip<<" PORT="<<port<<" PROTOCOL="<<protocolType<<" SEQ="<<seq<<" DATA="<<data<<"]"<<endl;
            if(strcasecmp(protocolType,"tcp")==0){
				char httpPackBuffer[4096]={0};
				int len=0;
				len+=snprintf(httpPackBuffer+len,sizeof(httpPackBuffer),"POST / HTTP/1.1\r\n");
				len+=snprintf(httpPackBuffer+len,sizeof(httpPackBuffer),"User-Agent:UpConnSetDemo\r\n");
				len+=snprintf(httpPackBuffer+len,sizeof(httpPackBuffer),"Content-Length:%d\r\n\r\n",strlen(data));
				len+=snprintf(httpPackBuffer+len,sizeof(httpPackBuffer),"%s",data);
                g_upStreamConnSet->netSendTcp(ip,atoi(port),httpPackBuffer,len,httpCheckComplete,atol(seq));
            }
			else if(strcasecmp(protocolType,"udp")==0){
				lastSndPkgLen=snprintf(lastSndPkg,sizeof(lastSndPkg),"[%d][%s]",rand(),data);
				g_upStreamConnSet->netSendUdp(ip,atoi(port),lastSndPkg,lastSndPkgLen);
			}
			else{
				cerr<<"Unknown Protocol:"<<protocolType<<endl;
			}
        }
    }
}

int main(int argc,char* argv[]){
	int a=1;
    bool autoMode=false;
    if(argc==2){
        autoMode=true;
    }
    srand(time(NULL));
    char inputLine[1024]={0};

    int tcpTimeout=120;//seconds

    g_epollFD=epoll_create1(EPOLL_CLOEXEC);
    if(g_epollFD<0){
        cerr<<"Init Epoll Failed!"<<endl;
        return 1;
    }

    g_upStreamConnSet=new CUpStreamConnSet(10000);
    if(!g_upStreamConnSet){
        cerr<<"Init UpStreamConnSet Failed!"<<endl;
        return 1;
    }

    g_upStreamConnSet->init(g_epollFD);

    pthread_mutex_init(&g_queueMutex,NULL);

    int ret;
    if(autoMode){
        ret=pthread_create(&g_autoTestThread,NULL,autoTestThread,NULL);
        if(ret!=0){
            cerr<<"Create Auto Test Thread Failed!"<<endl;
            return 1;
        }
    }
    else{
        ret=pthread_create(&g_inputThread,NULL,termInputThread,NULL);
        if(ret!=0){
            cerr<<"Create Input Thread Failed!"<<endl;
            return 1;
        }

    }

    time_t currentTime=time(NULL);
    time_t lastCheckExpireTS=currentTime;
    while(1){
        struct epoll_event epollEvents[1024];

        int eventCnt=epoll_wait(g_epollFD,epollEvents,1024,10);
        if(eventCnt<0){
            cerr<<"epoll_wait failed:"<<strerror(errno)<<endl;
            return 1;
        }

        for(int i=0;i<eventCnt;i++){
            void* eventData=NULL;
            eventData=epollEvents[i].data.ptr;

            if(g_upStreamConnSet->isInConnSet(eventData)){
                StUpStreamConn* upStreamConn=(StUpStreamConn*)eventData;
                if(upStreamConn->_type==CONN_TYPE_TCP){
                    handleUpStreamConnTCP(upStreamConn,&epollEvents[i]);
                }else if(upStreamConn->_type==CONN_TYPE_UDP){
                    handleUpStreamConnUDP(upStreamConn,&epollEvents[i]);
                }
                else{
                    cerr<<"Unknown _type:"<<upStreamConn->_type<<" Ignore!"<<endl;
                    continue;
                }

            }
        }
        currentTime=time(NULL);
        if(currentTime-lastCheckExpireTS>tcpTimeout/4){
            g_upStreamConnSet->checkExpireConn(currentTime-tcpTimeout);
            lastCheckExpireTS=currentTime;
        }


    }

    close(g_epollFD);
}


