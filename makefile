all: Ccode.o CUcode.o
  mpicc -xhost -pthread -O3 Ccode.o CUcode.o -lcudart -L$TACC_CUDA_LIB -o sat


CUCode.o: CudaSat.cu
  nvcc -O3 -arch=sm_35 -c cudaSat.cu -o CUcode.o -lcuda

Ccode.o: pthreadSat.c
  mpicc -xhost -pthread -O3 -c pthreadSat.c -o Ccode.o
  

clean:
  rm *.cu
  rm *.c
  rm *.o
