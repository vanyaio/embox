/**
 * @file
 *
 * @date Aug 1, 2013
 * @author: Anton Bondarev
 */

#ifndef STRATEGY_NO_PRIORITY_H_
#define STRATEGY_NO_PRIORITY_H_

#include <sys/cdefs.h>

struct schedee;

extern int schedee_is_idle(struct schedee *s);
extern int schedee_is_boot(struct schedee *s);

struct schedee_priority {
	EMPTY_STRUCT_BODY
};

#define __SCHED_PRIORITY_INIT(prio) \
	{ }

typedef struct schedee_priority __schedee_priority_t;

static inline int schedee_priority_set(struct schedee *s, int new_priority) {
	return 0;
}

static inline int schedee_priority_get(struct schedee *s) {
	if (schedee_is_idle(s)) {
		return 0;
	}
	if (schedee_is_boot(s)) {
		return 1;
	}
	return 2; /* lthread */
}

static inline int schedee_priority_inherit(struct schedee *s, int priority) {
	return 0;
}

static inline int schedee_priority_reverse(struct schedee *s) {
	return 0;
}

static inline int schedee_priority_init(struct schedee *schedee, int new_priority) {
	return 0;
}

#endif /* STRATEGY_NO_PRIORITY_H_ */
