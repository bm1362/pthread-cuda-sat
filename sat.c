#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int x, y;
    int * vertices;
    int num_vertices;
} Polygon;

typedef struct {
    Polygon * p1, p2;
    int n_x, n_y; /* normal axis to the polygon */
    int penetration;
} Contact;

static Polygon * polygons;
static Contact * contacts;
static int NUM_POLYGONS, NUM_THREADS;

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
    for(i = 0; i < NUM_POLYGONS; i++) {
        int x = 5; // generate position
        int y = 5; // generate position
        int num_vertices = 4; // generate as well
        polygons[i].x = i * 10 + x;
        polygons[i].y = i * 10 + y;
        polygons[i].num_vertices = num_vertices;
        polygons[i].vertices = malloc(sizeof(int) * num_vertices * 2);

        int size = rand() % 10 + 3;
        square(&polygons[i], size);

        // int k = 0;
        // for(k = 0; k < polygons[i].num_vertices*2; k+=2) {
        //     printf("polygon %d vertice %d: %d %d\n", i, k, polygons[i].vertices[k], polygons[i].vertices[k+1]);
        // }

        // int j = 0;
        // for(j = 0; j < num_vertices; j++) {
        //     int v1 = 5 * j; // generate random position, x > prev x
        //     int v2 = 6 * j; // generate random position, y > prev y produces trapezoids

        //     polygons[i].vertices[j] = v1;
        //     polygons[i].vertices[j+1] = v2;
        // }
    }
}

void printPolygons() {
    int i;
    for(i = 0; i < NUM_POLYGONS; i++) {
        printf("Polygon %d: %d %d %d\n", i, polygons[i].x, polygons[i].y, polygons[i].num_vertices);
    }
}

/* Projects a polygon onto a axis and returns the min, max positions. */
void projectPolygon(Polygon * polygon, int * axis, int * ret) {
    int max = -INT_MAX;
    int min = INT_MAX;
    int i;
    for(i = 0; i < polygon->num_vertices*2; i+=2) {
        int res = polygon->vertices[i] * axis[0] + polygon->vertices[i+1] * axis[1];

        if(max < res) { max = res; }
        if(min > res) { min = res; }
    }

    ret[0] = min;
    ret[1] = max;
}

int * getEdges(Polygon * polygon) {
    int num_verts = polygon->num_vertices * 2;
    int * edges = malloc(sizeof(int) * num_verts);

    int i;
    for(i = 0; i < num_verts-2; i+=2) {
        int v2x = polygon->vertices[i+2];
        int v2y = polygon->vertices[i+3];
        int v1x = polygon->vertices[i];
        int v1y = polygon->vertices[i+1];

        int e_x = v2x - v1x;
        int e_y = v2y - v1y;

        edges[i] = e_x;
        edges[i+1] = e_y;
    }

    // add the last edge- last vertice to the first
    int v2x = polygon->vertices[i];
    int v2y = polygon->vertices[i+1];
    int v1x = polygon->vertices[0];
    int v1y = polygon->vertices[1];

    int e_x = v2x - v1x;
    int e_y = v2y - v1y;

    edges[i] = e_x;
    edges[i+1] = e_y;

    return edges;
}

static void * detectCollisions(void * r) {
    int i;
    long rank = (long) r;
    for(i = rank; i < NUM_POLYGONS; i += NUM_THREADS) {
        int j = 0;
        for(j = i+1; j < NUM_POLYGONS; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.
            // invoke cuda stuff
            int * i_edges = getEdges(&polygons[i]);
            int * j_edges = getEdges(&polygons[j]);
            
            // merge i_edges and j_edges
            int num_edges = (polygons[i].num_vertices*2 + polygons[j].num_vertices*2);
            int * edges = malloc(sizeof(int) * num_edges);
            memcpy(edges, i_edges, sizeof(int) * polygons[i].num_vertices*2);
            memcpy(edges + (polygons[i].num_vertices*2), j_edges, sizeof(int) * polygons[j].num_vertices*2);

            int k = 0;
            for(k = 0; k < num_edges; k+=2) {
                int i_proj[2], j_proj[2];

                // project each polygon onto the axis
                projectPolygon(&polygons[i], &edges[k], i_proj);
                projectPolygon(&polygons[j], &edges[k], j_proj);

                // check for overlap- determines the overlap of two line segments
                int overlap;
                if(i_proj[0] < j_proj[0])
                    overlap = j_proj[0] - i_proj[1];
                else
                    overlap = i_proj[0] - j_proj[1];

                if(overlap > 0) {
                    // printf("Not intersecting!\n");
                    break;
                }
                // printf("Overlap: %d\n", overlap);
            }

            // printf("Intersecting!\n");

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

    NUM_POLYGONS = atoi(argv[1]);
    NUM_THREADS = atoi(argv[2]);
    
    printf("Separating Axis v1.0: %d polygons %d threads\n", NUM_POLYGONS, NUM_THREADS);

    /* Generate Threads */
    pthread_t threads[NUM_THREADS-1];

    /* Generate Polygons */
    polygons = malloc(sizeof(Polygon) * NUM_POLYGONS);
    createPolygons();

    /* Start Time */
    gettimeofday(&start, NULL);

    for(i = 0; i < NUM_THREADS-1; i++) {
        long rank = i+1;
        pthread_create(&threads[i], NULL, detectCollisions, (void *)(rank));
    }

    /* Have main process as well */
    detectCollisions(0);

    /* Join Threads */
    for(i = 0; i < NUM_THREADS-1; i++) {
        pthread_join(threads[i], NULL);
    }

    /* End Time */
    gettimeofday(&end, NULL);

    /* Output Result */
    printf("runtime: %.4lf s\n", end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0);

    free(polygons);
    free(contacts);
    return 0;
}