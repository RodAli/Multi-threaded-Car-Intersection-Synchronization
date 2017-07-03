#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#include <pthread.h>
#define LANE_LENGTH 10

/* directions */
enum direction {
	NORTH,
	SOUTH,
	EAST,
	WEST
};

/* representation of a car entering the intersection */
struct car {
	/* id that uniquely identifies each car */
	int 			      id;
	/* direction from which the car is entering and leaving */
	enum direction  in_dir, out_dir;
	/* support for singly linked list */
	struct car 		  *next;
};

/* entry lane feeding into the intersection */
struct lane {
	/* synchronization */
	pthread_mutex_t lock;
	pthread_cond_t  producer_cv, consumer_cv;
	
	/* counters */
	int             inc, passed;
	
	/* circular buffer implementation */
	struct car      **buffer;
	int             head, tail;
	int             capacity, in_buf;
};

/* complete representation of the intersection */
struct intersection {
	/* quadrants within the intersection
	               N
	  +------------+------------+
	  | Quadrant 2 | Quadrant 1 |
	W +------------+------------+ E
	  | Quadrant 3 | Quadrant 4 |
	  +------------+------------+
	               S
	Locks are ordered in priority 1 > 2 > 3 > 4, to avoid deadlock
	*/
	pthread_mutex_t quad[4];
	struct lane 	  lanes[4];
};

/* forward declaration of logic functions */
void parse_schedule(char *f_name);
void init_intersection();
void init_lane();

void *car_arrive(void *arg);
void *car_cross(void *arg);
int *compute_path(enum direction in_dir, enum direction out_dir);

#endif

