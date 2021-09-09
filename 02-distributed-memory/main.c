
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

typedef int bool;
#define true 1
#define false 0
 
 /* Function Prototypes - for full descriptions, see end of document */
void importData( double** arr, int size );
int verifyArr( double** arr, int size );
void copyData( double** arr1, double** arr2, int size );
void copyChunk( double** arr1, double** arr2, int size, int start, int end );
void printArr( double** arr, int size );
void relax( double** arr, double** tmp, double p, int d, int myId, int nProcs );
 
int main( int argc, char** argv ) 
{
	const int dataSize = 5000;		// Size of full data set
	int arrSize = 500;				// Size of working data set (will be forced odd)
	const double precision = 0.1;	// Level of precison to achieve with relaxation
	
	int i, j;		// Used in loops
	int ret;		// Contains return values for MPI functions
	int myId;		// Process rank
	int nProcs;		// Total number of processes
	int nameLen;	// Char length of processor name
	char coreName [MPI_MAX_PROCESSOR_NAME];		// Processor Name

	/* Variables used to measure duration of relaxtion function */
	struct timespec ts1, ts2;
	
	/**
	* Force working data set to have odd integer dimension - 
	* creates data symentry around a central point, thus making
	* comparisons across diffent values of arrSize more meaningful 
	*/	
	if ( arrSize % 2 == 0 )
		arrSize += 1;
	
	 
	/* Dynamically allocate memory for full and working (reduced) data sets */
	double **data = (double **)malloc(dataSize * sizeof(double *));
	double **arr = (double **)malloc(arrSize * sizeof(double *));
	double **tmp = (double **)malloc(arrSize * sizeof(double *));
	
	/* Allocate collumns for full data set */
	for (i = 0; i < dataSize; i++) {
		data[i] = (double *)malloc(dataSize * sizeof(double));
	}
	
	/* Allocate working data set and its temporary counterpart */
	for (i = 0; i < arrSize; i++) {
		arr[i] = (double *)malloc(arrSize * sizeof(double));
		tmp[i] = (double *)malloc(arrSize * sizeof(double));
	}
	
    // Initialise the MPI environment
    ret = MPI_Init( NULL, NULL );
	if ( ret != MPI_SUCCESS) {
		printf ( "ERR: Error starting MPI program\n" );
		MPI_Abort( MPI_COMM_WORLD, ret );
	}
 
	/* Get MPI World info */
    MPI_Comm_size( MPI_COMM_WORLD, &nProcs );	 	// Get total number of processes
    MPI_Comm_rank( MPI_COMM_WORLD, &myId );			// Get process rank
    MPI_Get_processor_name( coreName, &nameLen );	// Get name of processor
 
	// Master process print total number of processes to console
	if ( myId == 0 ) {
		printf( "INFO: Process %d reports %d total processes\n", myId, nProcs );
	}
	
	/* Populate malloc arrays with sentinal values */
	for ( i = 0; i < dataSize; i++ ) {
		for ( j = 0; j < dataSize; j++ ) {
			data[i][j] = -1;
		}
	}
	
	for ( i = 0; i < arrSize; i++ ) {
		for ( j = 0; j < arrSize; j++ ) {
			arr[i][j] = -1;
			tmp[i][j] = -1;
		}
	}
	
	/* Import full data set */
	importData( data, dataSize );
	
	/* Verify that *some* data was imported to all cells in data array */
	if ( verifyArr( data, dataSize ) == 1 ) {
		printf ( "INFO: Process %d on %s successfully imported test data\n", myId, coreName );
	}
	else {
		printf ( "ERR: Process %d on %s failed to import test data\n", myId, coreName );
		MPI_Abort( MPI_COMM_WORLD, 5 );
	}
	
	/* Fill arr with values from data */
	copyData ( data, arr, arrSize );
	/* Copy contents of arr to tmp */
	copyData ( arr, tmp, arrSize );
	
	/* Wait for all threads before attempting relaxation */
	MPI_Barrier( MPI_COMM_WORLD );
	
	/* Master Process Prints Information to Console */
	if ( myId == 0 ) {
		printf("\n BEGIN INFORMATION STREAM \n");
		printf("-----------------------------------\n");
		printf("Array Size = %d x %d\n", arrSize, arrSize);		// Print Array Size
		printf("Precision Size = %f\n", precision);				// Print Precision Size
		printf("Number of Processes = %d\n", nProcs);			// Print Precision Size
		clock_gettime(CLOCK_REALTIME, &ts1);					// Store current system time
	}
	
	/* Perform Relaxtion */
	relax( arr, tmp, precision, arrSize, myId, nProcs );
	
	/* Master Process Calculates time taken and Prints Information to Console */
	if ( myId == 0 ) {
		clock_gettime(CLOCK_REALTIME, &ts2);
		if (ts2.tv_nsec < ts1.tv_nsec) {
			ts2.tv_nsec += 1000000000;
			ts2.tv_sec--;
		}
		printf("\n Time(s): %ld.%09ld \n", (long)(ts2.tv_sec - ts1.tv_sec),
			ts2.tv_nsec - ts1.tv_nsec);

		printf("-----------------------------------\n");	
	}
	
	/* Free dynamically allocated memory before exiting */
	free( data );
	free( arr );
	free( tmp );

    // Finalise the MPI environment.
    MPI_Finalize();
}

 /**
  * 
  * void relax( double** arr, double** tmp, double p, int d, int myId, int nProcs )
  * 
  *    The relax function performs the jacobi relaxtion method on arr
  * 
  * Parameters   : arr: square array containing double precsion floating point values
  *              : tmp: identical to arr
  *              : p: level of precison to achieve with relaxation
  *				 : d: sqaure integer dimension of arr and tmp
  *				 : myId: processor rank
  *				 : nProcs: total number of processes
  *
  * Return Value : None. - arr modified in situ 
  *						 - tmp is also modified but will not contain meaningful data
  * 
  * Description: 
  * 
  *    Relaxation technique perfomed on arr using all processes in MPI_COMM_WORLD. 
  *
  *    Although each process is responsible for a particular chunk of the array, 
  *    all processes broadcast their data globally before the function 
  *    exits so that it can be accessed throughout the MPI_COMM_WORLD.  
  * 
  */ 

void relax( double** arr, double** tmp, double p, int d, int myId, int nProcs ) 
{
	int i, j;		// Used in loops
	int iEnd;		// Row number after the last editable row in chunk
	int chunkSize = (int)floor(d / nProcs);		// Number of rows in chunk
	int iStart = 1 + (myId * chunkSize);		// Row number of first editable row in chunk
	bool stop = false;		// Set to false if precision is not met across all processes
	
	/** 
	* 	prec_Flags is an array of flags with one cell for each process
	*		- array index corresponds to processor rank
	*		- value in each cell : 0: processor did reach precision
	*						   	   1: processor did not reach precision
    */
	int* prec_Flags = malloc(d * sizeof(int));

	/* For the last process, iEnd = d - 1 */
	if ( myId == nProcs - 1 )
		iEnd = d - 1;
	/* For all other processes, iEnd = iStart of next ranked process */
	else
		iEnd = 1 + (1 + myId) * chunkSize;
	
	while ( stop == false ) 
	{		
		prec_Flags[myId] = 0;		// Set own processor flag low
		
		// Maniulate all rows in processors designated chunk
		for (i = iStart; i < iEnd; i++ ){
			// Manipulate all editable cells from that row 
			for (j = 1; j < d - 1; j++){
				// Calculate from tmp array average and overwite arr 
				arr[i][j] = (tmp[i-1][j] + tmp[i+1][j] + tmp[i][j-1] + tmp[i][j+1]) / 4;
				// Raise own processor flag if precision not achieved on this cell 
				if(prec_Flags[myId] == 0 && fabs(tmp[i][j] - arr[i][j]) > p) 
					prec_Flags[myId] = 1;
			}
		}
		
		/** 
		 * 	MPI Send / Receive Chunk Edges
		 *	
		 *  When i = 0:
		 *		1) Even process numbers send (iEnd - 1) row to the next ranked process
		 *  	2) Odd process numbers reviece (iStart - 1) row from preceding ranked process
		 *  	3) Even process numbers send iStart row to preceding ranked process
		 *  	4) Odd process numbers recieve iEnd row from next ranked process
		 *
		 *	When i = 1:	
		 *  	Steps 1-4 repeated with Odds and Evens swapped roles
		 *	
		 */
		for ( i = 0; i < 2; i++ ) {
			/* Sending Information */
			if ( myId % 2 == i) {
				// If process number is not last in MPI_COMM_WORLD
				if ( myId != nProcs - 1 ) {
					// Send last editable row of chuck to the next ranked process
					MPI_Send ( *&arr[iEnd - 1], d, MPI_DOUBLE, myId + 1, 0, MPI_COMM_WORLD );
				}
				// If process number is not first in MPI_COMM_WORLD
				if ( myId != 0 ) {
					// Send first editable row of chuck to the preceding ranked process
					MPI_Send ( *&arr[iStart], d, MPI_DOUBLE, myId - 1, 1, MPI_COMM_WORLD );	
				}
			}
			/* Recieving Information */
			else {
				// If process number is not first in MPI_COMM_WORLD
				if ( myId != 0 ) {
					// Receive row before my first editable row of chuck from preceding ranked process
					MPI_Recv ( *&arr[iStart - 1], d, MPI_DOUBLE, myId - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE );
				}
				// If process number is not last in MPI_COMM_WORLD
				if ( myId != nProcs - 1 ) {
					// Receive row after my last editable row of chuck from the next ranked process
					MPI_Recv ( *&arr[iEnd], d, MPI_DOUBLE, myId + 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE );
				}
			}
		}

		/* Broadcast flag status to all other processes */
		for ( i = 0; i < nProcs; i++ ) {
			MPI_Bcast(&prec_Flags[i], 1, MPI_INT, i, MPI_COMM_WORLD);  
		}
		
		/* Verify all processes achieved precision */
		for ( i = 0; i < nProcs; i++ )  {
			// If process i did not reach precision
			if ( prec_Flags[i] == 1 ) {
				stop = false;
				break;
			}
			else {
				/* stop only remains true if precision 
					was achieved for all values of i */
				stop = true;			
			}
		}

		/* Copy chunk from arr to tmp */
		copyChunk( arr, tmp, d, iStart, iEnd);
	}	
	
	/* Broadcast all own chunk data (row by row) to all other processes */
	for ( i = 0; i < nProcs - 1; i++ ) {
		for ( j = 1 + (i * chunkSize); j < 1 + (1 + i) * chunkSize; j++ ) {
			MPI_Bcast(*&arr[j], d, MPI_DOUBLE, i, MPI_COMM_WORLD);  
		} 		
	}
	
	/* End process has different style end point so requires own loop */
	for ( j = 1 + ((nProcs - 1) * chunkSize); j < d - 1; j++ ) {
		MPI_Bcast(*&arr[j], d, MPI_DOUBLE, nProcs - 1, MPI_COMM_WORLD); 
	}
}

 /**
  * 
  * void copyChunk( double** arr1, double** arr2, int size, int start, int end ) 
  * 
  *    The copyData function copies all a chunk of data from arr1 to arr2
  * 
  * Parameters   : arr1: square array to be copied from
  *              : arr2: square array to be copied to
  *				 : size: sqaure integer dimension of array
  *				 : start: starting row of chunk
  *				 : end: ending row of chunk
  *
  * Return Value : None. - arr2 modified in situ
  * 
  * Description: 
  * 
  *    Arrays arr1 and arr2 must have the same square dimension which
  *    must be equal to 'size'. The chunk copied includes the rows neighbouring
  *	   the defined chunk as the start and end paramters
  * 
  */ 

void copyChunk( double** arr1, double** arr2, int size, int start, int end ) 
{
	int i, j;
	for ( i = start - 1; i < end + 1; i++ )
		for ( j = 1; j < size - 1; j++ ) {
			arr2[i][j] = arr1[i][j];
		}
}

 /**
  * 
  * void copyData( double** arr1, double** arr2, int size )
  * 
  *    The copyData function copies all values from arr1 to arr2
  * 
  * Parameters   : arr1: square array to be copied
  *              : arr2: square array to have data copied to it
  *				 : size: sqaure integer dimension of data to be compied
  * 
  * Return Value : None. - arr2 modified in situ
  * 
  * Description: 
  * 
  *    Arrays arr1 and arr2 are not required to be the same size, 
  *    but arr2 <= arr1 and size <= square integer dimension of arr2.
  * 
  */ 

void copyData( double** arr1, double** arr2, int size ) 
{
	int i, j;
	for ( i = 0; i < size; i++ )
		for ( j = 0; j < size; j++ )
			arr2[i][j] = arr1[i][j];
}

 /**
  * 
  * void verifyArr( double** arr, int size ) 
  * 
  *    The verifyArr function checks to see if all values in an array are non-sentinal;
  *    in this instance, sentinal is taken to be -1
  * 
  * Parameters   : arr: square array to be checked for sentinal values
  *				 : size: sqaure integer dimension of array
  * 
  * Return Value : 0: Fail - Sentinal values still exhist
  *  			   1: Success - Sentinal values overwritten
  * 
  * Description: 
  * 
  *    Array must have been previously populated with sentinal values for
  *    verification to carry any meaning. Note: just because values are no
  *    longer sentinal, does not mean they are correct.
  * 
  */ 

int verifyArr( double** arr, int size ) 
{
	int i, j;
	for ( i = 0; i < size; i++ )
		for ( j = 0; j < size; j++ )
			// Cell contains sentinal value, test failed
			if ( arr[i][j] == -1 )
				return 0;
			
	// All cells contail non-sentinal values, test success
	return 1;
}

 /**
  * 
  * void importData( double** arr, int size ) 
  * 
  *    The importData function imports data from a predetermined 
  *    binary file into a doubel precision floating point array.
  * 
  * Parameters   : arr: 2D array with sufficent memory to store size^2 data
  *				 : size: sqaure integer dimension of array
  * 
  * Return Value : None. - arr modified in situ
  * 
  * Description: 
  * 
  *    This function imports binary data from the predetermined binary file. 
  *    Parameter 'size' must be the xy dimension of arr and the same as the
  *    dimension of the exported data for the values to be imported correctly.
  * 
  */ 

void importData( double** arr, int size ) 
{
	int i, j;
	FILE *fp;

	fp = fopen("u5000.bin", "r");

	for (i = 0; i < size; i++)
		for (j = 0; j < size; j++)
			// Read one double from binary file into arr
			fread(&arr[i][j], sizeof(double), 1, fp);

	fclose(fp);
}

 /**
  * 
  * void printArr( double** arr, int size ) 
  * 
  *    The printArr function will print a correctly formatted array
  *	   into the console
  * 
  * Parameters   : arr: square array containing double precsion floating point values
  *				 : size: sqaure integer dimension of array
  * 
  * Return Value : None. 
  * 
  * Description: 
  * 
  *    This function prints the contents of the passed array into the console,
  *     note that it must be a square array of double data type.
  * 
  */ 
  
void printArr( double** arr, int size ) 
{
	int i, j;
	// Cycle rows
	for (i = 0; i < size; i++)
	{
		printf("\n ");		// Next row
		// Cycle columns
		for (j = 0; j < size; j++)
			printf("%f | ", arr[i][j]);	// Print cell contents
	}
	printf("\n");
}
