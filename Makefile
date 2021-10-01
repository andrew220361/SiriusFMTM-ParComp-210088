OPTS = -Wall -g

HTTPClient1: HTTPClient1.c
	cc -o $@ $(OPTS) $<
