CC=g++
CFLAG=-Werror -g -D_UNITTEST

LIBBASE=/data/francisxli/common-libs

INC=-I$(LIBBASE)/gtest/include -I$(LIBBASE)/jemalloc/include
LIB=-pthread -lrt -L$(LIBBASE)/gtest/lib -lgtest -L$(LIBBASE)/jemalloc/lib -ljemalloc
BIN=

OBJ_SYSVSHM_TESTMAIN=SysVShm.o \
					 SysVShmTestmain.o

SYSVSHM_TESTBIN=SysVShmTestmain
BIN+=$(SYSVSHM_TESTBIN)


all:$(BIN)
$(SYSVSHM_TESTBIN):$(OBJ_SYSVSHM_TESTMAIN)
	$(CC) $(CFLAG) -o $@ $(OBJ_SYSVSHM_TESTMAIN) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(INC) -c -o $@ $<
clean:
	rm -f $(BIN)
	rm -f ./*.o

