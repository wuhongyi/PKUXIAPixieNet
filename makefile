TARGET = cgitraces.cgi cgistats.cgi pkudaq
LIBS = -lm 
CFLAGS = -std=c99 -Wall
CXXFLAGS = -Wall -O3 -DNDEBUG   -pthread -std=gnu++98
INCDIRS = -I/usr  -I/usr/include -I/usr/local/include
LINKFLAGS =  #-static -static-libstdc++
BOOSTLIBS = -L/usr/local/lib -lboost_date_time -lboost_chrono -lboost_atomic -lboost_program_options -lboost_system -lboost_thread -lrt -pthread

.PHONY: default all clean

default: $(TARGET)
all: default

%.o: %.c 
	gcc  $(CFLAGS) -c $< -o $@

%.o: %.cpp 
	g++  $(CXXFLAGS) $(INCDIRS) -c $< -o $@
%.o: %.cc 
	g++  $(CXXFLAGS) $(INCDIRS) -c $< -o $@

	
cgitraces.cgi: cgitraces.o
	gcc cgitraces.o $(LIBS) -o cgitraces.cgi

cgistats.cgi: cgistats.o PixieNetCommon.o
	gcc cgistats.o PixieNetCommon.o $(LIBS) -o cgistats.cgi

pkudaq: pkudaq.o PixieNetCommon.o PixieNetConfig.o
	g++ pkudaq.o PixieNetCommon.o PixieNetConfig.o  $(LIBS) -o pkudaq

clean:
	-rm -f *.o
	-rm -f $(TARGET)
