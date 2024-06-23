# Parallel Marching Squares <br> (Assignment 1 APD)

The objective of this assignment was to parallelize a serial marching squares algorithm implementation *(used to find contours in topological maps)* using `pthreads`.

## Summary

To begin with, the program reads the ppm image and assigns the number of threads to be used, which are passed in as arguments, and initializes the common data structures used by all of the threads (the **contour map**, the **scaled image** and the **grid**). After that, all of the threads are created, with a pointer to an arguments `struct` containing all of the data that a thread needs.

The threads execute the same steps of the algorithm as in the serial implementation, but that the functions corresponding to each step have been parallelized: the `for {}` blocks execution is split between the available threads, each one executing it's own portion of the code, based on the ***thread_id***.

Between some steps of the algorithm, a ***barrier*** has been used to ensure that the previous step has been fully executed by all threads.

At the end, the obtained image is written to the output file, and this is done only by **thread_id 0**.

## Important files

- [parallel.c](/src/parallel.c) - the multithreaded implementation, written by me.
- [serial.c](/src/serial.c) - the serial implementation from which the parallel one was derived.
- [helpers.c](/src/helpers.c) - relevant procedures used by both implementations, used mainly for reading/writing `.ppm` images and rescaling.