#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "traffic.h"

struct intersection isection;
struct car *in_cars[] = { NULL, NULL, NULL, NULL };
struct car *out_cars[] = { NULL, NULL, NULL, NULL };

int main(int argc, char *argv[]) {
	int i;
	pthread_t in_threads[4], cross_threads[4];
	int in_tid[4], cross_tid[4];

	if (argc != 2) {
		printf("Usage: %s <schedules_file>\n", argv[0]);
		exit(1);
	}

	init_intersection();
	parse_schedule(argv[1]);

	/* spin up threads */
	for (i = 0; i < 4; i++) {
		in_tid[i] = i;
		cross_tid[i] = i;
		pthread_create(&cross_threads[i], NULL, &car_cross, (void *) &(cross_tid[i]));
		pthread_create(&in_threads[i], NULL, &car_arrive, (void *) &(in_tid[i]));
	}

	/* wait for all arrival threads */
	for (i = 0; i < 4; i++) {
		pthread_join(in_threads[i], NULL);
	}

	/* wait for all crossing threads */
	for (i = 0; i < 4; i++) {
		pthread_join(cross_threads[i], NULL);
	}

	return 0;
}
