#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "relax.h"

/* Number of Locks */
#define MUTNUM 1

void relax(double** arr, int d, int n_threads, double p)
{	
	/* Define threads, locks, barriers and other variables*/
	int i, active = 0, flag = 0;
	pthread_t threads[n_threads];
	pthread_mutex_t locks[MUTNUM];
	pthread_barrier_t barrier;
	
	/* Dynamically allocate memory for dummy array */
	double **tmp = (double **)malloc(d * sizeof(double *));
	for (i = 0; i < d; i++) {
		tmp[i] = (double *)malloc(d * sizeof(double));
	}

	/* Initialise mutex set (non-recursive lock) */
	for (i = 0; i < MUTNUM; i++)
		pthread_mutex_init(&locks[i], NULL);

	/* Initialise barrier */
	pthread_barrier_init(&barrier, NULL, n_threads);

	/* Initialise and contract parameters */
	struct param params;
		params.arr = arr;				// Shared pouinter to data array
		params.tmp = tmp;				// Shared pouinter to empty array
		params.active = &active;		// Shared pointer to active (number of initialised threads)
		params.flag = &flag;			// Shared pointer to flag
		params.locks = locks;			// Shared array of locks
		params.barrier = &barrier;		// Shared barrier
		params.p = p;					// Precision
		params.d = d;					// Array dimension
		params.n_threads = n_threads;	// Number of threads
	
	/* Generate child threads */
	for (i = 0; i < n_threads; i++) {
		if (pthread_create(&threads[i], NULL, manipulate, &params)) {
			fprintf (stderr, "Thread creation failed! \n");
			exit(1);
		}	  
	}

	/* Wait for all threads to rejoin */
	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);
 
	/* Destroy mutex set */
	for (i = 0; i < MUTNUM; i++)
		pthread_mutex_destroy(&locks[i]);
	
	/* Destroy barrier */
	pthread_barrier_destroy(&barrier);
	
	/* Free dynamically allocated memory before exiting */
	free(tmp);
	
}


void *manipulate(void *ptr)
{
	/* Create new struct pointer and copy argument value */
	struct param *params = (struct param *)ptr;
	
	/* Initialise locals and retreive external parameters */
	int i, j, pid; 						// Loops: i and j | pid: Private ID
	double **arr = params->arr;			// Pointer to newest array
	double **tmp = params->tmp;			// Pointer to older array
	int *active = params->active;		// Pointer to number of initialised threads
	int *flag = params->flag;			// Shared flag = 1 when precision not met
	int d = params->d;					// Dimension of array
	int threads = params->n_threads;	// Number of threads
	double p = params->p;				// Precision to be achieved
	
	/* Retrieve locks and barriers */
	pthread_mutex_t *locks = params->locks;
	pthread_barrier_t *barrier = params->barrier;

	/* Register each thread with a private ID */
	pthread_mutex_lock(&locks[0]);
		pid = *active;
		*active += 1; 
    pthread_mutex_unlock(&locks[0]);

	while(1) 
	{
		/* Wait for all threads to arrive */
		pthread_barrier_wait(barrier);
		*flag = 0;		// Lower flag
			
		/* Thread manages its allocated rows */
		for (i = 1 + pid; i < d - 1; i = i + threads){
			/* Copy all editable cells from that row */
			for (j = 1; j < d - 1; j++)
				tmp[i][j] = arr[i][j];	// Copy
		}
		/* Perform relaxation on allocated rows */
		for (i = 1 + pid; i < d - 1; i = i + threads){
			/* Manipulate all editable cells from that row */
			for (j = 1; j < d - 1; j++){
				/* Calculate from tmp array average and overwite arr */
				arr[i][j] = (tmp[i-1][j] + tmp[i+1][j] + tmp[i][j-1] + tmp[i][j+1]) / 4;
				/* Raise flag if precision not achieved on this cell */
				if(*flag == 0 && fabs(tmp[i][j] - arr[i][j]) > p) 
					*flag = 1;
			}
		}	
		
		/* Wait for all threads to complete computation */
		pthread_barrier_wait(barrier);
		if(*flag == 0)
			break;		// Break if flag is still low*/
	}
	
	pthread_exit(NULL);
	
} /* manipulate() */

