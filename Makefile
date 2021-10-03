OPTS = -Wall -g

all: HTTPClient1 HTTPServer1 HTTPServer2 HTTPServer3

HTTPClient1: HTTPClient1.c
	cc -o $@ $(OPTS) $<

HTTPServer1: HTTPServer1.c \
             ProcessHTTPReqs.o ProcessHTTPReqs.h \
             ServerSetup.o     ServerSetup.h
	cc -o $@ $(OPTS) HTTPServer1.c ProcessHTTPReqs.o ServerSetup.o

HTTPServer2: HTTPServer2.c \
             ProcessHTTPReqs.o ProcessHTTPReqs.h \
             ServerSetup.o     ServerSetup.h
	cc -o $@ $(OPTS) HTTPServer2.c ProcessHTTPReqs.o ServerSetup.o

HTTPServer3: HTTPServer3.c \
             ProcessHTTPReqs.o ProcessHTTPReqs.h \
             ServerSetup.o     ServerSetup.h
	cc -o $@ -pthread $(OPTS) HTTPServer3.c ProcessHTTPReqs.o ServerSetup.o

ProcessHTTPReqs.o: ProcessHTTPReqs.c ProcessHTTPReqs.h
	cc -o $@ -c $(OPTS) $<

ServerSetup.o: ServerSetup.c ServerSetup.h
	cc -o $@ -c $(OPTS) $<

