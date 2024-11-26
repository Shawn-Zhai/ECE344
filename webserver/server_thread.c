#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <string.h>

// Entry in hash table
struct node{
    struct file_data *file;
    struct node *next;
};

// Hash table with cache
struct hash_table{
    int cache_space_available;
    pthread_mutex_t *lock;
    struct node *LRU_cache;
    struct node *ht[20101];
};

unsigned long hash(char* file_name){
    int key = 0;
    for(int i = 0; i < strlen(file_name); i++){
        key = ((key << 5) + key) + file_name[i];
    }

    if(key < 0)
        key *= -1;

    key %= 20101;
    
    return key;
}

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	int *conn_buf;
	pthread_t *threads;
	int request_head;
	int request_tail;
	pthread_mutex_t mutex;
	pthread_cond_t prod_cond;
	pthread_cond_t cons_cond;	
};

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

// Global hash table
struct hash_table *hash_brown;

// Evict cache for insert
void cache_evict(int space_required){
    
    // Keep evicting until the new entry can be inserted
    while(space_required > 0){
        
        // Remove from cache and update spaces
        struct node* die = hash_brown -> LRU_cache;
        hash_brown -> LRU_cache = hash_brown -> LRU_cache -> next;
        die -> next = NULL;
        
        space_required -= die -> file -> file_size;
        hash_brown -> cache_space_available += die -> file -> file_size;
        
        // Remove from hash table
        unsigned long key = hash(die -> file -> file_name);
        struct node* cur = hash_brown -> ht[key];
        struct node* pre = cur;
        
        while(strcmp(cur -> file -> file_name, die -> file -> file_name) != 0){
            pre = cur;
            cur = cur -> next;
        }
        
        pre -> next = cur ->next;
        cur -> next = NULL;
        
        // Set the index to null if evicted node is the only one at it
        if(hash_brown -> ht[key] -> next == NULL)
            hash_brown -> ht[key] = NULL;
        
        // Free the memory
        file_data_free(cur -> file);
        free(cur);
        file_data_free(die -> file);
        free(die);
    }
    
    return;
}

// Insert file at the tail of the cache if it is not already in the cache
// head = least recently used, tail = most recently used
void cache_insert(struct file_data* file){
    
    // Get some free space in cache if there's not enough
    if(hash_brown -> cache_space_available < file -> file_size)
        cache_evict(file -> file_size - hash_brown -> cache_space_available);
    
    // Insert cache
    struct node *new_cache_entry = Malloc(sizeof(struct node));
    new_cache_entry -> file = file_data_init();
    new_cache_entry -> file -> file_buf = strdup(file -> file_buf);
    new_cache_entry -> file -> file_name = strdup(file -> file_name);
    new_cache_entry -> file -> file_size = file -> file_size;
    // new_cache_entry -> file = file;
    new_cache_entry -> next = NULL;
    
    // Cache is empty
    if(hash_brown -> LRU_cache == NULL)
        hash_brown -> LRU_cache = new_cache_entry;
    
    // Cache is not empty
    else{
        struct node * cur = hash_brown -> LRU_cache;
        
        // Find the last entry in the cache
        while(cur -> next != NULL)
            cur = cur -> next;
        
        cur -> next = new_cache_entry;
    }
    
    // Insert hash table
    struct node *new_ht_entry = Malloc(sizeof(struct node));
    new_ht_entry -> file = file_data_init();
    new_ht_entry -> file -> file_buf = strdup(file -> file_buf);
    new_ht_entry -> file -> file_name = strdup(file -> file_name);
    new_ht_entry -> file -> file_size = file -> file_size;
    //new_ht_entry -> file = file;
    new_ht_entry -> next = NULL;
    unsigned long key = hash(file -> file_name);
    
    // Index is empty
    if(hash_brown -> ht[key] == NULL)
        hash_brown -> ht[key] = new_ht_entry;
    
    // Index is not empty
    else{
        struct node * cur = hash_brown -> ht[key];
        
        // Find the last node at the index
        while(cur -> next != NULL)
            cur = cur -> next;
        
        cur -> next = new_ht_entry;
    }
    
    // Update cache space
    hash_brown -> cache_space_available -= file -> file_size;
    
    return;
}

// Check if a file is in the cache
// Cache content = hash table content
struct file_data* cache_lookup(struct file_data* file){
    
    unsigned long key = hash(file -> file_name);
    struct node *cur = hash_brown -> ht[key];
    
    while(cur != NULL){
        // file is in the cache
        if(strcmp(cur -> file -> file_name, file -> file_name) == 0)
            return cur -> file;
    }
    
    // file is not in the cache
    return NULL;
}

// If a file in the cache is accessed, move it to the tail making it most recently used
void cache_update(struct file_data* file){
    
    // If only one file in the cache
    if(hash_brown -> LRU_cache -> next == NULL)
        return;
    
    struct node *cur = hash_brown -> LRU_cache;
    struct node *pre = cur;
    
    // Look for the file
    while(cur != NULL){
        
        if(strcmp(cur -> file -> file_name, file -> file_name) == 0)
            break;
        
        pre = cur;
        cur = cur -> next;
    }
    
    // File is at the head
    if(strcmp(cur -> file -> file_name, hash_brown -> LRU_cache -> file -> file_name) == 0)
        hash_brown -> LRU_cache = hash_brown -> LRU_cache -> next;
    
    // Otherwise
    else
        pre -> next = cur -> next;
    
    // Put it at the tail
    cur -> next = NULL;
    struct node *tail = hash_brown -> LRU_cache;
    
    while(tail -> next != NULL)
        tail = tail -> next;
    
    tail -> next = cur;
    
    return;
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
        
        // See if file is in cache if it can fit
        struct file_data* file_in_cache = NULL;
        
        pthread_mutex_lock(hash_brown -> lock);
        
        if(data -> file_size <= sv -> max_cache_size){
            file_in_cache = cache_lookup(data);
            if(file_in_cache != NULL)
                cache_update(file_in_cache);
        }
            
        pthread_mutex_unlock(hash_brown -> lock);
        
        // Send the file if found in cache
        if(file_in_cache != NULL){
            request_set_data(rq, file_in_cache);
            request_sendfile(rq);
        }
        
        // Otherwise, read from disk
        else{
            /* read file, 
             * fills data->file_buf with the file contents,
             * data->file_size with file size. */
            ret = request_readfile(rq);
            if (ret == 0) { /* couldn't read file */
                    goto out;
            }
            /* send file to client */
            request_sendfile(rq);
            
            // Cache the file if the file size is less than the cache size
            pthread_mutex_lock(hash_brown -> lock);
            
            if(data -> file_size <= sv -> max_cache_size){
                if(cache_lookup(data) == NULL)
                    cache_insert(data);
            }

            pthread_mutex_unlock(hash_brown -> lock);
        }
out:
	request_destroy(rq);

        // Having this line is problematic
	file_data_free(data);
}

static void *
do_server_thread(void *arg)
{
	struct server *sv = (struct server *)arg;
	int connfd;

	while (1) {
		pthread_mutex_lock(&sv->mutex);
		while (sv->request_head == sv->request_tail) {
			/* buffer is empty */
			if (sv->exiting) {
				pthread_mutex_unlock(&sv->mutex);
				goto out;
			}
			pthread_cond_wait(&sv->cons_cond, &sv->mutex);
		}
		/* get request from tail */
		connfd = sv->conn_buf[sv->request_tail];
		/* consume request */
		sv->conn_buf[sv->request_tail] = -1;
		sv->request_tail = (sv->request_tail + 1) % sv->max_requests;
		
		pthread_cond_signal(&sv->prod_cond);
		pthread_mutex_unlock(&sv->mutex);
		/* now serve request */
		do_server_request(sv, connfd);
	}
out:
	return NULL;
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;
	int i;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	/* we add 1 because we queue at most max_request - 1 requests */
	sv->max_requests = max_requests + 1;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	/* Lab 4: create queue of max_request size when max_requests > 0 */
	sv->conn_buf = Malloc(sizeof(*sv->conn_buf) * sv->max_requests);
	for (i = 0; i < sv->max_requests; i++) {
		sv->conn_buf[i] = -1;
	}
	sv->request_head = 0;
	sv->request_tail = 0;

	/* Lab 5: init server cache and limit its size to max_cache_size */
        hash_brown = Malloc(sizeof(struct hash_table));
        hash_brown -> cache_space_available = max_cache_size;
        hash_brown -> LRU_cache = NULL;
        hash_brown -> lock = Malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(hash_brown -> lock, NULL);
        for(int i = 0; i < 20101; i++)
            hash_brown -> ht[i] = NULL;
        
	/* Lab 4: create worker threads when nr_threads > 0 */
	pthread_mutex_init(&sv->mutex, NULL);
	pthread_cond_init(&sv->prod_cond, NULL);
	pthread_cond_init(&sv->cons_cond, NULL);	
	sv->threads = Malloc(sizeof(pthread_t) * nr_threads);
	for (i = 0; i < nr_threads; i++) {
		SYS(pthread_create(&(sv->threads[i]), NULL, do_server_thread,
				   (void *)sv));
	}
	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */

		pthread_mutex_lock(&sv->mutex);
		while (((sv->request_head - sv->request_tail + sv->max_requests)
			% sv->max_requests) == (sv->max_requests - 1)) {
			/* buffer is full */
			pthread_cond_wait(&sv->prod_cond, &sv->mutex);
		}
		/* fill conn_buf with this request */
		assert(sv->conn_buf[sv->request_head] == -1);
		sv->conn_buf[sv->request_head] = connfd;
		sv->request_head = (sv->request_head + 1) % sv->max_requests;
		pthread_cond_signal(&sv->cons_cond);
		pthread_mutex_unlock(&sv->mutex);
	}
}

void
server_exit(struct server *sv)
{
	int i;
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	pthread_mutex_lock(&sv->mutex);
	sv->exiting = 1;
	pthread_cond_broadcast(&sv->cons_cond);
	pthread_mutex_unlock(&sv->mutex);
	for (i = 0; i < sv->nr_threads; i++) {
		pthread_join(sv->threads[i], NULL);
	}

	/* make sure to free any allocated resources */
        struct node *free_cache = hash_brown -> LRU_cache;
        struct node *temp = NULL;
        while(free_cache != NULL){
            temp = free_cache -> next;
            file_data_free(free_cache -> file);
            free(free_cache);
            free_cache = temp;
        }
        
        struct node *free_ht = NULL;
        temp = NULL;
        for(int i = 0; i < 20101; i++){
            free_ht = hash_brown -> ht[0];
            while(free_ht != NULL){
                temp = free_ht -> next;
                file_data_free(free_ht -> file);
                free(free_ht);
                free_ht = temp;
            }
        }
        
        pthread_mutex_destroy(hash_brown -> lock);
        
        free(hash_brown);
        
	free(sv->conn_buf);
	free(sv->threads);
	free(sv);
}
