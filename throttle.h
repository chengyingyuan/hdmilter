#ifndef THROTTLE_H
#define THROTTLE_H

#include <libmemcached/memcached.h>

typedef struct throttle_struct {
	memcached_st *mc;

} throttle_t;

throttle_t* throttle_init();
void throttle_destroy(throttle_t *t);
int throttle_test_address(throttle_t *t, const char *addr);
int throttle_test_user(throttle_t *t, const char *user, int nrcp);

#endif
