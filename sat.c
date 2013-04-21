#include <stdlib.h>
#include <pthread.h>

#define NUM_POLYGONS 10
#define NUM_THREADS 4

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

void createPolygons() {
    int i = 0;
    for(i = 0; i < NUM_POLYGONS; i++) {
        int x = 5; // generate position
        int y = 5; // generate position
        int num_vertices = 4; // generate as well
        polygons[i].x = x;
        polygons[i].y = y;
        polygons[i].num_vertices = num_vertices;
        polygons[i].vertices = malloc(sizeof(int) * num_vertices);

        int j = 0;
        for(j = 0; j < num_vertices; j++) {
            int v1 = 5; // generate random position, x > prev x
            int v2 = 6; // generate random position, y > prev y produces trapezoids

            polygons[i].vertices[j] = v1;
            polygons[i].vertices[j+1] = v2;
        }
    }
}

void printPolygons() {
    int i;
    for(i = 0; i < NUM_POLYGONS; i++) {
        printf("Polygon %d: %d %d %d\n", i, polygons[i].x, polygons[i].y, polygons[i].num_vertices);
    }
}

int * getEdges(Polygon * polygon) {
    num_verts = polygon.num_vertices;
    int * edges = malloc(sizeof(int) * num_verts);
    int i;
    for(i = 0; i < num_verts; i++) {
        
    }
}

static void * detectCollisions(void * r) {
    int i;
    long rank = (long) r;
    for(i = rank+1; i < NUM_POLYGONS; i += NUM_THREADS) {
        int j = 0;
        for(j = i; j < NUM_POLYGONS; j++) { // prevents duplicate checks- each polygon only checks the one behind it in the list.
            // invoke cuda stuff
            // int i_edges[]
        }
    }
}

int main() {

    register int i;
    struct timeval start, end;

    // generate polygons
    polygons = malloc(sizeof(Polygon) * NUM_POLYGONS);
    createPolygons();
    printPolygons();

    printf("Separating Axis v1.0: %d polygons %d threads\n", NUM_POLYGONS, NUM_THREADS);

    // generate threads
    pthread_t threads[NUM_THREADS-1];
    
    /* start time */
    gettimeofday(&start, NULL);

    for(i = 0; i < NUM_THREADS-1; i++) {
        long rank = i+1;
        pthread_create(&threads[i], NULL, detectCollisions, (void *)(rank));
    }

    /* have main process as well */
    detectCollisions(0);

    /* join threads */
    for(i = 0; i < NUM_THREADS-1; i++) {
        pthread_join(threads[i], NULL);
    }

    /* end time */
    gettimeofday(&end, NULL);

    /* output result */
    printf("runtime: %.4lf s\n", end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0);

    return 0;
}

// pthreads function detect_collisions(polygons):
// for i in polygons:
// for j in polygons, where j > i:
// kernel: i_edges = get_edges(i)
// kernel: j_edges = get_edges(j)
// edges = i_edges + j_edges
// kernel: check_edges(edges)