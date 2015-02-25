LDFLAG += -g -L/usr/lib/libmilter -lmilter -lmemcached -lpthread

all: hdmilter test_throttle

hdmilter: hdmilter.c throttle.c config.h throttle.h
	gcc $(LDFLAG) -o hdmilter throttle.c hdmilter.c

test_throttle:throttle.c test_throttle.c config.h throttle.h
	gcc -g -o test_throttle throttle.c test_throttle.c -lmemcached

clean:
	-@rm hdmilter test_throttle

test:
	./hdmilter -p inet:5888

