pthread-cuda-sat
================

A implementation of Separating Axis Theorem using pthreads and cuda. Currently fixed to 4 vertices, builds squares.
This was last minute effort- doesn't perform as expected.


Compiling (on TACC/Stampede):
	
	module load cuda
	nvcc -O3 -arch=sm_35 -c cudaSat.cu -o CUcode.o -lcuda
	mpicc -xhost -pthread -O3 -c pthreadSat.c -o Ccode.o

Linking:

	mpicc -xhost -pthread -O3 Ccode.o CUcode.o -lcudart -L$TACC_CUDA_LIB -o sat

Usage:

	./sat <num polygons> <num threads>
