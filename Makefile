# simple_proxy
# Jonathan Yang - the.jonathan.yang@gmail.com

all: server

server: 
	gcc -std=gnu99 -o server server.c

clean:
	rm server

again: clean all