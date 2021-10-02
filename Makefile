OPTS = -Wall -g

all: HTTPClient1 HTTPServer1

HTTPClient1: HTTPClient1.c
	cc -o $@ $(OPTS) $<

HTTPServer1: HTTPServer1.c
	cc -o $@ $(OPTS) $<
