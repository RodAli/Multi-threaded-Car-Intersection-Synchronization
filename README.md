# Multi-threaded-Car-Intersection-Synchronization

A traffic system that coordinates cars going through an intersection, by enforcing ordering between car arrivals in the same direction, avoiding potential crashes, and avoiding deadlocks. Done in Fall 2016.

![Alt text](/images/IntersectionDiagram.png "Visual image of intersection")

#### Policies
1. Once a car approaches an intersection, it is placed in the corresponding lane. The car can only cross the intersection when all other cars already in the lane have crossed.
2. Once a car is the first in the lane, it can begin to cross the intersection. Before the car can enter the intersection it must guarantee that it has a completely clear path to its destination.

#### Details

Cars are represented by a 3-tuple (id, in_dir, out_dir) where id is a car's unique id, in_dir is the direction from which the car enters the intersection and out_dir is the direction the car leaves the intersection.

Sample input text file:
~~~
1 0 2
2 1 2
3 1 3
4 0 1
~~~

Run "make" to compile and then run. :)
