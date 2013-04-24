#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

typedef struct {
    float x, y;
    float * vertices;
    int num_vertices;
    pthread_mutex_t lock;
} Polygon;

typedef struct {
    Polygon * p1;
    Polygon * p2;
    float n_x, n_y; /* normal axis to the polygon */
    float penetration;
    int used_flag;
} Contact;

static Polygon * polygons;
static Contact * contacts;
int num_polygons, num_threads, num_contacts;

void square(Polygon * polygon, int size) {
    polygon->vertices[0] = polygon->x;
    polygon->vertices[1] = polygon->y;

    polygon->vertices[2] = polygon->x+size;
    polygon->vertices[3] = polygon->y;

    polygon->vertices[4] = polygon->x+size;
    polygon->vertices[5] = polygon->y-size;

    polygon->vertices[6] = polygon->x;
    polygon->vertices[7] = polygon->y-size;
}

void createPolygons() {
    int i = 0;
    for(i = 0; i < num_polygons; i++) {
        float x = 5; // generate position
        float y = 5; // generate position
        int num_vertices = 4; // generate as well
        polygons[i].x = i * 10 + x;
        polygons[i].y = i * 10 + y;
        polygons[i].num_vertices = num_vertices;
        polygons[i].vertices = malloc(sizeof(float) * num_vertices * 2);
        pthread_mutex_init(&polygons[i].lock, NULL);

        int size = rand() % 10 + 3;
        square(&polygons[i], size);
    }
}

void printPolygons() {
    int i;
    for(i = 0; i < num_polygons; i++) {
        printf("Polygon %d: %d %d %d\n", i, polygons[i].x, polygons[i].y, polygons[i].num_vertices);
    }
}

/* Projects a polygon onto a axis and returns the min, max positions. */
void projectPolygon(Polygon * polygon, float axis[2], float ret[2]) {
    float max = -FLT_MAX;
    float min = FLT_MAX;
    int i;
    for(i = 0; i < polygon->num_vertices*2; i+=2) {
        int res = polygon->vertices[i] * axis[0] + polygon->vertices[i+1] * axis[1];

        if(max < res) { max = res; }
        if(min > res) { min = res; }
    }

    ret[0] = min;
    ret[1] = max;
}

float * getEdges(Polygon * polygon) {
    int num_verts = polygon->num_vertices * 2;
    float * edges = malloc(sizeof(int) * num_verts);

    int i;
    for(i = 0; i < num_verts-2; i+=2) {
        float v2x = polygon->vertices[i+2];
        float v2y = polygon->vertices[i+3];
        float v1x = polygon->vertices[i];
        float v1y = polygon->vertices[i+1];

        float e_x = v2x - v1x;
        float e_y = v2y - v1y;

        edges[i] = e_x;
        edges[i+1] = e_y;
    }

    // add the last edge- last vertice to the first
    float v2x = polygon->vertices[i];
    float v2y = polygon->vertices[i+1];
    float v1x = polygon->vertices[0];
    float v1y = polygon->vertices[1];

    float e_x = v2x - v1x;
    float e_y = v2y - v1y;

    edges[i] = e_x;
    edges[i+1] = e_y;

    return edges;
}

static void * detectCollisions(void * r) {
    register int i, j;
    register long rank = (long) r;
    for(i = rank; i < num_polygons; i += num_threads) {
        for(j = i+1; j < num_polygons; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.
            // invoke cuda stuff
            float * i_edges = getEdges(&polygons[i]);
            float * j_edges = getEdges(&polygons[j]);
            
            // merge i_edges and j_edges
            float num_edges = (polygons[i].num_vertices*2 + polygons[j].num_vertices*2);
            float * edges = malloc(sizeof(float) * num_edges);
            memcpy(edges, i_edges, sizeof(float) * polygons[i].num_vertices*2);
            memcpy(edges + (polygons[i].num_vertices*2), j_edges, sizeof(float) * polygons[j].num_vertices*2);

            int k = 0;
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
                projectPolygon(&polygons[i], axis, i_proj);
                projectPolygon(&polygons[j], axis, j_proj);

                // check for overlap- determines the overlap of two line segments
                float overlap;
                if(i_proj[0] < j_proj[0])
                    overlap = j_proj[0] - i_proj[1];
                else
                    overlap = i_proj[0] - j_proj[1];

                if(overlap > 0) {
                    // printf("Not intersecting!\n");
                    collision = 0;
                    break;
                }

                if(overlap < min_overlap) {
                    min_overlap = overlap;
                    min_axis[0] = axis[0];
                    min_axis[1] = axis[1];
                }

                // printf("Overlap: %d\n", overlap);
            }

            if(collision == 1) {
                int index = rank * num_polygons + j;
                contacts[index].p1 = &polygons[i];
                contacts[index].p2 = &polygons[j];
                contacts[index].n_x = min_axis[0];
                contacts[index].n_y = min_axis[1];
                contacts[index].penetration = min_overlap;
                contacts[index].used_flag = 1;
            }
            // printf("Intersecting!\n");

            free(i_edges);
            free(j_edges);
            free(edges);
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
            pthread_mutex_lock(&contacts[i].p1->lock);
            // update p1- moving it 1/2 the penetration along the normal axis
            contacts[i].p1->x += dx;
            contacts[i].p1->y += dy;
            // release lock
            pthread_mutex_unlock(&contacts[i].p1->lock);

            // get lock for p2
            pthread_mutex_lock(&contacts[i].p2->lock);
            // update p2- moving it 1/2 the penetration away on the normal axis
            contacts[i].p2->x -= dx;
            contacts[i].p2->y -= dy;
            // release lock
            pthread_mutex_unlock(&contacts[i].p2->lock);
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
    
    printf("Separating Axis v1.0: %d polygons %d threads\n", num_polygons, num_threads);

    /* Allocate Arrays */
    num_contacts = num_polygons;
    polygons = malloc(sizeof(Polygon) * num_polygons);
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

    /* Have main process as well */
    detectCollisions(0);

    /* Join Threads */
    for(i = 0; i < num_threads-1; i++) {
        pthread_join(detect_threads[i], NULL);
    }
    
    for(i = 0; i < num_threads-1; i++) {
        long rank = i+1;
        pthread_create(&update_threads[i], NULL, updateBodies, (void *)(rank));
    }

    /* Have main process as well */
    updateBodies(0);
    
    /* Join Threads */
    for(i = 0; i < num_threads-1; i++) {
        pthread_join(update_threads[i], NULL);
    }

    /* End Time */
    gettimeofday(&end, NULL);

    /* Output Result */
    printf("runtime: %.4lf s\n", end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0);

    free(polygons);
    free(contacts);
    return 0;
}