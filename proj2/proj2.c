/* CPSC/ECE 3220 - Mark Smotherman - Spring 2017
 *
 * message passing adapted from Brinch Hansen, "RC 4000 Software:
 * Multiprogramming System," brinch-hansen.net/papers/1969c.pdf
 * - process communication, pp. 21-26
 * - send and wait procedure definitions, pp. 66-69
 * - queue representation, p. 139
 * - message buffer representation, pp. 141-142
 *
 * this version uses a pthread mutex and a condition variable to
 * implement the message passing primitives in the context of threads
 *
 * while modeled on the RC 4000 interprocess communication design,
 * there are several differences in this implementation:
 * - only the first four of the six IPC primitives are implemented
 * - a thread id parameter is added to most of the primitives
 * - there is no missing-thread checking and thus there are no
 *   result code parameters for send_answer() and wait_answer()
 *   and there are fewer buffer element states
 * - explicit buffer element id and state fields are used
 * - the receiver address is not overwritten in a buffer element
 *   when an answer is sent
 * - the buffer element payload is a single integer
 * - to avoid extensive casting, queue headers point to the first
 *   few buffer elements, which are then used as dummy nodes in
 *   circularly-linked lists
 *
 * buffer element usage according to buffer element index
 *   0 to (NUM_THREADS-1)               - dummy nodes for event queues
 *                                          (one queue per thread)
 *   NUM_THREADS                        - dummy node for buffer pool
 *   (NUM_THREADS+1) to (NUM_BUFFERS-1) - message buffers
 *
 * buffer element states and locations
 *   0 - available                          - in message pool
 *   1 - sent and pending wait_message()    - in receiver's event queue
 *   2 - received and pending send_answer() - unlinked
 *   3 - sent and pending wait_answer()     - in sender's event queue
 *
 * possible projects
 * - reimplement using semaphores
 * - add wait_event() and get_event()
 * - add signature checking for message buffers
 * - remove code segments and have students fill in
 * - create version that deadlocks and have students debug
 */


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>

#define NUM_THREADS 10
#define NUM_BUFFERS 20

pthread_t threads[NUM_THREADS];         /* pthread thread-local info */

pthread_mutex_t big_lock;               /* lock for mutual exclusion */

pthread_cond_t waiting[NUM_THREADS];    /* condition variable */


struct buffer_element{                  /* message buffer structure */

  struct buffer_element *back,          /* lists will be circular */
                        *next;          /*   and doubly-linked    */
  int id,                               /* id will be same as index */
      state,                            /* state encoding is shown above */
      sender_address,                   /* addresses are thread ids */
      receiver_address,
      value;                            /* single word payload */

} buffer[NUM_BUFFERS];                  /* static allocation of buffers; */
                                        /*   usage is shown above        */


struct buffer_element *event_queue[NUM_THREADS],        /* head ptrs */
                      *buffer_pool;


/**************************************
 ****                              ****
 ****    bufffer pool functions    ****
 ****                              ****
 ****    each function assumes     ****
 ****    that the caller holds     ****
 ****    big_lock or is part of    ****
 ****    the initialization        ****
 ****                              ****
 **************************************/


/* alloc_buffer_element()
 *
 * allocate a buffer element from the front of the buffer pool,
 *   returning NULL if buffer pool is empty
 */

struct buffer_element *alloc_buffer_element( void ){
  struct buffer_element *return_element;

  if( buffer_pool->next == buffer_pool ) return( NULL );

  return_element = buffer_pool->next;
  return_element->back->next = return_element->next;
  return_element->next->back = return_element->back;
  return_element->next = return_element->back = return_element;

  return( return_element );
}


/* return_buffer_element( in buffer )
 *
 * return the buffer element to the rear of the buffer pool,
 *   re-initializing the buffer fields other than id
 *
 * checks that the buffer element pointer is not NULL
 */

void return_buffer_element( struct buffer_element *element ){
  
  if( element == NULL ){
    printf( "--return element error - NULL pointer\n" );
    exit( 1 );
  }

  element->state = 0;
  element->sender_address = element->receiver_address = -1;
  element->value = 0;

  element->next = buffer_pool;
  element->back = buffer_pool->back;
  element->back->next = element;
  buffer_pool->back = element;
}


/*********************************************
 ****                                     ****
 ****    initialization of buffer pool    ****
 ****    and thread data structures       ****
 ****                                     ****
 ****    will execute without locking     ****
 ****    since only the main thread is    ****
 ****    active during initialization     ****
 ****                                     ****
 *********************************************/


void init_objects(){
  int i = 0;
  struct buffer_element *new_element;

  /* sanity check */
  if( NUM_BUFFERS < NUM_THREADS+5 ){
    printf( "increase buffer size to at least %d\n", NUM_THREADS+5 );
    exit( 0 );
  }

  /* assign matching id for print statements */
  for( i = 0; i < NUM_BUFFERS; i++ ){
    buffer[i].id= i;
  }

  /* each queue and the buffer pool has a dummy node */
  for( i = 0; i < NUM_THREADS; i++ ){
    event_queue[i] = &buffer[i];
    event_queue[i]->next = event_queue[i]->back = event_queue[i];
  }
  buffer_pool = &buffer[NUM_THREADS];
  buffer_pool->next = buffer_pool->back = buffer_pool;

  /* place the rest of the buffer elements into the buffer pool */
  for( i = NUM_THREADS+1; i < NUM_BUFFERS; i++ ){
    new_element = &buffer[i];
    return_buffer_element( new_element );
  }
}


/*************************************
 ****                             ****
 ****    event queue functions    ****
 ****                             ****
 ****    each function assumes    ****
 ****    that the caller holds    ****
 ****    big_lock                 ****
 ****                             ****
 *************************************/


/* append_event_queue( in sender, in buffer )
 *
 * append the buffer element at the rear of the thread's event queue
 *
 * checks that the buffer element pointer is not NULL
 */

void append_event_queue( int thread_id, struct buffer_element *element ){

  if( element == NULL ){
    printf( "--append queue error - NULL pointer\n" );
    exit( 1 );
  }

//
// ASSIGNMENT PART 1 - add necessary code here
//

	element->next = event_queue[thread_id];
	element->back = event_queue[thread_id]->back;
	element->back->next = element;
	event_queue[thread_id]->back = element;

}


/* remove_first_event_queue( in sender )
 *
 * remove the buffer element at the front of the thread's event queue
 *   and return it to the caller
 *
 * checks that the event queue is not empty
 */

struct buffer_element *remove_first_event_queue( int thread_id ){
  struct buffer_element *return_element;

  if( event_queue[thread_id]->next == event_queue[thread_id] ){
    printf( "--remove queue error - thread %d - empty queue\n",
      thread_id );
    exit( 1 );
  }

  return_element = event_queue[thread_id]->next;
  return_element->back->next = return_element->next;
  return_element->next->back = return_element->back;
  return_element->next = return_element->back = return_element;

  return( return_element );
}


/* remove_buffer_element( in buffer )
 *
 * removes the specified buffer element from a queue
 *
 * checks that the buffer element is not a dummy node in an event
 *   queue or buffer pool and that element is not already unlinked
 */

void remove_buffer_element( struct buffer_element *element ){

  if( element->id <= NUM_THREADS ){
    printf( "--remove element error - dummy node\n" );
    exit( 1 );
  }

  if( element->next == element ){
    printf( "--remove element error - unlinked\n" );
    exit( 1 );
  }

  element->back->next = element->next;
  element->next->back = element->back;
  element->next = element->back = element;
}


/******************************************
 ****                                  ****
 ****     message passing functions    ****
 ****                                  ****
 ****  each function uses big_lock to  ****
 ****  protect updates to the event    ****
 ****  queues and the buffer pool      ****
 ****                                  ****
 ******************************************/


/* send_message( in sender, in receiver, in message, out buffer )
 *
 * select a buffer element from the buffer pool, returning
 *   an error code if no buffer elements are available
 * set the state and address fields in the buffer element
 * copy the message value from the sender into the buffer element
 * place the buffer element into the receiver's message queue
 * provide the sender with a pointer to the buffer element
 * wake up the receiver if it is waiting for a message
 */

int send_message( int sender, int receiver, int message,
  struct buffer_element **p_element ){

  struct buffer_element *element;

  pthread_mutex_lock( &big_lock );

  element = alloc_buffer_element();
  if( element == NULL ){
    pthread_mutex_unlock( &big_lock );
    return( 1 );
  }

  element->state = 1;
  element->sender_address = sender;
  element->receiver_address = receiver;
  element->value = message;

  append_event_queue( receiver, element );

  *p_element = element;

  pthread_cond_signal( &waiting[receiver] );

  pthread_mutex_unlock( &big_lock );

  return( 0 );
}


/* wait_message( in receiver, out sender, out message, out buffer )
 *
 * block if message queue is empty
 * unlink the first buffer element from the message queue
 * copy the message into the receiver's memory
 * provide the receiver with a pointer to the buffer element
 *
 * check that the buffer element state is sent as message or answer
 */

void wait_message( int thread_id, int *sender, int *message,
  struct buffer_element **p_element ){

  struct buffer_element *element;

  pthread_mutex_lock( &big_lock );

//
// ASSIGNMENT PART 2 - add the necessary code here
//

	while (event_queue[thread_id]->next == event_queue[thread_id]) {
		pthread_cond_wait(&waiting[thread_id], &big_lock);
	}
	

  element = remove_first_event_queue( thread_id );

  if(( element->state != 1 ) && ( element->state != 3 )){
    printf( "--wait message - wrong state\n" );
    exit( 1 );
  }

  element->state = 2;
  *sender = element->sender_address;
  *message = element->value;

  *p_element = element;

  pthread_mutex_unlock( &big_lock );
}


/* send_answer( in answer, in buffer )
 *
 * original receiver reuses the same buffer element
 * copy answer from memory into the buffer element
 * place the buffer element into the receiver's message queue
 * wake up the original sender if it is waiting for an
 *   answer to be placed into this buffer element
 *
 * checks that the buffer element pointer is not NULL and that
 *   buffer element state is received
 */

void send_answer( int answer, struct buffer_element *element ){
  int sender;

  if( element == NULL ){
    printf( "--send answer error - NULL pointer\n" );
    exit( 1 );
  }

  if( element->state != 2 ){
    printf( "--send answer error - wrong state\n" );
    exit( 1 );
  }

  pthread_mutex_lock( &big_lock );

//
// ASSIGNMENT PART 3 - add the necessary code here
//

	element->state = 3;
	element->value = answer;
	append_event_queue(element->receiver_address, element);
	pthread_cond_signal(&waiting[element->sender_address]);

  pthread_mutex_unlock( &big_lock );
}


/* wait_answer( in sender, out answer, in buffer )
 *
 * original sender reuses the same buffer element
 * block if the answer is not yet available
 * remove the buffer element from the event queue
 * copy answer from the buffer element into memory
 * return the buffer element to the buffer pool
 *
 * checks that the buffer element pointer is not NULL
 */

void wait_answer( int thread_id, int *answer,
  struct buffer_element *element ){

  if( element == NULL ){
    printf( "--wait answer error - NULL pointer\n" );
    exit( 1 );
  }

  pthread_mutex_lock( &big_lock );

  while( element->state != 3 ){
    pthread_cond_wait( &waiting[thread_id], &big_lock );
  }

  remove_buffer_element( element );

  *answer = element->value;

  return_buffer_element( element );

  pthread_mutex_unlock( &big_lock );
}


/*************************************
 ****                             ****
 ****   server and client code    ****
 ****                             ****
 ****   locking and condition     ****
 ****   variable waiting not      ****
 ****   visible at this level     ****
 ****                             ****
 *************************************/


/* server thread code - can create multiple servers */

void *server( void *arg ){
  int thread_id = *((int *)arg);
  struct buffer_element *element;
  int i, sender, message, answer;

  for( i = 0; i < 6; i++ ){

    wait_message( thread_id, &sender, &message, &element );

    printf( "server %d rcvd message %2d from thread %d in buffer %d\n",
      thread_id, sender, message, element->id );

    answer = message + 10 + 10*thread_id;
    sleep( 1 );

    send_answer( answer, element );

    printf( "server %d sent answer  %2d to   sender %d in buffer %d\n",
      thread_id, answer, sender, element->id );
  }
  return( NULL );
}


/* client thread code that uses server 0 - can create multiple clients */

void *client0( void *arg ){
  int thread_id = *((int *)arg);
  struct buffer_element *element;
  int i, rc, answer;

  for( i = 0; i < 3; i++ ){

    rc = send_message( thread_id, 0, thread_id, &element );

    if( rc ){

      printf( "message allocate failed for client %d\n", thread_id );

    }else{

      printf( "client %d sent message %2d to   server 0 in buffer %d\n",
        thread_id, thread_id, element->id );

      wait_answer( thread_id, &answer, element );

      printf( "client %d rcvd answer  %2d from server 0 in buffer %d\n",
        thread_id, answer, element->id );
    }
  }
  return( NULL );
}


/* client thread code that uses server 1 - can create multiple clients */

void *client1( void *arg ){
  int thread_id = *((int *)arg);
  struct buffer_element *element;
  int i, rc, answer;

  for( i = 0; i < 3; i++ ){

    rc = send_message( thread_id, 1, thread_id, &element );

    if( rc ){

      printf( "message allocate failed for client %d\n", thread_id );

    }else{

      printf( "client %d sent message %2d to   server 1 in buffer %d\n",
        thread_id, thread_id, element->id );

      wait_answer( thread_id, &answer, element );

      printf( "client %d rcvd answer  %2d from server 1 in buffer %d\n",
        thread_id, answer, element->id );
    }
  }
  return( NULL );
}


/* client thread code that uses both servers 0 and 1 */

void *client01( void *arg ){
  int thread_id = *((int *)arg);
  struct buffer_element *e0,*e1;
  int i, rc0, rc1, answer;

  for( i = 0; i < 3; i++ ){

    rc0 = send_message( thread_id, 0, thread_id, &e0 );

    if( rc0 ){

      printf( "message allocate failed for client %d\n", thread_id );

    }else{

      printf( "client %d sent message %2d to   server 0 in buffer %d\n",
        thread_id, thread_id, e0->id );

      rc1 = send_message( thread_id, 1, thread_id, &e1 );

      if( rc1 ){

        printf( "message allocate failed for client %d\n", thread_id );

      }else{

        printf( "client %d sent message %2d to   server 1 in buffer %d\n",
          thread_id, thread_id, e1->id );
      }
    }
    if( !rc0 ){

      wait_answer( thread_id, &answer, e0 );

      printf( "client %d rcvd answer  %2d from server 0 in buffer %d\n",
        thread_id, answer, e0->id );
    }
    if( !rc1 ){

      wait_answer( thread_id, &answer, e1 );

      printf( "client %d rcvd answer  %2d from server 1 in buffer %d\n",
        thread_id, answer, e1->id );
    }
  }
  return( NULL );
}


/**************************
 ****                  ****
 ****   test driver    ****
 ****                  ****
 **************************/


int main( int argc, char **argv ){
  int i, rc, thread_id[NUM_THREADS];

  for( i = 0; i < NUM_THREADS; i++ ) thread_id[i] = i;

  pthread_mutex_init( &big_lock, NULL );
  for( i = 0; i < NUM_THREADS; i++ ) pthread_cond_init( &waiting[i], NULL );

  init_objects();

  for( i = 0; i < 2; i++ ){
    rc = pthread_create( &threads[i], NULL, &server, (void *) &thread_id[i] );
    if( rc ){ printf( "** could not create server thread\n" ); exit( -1 ); }
  }

  for( i = 2; i < 4; i++ ){
    rc = pthread_create( &threads[i], NULL, &client0, (void *) &thread_id[i] );
    if( rc ){ printf( "** could not create client0 thread\n" ); exit( -1 ); }
  }

  for( i = 4; i < 6; i++ ){
    rc = pthread_create( &threads[i], NULL, &client1, (void *) &thread_id[i] );
    if( rc ){ printf( "** could not create client1 thread\n" ); exit( -1 ); }
  }

  for( i = 0; i < 6; i++ ){
    rc = pthread_join( threads[i], NULL );
    if( rc ){ printf( "** could not join thread %d\n", i ); exit( -1 ); }
  }

  pthread_mutex_destroy( &big_lock );
  for( i = 0; i < NUM_THREADS; i++ ) pthread_cond_destroy( &waiting[i] );

  return( 0 );
}
