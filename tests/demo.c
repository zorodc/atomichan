#include "threads.h"
#include "../chan.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <tgmath.h>
#include <stdint.h>

/*
 * Demo of one thread doing square roots for another.
 * Demonstrates the sort of 'worker threads watching a queue' idea.
 */

int processing_fn(void* v)
{
	chan_ref chan = v;
	uintptr_t elem;
	while (chan_recv(chan, &elem) && elem != 0) {
		double d; memcpy(&d, &elem, sizeof(d));
		printf("The sqrt of %lf is %lf.\n", d, sqrt(d));
		fflush(stdout);
	}
	printf("Done!\n");
	fflush(stdout);

	return 0; // the return value is unimportant
}

int main()
{
	chan_ref chan = chan_new(0, NULL);
	thrd_t processing_thread;
	thrd_create(&processing_thread, processing_fn, chan);

	for (double i = 1; i < 127; ++i) {
		uintptr_t u; memcpy(&u, &i, sizeof(u));
		chan_send(chan, u);
	}
	chan_send(chan, 0);

	printf("Waiting...\n");
	thrd_join(processing_thread, NULL);
	return 0;
}
