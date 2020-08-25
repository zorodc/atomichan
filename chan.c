#include "chan.h"
#include "bitops.h"
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define DEFAULT_INITIAL_POWER 6 // Initial capacity: 64, as a power of two.

#define SIZE_FROM_CAP(cap, addnl) ((cap) * sizeof(uintptr_t) + (addnl))
#define CAP_FROM_POWER(power) CAP_FROM_RAISED((size_t)1 << (power))
#define CAP_FROM_RAISED(raised) (raised)

#define MODULO(lhs, rhs) POW2_MODULO((lhs), (rhs))
#define IDX_OF(idx, node) (MODULO((idx), (node)->cap))
#define IS_READABLE(idx, node) ((node)->back - (idx) != 0)
#define IS_WRITABLE(idx, node) ((idx) - (node)->front < (node)->cap)

// The head of the channel resides contiguously after the chan_base struct itself.
// Here, two objects are stored in the same block of memory, but are accessed separately.
#define HEAD_OF(chan) ((struct chan_node*)(void*)((struct chan_base*)(chan) + 1))

static void init_node(struct chan_node* node, struct chan_node* next, size_t cap) {
	node->cap = cap;
	atomic_init(&node->front, 0);
	atomic_init(&node->back, 0);
	atomic_init(&node->next, next);
}

chan_ref chan_new(size_t initial_cap, chan_dtor_f dtor) {
	assert(initial_cap < sizeof(size_t) * CHAR_BIT);
	const size_t power = (initial_cap) ? (initial_cap) : DEFAULT_INITIAL_POWER;
	const size_t cap   = CAP_FROM_POWER(power);

	// Allocate the chan_base and head chan_node in the same underlying buffer.
	chan_ref chan = malloc(SIZE_FROM_CAP(cap, sizeof(struct chan_base) + sizeof(struct chan_node)));
	struct chan_node* const head = HEAD_OF(chan);
	init_node(head, head, cap);

	atomic_init(&chan->curr_send, head);
	atomic_init(&chan->curr_recv, head);
	chan->dtor = dtor;
	chan->last_power = power;

	return chan;
}

void chan_delete(chan_ref chan) {
	const struct chan_node* const head = HEAD_OF(chan);
	struct chan_node* prev;
	if (chan->dtor)
		for (struct chan_node* node = head->next;
		     node != head;
		     prev = node, node = node->next, free(prev))
			for (size_t i = node->front; IS_READABLE(i, node); ++i)
				chan->dtor(node->buf[i]);
	else for (struct chan_node* node = head->next;
				 node != head;
				 prev = node, node = node->next, free(prev));
	free(chan); // Also frees the head; it resides reside in the same buffer.
}

_Bool chan_send(chan_ref chan, uintptr_t elem) {
	struct chan_node* node = atomic_load_explicit(&chan->curr_send, memory_order_relaxed);
retry:;
	size_t idx = atomic_load_explicit(&node->back, memory_order_consume);
	if (!IS_WRITABLE(idx, node)) {
		struct chan_node* const next = atomic_load_explicit(&node->next, memory_order_relaxed);
		// Never move to write on top of the node that's currently being read;
		// In that case, items would be read out out of order they were sent in.
		if (next != atomic_load_explicit(&chan->curr_recv, memory_order_relaxed)) {
			node = next;
			goto retry;
		}

		const size_t power = ++chan->last_power;
		assert(power < sizeof(size_t) * CHAR_BIT);
		const size_t cap = CAP_FROM_POWER(power);

		struct chan_node* new_node = malloc(SIZE_FROM_CAP(cap, sizeof(struct chan_node)));
		if (!new_node) return false;

		init_node(new_node, next, cap);
		atomic_store_explicit(&node->next, new_node, memory_order_release);
		idx = 0; node = new_node;
	}
	node->buf[IDX_OF(idx, node)] = elem;
	atomic_store_explicit(&chan->curr_send, node, memory_order_relaxed);
	atomic_fetch_add_explicit(&node->back, 1, memory_order_release);
	return true;
}

_Bool chan_recv(chan_ref chan, uintptr_t* slot) {
	struct chan_node* node = atomic_load_explicit(&chan->curr_recv, memory_order_consume);
	size_t idx;
	size_t i = 0;
no_increment:
	do {
		idx = atomic_load_explicit(&node->front, memory_order_consume);
		if (!IS_READABLE(idx, node)) {
			if (node != chan->curr_send)
				atomic_compare_exchange_strong(&chan->curr_recv, &node,
					atomic_load_explicit(&node->next, memory_order_relaxed));
			goto no_increment;
		} else *slot = node->buf[IDX_OF(idx, node)];
	} while (!atomic_compare_exchange_weak(&node->front, &(size_t){idx},idx+1));
	return true;
}
