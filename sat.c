#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

// typedef struct {
//     float x, y;
//     float * vertices;
//     int num_vertices;
//     pthread_mutex_t lock;
// } Polygon;

typedef struct {
    int p1;
    int p2;
    float n_x, n_y; /* normal axis to the polygon */
    float penetration;
    int used_flag;
} Contact;

// static Polygon * polygons;
static Contact * contacts;

float * polygon_x;
float * polygon_y;
float ** polygon_vertices; // fixed to size [polygons][8]
int * polygon_num_vertices;
pthread_mutex_t * polygon_lock;

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
        pthread_mutex_init(&polygon_lock[i], NULL);

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

// void printPolygons() {
//     int i;
//     for(i = 0; i < num_polygons; i++) {
//         printf("Polygon %d: %d %d %d\n", i, polygons[i].x, polygons[i].y, polygons[i].num_vertices);
//     }
// }

static void * detectCollisions(void * r) {
    register int i, j;
    register long rank = (long) r;

    for(i = rank; i < num_polygons; i += num_threads) {
        for(j = i+1; j < num_polygons; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.
            // invoke cuda stuff
            float * i_edges = malloc(sizeof(int) * polygon_num_vertices[i] * 2);
            printf("Verts: %d\n", polygon_num_vertices[i]);
            int k;
            for(k = 0; k < 6; k+=2) {
                printf("i: %d k: %d limit: %d\n", i, k, polygon_num_vertices[i]);
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
            // float v2x = polygon_vertices[i][k];
            // float v2y = polygon_vertices[i][k+1];
            // float v1x = polygon_vertices[i][0];
            // float v1y = polygon_vertices[i][1];

            // float e_x = v2x - v1x;
            // float e_y = v2y - v1y;
            // printf("!\n");
            // i_edges[k] = e_x;
            // i_edges[k+1] = e_y;

            

            // float * j_edges = malloc(sizeof(int) * polygon_num_vertices[i] * 2);

            // for(k = 0; k < polygon_num_vertices[j]*2-2; k+=2) {
            //     float v2x = polygon_vertices[i][k+2];
            //     float v2y = polygon_vertices[i][k+3];
            //     float v1x = polygon_vertices[i][k];
            //     float v1y = polygon_vertices[i][k+1];

            //     float e_x = v2x - v1x;
            //     float e_y = v2y - v1y;

            //     j_edges[k] = e_x;
            //     j_edges[k+1] = e_y;
            // }

            // // add the last edge- last vertice to the first
            // v2x = polygon_vertices[j][k];
            // v2y = polygon_vertices[j][k+1];
            // v1x = polygon_vertices[j][0];
            // v1y = polygon_vertices[j][1];

            // e_x = v2x - v1x;
            // e_y = v2y - v1y;

            // j_edges[k] = e_x;
            // j_edges[k+1] = e_y;

            // printf("2!\n");

            // // merge i_edges and j_edges
            // float num_edges = (polygon_num_vertices[i]*2 + polygon_num_vertices[j]*2);
            // float * edges = malloc(sizeof(float) * num_edges);
            // memcpy(edges, i_edges, sizeof(float) * polygon_num_vertices[i]*2);
            // memcpy(edges + (polygon_num_vertices[i]*2), j_edges, sizeof(float) * polygon_num_vertices[j]*2);

            // float min_overlap = FLT_MAX;
            // float min_axis[2];
            // int collision = 1; // True
            // for(k = 0; k < num_edges; k+=2) {
            //     float i_proj[2], j_proj[2];

            //     // perp vector
            //     float axis[2];
            //     axis[0] = -1 * edges[k+1];
            //     axis[1] = edges[k];

            //     // normalize vector
            //     float esp = .0000001; // prevent division by zero by adding trivial amount
            //     float lengthsq = axis[0] * axis[0] + axis[1] * axis[1];
            //     /*
            //     axis[0] = axis[0] * rsqrtf(lengthsq + esp);
            //     axis[1] = axis[1] * rsqrtf(lengthsq + esp);
            //     */

            //     axis[0] = axis[0] / sqrtf(lengthsq + esp);
            //     axis[1] = axis[1] / sqrtf(lengthsq + esp);

            //     // project each polygon onto the axis
            //     float max = -FLT_MAX;
            //     float min = FLT_MAX;
            //     int l;
            //     for(l = 0; l < polygon_num_vertices[i]*2; l+=2) {
            //         int res = polygon_vertices[i][l] * axis[0] + polygon_vertices[i][l+1] * axis[1];
            //         if(max < res) { max = res; }
            //         if(min > res) { min = res; }
            //     }

            //     i_proj[0] = min;
            //     i_proj[1] = max;

            //     max = -FLT_MAX;
            //     min = FLT_MAX;
            //     for(l = 0; l < polygon_num_vertices[j]*2; l+=2) {
            //         int res = polygon_vertices[j][l] * axis[0] + polygon_vertices[j][l+1] * axis[1];
            //         if(max < res) { max = res; }
            //         if(min > res) { min = res; }
            //     }

            //     j_proj[0] = min;
            //     j_proj[1] = max;

            //     // check for overlap- determines the overlap of two line segments
            //     float overlap;
            //     if(i_proj[0] < j_proj[0])
            //         overlap = j_proj[0] - i_proj[1];
            //     else
            //         overlap = i_proj[0] - j_proj[1];

            //     if(overlap > 0) {
            //         // printf("Not intersecting!\n");
            //         collision = 0;
            //         break;
            //     }

            //     if(overlap < min_overlap) {
            //         min_overlap = overlap;
            //         min_axis[0] = axis[0];
            //         min_axis[1] = axis[1];
            //     }

                // printf("Overlap: %d\n", overlap);
            //}

            // if(collision == 1) {
            //     int index = rank * num_polygons + j;
            //     contacts[index].p1 = i;
            //     contacts[index].p2 = j;
            //     contacts[index].n_x = min_axis[0];
            //     contacts[index].n_y = min_axis[1];
            //     contacts[index].penetration = min_overlap;
            //     contacts[index].used_flag = 1;
            // }
            // printf("Intersecting!\n");

            // free(i_edges);
            // free(j_edges);
            // free(edges);
        }
    }
}

static void * updateBodies(void * r) {
    register int i;
    register long rank = (long) r;

    for(i = rank; i < num_contacts; i += num_threads) {
        if(contacts[i].used_flag == 1) {

            float half_pen = contacts[i].penetration/2;
            float dx = contacts[i].n_x * half_pen;
            float dy = contacts[i].n_y * half_pen;

            // get lock for p1
            pthread_mutex_t locki = polygon_lock[contacts[i].p1];
            pthread_mutex_lock(&locki);
            // update p1- moving it 1/2 the penetration along the normal axis
            polygon_x[contacts[i].p1] += dx;
            polygon_y[contacts[i].p1] += dy;
            // release lock
            pthread_mutex_unlock(&locki);

            // get lock for p2
            pthread_mutex_t lockj = polygon_lock[contacts[i].p2];
            pthread_mutex_lock(&lockj);
            // update p2- moving it 1/2 the penetration away on the normal axis
            polygon_x[contacts[i].p2] -= dx;
            polygon_y[contacts[i].p2] -= dy;
            // release lock
            pthread_mutex_unlock(&lockj);
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
    num_vertices = 4; // fixed for now

    printf("Separating Axis v1.0: %d polygons %d threads %d \n", num_polygons, num_threads, num_vertices);

    /* Allocate Arrays */
    
    polygon_x = malloc(sizeof(float) * num_polygons); 
    polygon_y = malloc(sizeof(float) * num_polygons);
    polygon_vertices = malloc(sizeof(float) * num_polygons);
    polygon_num_vertices = malloc(sizeof(int) * num_polygons);
    polygon_lock = malloc(sizeof(pthread_mutex_t) * num_polygons);

    contacts = malloc(sizeof(Contact) * num_contacts);
    for(i = 0; i < num_contacts; i++) { contacts[i].used_flag = 0; }    

    /* Generate Threads */
    pthread_t detect_threads[num_threads-1];
    pthread_t update_threads[num_threads-1];
    
    /* Generate Polygons */
    createPolygons();

    /* Start Time */
    gettimeofday(&start, NULL);

    /* CUDA kernel invocaiton */

    for(i = 0; i < num_threads-1; i++) {
        long rank = i+1;
        pthread_create(&detect_threads[i], NULL, detectCollisions, (void *)(rank));
    }

    detectCollisions(0);

    /* Join Threads */
    for(i = 0; i < num_threads-1; i++) {
        pthread_join(detect_threads[i], NULL);
    }
    
    // for(i = 0; i < num_threads-1; i++) {
    //     long rank = i+1;
    //     pthread_create(&update_threads[i], NULL, updateBodies, (void *)(rank));
    // }

    //  Have main process as well 
    // updateBodies(0);
    
    // /* Join Threads */
    // for(i = 0; i < num_threads-1; i++) {
    //     pthread_join(update_threads[i], NULL);
    // }

    /* End Time */
    gettimeofday(&end, NULL);

    /* Output Result */
    printf("runtime: %.4lf s\n", end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0);

    // free(polygons);
    // free(contacts);
    return 0;
}