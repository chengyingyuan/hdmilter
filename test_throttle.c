#include <syslog.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "throttle.h"

void usage(int argc, const char *argv[])
{
	fprintf(stderr, "USAGE: %s <user> <ncalls>\n", argv[0]);
}

int main(int argc, const char *argv[])
{
	throttle_t *thro;
	const char *user;
	int ncalls;
	int i;

	if (argc != 3) {
		usage(argc, argv);
		return -1;
	}
	user = argv[1];
	ncalls = atoi(argv[2]);

	thro = throttle_init();
	if (!thro) {
		fprintf(stderr, "Failed initiate throttle\n");
		return -1;
	}
	for (i = 0; i < ncalls; i++) {
		if (!throttle_test_user(thro, user, 1)) {
			fprintf(stderr, "User throttled after %d calls\n", i+1);
		}
	}
	throttle_destroy(thro);
	fprintf(stderr, "Done\n");
	return 0;
}

