#pragma once

#ifndef RELAX
# define RELAX

/* Multi-threaded function */
void *manipulate(void *p);

/* Parameter structure */
struct param {
	double **arr;
	double **tmp;
	int *active;
	int *flag;
	double p;	
	int d;
	int n_threads;
	pthread_mutex_t *locks;
	pthread_barrier_t *barrier;
};

#endif