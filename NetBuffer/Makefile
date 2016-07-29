CC=g++
CFLAG=-Werror -g -D_UNITTEST

LIBBASE=/data/francisxli/common-libs

INC=-I$(LIBBASE)/gtest/include -I$(LIBBASE)/jemalloc/include
LIB=-pthread -lrt -L$(LIBBASE)/gtest/lib -lgtest -L$(LIBBASE)/jemalloc/lib -ljemalloc

OBJ_NETBUFFER_UNITTEST=NetBuffer.o

NETBUFFER_UNITTESTBIN=NetBufferUnittest

BIN=$(NETBUFFER_UNITTESTBIN)

all:$(BIN)
$(NETBUFFER_UNITTESTBIN):$(OBJ_NETBUFFER_UNITTEST)
	$(CC) $(CFLAG) -o $@ $(OBJ_NETBUFFER_UNITTEST) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(INC) -c -o $@ $<
clean:
	rm -f $(BIN)
	rm -f ./*.o

