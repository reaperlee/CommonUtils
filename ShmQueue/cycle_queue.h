#ifndef __CYCLE_QUEUE_H__
#define __CYCLE_QUEUE_H__

/*
	wdd
	2011-06
*/

#ifdef __cplusplus
extern "C" {
#endif


#ifndef mb
#define mb() __sync_synchronize()
#endif


#define ERR_RET_VAL -1
#define ERR_Q_FULL -2
#define ERR_Q_EMPTY -3
#define ERR_BUF_INSUFF -4


//struct cycle_queue;
typedef volatile unsigned long atomic_ulong;
struct cycle_queue
{
	char magic[8];
	unsigned long capacity;
	atomic_ulong head;
	atomic_ulong tail;
	char data[0];
}__attribute__ ((__packed__));

struct cycle_queue* cycle_queue_init(unsigned char* mem, unsigned long sz);
struct cycle_queue* cycle_queue_attach(unsigned char* mem, unsigned long sz);


int cycle_enqueue(struct cycle_queue* q, const char* buf, int sz);

int cycle_dequeue(struct cycle_queue* q, char* outbuf, int* inoutsz);

int cycle_queue_peek(struct cycle_queue* q, char* outbuf, int* inoutsz);

int cycle_queue_pop(struct cycle_queue* q);

int cycle_queue_full(struct cycle_queue* q, int sz);


#ifdef __cplusplus
}
#endif

#endif


