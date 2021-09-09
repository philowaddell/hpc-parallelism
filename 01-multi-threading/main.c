#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h> 
#include "relax.c"


/* Generates array of random numbers which are passed to relax.c 
	these are then wrapped with other variables into a struct
	struct passed to another function where the variables are dereferanced
	array is then edited and thread returns to main to print new array */
	
void printArr(double** arr, int size);

int main(int argc, char **argv)
{
	int size = 50;					// Size of array
	int threads = 1;				// Number of threads to use
	int i, j, k;					// Integers used for 2d loops
	double precision = 0.000001;	// Relaxation precision
	time_t t;						// Initialise time for random number generation
	struct timespec ts1, ts2;		// Structure for extracting system time
	
	/* Intialises rand seed */
	srand((unsigned)time(&t));

	/* Dynamically allocate memory for two arrays of test data */
	double **start_array = (double **)malloc(size * sizeof(double *));
	double **end_array = (double **)malloc(size * sizeof(double *));
	
	for (i = 0; i < size; i++) {
		start_array[i] = (double *)malloc(size * sizeof(double));
		end_array[i] = (double *)malloc(size * sizeof(double));
	}

	/* Populate array with random doubles */
	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			start_array[i][j] = (double)rand() / ((double)(RAND_MAX) / 5);
		}
	}
	
	/* Change Threads */
	for(threads = 1; threads < 17; threads = threads+1){
		/* Do Repeats */
		for(k = 0; k < 5; k++){
			/* Copy Starter Array */
			for (i = 0; i < size; i++) {
				for (j = 0; j < size; j++) {
					end_array[i][j] = start_array[i][j];
				}
			}
			
			/* Get system clock (start) */
			clock_gettime(CLOCK_MONOTONIC, &ts1);
			
			/* Perform relaxtaion technique on temp_array */
			relax(end_array, size, threads, precision);
			
			/* Get system clock (end) and maniulate ready for calculation */
			clock_gettime(CLOCK_MONOTONIC, &ts2);
			if (ts2.tv_nsec < ts1.tv_nsec) {
				ts2.tv_nsec += 1000000000;
				ts2.tv_sec--;
			}
			
			/* Prints Information to Console*/
			printf(" Array Size = %d x %d\n ", size, size);			// Print Array Size
			printf("Precision Size = %f\n ", precision);			// Print Precision Size
			printf("Number of Threads = %d\n ", threads);			// Print Precision Size
			printf("\n%ld.%09ld", 						// Print Runtime
				(long)(ts2.tv_sec - ts1.tv_sec), 
				ts2.tv_nsec - ts1.tv_nsec);

			printf("\n-----------------------------------\n");		
		}
	}
	/* Free dynamically allocated memory before exiting */
	//printArr(start_array, size);
	//printArr(end_array, size);
	free(start_array);
	free(end_array);
	
	printf("Program run success - exiting.");
	exit(0);
}

/* Prints passed array */
void printArr(double** arr, int size){
	int i, j;
	// Cycle rows
	for (i = 0; i < size; i++)
	{
		printf("\n ");		// Next row
		// Cycle through cols
		for (j = 0; j < size; j++)
			printf("%f | ", arr[i][j]);	// Print cell contents
	}
	printf("\n\n");	// Print newline
}
