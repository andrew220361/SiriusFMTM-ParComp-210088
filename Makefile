OPTS = -Wall -g

all: HTTPClient1 HTTPServer1 HTTPServer2

HTTPClient1: HTTPClient1.c
	cc -o $@ $(OPTS) $<

HTTPServer1: HTTPServer1.c
	cc -o $@ $(OPTS) $<

HTTPServer2: HTTPServer2.c
	cc -o $@ $(OPTS) $<
