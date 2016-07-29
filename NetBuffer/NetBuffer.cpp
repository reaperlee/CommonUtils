//
// Created by 李兴 on 16/7/25.
//
#include "NetBuffer.h"

#include <stdlib.h>
#include <string.h>
#include <jemalloc/jemalloc.h>
#ifdef _UNITTEST
#include <assert.h>
#include <gtest/gtest.h>
#endif

using namespace francis_module;

int StNetBuffer::append(const char *data, uint32_t dataLen) {
	if(data==NULL){
		return -1;
	}

	if(_buffer==NULL){
		uint32_t allocSize=dataLen>_buffer_init_size?dataLen:_buffer_init_size;
		_buffer=(char*)malloc(allocSize);
		if(!_buffer){
			_data_offset=0;
			_current_data_len=0;
			_current_size=0;
			return -2;
		}
		_data_offset=0;
		_current_data_len=0;
		_current_size=allocSize;
	}

	if(dataLen+_data_offset+_current_data_len<=_current_size){
		memcpy(_buffer+_data_offset+_current_data_len,data,dataLen);
		_current_data_len+=dataLen;
	}
	else if(dataLen+_current_data_len<=_current_size){
		memmove(_buffer,_buffer+_data_offset,_current_data_len);
		memcpy(_buffer+_current_data_len,data,dataLen);

		_data_offset=0;
		_current_data_len+=dataLen;
	}
	else{
		uint32_t newSize=calculateNewSize(dataLen);

		_buffer=(char*)realloc(_buffer,newSize);
		if(!_buffer){
			return -3;
		}
		_current_size=newSize;

		memmove(_buffer,_buffer+_data_offset,_current_data_len);
		memcpy(_buffer+_current_data_len,data,dataLen);

		_data_offset=0;
		_current_data_len+=dataLen;
	}

	return 0;
}

void StNetBuffer::skip(uint32_t skipLen) {
	if(!_buffer){
		return;
	}

	if(skipLen<_current_data_len){
		_data_offset+=skipLen;
		_current_data_len-=skipLen;
	}
	else{
		//TODO:resize to init size?
		cleanData();
	}
}

uint32_t StNetBuffer::calculateNewSize(uint32_t appendSize) {
	uint32_t linearSize=_current_data_len+appendSize;
	if(_current_size<LARGE_MEM_THRESHOLD){
		return linearSize;
	}

	uint32_t exponentSize=_current_data_len+_current_data_len/LARGE_INCREMENT_PERCENTAGE;
	return exponentSize>linearSize?exponentSize:linearSize;
}

/*
inline const char* StNetBuffer::getCurrentData() {
	return (_buffer+_data_offset);
}

inline uint32_t StNetBuffer::getCurrentDataLen() {
	return _current_data_len;
}

uint32_t StNetBuffer::getCurrentSize() {
	return _current_size;
}

*/
void StNetBuffer::cleanData() {
	_data_offset=0;
	_current_data_len=0;
}

void StNetBuffer::reinit() {
	if(_buffer){
		free(_buffer);
		_buffer=NULL;
	}
	_data_offset=0;
	_current_data_len=0;
	_current_size=0;
}

StNetBuffer::~net_buffer_t() {
	if(_buffer){
		free(_buffer);
	}
}

#ifdef _UNITTEST

const char* getRandomStr(uint32_t &randomStrSize){
	static char buffer[8192]={0};
	if(randomStrSize>sizeof(buffer)){
		randomStrSize=sizeof(buffer);
	}

	for(uint32_t i=0;i<randomStrSize;i++){
		buffer[i]=random()%26+'a';
	}

	return buffer;
}

void paddingRandomChar(char** buffer,uint32_t bufferSize){
	*buffer=(char*)malloc(bufferSize);
	assert(*buffer!=NULL);

	for(uint32_t i=0;i<bufferSize;i++){
		*((*buffer)+i)=random()%26+'a';
	}
}

TEST(StNetBuffer_Construct,StNetBuffer){
	StNetBuffer netBuffer;
	EXPECT_EQ((void*)netBuffer.getCurrentData(),(void*)NULL);
	EXPECT_EQ(netBuffer.getCurrentDataLen(),0);
	EXPECT_EQ(netBuffer.getCurrentSize(),0);
	EXPECT_EQ(netBuffer._buffer_init_size,0);
	EXPECT_EQ(netBuffer._data_offset,0);

	StNetBuffer netBuffer1(4096);
	EXPECT_EQ((void*)netBuffer1.getCurrentData(),(void*)NULL);
	EXPECT_EQ(netBuffer1.getCurrentDataLen(),0);
	EXPECT_EQ(netBuffer1.getCurrentSize(),0);
	EXPECT_EQ(netBuffer1._buffer_init_size,4096);
	EXPECT_EQ(netBuffer1._data_offset,0);
}

TEST(StNetBuffer_Append_Case1,StNetBuffer){
	StNetBuffer netBuffer;

	int ret=netBuffer.append(NULL,0);
	EXPECT_EQ(ret,-1);
}

TEST(StNetBuffer_Append_Case2,StNetBuffer){
	StNetBuffer netBuffer(4096);

	uint32_t testStrLen=random()%4096+1;
	const char* testStr=getRandomStr(testStrLen);
	assert((netBuffer.append(testStr,testStrLen)==0));

	EXPECT_NE(netBuffer.getCurrentData(),(char*)NULL);
	EXPECT_TRUE(strncmp(testStr,netBuffer.getCurrentData(),testStrLen)==0);
	EXPECT_EQ(netBuffer.getCurrentSize(),4096);
	EXPECT_EQ(netBuffer.getCurrentDataLen(),testStrLen);
	netBuffer.reinit();

}

TEST(StNetBuffer_Append_Case3,StNetBuffer){
	StNetBuffer	netBuffer(16);

	uint32_t testStrLen=random()%4096+16;
	const char* testStr=getRandomStr(testStrLen);
	int ret=netBuffer.append(testStr,testStrLen);
	assert((ret==0));

	//EXPECT_EQ(ret,0);
	EXPECT_NE(netBuffer._buffer,(char*)NULL);
	EXPECT_EQ(netBuffer._current_data_len,testStrLen);
	EXPECT_EQ(netBuffer._current_size,testStrLen);
	EXPECT_EQ(netBuffer._data_offset,0);
	EXPECT_EQ(netBuffer._buffer_init_size,16);
	netBuffer.reinit();

}

TEST(StNetBuffer_Append_Case4,StNetBuffer){
	StNetBuffer netBuffer(128);
	uint32_t testStrLen0=64;
	char* testStr0=NULL;
	paddingRandomChar(&testStr0,testStrLen0);
	int ret=netBuffer.append(testStr0,testStrLen0);
	//EXPECT_EQ(ret,0);

	EXPECT_EQ(ret,0);

	char* testStr1=NULL;
	uint32_t testStrLen1=32;
	paddingRandomChar(&testStr1,testStrLen1);
	ret=netBuffer.append(testStr1,testStrLen1);

	EXPECT_EQ(ret,0);
	EXPECT_EQ(netBuffer._current_data_len,32+64);
	EXPECT_EQ(netBuffer._buffer_init_size,128);
	EXPECT_EQ(netBuffer._current_size,netBuffer._buffer_init_size);
	EXPECT_EQ(netBuffer._data_offset,0);

	const char* bufferCache=netBuffer.getCurrentData();

	netBuffer.skip(testStrLen0);

	EXPECT_EQ(netBuffer._data_offset,testStrLen0);
	EXPECT_EQ(strncmp(netBuffer.getCurrentData(),testStr1,testStrLen1),0);

	char* testStr2=NULL;
	uint32_t testStrLen2=128-32;
	paddingRandomChar(&testStr2,testStrLen2);
	ret=netBuffer.append(testStr2,testStrLen2);
	EXPECT_EQ(ret,0);
	EXPECT_EQ(bufferCache,netBuffer.getCurrentData());
	EXPECT_EQ(netBuffer._current_data_len,128);
	EXPECT_EQ(netBuffer._current_size,128);
	EXPECT_EQ(netBuffer._data_offset,0);

	netBuffer.skip(testStrLen1);

	EXPECT_EQ(strncmp(netBuffer.getCurrentData(),testStr2,testStrLen2),0);

	char* testStr3=NULL;
	uint32_t testStrLen3=33;
	paddingRandomChar(&testStr3,testStrLen3);
	ret=netBuffer.append(testStr3,testStrLen3);
	EXPECT_EQ(ret,0);

	EXPECT_NE(bufferCache,netBuffer.getCurrentData());
	EXPECT_EQ(netBuffer._current_data_len,129);
	EXPECT_EQ(netBuffer._current_size,129);
	EXPECT_EQ(netBuffer._buffer_init_size,128);
	EXPECT_EQ(netBuffer._data_offset,0);
	EXPECT_EQ(strncmp(netBuffer.getCurrentData(),testStr2,testStrLen2),0);
	netBuffer.skip(testStrLen2);
	EXPECT_EQ(netBuffer._data_offset,testStrLen2);
	netBuffer.skip(testStrLen3);
	//should invoke cleanData
	EXPECT_EQ(netBuffer._data_offset,0);
	EXPECT_EQ(netBuffer._current_data_len,0);

	free(testStr0);
	free(testStr1);
	free(testStr2);
	free(testStr3);
}

TEST(StNetBuffer_Reinit,StNetBuffer){
	StNetBuffer netBuffer(4096);

	uint32_t testStrLen=random()%4096;
	const char* testStr=getRandomStr(testStrLen);
	assert((netBuffer.append(testStr,testStrLen)>=0));

	netBuffer.reinit();

	EXPECT_EQ((void*)netBuffer._buffer,(void*)NULL);
	EXPECT_EQ(netBuffer._buffer_init_size,4096);
	EXPECT_EQ(netBuffer._current_size,0);
	EXPECT_EQ(netBuffer._current_data_len,0);
	EXPECT_EQ(netBuffer._data_offset,0);

	StNetBuffer netBuffer1;
	netBuffer1.reinit();
	EXPECT_EQ((void*)netBuffer1._buffer,(void*)NULL);
	EXPECT_EQ(netBuffer1._buffer_init_size,0);
	EXPECT_EQ(netBuffer1._current_size,0);
	EXPECT_EQ(netBuffer1._current_data_len,0);
	EXPECT_EQ(netBuffer1._data_offset,0);
}

int main(int argc,char* argv[]){
	srand(time(NULL));
	testing::InitGoogleTest(&argc,argv);
	int ret=RUN_ALL_TESTS();
}


#endif
