# Module nginx-multiport-module
---
## Instructions

Independent port for per nginx worker process

## Directives

	Syntax  : multi_listen multiport relationport;
	Default : None;
	Context : events

multiport can configured as below:

	address:port
	port
	unix:path

when configured with IPv4 or IPv6 port, worker process listen port plus with worker process's slot. For Example, we start four workers, add configured multiport with 9000. worker 0 will listen 9000, worker 1 will listen 9001, worker 2 will listen 9002, worker 3 will listen 9003

when configured with unix path, worker will listen path plus with suffix of worker process's slot. For Example, we start four workers, add configured multiport with unix:/tmp/http. worker 0 will listen /tmp/http.0, worker 1 will listen /tmp/http.1, worker 2 will listen /tmp/http.2, worker 3 will listen /tmp/http.3


relationport must configured same as listen directives in http server, rtmp server, stream server or other server

## API

- ngx\_process\_slot\_get\_slot

		ngx_int_t ngx_process_slot_get_slot(ngx_uint_t wpid);

	- para:

		wpid: worker process id, 0 to ccf->worker_processes - 1

	- return value:

		sucessed return ngx_process_slot, else return -1


## Build

cd to NGINX source directory & run this:

	./configure --add-module=/path/to/nginx-multiport-module/
	make && make install

## Example

See t/ngx\_http\_process\_slot\_test\_module.c as reference

Build:

	./configure --with-debug --add-module=/path/to/nginx-multiport-module/t/ --add-module=/path/to/nginx-multiport-module/
	make && make install

Configure:

	worker_processes  4;

	events {
		...
		multi_listen 9000 80;
		multi_listen unix:/tmp/http.sock.80 80;
	}

Test:

	curl -v http://127.0.0.1/
	curl -v http://127.0.0.1:9000/
	curl -v http://127.0.0.1:9001/
	curl -v http://127.0.0.1:9002/
	curl -v http://127.0.0.1:9003/

	curl -v --unix-socket /tmp/http.sock.80.0 http:/
	curl -v --unix-socket /tmp/http.sock.80.1 http:/
	curl -v --unix-socket /tmp/http.sock.80.2 http:/
	curl -v --unix-socket /tmp/http.sock.80.3 http:/

Tests will get the same result, for port 9000 will always send to worker process 0, 9001 to worker process 1 and so on
