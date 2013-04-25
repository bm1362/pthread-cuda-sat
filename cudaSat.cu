#include <stdlib.h>
#include <sys/time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <cuda.h>

#define THREADS 512

/* Polygon 'Struct' */
static float * polygon_x;
static float * polygon_y;
static float ** polygon_vertices; // fixed to size [polygons][8]
static int * polygon_num_vertices;

/* Contact 'Struct' */
static int * contact_p1;
static int * contact_p2;
static float * contact_n_x; /* normal axis to the polygon */
static float * contact_n_y;
static float * contact_penetration;
static int * contact_used_flag;

static int num_polygons, num_threads, num_contacts, num_vertices;
void pthread_init(float * p_x, float * p_y, int * c_p1, int * c_p2, float * c_n_x, float * c_n_y, float * c_p, int * c_u_f, int n_polygons, int n_contacts, int n_threads);

void createPolygons() {
    int i = 0;
    for(i = 0; i < num_polygons; i++) {
        float x = 5; // generate position- needs to be random
        float y = 5; // generate position- needs to be random
        polygon_x[i] = i * 10 + x;
        polygon_y[i] = i * 10 + y;
        polygon_num_vertices[i] = num_vertices;
        polygon_vertices[i] = (float *) malloc(sizeof(float) * num_vertices * 2);

        /* make a square */
        int size = rand() % 10 + 3;
        polygon_vertices[i][0] = polygon_x[i];
        polygon_vertices[i][1] = polygon_y[i];

        polygon_vertices[i][2] = polygon_x[i]+size;
        polygon_vertices[i][3] = polygon_y[i];

        polygon_vertices[i][4] = polygon_x[i]+size;
        polygon_vertices[i][5] = polygon_y[i]-size;

        polygon_vertices[i][6] = polygon_x[i];
        polygon_vertices[i][7] = polygon_y[i]-size;
    }
}

void printPolygons() {
    int i;
    for(i = 0; i < num_polygons; i++) {
        printf("Polygon %d: %f %f %d\n", i, polygon_x[i], polygon_y[i], polygon_num_vertices[i]);
    }
}

__global__ void detectCollisions(int num_polygons, float * polygon_x, float * polygon_y, 
                                 int * polygon_num_vertices, float ** polygon_vertices, int * contact_p1, int * contact_p2,
                                 float * contact_n_x, float * contact_n_y, float * contact_penetration, int * contact_used_flag) {
    register int i, j;
    int rank = threadIdx.x + blockIdx.x * blockDim.x;


    if(rank < num_polygons) {
        for(j = i+1; j < num_polygons; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.

            // get edges
            float i_edges[4 * 2];

            int k;
            for(k = 0; k < 4 * 2 - 2; k+=2) {
                float v2x = polygon_vertices[i][k+2];
                float v2y = polygon_vertices[i][k+3];
                float v1x = polygon_vertices[i][k];
                float v1y = polygon_vertices[i][k+1];

                float e_x = v2x - v1x;
                float e_y = v2y - v1y;

                i_edges[k] = e_x;
                i_edges[k+1] = e_y;
            }

            // add the last edge- last vertice to the first
            float v2x = polygon_vertices[i][k];
            float v2y = polygon_vertices[i][k+1];
            float v1x = polygon_vertices[i][0];
            float v1y = polygon_vertices[i][1];

            float e_x = v2x - v1x;
            float e_y = v2y - v1y;

            i_edges[k] = e_x;
            i_edges[k+1] = e_y;            

            float j_edges[4 * 2];

            for(k = 0; k < 4*2-2; k+=2) {
                float v2x = polygon_vertices[j][k+2];
                float v2y = polygon_vertices[j][k+3];
                float v1x = polygon_vertices[j][k];
                float v1y = polygon_vertices[j][k+1];

                float e_x = v2x - v1x;
                float e_y = v2y - v1y;

                j_edges[k] = e_x;
                j_edges[k+1] = e_y;
            }

            // add the last edge- last vertice to the first
            v2x = polygon_vertices[j][k];
            v2y = polygon_vertices[j][k+1];
            v1x = polygon_vertices[j][0];
            v1y = polygon_vertices[j][1];

            e_x = v2x - v1x;
            e_y = v2y - v1y;

            j_edges[k] = e_x;
            j_edges[k+1] = e_y;

            // printf("2!\n");

            // merge i_edges and j_edges
            float num_edges = (4*2 + 4*2);
            float edges[4*2 + 4*2];
            memcpy(edges, i_edges, sizeof(float) * 4*2);
            memcpy(edges + (4*2), j_edges, sizeof(float) * 4*2);

            float min_overlap = FLT_MAX;
            float min_axis[2];
            int collision = 1; // True
            for(k = 0; k < num_edges; k+=2) {
                float i_proj[2], j_proj[2];

                // perp vector
                float axis[2];
                axis[0] = -1 * edges[k+1];
                axis[1] = edges[k];

                // normalize vector
                float esp = .0000001; // prevent division by zero by adding trivial amount
                float lengthsq = axis[0] * axis[0] + axis[1] * axis[1];
                /*
                axis[0] = axis[0] * rsqrtf(lengthsq + esp);
                axis[1] = axis[1] * rsqrtf(lengthsq + esp);
                */

                axis[0] = axis[0] / sqrtf(lengthsq + esp);
                axis[1] = axis[1] / sqrtf(lengthsq + esp);

                // project each polygon onto the axis
                float max = -FLT_MAX;
                float min = FLT_MAX;
                int l;
                for(l = 0; l < 4*2; l+=2) {
                    int res = polygon_vertices[i][l] * axis[0] + polygon_vertices[i][l+1] * axis[1];
                    if(max < res) { max = res; }
                    if(min > res) { min = res; }
                }

                i_proj[0] = min;
                i_proj[1] = max;

                max = -FLT_MAX;
                min = FLT_MAX;
                for(l = 0; l < 4*2; l+=2) {
                    int res = polygon_vertices[j][l] * axis[0] + polygon_vertices[j][l+1] * axis[1];
                    if(max < res) { max = res; }
                    if(min > res) { min = res; }
                }

                j_proj[0] = min;
                j_proj[1] = max;

                // check for overlap- determines the overlap of two line segments
                float overlap;
                if(i_proj[0] < j_proj[0])
                    overlap = j_proj[0] - i_proj[1];
                else
                    overlap = i_proj[0] - j_proj[1];

                if(overlap > 0) {
                    collision = 0;
                    break;
                }

                if(overlap < min_overlap) {
                    min_overlap = overlap;
                    min_axis[0] = axis[0];
                    min_axis[1] = axis[1];
                }
            }

            if(collision == 1) {
                int index = rank * num_polygons + j;
                contact_p1[index] = i;
                contact_p2[index] = j;
                contact_n_x[index] = min_axis[0];
                contact_n_y[index] = min_axis[1];
                contact_penetration[index] = min_overlap;
                contact_used_flag[index] = 1;
            }

            free(i_edges);
            free(j_edges);
            free(edges);
        }
    }
}


int main(int argc, char * argv[]) {
    register int blocks;
    /* Initialize */
    srand(time(NULL));
    register int i;
    struct timeval start, end;
    /* check command line */
    if (argc != 3) {
        fprintf(stderr, "usage: %s number_of_polygons number_of_threads\n", argv[0]);
        exit(-1);
    }

    num_polygons = atoi(argv[1]);
    num_threads = atoi(argv[2]);
    num_contacts = num_polygons * num_polygons;
    num_vertices = 4;

    printf("Separating Axis v1.0: %d polygons %d threads %d \n", num_polygons, num_threads, num_vertices);

    /* Allocate Arrays */
    polygon_x = (float *) malloc(sizeof(float) * num_polygons); 
    polygon_y = (float *) malloc(sizeof(float) * num_polygons);
    polygon_num_vertices = (int *) malloc(sizeof(int) * num_polygons);
    polygon_vertices = (float **) malloc(sizeof(float*) * num_polygons);
    

    contact_p1 = (int *) malloc(sizeof(int) * num_contacts);
    contact_p2 = (int *) malloc(sizeof(int) * num_contacts);
    contact_n_x = (float *) malloc(sizeof(float) * num_contacts);
    contact_n_y = (float *) malloc(sizeof(float) * num_contacts);
    contact_penetration = (float *) malloc(sizeof(float) * num_contacts);
    contact_used_flag = (int *) malloc(sizeof(int) * num_contacts);

    for(i = 0; i < num_contacts; i++) { contact_used_flag[i] = 0; }    


    /* Generate Polygons */
    createPolygons();

    // printPolygons();

    /* Start Time */
    gettimeofday(&start, NULL);

    // /* CUDA kernel invocaiton */
    blocks = (num_polygons + THREADS - 1) / THREADS;
    float *pd_x, *pd_y, **pdvert;
    int *pd_num_vert;
    int *cd_p1; int *cd_p2; float *cd_n_x; float *cd_n_y; float *cd_pen; int *cd_used_flag;

    if (cudaSuccess != cudaMalloc((void **)&pd_x, num_polygons * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&pd_y, num_polygons * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&pd_num_vert, num_polygons * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void ***)&pdvert, num_polygons * sizeof(float*))) fprintf(stderr, "could not allocate array\n");

    if (cudaSuccess != cudaMalloc((void **)&cd_p1, num_contacts * sizeof(int))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&cd_p2, num_contacts * sizeof(int))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&cd_n_x, num_contacts * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&cd_n_y, num_contacts * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&cd_pen, num_contacts * sizeof(float))) fprintf(stderr, "could not allocate array\n");
    if (cudaSuccess != cudaMalloc((void **)&cd_used_flag, num_contacts * sizeof(int))) fprintf(stderr, "could not allocate array\n");

    if (cudaSuccess != cudaMemcpy(pd_x, polygon_x, num_polygons * sizeof(float), cudaMemcpyHostToDevice)) fprintf(stderr, "copying of polygon_x to device failed\n");
    if (cudaSuccess != cudaMemcpy(pd_y, polygon_y, num_polygons * sizeof(float), cudaMemcpyHostToDevice)) fprintf(stderr, "copying of polygon_y to device failed\n");
    if (cudaSuccess != cudaMemcpy(pd_num_vert, polygon_num_vertices, num_polygons * sizeof(int), cudaMemcpyHostToDevice)) fprintf(stderr, "copying of polygon_num_vertices to device failed\n");
    if (cudaSuccess != cudaMemcpy(pdvert, polygon_vertices, num_polygons * sizeof(float*), cudaMemcpyHostToDevice)) fprintf(stderr, "copying of polygon_vertices to device failed\n");
    detectCollisions<<<blocks, THREADS>>>(num_polygons, pd_x, pd_y, pd_num_vert, pdvert, cd_p1, cd_p2, cd_n_x, cd_n_y, cd_pen, cd_used_flag);
    if (cudaSuccess != cudaMemcpy(contact_p1, cd_p1, num_contacts * sizeof(int), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_p1 from device failed\n");
    if (cudaSuccess != cudaMemcpy(contact_p2, cd_p2, num_contacts * sizeof(int), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_p2 from device failed\n");
    if (cudaSuccess != cudaMemcpy(contact_n_x, cd_n_x, num_contacts * sizeof(float), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_n_x from device failed\n");
    if (cudaSuccess != cudaMemcpy(contact_n_y, cd_n_y, num_contacts * sizeof(float), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_n_y from device failed\n");
    if (cudaSuccess != cudaMemcpy(contact_penetration, cd_pen, num_contacts * sizeof(float), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_pen from device failed\n");
    if (cudaSuccess != cudaMemcpy(contact_used_flag, cd_used_flag, num_contacts * sizeof(int), cudaMemcpyDeviceToHost)) fprintf(stderr, "copying of cd_used_flag from device failed\n");
    /* Execute pthread_init */
    pthread_init(polygon_x, polygon_y, contact_p1, contact_p2, contact_n_x, contact_n_y, contact_penetration, contact_used_flag, num_polygons, num_contacts, num_threads);

    /* End Time */
    gettimeofday(&end, NULL);

    /* Output Result */
    printf("runtime: %.4lf s\n", end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0);

    // free(polygons);
    // free(contacts);
    return 0;
}