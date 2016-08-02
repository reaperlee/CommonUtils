CC=g++
CFLAG=-Wno-invalid-offsetof -Werror -g
CFLAG+=-D_MODULE_DEBUG

LIBBASE=/data/francisxli/common-libs

INC=-I$(LIBBASE)/gtest/include -I$(LIBBASE)/jemalloc/include -I../
LIB=-pthread -lrt -L$(LIBBASE)/gtest/lib -lgtest -L$(LIBBASE)/jemalloc/lib -ljemalloc
BIN=

OBJ_TEST_SVR=UnixDomainConnEpoller.o \
			 TestUnixDomainSvr.o

OBJ_TEST_CLIENT=TestUnixDomainClient.o

SVR_TESTBIN=test-svr
CLI_TESTBIN=test-client
BIN+=$(SVR_TESTBIN)
BIN+=$(CLI_TESTBIN)


all:$(BIN)
$(SVR_TESTBIN):$(OBJ_TEST_SVR)
	$(CC) $(CFLAG) -o $@ $(OBJ_TEST_SVR) $(LIB)
$(CLI_TESTBIN):$(OBJ_TEST_CLIENT)
	$(CC) $(CFLAG) -o $@ $(OBJ_TEST_CLIENT) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(INC) -c -o $@ $<
clean:
	rm -f $(BIN)
	rm -f ./*.o

