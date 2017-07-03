#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "traffic.h"

extern struct intersection isection;
extern struct car *in_cars[];
extern struct car *out_cars[];

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with 
 * its in_direction
 * 
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
	int id;
	struct car *cur_car;
	enum direction in_dir, out_dir;
	FILE *f = fopen(file_name, "r");

	/* parse file */
	while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {
		printf("Car %d going from %d to %d\n", id, in_dir, out_dir);
		/* construct car */
		cur_car = malloc(sizeof(struct car));
		cur_car->id = id;
		cur_car->in_dir = in_dir;
		cur_car->out_dir = out_dir;

		/* append new car to head of corresponding list */
		cur_car->next = in_cars[in_dir];
		in_cars[in_dir] = cur_car;
		isection.lanes[in_dir].inc++;
	}

	fclose(f);
}
/**
 * Function to intialize all the lanes in the intersection
 */
void init_lane(){
	int i;
	for(i = 0; i < 4; i++){
		// Initialize the lanes
		pthread_mutex_init(&isection.lanes[i].lock, NULL);
		pthread_cond_init(&isection.lanes[i].producer_cv, NULL);
		pthread_cond_init(&isection.lanes[i].consumer_cv, NULL);
		isection.lanes[i].inc = 0;
		isection.lanes[i].passed = 0;
		isection.lanes[i].buffer = malloc(sizeof(struct car *) * LANE_LENGTH);
		isection.lanes[i].head = 0;
		isection.lanes[i].tail = 0;
		isection.lanes[i].capacity = LANE_LENGTH;
		isection.lanes[i].in_buf = 0;
	}
}

/**
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 * 
 */
void init_intersection() {

	// Initialize quadrant locks in intersection
	int i;
	for(i = 0; i < 4; i++){
		pthread_mutex_init(&isection.quad[i], NULL);
	}

	// Initialize all the lanes
	init_lane();
	
}

/**
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 * 
 */
void *car_arrive(void *arg) {

	enum direction lane_num = *(int*) arg; 

	// Point to the start of the in_cars linked list for this lane
	struct car *car_iterator = in_cars[lane_num];
	while (car_iterator != NULL){

		// Grab the lock and wait if there is no room in buffer
		pthread_mutex_lock(&isection.lanes[lane_num].lock);
		while (isection.lanes[lane_num].in_buf == isection.lanes[lane_num].capacity){
			// Buffer is full so wait
			pthread_cond_wait(&isection.lanes[lane_num].producer_cv, 
				&isection.lanes[lane_num].lock);
		}

		// Add car from linked list to the buffer
		isection.lanes[lane_num].in_buf++;
		// Add the car from linked list into the buffer at the tail position
		isection.lanes[lane_num].buffer[isection.lanes[lane_num].tail] = car_iterator;
		// Increment tail properly along the circular buffer
		isection.lanes[lane_num].tail = (isection.lanes[lane_num].tail + 1) % isection.lanes[lane_num].capacity;

		// Signal consumers to empty the buffer
		pthread_cond_signal(&isection.lanes[lane_num].consumer_cv);
		pthread_mutex_unlock(&isection.lanes[lane_num].lock);

		// Move on to the next car in the linked list
		car_iterator = car_iterator->next;
	}

	return NULL;
}

/**
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 */
void *car_cross(void *arg) {

	enum direction lane_num = *(int*) arg; 

	// Loop until all cars in lane have passed the intersection
	while (isection.lanes[lane_num].passed < isection.lanes[lane_num].inc){

		// Grab the lock and wait if there are no cars in buffer
		pthread_mutex_lock(&isection.lanes[lane_num].lock);
		while (isection.lanes[lane_num].in_buf == 0){
			// Buffer is empty so wait
			pthread_cond_wait(&isection.lanes[lane_num].consumer_cv, 
					&isection.lanes[lane_num].lock);
		}

		// Remove the first car at head of buffer
		struct car *cross_car = isection.lanes[lane_num].buffer[isection.lanes[lane_num].head];

		// Get the path this car must go through the intersection
		int *path = compute_path(cross_car->in_dir, cross_car->out_dir);
		// Loop through all the quadrants and aquire the locks needed
		int i;
		for (i = 0; i < 4; i++){
			if (path[i] != -1){
				pthread_mutex_lock(&isection.quad[i]);
			}
		}

		// Once all necessary locks aquired then add car to out_cars
		// Remove car from the buffer
		isection.lanes[lane_num].buffer[isection.lanes[lane_num].head] = NULL;
		// Increment the head properly along the circular buffer
		isection.lanes[lane_num].head = (isection.lanes[lane_num].head + 1) % isection.lanes[lane_num].capacity;
		// Decrement the number of cars in the buffer
		isection.lanes[lane_num].in_buf--;
		// append new car to head of corresponding out_car list
		cross_car->next = out_cars[cross_car->out_dir];
		out_cars[cross_car->out_dir] = cross_car;
		// Print message to standard output
		printf("Car ID: [%d], in_dir: [%d], out_dir: [%d] crossed the intersection\n", 
			cross_car->id, cross_car->in_dir, cross_car->out_dir);

		// Once done with crossing intersection, release the locks
		for (i = 0; i < 4; i++){
			if (path[i] != -1){
				pthread_mutex_unlock(&isection.quad[i]);
			}
		}

		// Deallocate the memory for the path
		free(path);

		// Signal producers to fill the buffer
		pthread_cond_signal(&isection.lanes[lane_num].producer_cv);
		pthread_mutex_unlock(&isection.lanes[lane_num].lock);

		// A car has passed the intersection
		isection.lanes[lane_num].passed++;
	}

	// After this lane is empty, free its buffer
	free(isection.lanes[lane_num].buffer);

	return NULL;
}

/**
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {

	// Initialize the path
	int *path = malloc(sizeof(int)*4);
	int i;
	for(i = 0; i < 4; i++){
		path[i] = -1;
	}

	// Set quadrant to enter the intersection
	if (in_dir == EAST){
		path[0] = 1;
	} else if (in_dir == NORTH){
		path[1] = 2;
	} else if (in_dir == WEST){
		path[2] = 3;
	} else if (in_dir == SOUTH){
		path[3] = 4;
	}
	// Set quadrant to leave the intersection
	if (out_dir == NORTH){
		path[0] = 1;
	} else if (out_dir == WEST){
		path[1] = 2;
	} else if (out_dir == SOUTH){
		path[2] = 3;
	} else if (out_dir == EAST){
		path[3] = 4;
	}

	// Straight, right turns and U turns are handled above.
	// Handle left turns here
	if (in_dir == SOUTH && out_dir == WEST){
		path[0] = 1;
	} else if (in_dir == EAST && out_dir == SOUTH){
		path[1] = 2;
	} else if (in_dir == NORTH && out_dir == EAST){
		path[2] = 3;
	} else if (in_dir == WEST && out_dir == NORTH){
		path[3] = 4;
	}

	return path;
}