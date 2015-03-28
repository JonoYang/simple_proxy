# simple_proxy
# Jonathan Yang - the.jonathan.yang@gmail.com

all: server

server: 
	gcc -std=gnu99 -o simple_proxy server.c

clean:
	rm simple_proxy

again: clean all
