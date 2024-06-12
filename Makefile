CC = g++

all: proxy

proxy: server.cpp
	$(CC) -o proxy server.cpp -lcurl

clean: 
	rm proxy