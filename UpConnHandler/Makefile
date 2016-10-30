CC=g++
CFLAG=-g

MCPP_BASE=../..

OBJS=UpConnSet.o \
	 $(MCPP_BASE)/ccd/tfc_net_socket_tcp.o \
	 $(MCPP_BASE)/ccd/tfc_net_socket_udp.o \
	testmain.o 

#INC=-I. -I$(MCPP_BASE)/libhttp -I$(MCPP_BASE)/new_common
INC=-I. -I$(MCPP_BASE)
LIB=-lpthread -L$(MCPP_BASE)/libhttp -lhttp

all:testmain

DEBUG_MODE=-D_DEBUG_MODULE

testmain:$(OBJS)
	$(CC) $(CFLAG) -o testmain $(OBJS) $(LIB)
%.o:%.cpp
	$(CC) $(CFLAG) $(DEBUG_MODE) $(INC) -c -o $@ $<
clean:
	rm ./*.o -f
	rm ./testmain -f


