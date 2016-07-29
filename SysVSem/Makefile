CC=g++
CFLAG=-Werror -g -D_UNITTEST

LIBBASE=/data/francisxli/common-libs

INC=-I$(LIBBASE)/gtest/include -I$(LIBBASE)/jemalloc/include
LIB=-pthread -lrt -L$(LIBBASE)/gtest/lib -lgtest -L$(LIBBASE)/jemalloc/lib -ljemalloc
BIN=

OBJ_SYSVSEM_TESTMAIN=SysVSem.o \
					 SysVSemTestmain.o

SYSVSEM_TESTBIN=SysVSemTestmain
BIN+=$(SYSVSEM_TESTBIN)


all:$(BIN)
$(SYSVSEM_TESTBIN):$(OBJ_SYSVSEM_TESTMAIN)
	$(CC) $(CFLAG) -o $@ $(OBJ_SYSVSEM_TESTMAIN) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(INC) -c -o $@ $<
clean:
	rm -f $(BIN)
	rm -f ./*.o

