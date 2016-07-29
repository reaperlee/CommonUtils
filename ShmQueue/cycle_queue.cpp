
/*
	wdd
	2011-06
*/


#include <stdlib.h>
#include <string.h>

#include "cycle_queue.h"


#define CQ_MAGIC "CycQMem"
#define MIN_CQ_SZ 64



//TODO:增加剩余长度的统计


struct cycle_queue* cycle_queue_init(unsigned char* mem, unsigned long sz)
{
	struct cycle_queue* q;

	if(sz < MIN_CQ_SZ)
	{
		return 0;
	}

	q = (struct cycle_queue*)mem;
	if(!q)
	{
		q = (struct cycle_queue*)malloc(sizeof(struct cycle_queue)+sz);
		if(!q)
		{
			return 0;
		}
	}

	strcpy(q->magic, CQ_MAGIC);
	q->head = 0;
	q->tail = 0;
	q->capacity = sz;
	
	return q;
}

struct cycle_queue* cycle_queue_attach(unsigned char* mem, unsigned long sz)
{
	struct cycle_queue* q;

	if(!mem)
	{
		return 0;
	}

	q = (struct cycle_queue*)mem;

	if(strcmp(q->magic, CQ_MAGIC) ||
		(sz != q->capacity) || 
		(q->head >= q->capacity) || 
		(q->tail >= q->capacity))
	{
		return 0;
	}

	return q;	
}

int cycle_enqueue(struct cycle_queue* q, const char* buf, int sz)
{
	unsigned long len, pos;
		
	if(q && buf && (sz >0))
	{
		len = (q->tail-q->head+q->capacity)%q->capacity;
		if(!len)
		{
			len = q->capacity;
		}
		if(len < (unsigned long)(sizeof(int)+sz+1))
		{
			/*full*/
			return ERR_Q_FULL;
		}

		q->data[q->head] = *(char*)&sz;
		q->data[(q->head+1)%q->capacity] = *((char*)&sz+1);
		q->data[(q->head+2)%q->capacity] = *((char*)&sz+2);
		q->data[(q->head+3)%q->capacity] = *((char*)&sz+3);

		pos = (q->head+4)%q->capacity;
		len = q->capacity-pos;

		if(len >= (unsigned long)sz)
		{
			memcpy(&q->data[pos], buf, sz);
		}
		else
		{
			memcpy(&q->data[pos], buf, len);
			memcpy(&q->data[0], buf+len, (unsigned long)sz-len);
		}

		mb();

		q->head = (q->head+sizeof(int)+sz)%q->capacity;

		return 0;
	}

	return ERR_RET_VAL;
}

int cycle_dequeue(struct cycle_queue* q, char* outbuf, int* inoutsz)
{
	int sz;
	unsigned long pos, len;

	if(q && outbuf && inoutsz)
	{
		if(q->tail == q->head)
		{
			/*empty*/
			return ERR_Q_EMPTY;
		}

		*(char*)&sz = q->data[q->tail];
		*((char*)&sz+1) = q->data[(q->tail+1)%q->capacity];
		*((char*)&sz+2) = q->data[(q->tail+2)%q->capacity];
		*((char*)&sz+3) = q->data[(q->tail+3)%q->capacity];

		if(sz <= *inoutsz)
		{
			*inoutsz = sz;

			pos = (q->tail+4)%q->capacity;
			len = q->capacity-pos;

			if(len >= (unsigned long)sz)
			{
				memcpy(outbuf, &q->data[pos], sz);
			}
			else
			{
				memcpy(outbuf, &q->data[pos], len);
				memcpy(outbuf+len, &q->data[0], (unsigned long)sz-len);		
			}
			
			mb();

			q->tail = (q->tail+sizeof(int)+sz)%q->capacity;
			return 0;
		}
		else
		{
			*inoutsz = sz;
			return ERR_BUF_INSUFF;
		}
	}

	return ERR_RET_VAL;
}

int cycle_queue_peek(struct cycle_queue* q, char* outbuf, int* inoutsz)
{
	int sz;
	unsigned long pos, len;

	if(q && outbuf && inoutsz)
	{
		if(q->tail == q->head)
		{
			/*empty*/
			return ERR_Q_EMPTY;
		}

		*(char*)&sz = q->data[q->tail];
		*((char*)&sz+1) = q->data[(q->tail+1)%q->capacity];
		*((char*)&sz+2) = q->data[(q->tail+2)%q->capacity];
		*((char*)&sz+3) = q->data[(q->tail+3)%q->capacity];

		if(sz <= *inoutsz)
		{
			*inoutsz = sz;

			pos = (q->tail+4)%q->capacity;
			len = q->capacity-pos;

			if(len >= (unsigned long)sz)
			{
				memcpy(outbuf, &q->data[pos], sz);
			}
			else
			{
				memcpy(outbuf, &q->data[pos], len);
				memcpy(outbuf+len, &q->data[0], (unsigned long)sz-len);		
			}

			return 0;
		}
		else
		{
			*inoutsz = sz;
			return ERR_BUF_INSUFF;
		}
	}

	return ERR_RET_VAL;
}

int cycle_queue_pop(struct cycle_queue* q)
{
	int sz;

	if(q)
	{
		if(q->tail == q->head)
		{
			/*empty*/
			return 0;
		}

		*(char*)&sz = q->data[q->tail];
		*((char*)&sz+1) = q->data[(q->tail+1)%q->capacity];
		*((char*)&sz+2) = q->data[(q->tail+2)%q->capacity];
		*((char*)&sz+3) = q->data[(q->tail+3)%q->capacity];

		q->tail = (q->tail+sizeof(int)+sz)%q->capacity;

		return 0;
	}

	return ERR_RET_VAL;
}

int cycle_queue_full(struct cycle_queue* q, int sz)
{
	if(q)
	{
		unsigned long len = (q->tail-q->head+q->capacity)%q->capacity;
		if(!len)
		{
			len = q->capacity;
		}
		if(len < (unsigned long)(sizeof(int)+sz+1))
		{
			/*full*/
			return 1;
		}
	}

	return 0;
}


