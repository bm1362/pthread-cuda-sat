#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

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

__device__ void detectCollisions() {
    register int i, j;
    register long rank = (long) r;

    for(i = rank; i < num_polygons; i += num_threads) {
        for(j = i+1; j < num_polygons; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.

            // get edges
            float * i_edges = malloc(sizeof(int) * polygon_num_vertices[i] * 2);

            int k;
            for(k = 0; k < polygon_num_vertices[i] * 2 - 2; k+=2) {
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

            float * j_edges = malloc(sizeof(int) * polygon_num_vertices[j] * 2);

            for(k = 0; k < polygon_num_vertices[j]*2-2; k+=2) {
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
            float num_edges = (polygon_num_vertices[i]*2 + polygon_num_vertices[j]*2);
            float * edges = malloc(sizeof(float) * num_edges);
            memcpy(edges, i_edges, sizeof(float) * polygon_num_vertices[i]*2);
            memcpy(edges + (polygon_num_vertices[i]*2), j_edges, sizeof(float) * polygon_num_vertices[j]*2);

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
                for(l = 0; l < polygon_num_vertices[i]*2; l+=2) {
                    int res = polygon_vertices[i][l] * axis[0] + polygon_vertices[i][l+1] * axis[1];
                    if(max < res) { max = res; }
                    if(min > res) { min = res; }
                }

                i_proj[0] = min;
                i_proj[1] = max;

                max = -FLT_MAX;
                min = FLT_MAX;
                for(l = 0; l < polygon_num_vertices[j]*2; l+=2) {
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

    //detectCollisions(0);

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