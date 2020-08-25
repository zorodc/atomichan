#ifndef CHAN_H
#define CHAN_H
/**
   A concurrent single-producer, multiple-consumer (SPMC) queue, or channel.
   Currently implemented using C11 atomics.
   The channel is lock-free, atomic, but it isn't wait-free.
   Allows one send-caller/producer, arbitrary amount of recv-callers/consumers.
   The channel holds uintptr_t, as those can represent essentially anything ;).

	The only issue I discovered in testing this channel was that, if one has multiple consumers, some of them will be swapped off the CPU after grabbing curr_recv, and will have dequeued an element from a different node, if that node ends up having free space. This is a one-off phenomenon, but confuses some of the tests. */
#include <stdint.h> // uintptr_t
#include <stddef.h> // size_t

typedef void(*chan_dtor_f)(uintptr_t);
struct chan_node {
	        size_t   cap; // One more than the number of slots available.
	_Atomic size_t front;
	_Atomic size_t  back;
	struct chan_node* _Atomic next;
	uintptr_t buf[];
};

enum chan_opts { CHNL_BLOCK = 0, CHNL_TRY = 1, CHNL_NOALLOC = 3};

typedef struct chan_base {
   struct chan_node* _Atomic curr_send; // Current node sending on.
   struct chan_node* _Atomic curr_recv; // Current node recieving on.
	size_t last_power;
   chan_dtor_f dtor; // Allowed to be NULL.
}*chan_ref;

/**
	In the event initial_cap is 0, the channel will select a default capacity.
	Takes capacities as powers of two.
	i.e., initial_cap argument of 4 => an allocation of ~16 machine words. */
chan_ref chan_new(size_t initial_cap, chan_dtor_f dtor);

/**
	Destroys the channel, freeing all nodes/elements now assoicated with it.
	Assumes all users of the channel are done with it. */
void chan_delete(chan_ref chan);

/**
   Enqueue an item onto the channel. */
_Bool chan_send(chan_ref chan, uintptr_t elem);

/**
   Recieve (dequeue) an item from the channel. */
_Bool chan_recv(chan_ref chan, uintptr_t* slot);

#endif /* CHAN_H */
