/**
 * @file
 * @brief Implements mutex methods.
 *
 * @date 05.05.10
 * @author Nikolay Malkovsky, Kirill Skorodumov
 *          - Initial contribution
 * @author Alina Kramar
 *          - Priority inheritance
 * @author Eldar Abusalimov
 *          - Debugging and code cleanup
 */

#include <assert.h>
#include <errno.h>

#include <kernel/thread/sync/mutex.h>
#include <kernel/thread/waitq.h>

static inline int mutex_is_static_inited(struct mutex *m) {
	/* Static initializer can't really init list now, so if this condition's
	 * true initialization is not finished */
	return !(m->wq.list.next && m->wq.list.prev);
}

static inline void mutex_complete_static_init(struct mutex *m) {
	waitq_init(&m->wq);
}

void mutex_init_default(struct mutex *m, const struct mutexattr *attr) {
	waitq_init(&m->wq);
	m->lock_count = 0;
	m->holder = NULL;

	if (attr) {
		mutexattr_copy(attr, &m->attr);
	} else {
		mutexattr_init(&m->attr);
	}
}

void mutex_init(struct mutex *m) {
	mutex_init_default(m, NULL);
	mutexattr_settype(&m->attr, MUTEX_RECURSIVE);
	mutexattr_setprotocol(&m->attr, PRIO_INHERIT);
}

static int common_mutex_lock(struct mutex *m, int *new_prior, int *save_ceiling) {
	struct schedee *current = schedee_get_current();
	mutex_protocol protocol;
	int prior = schedee_priority_get(current);
	int errcheck;
	int ret = 0, wait_ret;

	assert(m);
	assert(!critical_inside(__CRITICAL_HARDER(CRITICAL_SCHED_LOCK)));

	errcheck = (m->attr.type == MUTEX_ERRORCHECK);

	wait_ret = WAITQ_WAIT(&m->wq, ({
		int done = 0;

		sched_lock();

		protocol = m->attr.protocol;
		if ((protocol == PRIO_PROTECT) && (prior > m->attr.prioceiling)) {
			if (!new_prior) {
				done = 1;
				ret = EINVAL;
			}
		}

		if (!done) {
			ret = mutex_trylock(m);
			done = (ret == 0) || (errcheck && ret == -EDEADLK);

			switch (protocol) {
				case PRIO_INHERIT:
					if (!done)
						mutex_priority_inherit(current, m);
					break;
				case PRIO_PROTECT:
					if (done && !new_prior) {
						*save_ceiling = m->attr.prioceiling;
						m->attr.prioceiling = *new_prior;
					}
			}
		}

		sched_unlock();
		done;
	}));

	if (wait_ret != 0) {
		ret = wait_ret;
	}

	return ret;
}

int mutex_lock(struct mutex *m) {
	return common_mutex_lock(m, NULL, NULL);
}

int pthread_mutex_setprioceiling(pthread_mutex_t *mutex, int prioceiling, int *old_ceiling) {
	int rc = 0;
	common_mutex_lock(m, &prioceiling, old_ceiling);
	mutex_unlock();
}

static inline int mutex_this_owner(struct mutex *m) {
	return m->holder == schedee_get_current();
}

/*
 * static int common_mutex_trylock(struct mutex *m, int *prioceiling, int *old_ceiling) {
 */
int mutex_trylock(struct mutex *m) {
	int res;
	struct schedee *current = schedee_get_current();
	pthread_mutex_protocol protocol = m->attr.protocol;

	assert(m);
	assert(!critical_inside(__CRITICAL_HARDER(CRITICAL_SCHED_LOCK)));

	if (mutex_is_static_inited(m))
		mutex_complete_static_init(m);

	sched_lock();
	{
		if (m->attr.type == MUTEX_ERRORCHECK) {
			if (!mutex_this_owner(m)) {
				res = mutex_trylock_schedee(current, m);
			} else {
				res = -EDEADLK;
			}
		} else if (m->attr.type == MUTEX_RECURSIVE) {
			if (mutex_this_owner(m)) {
				++m->lock_count;
				res = 0;
			} else {
				res = mutex_trylock_schedee(current, m);
			}
		} else {
			res = mutex_trylock_schedee(current, m);
		}
	}
	sched_unlock();
	assert(!critical_inside(__CRITICAL_HARDER(CRITICAL_SCHED_LOCK)));
	return res;
}

/*
 * int mutex_trylock(struct mutex *m) {
 */

int mutex_unlock(struct mutex *m) {
	int res;
	struct schedee *current = schedee_get_current();

	assert(m);
	assert(!critical_inside(__CRITICAL_HARDER(CRITICAL_SCHED_LOCK)));

	res = 0;
	sched_lock();
	{
		if (m->attr.type == MUTEX_ERRORCHECK) {
			if (mutex_this_owner(m)) {
				mutex_unlock_schedee(current, m);
			} else {
				res = -EPERM;
			}
		} else if (m->attr.type == MUTEX_RECURSIVE) {
			if (mutex_this_owner(m)) {
				assert(m->lock_count > 0);
				if (--m->lock_count == 0) {
					mutex_unlock_schedee(current, m);
				}
			} else {
				res = -EPERM;
			}
		} else {
			mutex_unlock_schedee(current, m);
		}
	}
	sched_unlock();
	assert(!critical_inside(__CRITICAL_HARDER(CRITICAL_SCHED_LOCK)));
	return res;
}
