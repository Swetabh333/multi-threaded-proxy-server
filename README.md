# Multi threaded proxy server with cache and concurrency control

This program implements a proxy server which intercepts and forwards browser requests.It implements a least recently used cache via a linkedlist which stores the requests and responses and allows for quicker responses for previously searched requests. It uses mutex locks for concurrency control while accessing the cache.

## Installation

Requires the [libcurl](https://ec.haxx.se/install/linux.html) library to function.

```bash
git clone https://github.com/Swetabh333/multi-threaded-proxy-server.git
cd multi-threaded-proxy-server
make all
```

## How to run

```
./proxy <port no.>
```

On your browser search your desired website in the following way `localhost:port no./https://www.wikipedia.com`
