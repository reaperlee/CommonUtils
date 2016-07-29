CC=g++
CFLAG=-Werror -g -D_UNITTEST

LIBBASE=/data/francisxli/common-libs

INC=-I$(LIBBASE)/gtest/include -I$(LIBBASE)/jemalloc/include -I../
LIB=-pthread -lrt -L$(LIBBASE)/gtest/lib -lgtest -L$(LIBBASE)/jemalloc/lib -ljemalloc
BIN=

OBJ_SHMQUEUE_WRITER=../SysVSem/SysVSem.o \
					../SysVShareMem/SysVShm.o \
					cycle_queue.o \
					ShmQueue.o \
					ShmQueueTestWriter.o

OBJ_SHMQUEUE_READER=../SysVSem/SysVSem.o \
					../SysVShareMem/SysVShm.o \
					cycle_queue.o \
					ShmQueue.o \
					ShmQueueTestReader.o

READER_TESTBIN=ShmQueueTestReader
WRITER_TESTBIN=ShmQueueTestWriter
BIN+=$(READER_TESTBIN)
BIN+=$(WRITER_TESTBIN)


all:$(BIN)
$(READER_TESTBIN):$(OBJ_SHMQUEUE_READER)
	$(CC) $(CFLAG) -o $@ $(OBJ_SHMQUEUE_READER) $(LIB)
$(WRITER_TESTBIN):$(OBJ_SHMQUEUE_WRITER)
	$(CC) $(CFLAG) -o $@ $(OBJ_SHMQUEUE_WRITER) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(INC) -c -o $@ $<
clean:
	rm -f $(BIN)
	rm -f ./*.o

