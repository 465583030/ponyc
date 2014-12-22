#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"
#include "../mem/pool.h"

struct asio_base_t
{
	pony_thread_id_t tid;
	asio_backend_t* backend;
	uint64_t noisy_count;
};

static asio_base_t* running_base;

/** Start an asynchronous I/O event mechanism.
 *
 *  Errors are always delegated to the owning actor of an I/O subscription and
 *  never handled within the runtime system.
 *
 *  In any case (independent of the underlying backend) only one I/O dispatcher
 *  thread will be started. Since I/O events are subscribed by actors, we do not
 *  need to maintain a thread pool. Instead, I/O is processed in the context of
 *  the owning actor.
 */
static void start()
{
  asio_base_t* new_base = POOL_ALLOC(asio_base_t);
	memset((void*)new_base, 0, sizeof(asio_base_t));

	new_base->backend = asio_backend_init();

	asio_base_t* existing = NULL;

	if(__pony_atomic_compare_exchange_n(&running_base, &existing,
		new_base, false, PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t))
	{
		if(!pony_thread_create(&running_base->tid, asio_backend_dispatch, -1,
			running_base->backend))
		  exit(EXIT_FAILURE);

  	pony_thread_detach(running_base->tid);
	}
	else
	{
		asio_backend_terminate(new_base->backend);
		POOL_FREE(asio_base_t, new_base);
	}
}

asio_backend_t* asio_get_backend()
{
	if(running_base == NULL)
		start();

	return running_base->backend;
}

bool asio_stop()
{
	if(running_base == NULL)
		return true;

	if(__pony_atomic_load_n(
		&running_base->noisy_count, PONY_ATOMIC_ACQUIRE, uint64_t) > 0)
		return false;

  asio_backend_terminate(running_base->backend);
	POOL_FREE(asio_base_t, running_base);
	running_base = NULL;

	return true;
}

void asio_noisy_add()
{
	__pony_atomic_fetch_add(&running_base->noisy_count, 1, PONY_ATOMIC_RELEASE,
	  uint64_t);
}

void asio_noisy_remove()
{
	__pony_atomic_fetch_sub(&running_base->noisy_count, 1, PONY_ATOMIC_RELEASE,
	  uint64_t);
}
