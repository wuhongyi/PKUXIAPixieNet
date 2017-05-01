TARGET = cgitraces.cgi gettraces progfippi runstats cgistats.cgi startdaq coincdaq findsettings acquire
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

gettraces: gettraces.o
	gcc gettraces.o $(LIBS) -o gettraces

progfippi: progfippi.o PixieNetCommon.o PixieNetConfig.o
	g++ progfippi.o PixieNetCommon.o PixieNetConfig.o $(LIBS) -o progfippi

runstats: runstats.o PixieNetCommon.o
	gcc runstats.o PixieNetCommon.o $(LIBS) -o runstats

cgistats.cgi: cgistats.o PixieNetCommon.o
	gcc cgistats.o PixieNetCommon.o $(LIBS) -o cgistats.cgi

pkudaq: pkudaq.o PixieNetCommon.o PixieNetConfig.o
	g++ pkudaq.o PixieNetCommon.o PixieNetConfig.o $(LIBS) -o pkudaq

startdaq: startdaq.o PixieNetCommon.o PixieNetConfig.o
	g++ startdaq.o PixieNetCommon.o PixieNetConfig.o $(LIBS) -o startdaq

coincdaq: coincdaq.o PixieNetCommon.o PixieNetConfig.o
	g++ coincdaq.o PixieNetCommon.o PixieNetConfig.o $(LIBS) -o coincdaq

findsettings: findsettings.o
	gcc findsettings.o PixieNetCommon.o $(LIBS) -o findsettings

acquire: acquire.o PixieNetConfig.o PixieNetCommon.o 
	g++ acquire.o PixieNetCommon.o PixieNetConfig.o -rdynamic $(LINKFLAGS) $(LIBS) $(BOOSTLIBS) -o acquire

clean:
	-rm -f *.o
	-rm -f $(TARGET)
