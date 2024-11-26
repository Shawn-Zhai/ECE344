#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

typedef enum {
	EMPTY = 0,
        READY = 1,
	RUNNING = 2,
	EXITED = 3,
        KILLED = 4,
        BLOCKED = 5
}State;

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
        struct thread* head;
        struct thread* tail;
};

/* This is the thread control block */
struct thread {
	/* ... Fill this in ... */
        Tid threadID;
        State state;
        ucontext_t context;
        void* stack;
        struct thread* next;
        int wait_successful;
};

// Push thread into a wait queue
void Wqueue_push(struct wait_queue* q, struct thread* t){
    
    t -> next = NULL;
    
    // Queue is empty
    if(q -> head == NULL || q -> tail == NULL){
        q -> head = t;
        q -> tail = t;
        return;
    }
    
    // Queue is not empty
    q -> tail -> next = t;
    q -> tail = t;
}

// Pop the first element in the wait queue
struct thread* Wqueue_pop(struct wait_queue* q){
    
    // Queue is empty
    if(q -> head == NULL)
        return NULL;
    
    // Queue only has one thread
    else if(q -> head == q -> tail){
        struct thread* front = q -> head;
        q -> head = NULL;
        q -> tail = NULL;
        front -> next = NULL;
        return front;
    }
    
    // Queue has more than one node
    struct thread* front = q -> head;
    q -> head = q -> head -> next;
    front -> next = NULL;
    return front;
}

// Remove a thread from a wait queue
void Wqueue_remove(struct wait_queue* q, Tid ID){
    struct thread* cur = q -> head;
    struct thread* pre = NULL;
    
    while(cur != NULL){
        
        // Found
        if(cur -> threadID == ID){
            
            // Only one thread in the queue
            if(cur == q -> head && cur == q -> tail){
                q -> head = NULL;
                q -> tail = NULL;
                cur -> next = NULL;
                return;
            }
            
            // Remove head
            else if(cur == q -> head){
                q -> head = cur -> next;
                cur -> next = NULL;
                return;
            }
            
            // Remove tail
            else if(cur == q -> tail){
                q -> tail = pre;
                pre -> next = NULL;
                cur -> next = NULL;
                return;
            }
            
            // Remove middle threads
            else{
                pre -> next = cur -> next;
                cur -> next = NULL;
                return;
            }
        }
        
        // Advance pointers
        pre = cur;
        cur = cur -> next;
    }
    
    return;
}

/* Ready queue */
struct thread_queue{
    struct thread* head;
    struct thread* tail;
};

// Push thread into a queue
void queue_push(struct thread_queue* q, struct thread* t){
    
    t -> next = NULL;
    
    // Queue is empty
    if(q -> head == NULL || q -> tail == NULL){
        q -> head = t;
        q -> tail = t;
        return;
    }
    
    // Queue is not empty
    q -> tail -> next = t;
    q -> tail = t;
}

// Pop the first element in the queue
struct thread* queue_pop(struct thread_queue* q){
    
    // Queue is empty
    if(q -> head == NULL)
        return NULL;
    
    // Queue only has one thread
    else if(q -> head == q -> tail){
        struct thread* front = q -> head;
        q -> head = NULL;
        q -> tail = NULL;
        front -> next = NULL;
        return front;
    }
    
    // Queue has more than one node
    struct thread* front = q -> head;
    q -> head = q -> head -> next;
    front -> next = NULL;
    return front;
}

// Get a thread from the queue
struct thread* queue_fetch(struct thread_queue* q, Tid ID){
    struct thread* cur = q -> head;
    
    while(cur != NULL){
        if(cur -> threadID == ID)
            return cur;
        cur = cur -> next;
    }
    
    return NULL;
}

// Remove a thread from the queue
void queue_remove(struct thread_queue* q, Tid ID){
    struct thread* cur = q -> head;
    struct thread* pre = NULL;
    
    while(cur != NULL){
        
        // Found
        if(cur -> threadID == ID){
            
            // Only one thread in the queue
            if(cur == q -> head && cur == q -> tail){
                q -> head = NULL;
                q -> tail = NULL;
                cur -> next = NULL;
                return;
            }
            
            // Remove head
            else if(cur == q -> head){
                q -> head = cur -> next;
                cur -> next = NULL;
                return;
            }
            
            // Remove tail
            else if(cur == q -> tail){
                q -> tail = pre;
                pre -> next = NULL;
                cur -> next = NULL;
                return;
            }
            
            // Remove middle threads
            else{
                pre -> next = cur -> next;
                cur -> next = NULL;
                return;
            }
        }
        
        // Advance pointers
        pre = cur;
        cur = cur -> next;
    }
    
    return;
}

/* Global variables*/
struct thread all_threads[THREAD_MAX_THREADS];
struct wait_queue all_wait_queues[THREAD_MAX_THREADS];
int exit_codes[THREAD_MAX_THREADS];
struct thread* running_thread;
int thread_counter;
struct thread_queue ready_queue;
struct thread_queue ID_reuse_queue;
 
// Destroy threads that have exited
void thread_destroy(){
    struct thread* cur = ID_reuse_queue.head;
    
    while(cur != NULL){
        free(cur -> stack);
        cur -> stack = NULL;
        
        cur = cur -> next;
    }
}

void thread_awaken(Tid tid){
    for(int i = 0; i < THREAD_MAX_THREADS; i++){
        Wqueue_remove(&all_wait_queues[i], tid);
    }
}

void
thread_init(void)
{
	/* your optional code here */
        
        int enabled = interrupts_off();
    
        // Initialize the control block for all threads
        for(int ID = 0; ID < THREAD_MAX_THREADS; ID++){
            all_threads[ID].threadID = ID;
            all_threads[ID].state = EMPTY;
            all_threads[ID].stack = NULL;
            all_threads[ID].next = NULL;
            
            // Initialize the wait queue for all threads
            all_wait_queues[ID].head = NULL;
            all_wait_queues[ID].tail = NULL;
            
            // Initialize the exit code for all threads
            exit_codes[ID] = 123;
            
            // Flag for each thread to ensure only one thread waiting on this thread return successfully
            all_threads[ID].wait_successful = 1;
        }
        
        // Initialize thread counter and make the main thread the running thread
        thread_counter = 1;
        running_thread = &all_threads[0];
        running_thread -> state = RUNNING;
        
        ready_queue.head = NULL;
        ready_queue.tail = NULL;
        
        ID_reuse_queue.head = NULL;
        ID_reuse_queue.tail = NULL;
        
        // Custom create main thread 0, but passing the tester still without this getcontext call
        // getcontext(&(running_thread -> context));
        
        interrupts_set(enabled);
}

Tid
thread_id()
{
	//TBD();
	return running_thread -> threadID;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
        interrupts_on();
    
        // Exit the running thread if it is KILLED
        if(running_thread -> state == KILLED){
            thread_exit(9);
        }
        
        thread_main(arg); // call thread_main() function with arg
        thread_exit(0);
        
        // Adding this gets rid of the seg fault after the basic test is done, but why?
        exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	//TBD();
    
        int enabled = interrupts_set(0);
    
        // Function fails due to number of threads exceeding limit and no ID can be reused
        if(thread_counter >= THREAD_MAX_THREADS && ID_reuse_queue.head == NULL)
            return THREAD_NOMORE;
        
        // Find an available ID for the new thread
        int ID;
        
        // Use the counter if it is within the limit
        if(thread_counter < THREAD_MAX_THREADS){
            ID = thread_counter;
            thread_counter++;
        }
        
        // Check if there is something in the ID reuse queue
        else{
            
            // If all used up, function fails due to number of threads exceeding limit
            if(ID_reuse_queue.head == NULL)
                return THREAD_NOMORE;
            
            struct thread* front = queue_pop(&ID_reuse_queue);
            ID = front -> threadID;
        }
        
        // Function fails due to stack memory allocation failure
        void* stack = malloc(THREAD_MIN_STACK);
        if(stack == NULL)
            return THREAD_NOMEMORY;
        
        // Reset the flag
        all_threads[ID].wait_successful = 1;
        
	all_threads[ID].state = READY;
	all_threads[ID].stack = stack;
        
        // Save the context of the created thread here, next time it gets yielded, it goes to execute
        // fn whose address is set below
        getcontext(&(all_threads[ID].context));
        
        // Set up u_context fields
        all_threads[ID].context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
	all_threads[ID].context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
	all_threads[ID].context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;
        
	stack += THREAD_MIN_STACK;
        
        // Frame pointer bit alignment
	stack -= ((unsigned long)stack) % 16;
        
        // Alternate stack and flag
        all_threads[ID].context.uc_stack.ss_sp = stack;
        all_threads[ID].context.uc_stack.ss_flags = 0;
        
        // Stack pointer for Stub
	stack -= 8; 
	all_threads[ID].context.uc_stack.ss_size = THREAD_MIN_STACK;
	all_threads[ID].context.uc_link = NULL;
	all_threads[ID].context.uc_mcontext.gregs[REG_RSP] = (unsigned long)stack;
        
        // Push into ready queue
	queue_push(&ready_queue, &all_threads[ID]);
        
        interrupts_set(enabled);
        
	return all_threads[ID].threadID;
}

Tid
thread_yield(Tid want_tid)
{
	//TBD();
    
        int enabled = interrupts_set(0);
    
        // Destroy threads that have exited
        thread_destroy();
        
        //printf("state1: %d\n", running_thread -> state);
        
        // If yield to running thread
        if(want_tid == THREAD_SELF || want_tid == running_thread -> threadID){
            interrupts_set(enabled);
            return thread_id();
        }
        
        // If run any ready thread
        else if (want_tid == THREAD_ANY){
            
            // Function fails due to no ready threads
            if(ready_queue.head == NULL){
                interrupts_set(enabled);
                return THREAD_NONE;
            }
            
            // Put current thread into corresponding queue
            if (running_thread -> state == EXITED || running_thread -> state == KILLED){
                queue_push(&ID_reuse_queue, running_thread);
            }
            else if(running_thread -> state != BLOCKED){
                running_thread -> state = READY;
                queue_push(&ready_queue, running_thread);
            }
            
            Tid yielded_thread_ID = ready_queue.head -> threadID;
            
            // Flag to control two returns of getcontext
            int current_thread_gets_yielded = 1;
            getcontext(&(running_thread -> context));
            
            // If the running thread gets yielded later, return the ID of the thread it yielded to
            current_thread_gets_yielded *= -1;
            if(current_thread_gets_yielded == 1){
                
                // Adding this passes test_wait_kill :D Need to check if the thread becomes KILLED after returning from context switch
                if(running_thread -> state == KILLED)
                    thread_exit(9);
                
                interrupts_set(enabled);
                return yielded_thread_ID;
            }
            
            // Make the yielded thread the running thread
            struct thread* front = queue_pop(&ready_queue);
            
            // If the yielded thread is in KILLED state, exit it in its stub, otherwise make it running
            if(front -> state != KILLED)
                front -> state = RUNNING;
            
            running_thread = front;
            
            setcontext(&(front -> context));
        }
        
        // want_tid is a specific thread ID
        struct thread* yielded_thread = queue_fetch(&ready_queue, want_tid);
        
        // want tid is not in the ready queue
        if(yielded_thread == NULL){
            interrupts_set(enabled);
            return THREAD_INVALID;
        }
        
        // Remove the yielded thread from the ready queue
        queue_remove(&ready_queue, want_tid);
        
        // Put current thread into corresponding queue
        if (running_thread -> state == EXITED || running_thread -> state == KILLED){
            queue_push(&ID_reuse_queue, running_thread);
        }
        else if(running_thread -> state != BLOCKED){
            running_thread -> state = READY;
            queue_push(&ready_queue, running_thread);
        }
        
        // Flag to control two returns of getcontext
        int current_thread_gets_yielded2 = 1;
        getcontext(&(running_thread -> context));
        
        // If the running thread gets yielded later, return the ID of the thread it yielded to
        current_thread_gets_yielded2 *= -1;
        if(current_thread_gets_yielded2 == 1){
            
            // Adding this passes test_wait_kill :D Need to check if the thread becomes KILLED after returning from context switch
            if(running_thread -> state == KILLED)
                thread_exit(9);
            
            interrupts_set(enabled);
            return want_tid;
        }
        
        // If the yielded thread is in KILLED state, exit it in its stub, otherwise make it running
        if(yielded_thread -> state != KILLED)
            yielded_thread -> state = RUNNING;
        
	running_thread = yielded_thread;
        
	setcontext(&(yielded_thread -> context));
        
        // Cannot reach here
	return -1;
}

void
thread_exit(int exit_code)
{
	//TBD();
    
        int enabled = interrupts_set(0);
    
        // Mark the thread as exited and save its exit code
        running_thread -> state = EXITED;
        exit_codes[running_thread -> threadID] = exit_code;
        
        // Wake up all threads waiting on this thread
        thread_wakeup(&(all_wait_queues[running_thread -> threadID]), 1);
        
        // Yield to the first thread in the ready queue
        thread_yield(THREAD_ANY);
        
        interrupts_set(enabled);
        
        return;
}

Tid
thread_kill(Tid tid)
{
	//TBD();
    
        int enabled = interrupts_set(0);
        // Running thread cannot kill thread with out of bound IDs, threads that are empty or have already exited, or itself
        if(tid >= THREAD_MAX_THREADS || tid < 0 || tid == running_thread -> threadID ||
           all_threads[tid].state == EXITED || all_threads[tid].state == EMPTY){
            interrupts_set(enabled);
            return THREAD_INVALID;
        }
        
        //unintr_printf("its over\n");
        
        // If thread is BLOCKED, meaning a sleeping thread is killed
        if (all_threads[tid].state == BLOCKED) {
            all_threads[tid].state = KILLED;
            
            // Remove it from all wait queues
            thread_awaken(tid);
            
            // Add it to the ready queue :) thread_waitttttttttttttttt
            queue_push(&ready_queue, &all_threads[tid]);
            interrupts_set(enabled);
            return tid;
	}
        
        // Get the thread to be killed and remove it from the ready queue, push it to the reuse queue
        struct thread* killed_thread = queue_fetch(&ready_queue, tid);
        
        if(killed_thread == NULL){
            interrupts_set(enabled);
            return THREAD_INVALID;
        }
        
        // Mark it as killed
	killed_thread -> state = KILLED;
        
        interrupts_set(enabled);
	return killed_thread -> threadID;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */

// Create wq on stack is much easier :)
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	// TBD();
        
        wq -> head = NULL;
        wq -> tail = NULL;

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	//TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	//TBD();
    
        int enable = interrupts_set(0);
        
        // Invalid WQ
        if(queue == NULL){
            interrupts_set(enable);
            return THREAD_INVALID;
        }
        
        // No other ready thread
        if(ready_queue.head == NULL){
            interrupts_set(enable);
            return THREAD_NONE;
        }
        
        //unintr_printf("Im sleeping\n");
        
        // Go to sleep!
        running_thread -> state = BLOCKED;
        Wqueue_push(queue, running_thread);
        
        Tid yieldTo = thread_yield(THREAD_ANY);
        
        interrupts_set(enable);
        
	return yieldTo;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	//TBD();
    
        int enable = interrupts_set(0);
        
        // Invalid queue or empty queue
        if(queue == NULL || queue->head == NULL){
            interrupts_set(enable);
            return 0;
        }
        
        // Wake up all threads
        if(all == 1){
            int counter = 0;
            struct thread* front = Wqueue_pop(queue);
            
            // Put all threads in wait queue into ready queue
            while(front != NULL){
                front -> state = READY;
                queue_push(&ready_queue, front);
                counter++;
                
                front = Wqueue_pop(queue);
            }
                
            interrupts_set(enable);
            return counter;
        }
        
        // Wake up one thread and move it to ready queue
        struct thread* front = Wqueue_pop(queue);
        front -> state = READY;
        queue_push(&ready_queue, front);
        
        interrupts_set(enable);
	return 1;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	//TBD();
    
        int enable = interrupts_set(0);
        
        //printf("%d\n", exit_codes[tid]);
        //printf("%d %d %d\n", running_thread -> threadID, tid, all_threads[tid].state);
        
        // Invalid tid
        if(tid < 0 || tid >= THREAD_MAX_THREADS || tid == running_thread -> threadID || 
           all_threads[tid].state == EMPTY){
            interrupts_set(enable);
            return THREAD_INVALID;
        }
        
        // Already exited
        if(all_threads[tid].state == EXITED){
            
            if(exit_code != NULL)
                *exit_code = exit_codes[tid];
            
            // Resetting increases the chance of test_wait_parent failing :)
//            if(exit_codes[tid] != 123){
//                exit_codes[tid] = 123;
//                interrupts_set(enable);
//                return tid;
//            }
            
            // If the thread has already exited, need to check only one waiting thread return successfully as well
            // This solve the occasional failing on test_wait_parent:
            // Found expect to be 1 but found 2
            int ret;
            if(all_threads[tid].wait_successful == 1){
                ret = tid;
                all_threads[tid].wait_successful = 0;
            }

            else{
                ret = THREAD_INVALID;
            }
            
            interrupts_set(enable);
            return ret;
        }
        
        // Go to sleep and perform the same ret decision process
        thread_sleep(&(all_wait_queues[tid]));
            
        if(exit_code != NULL)
            *exit_code = exit_codes[tid];
        
        int ret;
        if(all_threads[tid].wait_successful == 1){
            ret = tid;
            all_threads[tid].wait_successful = 0;
        }
        
        else{
            ret = THREAD_INVALID;
        }

        interrupts_set(enable);
        return ret;
        
//        // if this is the first thread waiting on tid, it should successfully return
//        if(all_wait_queues[tid].head == NULL){
//            
//            thread_sleep(&(all_wait_queues[tid]));
//            
//            if(exit_code != NULL)
//                *exit_code = exit_codes[tid];
//            
////            if(exit_codes[tid] != 123){
////                exit_codes[tid] = 123;
////                interrupts_set(enable);
////                return tid;
////            }
//            
//            interrupts_set(enable);
//            return tid;
//        }
//        
//        // Else. it should return failed
//        else{
//            
//            thread_sleep(&(all_wait_queues[tid]));
//            
//            if(exit_code != NULL)
//                *exit_code = exit_codes[tid];
//            
////            if(exit_codes[tid] != 123){
////                exit_codes[tid] = 123;
////                interrupts_set(enable);
////                return tid;
////            }
//            
//            interrupts_set(enable);
//            return THREAD_INVALID;
//        }
        
        //printf("%d\n", exit_codes[tid]);
        
        // Should not reach here
        interrupts_set(enable);
	return THREAD_INVALID;
}

struct lock {
	/* ... Fill this in ... */
        int held;
        struct wait_queue wq;
};

int testNset(struct lock *l){
    int enable = interrupts_set(0);
    
    // Someone has the lock
    if(l -> held == 1){
        interrupts_set(enable);
        return 1;
    }
    
    // Lock is free, take it
    l -> held = 1;
    interrupts_set(enable);
    return 0;
}

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	//TBD();
        
        lock -> held = 0;
        lock -> wq.head = NULL;
        lock -> wq.tail = NULL;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();
        
        // Sleep until lock is available and get it
        while(testNset(lock) == 1)
            thread_sleep(&(lock -> wq));
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();
        
        // Release the lock and wake up all threads waiting for the lock
        lock -> held = 0;
	thread_wakeup(&(lock -> wq), 1);
}

struct cv {
	/* ... Fill this in ... */
        int cond;
        struct wait_queue wq;
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	//TBD();
        
        cv -> cond = 0;
        cv -> wq.head = NULL;
        cv -> wq.tail = NULL;

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	//TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
        
        // Release lock -> sleep -> reacquire
        lock_release(lock);
        thread_sleep(&(cv -> wq));
        lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
        
        // Wake up one thread
        thread_wakeup(&(cv -> wq), 0);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
        
        // Wake up all threads
        thread_wakeup(&(cv -> wq), 1);
}