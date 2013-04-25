#include <stdlib.h>
#include <pthread.h>

static float * polygon_x;
static float * polygon_y;
static pthread_mutex_t * polygon_lock;
static int * contact_p1;
static int * contact_p2;
static float * contact_n_x; /* normal axis to the polygon */
static float * contact_n_y;
static float * contact_penetration;
static int * contact_used_flag;

static int num_contacts, num_polygons, num_threads;

static void * updateBodies(void * r) {
    register int i;
    register long rank = (long) r;

    for(i = rank; i < num_contacts; i += num_threads) {
        if(contact_used_flag[i] == 1) {
            float half_pen = contact_penetration[i]/2;
            float dx = contact_n_x[i] * half_pen;
            float dy = contact_n_y[i] * half_pen;

            int p1 = contact_p1[i];
            int p2 = contact_p2[i];
            // get lock for p1
            pthread_mutex_lock(&polygon_lock[p1]);
            // update p1- moving it 1/2 the penetration along the normal axis
            polygon_x[p1] += dx;
            polygon_y[p1] += dy;
            // release lock
            pthread_mutex_unlock(&polygon_lock[p1]);

            // get lock for p2
            pthread_mutex_lock(&polygon_lock[p2]);
            // update p2- moving it 1/2 the penetration away on the normal axis
            polygon_x[p2] -= dx;
            polygon_y[p2] -= dy;
            // release lock
            pthread_mutex_unlock(&polygon_lock[p2]);

        }
    }
    return NULL;
}

void pthread_init(float * p_x, float * p_y, int * c_p1, int * c_p2, float * c_n_x, float * c_n_y, float * c_p, int * c_u_f, int n_polygons, int n_contacts, int n_threads){
	register int i;
	pthread_t * update_threads;
    polygon_x = p_x;
    polygon_y = p_y;
    contact_p1 = c_p1;
    contact_p2 = c_p2;
    contact_n_x = c_n_x;
    contact_n_y = c_n_y;
    contact_penetration = c_p;
    contact_used_flag = c_u_f;
    num_polygons = n_polygons;
    num_contacts = n_contacts;
    num_threads = n_threads;

	/* Generate threads */
	polygon_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t*) * num_polygons);
	update_threads = malloc((num_threads - 1) * sizeof(pthread_t));

	for(i = 0; i < num_polygons; i++){
		pthread_mutex_init(&polygon_lock[i], NULL);
	}

	for(i = 0; i < num_threads-1; i++) {
        long rank = i+1;
        pthread_create(&update_threads[i], NULL, updateBodies, (void *)(rank));
    }

    updateBodies(0);
    
    /* Join Threads */
    if(num_threads > 1)
	    for(i = 0; i < num_threads-1; i++) {
	        pthread_join(update_threads[i], NULL);
	    }
}