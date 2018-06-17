/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Thomas Gschwantner <tharre3@gmail.com>. All Rights Reserved.
 * Copyright (C) 2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifdef DEBUG

#include "../mpmc_ptr_ring.h"
#include "../queueing.h"
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/wait.h>

#define THREADS_PRODUCER 2
#define THREADS_CONSUMER 2
#define ELEMENT_COUNT 100000000LL /* divisible by threads_{consumer,producer} */
#define QUEUE_SIZE 1024

#define EXPECTED_TOTAL ((ELEMENT_COUNT * (ELEMENT_COUNT + 1)) / 2)
#define PER_PRODUCER (ELEMENT_COUNT/THREADS_PRODUCER)
#define PER_CONSUMER (ELEMENT_COUNT/THREADS_CONSUMER)
#define THREADS_TOTAL (THREADS_PRODUCER + THREADS_CONSUMER)

struct worker_producer {
	struct work_struct work;
	struct mpmc_ptr_ring *ring;
	int thread_num;
};

struct worker_consumer {
	struct work_struct work;
	struct mpmc_ptr_ring *ring;
	int thread_num;
	int64_t total;
	int64_t count;
};

static __init void producer_function(struct work_struct *work)
{
	struct worker_producer *td = container_of(work, struct worker_producer, work);
	uintptr_t i;

	for (i = td->thread_num * PER_PRODUCER + 1; i <= (td->thread_num + 1) * PER_PRODUCER; ++i) {
		while (mpmc_ptr_ring_produce(td->ring, (void *)i)) {
			schedule();
			/*pr_info("We have awoken (producer)");*/
		}
	}
}

static __init void consumer_function(struct work_struct *work)
{
	struct worker_consumer *td = container_of(work, struct worker_consumer, work);
	int i;

	for (i = 0; i < PER_CONSUMER; ++i) {
		uintptr_t value;
		while (!(value = (uintptr_t)mpmc_ptr_ring_consume(td->ring))) {
			schedule();
			/*cpu_relax();*/
			/*pr_info("We have awoken (consumer)");*/
		}

		td->total += value;
		++td->count;
	}
}

bool __init mpmc_ring_selftest(void)
{
	struct workqueue_struct *wq;
	struct worker_producer *producers;
	struct worker_consumer *consumers;
	struct mpmc_ptr_ring ring;
	int64_t total = 0, count = 0;
	int i;
	int cpu = 0;

	producers = kmalloc_array(THREADS_PRODUCER, sizeof(*producers), GFP_KERNEL);
	consumers = kmalloc_array(THREADS_CONSUMER, sizeof(*consumers), GFP_KERNEL);

	BUG_ON(!producers || !consumers);
	BUG_ON(mpmc_ptr_ring_init(&ring, QUEUE_SIZE, GFP_KERNEL));

	wq = alloc_workqueue("mpmc_ring_selftest", WQ_UNBOUND, 0);

	for (i = 0; i < THREADS_PRODUCER; ++i) {
		producers[i].ring = &ring;
		producers[i].thread_num = i;
		INIT_WORK(&producers[i].work, producer_function);
		queue_work_on(cpumask_next_online(&cpu), wq, &producers[i].work);
	}

	for (i = 0; i < THREADS_CONSUMER; ++i) {
		consumers[i].ring = &ring;
		consumers[i].thread_num = i;
		consumers[i].total = 0;
		consumers[i].count = 0;
		INIT_WORK(&consumers[i].work, consumer_function);
		queue_work_on(cpumask_next_online(&cpu), wq, &consumers[i].work);
	}

	destroy_workqueue(wq);
	BUG_ON(!mpmc_ptr_ring_empty(&ring));
	mpmc_ptr_ring_cleanup(&ring, NULL);

	for (i = 0; i < THREADS_CONSUMER; ++i) {
		total += consumers[i].total;
		count += consumers[i].count;
	}

	kfree(producers);
	kfree(consumers);

	if (count == ELEMENT_COUNT && total == EXPECTED_TOTAL) {
		pr_info("mpmc_ring self-tests: pass");
		return true;
	}

	pr_info("mpmc_ring self-test failed:");
	pr_info("Count: %lld, expected: %lld", count, ELEMENT_COUNT);
	pr_info("Total: %lld, expected: %lld", total, EXPECTED_TOTAL);

	return false;
}

#endif