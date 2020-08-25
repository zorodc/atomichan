#include "threads.h"
#include "../chan.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#define N_ITEMS (1024UL * 8)

static int consumer_thread(void* vp);

static int producer_thread(void* vp)
{
	chan_ref chan = vp;
	int ret = 0;
	for (uintptr_t i = 0; i < N_ITEMS; ++i) {
		_Bool stat = chan_send(chan, i);
		if (!stat) {
			++ret;
			fprintf(stderr, "Failed chan_send on %zu.\n", i);
		}
	}

	return ret;
}

static int consumer_thread(void* vp)
{
	chan_ref chan = vp;
	uintptr_t elem;
	int fail_count = 0;
	for (size_t i = 0; i < N_ITEMS; ++i) {
		if (!chan_recv(chan, &elem))
			fprintf(stderr, "Failed chan_recv on %zu'th iteration: %zu.\n",
			        i, (size_t)elem);
		printf("%zu'th => %zu.\n", i, (size_t)elem);
		if (i != (size_t)elem)
			++fail_count;
	}
	return fail_count;
}

static _Bool spsc_test(void) {
	chan_ref chan = chan_new(0, NULL);

	thrd_t producer;
	thrd_t consumer;

	int stat = 0;
	stat |= thrd_create(&consumer, consumer_thread, chan);
	stat |= thrd_create(&producer, producer_thread, chan);
	assert(thrd_success == stat);


	int ret;
	stat = 0;
	stat |= thrd_join(producer, NULL); assert(thrd_success == stat);
	printf("Producer done.\n");
	stat |= thrd_join(consumer, &ret); assert(thrd_success == stat);
	printf("Return value of consumer: %d.\n", ret);

	chan_delete(chan);
	return !stat;
}

#define MC_N_ITEMS (1024UL * 8)
_Atomic size_t observed_count[MC_N_ITEMS+1];
struct mc_thrd_arg { chan_ref chan; _Bool should_print; };
static int mc_thread(void* vp) {
	struct mc_thrd_arg* args = vp;
	chan_ref      chan = args->chan;
	uintptr_t     elem = 0;
	uintptr_t greatest = 0;

	for (;;) {
		greatest = (greatest > elem) ? greatest : elem;
		if (!chan_recv(chan, &elem))
			fprintf(stderr, "Recv failed in mc_thread.\n");
		else if (observed_count[elem]++) fprintf(stderr, "Consumed twice!\n");
		else if (elem < greatest) fprintf(stderr, "%zu after %zu; bad order!\n",
		                                  (size_t) elem, (size_t) greatest);
		else if (args->should_print) printf("Observed %zu.\n", (size_t) elem);

		// Test for sentinel signalling termination.
		if ((elem) >= (MC_N_ITEMS-1)) {
			// Notify other threads of termination
			chan_send(chan, elem+1);
			break;
		}
	}

	return 0; // never reached
}

static _Bool spmc_test(void) {
	chan_ref chan = chan_new(0, NULL);

	thrd_t mc_1;
	thrd_t mc_2;
	thrd_t mc_3;
	thrd_t mc_4;
	thrd_t mc_5;
	thrd_t mc_6;
	thrd_t producer;
	thrd_create(&producer, producer_thread, chan);
	thrd_create(&mc_1, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});
	thrd_create(&mc_2, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});
	thrd_create(&mc_3, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});
	thrd_create(&mc_4, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});
	thrd_create(&mc_5, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});
	thrd_create(&mc_6, mc_thread, &(struct mc_thrd_arg){.chan=chan, .should_print=true});

	thrd_join(producer, NULL);
	thrd_join(mc_1, NULL);
	thrd_join(mc_2, NULL);
	thrd_join(mc_3, NULL);
	thrd_join(mc_4, NULL);
	thrd_join(mc_5, NULL);
	thrd_join(mc_6, NULL);

	_Bool flag = true;
	for(size_t i = 0; i < MC_N_ITEMS; ++i)
		if (observed_count[i] != 1) {
			fprintf(stderr, "An item seen %zu times: %zu.\n", observed_count[i], i);
			flag = false;
		}
	chan_delete(chan);
	return flag;
}

_Bool chan_test(void) {
	return spsc_test() && spmc_test();
}

int main(void) { return !chan_test(); }
