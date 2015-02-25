#include <syslog.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "throttle.h"

#define LOGFLAG LOG_DAEMON|LOG_INFO

int mc_incget(memcached_st* mc, const char *key, int off,uint64_t *val, time_t expire)
{
	memcached_return rc = memcached_increment_with_initial(mc,key,strlen(key),off,0,expire,val);
	if (rc != MEMCACHED_SUCCESS) {
		syslog(LOGFLAG, "mc error:%s",memcached_strerror(mc, rc));
		return -1;
	}

	return 0;
}

memcached_st* mc_init()
{
	memcached_return rc;
	memcached_st *mc = memcached_create(NULL);
	if (!mc) {
		syslog(LOGFLAG, "mc error:%s","oom");
		return NULL;
	}
	memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
	rc = memcached_server_add(mc, CONF_MC_HOST, CONF_MC_PORT);
	if (rc != MEMCACHED_SUCCESS) {
		syslog(LOGFLAG, "mc error:%s",memcached_strerror(mc, rc));
		memcached_free(mc);
		return NULL;
	}
	return mc;
}

throttle_t* throttle_init()
{
	throttle_t* t = (throttle_t *)calloc(1, sizeof(throttle_t));
	return t;
}

void throttle_destroy(throttle_t *t)
{
	if (t && t->mc)
		memcached_free(t->mc);
	free(t);
}

int throttle_test_address(throttle_t *t, const char *addr)
{
	if (!addr) /* remote address not found */
		return 1;
	if (strstr(CONF_BLACKLIST_ADDR,addr)) {
		/*syslog(LOGFLAG, "throttled addr:%s", addr);*/
		return 0;
	}
	return 1;
}

int throttle_test_user(throttle_t *t, const char *user, int nrcpt)
{
#define MCKLEN	128
	time_t now;
	struct tm tmnow;
	char kday[MCKLEN],khour[MCKLEN],kmin[MCKLEN];
	uint64_t vday, vhour, vmin;

	if (!user || strlen(user)==0) { /* not login user */
		return 1;
	}
	if (strstr(CONF_WHITELIST_USER,user)) { /* user in whitelist */
		return 1;
	}
	if (!t->mc) {
		t->mc = mc_init();
		if (!t->mc) /* memcache not available */
			return 1;
	}

	now = time(NULL);
	localtime_r(&now, &tmnow);
	snprintf(kday, MCKLEN, "thro:d:%d:%s", tmnow.tm_mday, user);
	snprintf(khour, MCKLEN, "thro:h:%d:%s", tmnow.tm_hour, user);
	snprintf(kmin, MCKLEN, "thro:m:%d:%s", tmnow.tm_min, user);
	if (mc_incget(t->mc,kday,nrcpt,&vday,24*3600) ||
		mc_incget(t->mc,khour,nrcpt,&vhour,3600) ||
		mc_incget(t->mc,kmin,nrcpt,&vmin,60)) { /* mc error */
		return 1;
	}
	vday += nrcpt;
	vhour += nrcpt;
	vmin += nrcpt;
	syslog(LOGFLAG, "throttle info:%s(%llu,%llu,%llu)",user,vday,vhour,vmin);
	if (vday>=CONF_DAYMAX || vhour>=CONF_HOURMAX || vmin>=CONF_MINMAX) {
		return 0;
	}

	return 1;
#undef MCKLEN
}

