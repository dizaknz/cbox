/*
 ============================================================================
 Name        : multiproc.c
 Author      : ITHD
 Version     :
 Copyright   : 
 Description : Calculate Pi in MPI
 ============================================================================
 */
#include <mpi.h>
#include <stdio.h>
#include <string.h>

void calc_pi(int rank, int num_procs)
{
	int		i;
	int		num_intervals;
	double	h;
	double	mypi;
	double	pi;
	double	sum;
	double	x;

	if (rank == 0) {
		num_intervals = 100000000;
	}

	MPI_Bcast(&num_intervals, 1, MPI_INT, 0, MPI_COMM_WORLD);

	h = 1.0 / (double) num_intervals; 
	sum = 0.0;

	for (i = rank + 1; i <= num_intervals; i += num_procs) { 
		x = h * ((double)i - 0.5); 
		sum += (4.0 / (1.0 + x*x)); 
	} 

	mypi = h * sum;

	MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		printf("PI is approximately %.16f\n", pi);
	}
}

int main(int argc, char *argv[])
{
	int			proc_rank;
	int			num_procs;
	int			source;
	int			dest = 0;
	int			tag = 0;
	char		message[100];
	MPI_Status	status ;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank); 
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs); 
	if (proc_rank != 0)
    {
		snprintf(message,26, "Process %d", proc_rank);
		MPI_Send(message, strlen(message)+1, MPI_CHAR, dest, tag, MPI_COMM_WORLD);
	}
    else 
    {
		printf("Num processes: %d\n",num_procs);
		for (source = 1; source < num_procs; source++)
        {
			MPI_Recv(message, 100, MPI_CHAR, source, tag, MPI_COMM_WORLD, &status);
			printf("Process 0 received \"%s\"\n",message);
		}
		snprintf(message, 26, "Done");
	}

	MPI_Bcast(message, strlen(message)+1, MPI_CHAR, dest, MPI_COMM_WORLD);
	if (proc_rank != 0)
    {
		printf("Process %d received \"%s\"\n", proc_rank, message);
	}
	calc_pi(proc_rank, num_procs);
	MPI_Finalize(); 

	return 0;
}
