//
// Created by 李兴 on 16/7/25.
//

#ifndef COMMONTOOLS_NETBUFFER_H
#define COMMONTOOLS_NETBUFFER_H

#include <stdint.h>
#include <jemalloc/jemalloc.h>

namespace francis_module{
	typedef struct net_buffer_t{
		const static uint32_t LARGE_MEM_THRESHOLD=1024*1024;
		const static uint32_t LARGE_INCREMENT_PERCENTAGE=10;

		char* _buffer;
		uint32_t _buffer_init_size;
		uint32_t _current_size;
		uint32_t _current_data_len;
		uint32_t _data_offset;

		net_buffer_t():_buffer(NULL),_buffer_init_size(0),_current_data_len(0),_data_offset(0){

		}

		net_buffer_t(uint32_t initSize):_buffer(NULL),_buffer_init_size(initSize),_current_size(0),_current_data_len(0),_data_offset(0){

		}

		~net_buffer_t();

		int append(const char* data,uint32_t dataLen);
		int append(const char* data0,uint32_t dataLen0,const char* data1,uint32_t dataLen1);
		void skip(uint32_t skipLen);
		void reinit();
		uint32_t calculateNewSize(uint32_t appendSize);

		inline void cleanData();
		inline const char* getCurrentData(){
			return (_buffer+_data_offset);
		}
		inline uint32_t getCurrentDataLen(){
			return _current_data_len;
		}
		inline uint32_t getCurrentSize(){
			return _current_size;
		}
	}StNetBuffer;
}

#endif //COMMONTOOLS_NETBUFFER_H
