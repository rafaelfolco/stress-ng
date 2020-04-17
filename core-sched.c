/*
 * Copyright (C) 2013-2020 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#include "stress-ng.h"

typedef struct {
	const int sched;
	const char *const name;
} stress_sched_types_t;

static stress_sched_types_t sched_types[] = {
#if defined(SCHED_BATCH)
	{ SCHED_BATCH,		"batch" },
#endif
#if defined(SCHED_DEADLINE)
	{ SCHED_DEADLINE,	"deadline" },
#endif
#if defined(SCHED_FIFO)
	{ SCHED_FIFO,		"fifo" },
#endif
#if defined(SCHED_IDLE)
	{ SCHED_IDLE,		"idle" },
#endif
#if defined(SCHED_OTHER)
	{ SCHED_OTHER,		"other" },
#endif
#if defined(SCHED_RR)
	{ SCHED_RR,		"rr" },
#endif
};

/*
 *  get_sched_name()
 *	convert sched class to human readable string
 */
const char *stress_get_sched_name(const int sched)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(sched_types); i++) {
		if (sched_types[i].sched == sched)
			return sched_types[i].name;
	}
	return "unknown";
}

#if (defined(_POSIX_PRIORITY_SCHEDULING) || defined(__linux__)) && \
    !defined(__OpenBSD__) && !defined(__minix__) && !defined(__APPLE__)

#if defined(SCHED_DEADLINE) && defined(__linux__)
#define HAVE_STRESS_SET_DEADLINE_SCHED	(1)

int stress_set_deadline_sched(
	const pid_t pid,
	const uint64_t period,
	const uint64_t runtime,
	const uint64_t deadline,
	const bool quiet)
{
	int rc;
	struct shim_sched_attr attr;

	attr.size = sizeof(attr);
	attr.sched_policy = SCHED_DEADLINE;
	/* make sched_deadline task be able to fork*/
	attr.sched_flags = SCHED_FLAG_RESET_ON_FORK;
	if (g_opt_flags & OPT_FLAGS_DEADLINE_GRUB)
		attr.sched_flags |= SCHED_FLAG_RECLAIM;

	attr.sched_nice = 0;
	attr.sched_priority = 0;
	if (!quiet)
		pr_dbg("%s: setting scheduler class '%s' (period=%lu, runtime=%lu, deadline=%lu)\n",
			__func__, "deadline", period, runtime, deadline);
	attr.sched_runtime = runtime;
	attr.sched_deadline = deadline;
	attr.sched_period = period;

	rc = shim_sched_setattr(pid, &attr, 0);
	if (rc < 0) {
		rc = -errno;
		if (!quiet)
			pr_inf("cannot set scheduler: errno=%d (%s)\n",
				errno, strerror(errno));
		return rc;
	}
	return 0;
}
#endif

#define HAVE_STRESS_SET_SCHED	(1)
/*
 *  set_sched()
 * 	are sched settings valid, if so, set them
 */
int stress_set_sched(
	const pid_t pid,
	const int sched,
	const int32_t sched_priority,
	const bool quiet)
{
#if defined(SCHED_FIFO) || defined(SCHED_RR)
	int min, max;
#endif
#if defined(SCHED_DEADLINE) && defined(__linux__)
	struct shim_sched_attr attr;
#endif
	int rc;
	struct sched_param param;
	const char *name = stress_get_sched_name(sched);

#if defined(SCHED_DEADLINE) && defined(__linux__)
	long sched_period = 0;
	long sched_runtime = 0;
	long sched_deadline = 0;
#endif

	(void)memset(&param, 0, sizeof(param));

	switch (sched) {
	case UNDEFINED:	/* No preference, don't set */
		return 0;
#if defined(SCHED_FIFO) || defined(SCHED_RR)
#if defined(SCHED_FIFO)
	case SCHED_FIFO:
#endif
#if defined(SCHED_RR)
	case SCHED_RR:
#endif
		min = sched_get_priority_min(sched);
		max = sched_get_priority_max(sched);
		param.sched_priority = sched_priority;

		if (sched_priority == UNDEFINED) {
			if (g_opt_flags & OPT_FLAGS_AGGRESSIVE)
				param.sched_priority = max;
			else
				param.sched_priority = (max - min) / 2;
			if (!quiet)
				pr_inf("priority not given, defaulting to %d\n",
					param.sched_priority);
		}
		if ((param.sched_priority < min) ||
		    (param.sched_priority > max)) {
			if (!quiet)
				pr_inf("scheduler priority level must be "
					"set between %d and %d\n",
					min, max);
			return -EINVAL;
		}
		if (!quiet)
			pr_dbg("sched: setting scheduler class '%s', priority %d\n",
				name, param.sched_priority);
		break;
#endif
#if defined(SCHED_DEADLINE) && defined(__linux__)
	case SCHED_DEADLINE:
		min = sched_get_priority_min(sched);
		max = sched_get_priority_max(sched);
		attr.size = sizeof(attr);
		attr.sched_policy = SCHED_DEADLINE;
		attr.sched_flags = SCHED_FLAG_RESET_ON_FORK;
		attr.sched_nice = SCHED_OTHER;
		attr.sched_priority = sched_priority;
		if (sched_priority == UNDEFINED) {
			if (g_opt_flags & OPT_FLAGS_AGGRESSIVE)
				attr.sched_priority = max;
			else
				attr.sched_priority = (max - min) / 2;
			if (!quiet)
				pr_inf("priority not given, defaulting to %d\n",
					attr.sched_priority);
		}
		if ((attr.sched_priority < (uint32_t)min) ||
		    (attr.sched_priority > (uint32_t)max)) {
			if (!quiet)
				pr_inf("scheduler priority level must be "
				"set between %d and %d\n",
				min, max);
			return -EINVAL;
		}
		if (!quiet) {
			pr_dbg("%s: setting scheduler class '%s'\n",
					__func__, name);
		}
		(void)stress_get_setting("sched-period", &sched_period);
		(void)stress_get_setting("sched-runtime", &sched_runtime);
		(void)stress_get_setting("sched-deadline", &sched_deadline);
		if (sched_deadline <= 0) {
			attr.sched_runtime = 10000;
			attr.sched_deadline = 100000;
			attr.sched_period = 0;
		} else {
			attr.sched_runtime = sched_runtime;
			attr.sched_deadline = sched_deadline;
			attr.sched_period = sched_period;
		}
		pr_dbg("%s: setting scheduler class '%s' (period=%lu, runtime=%lu, deadline=%lu)\n",
				__func__, "deadline", attr.sched_period, attr.sched_runtime, attr.sched_deadline);

		rc = shim_sched_setattr(pid, &attr, 0);
		if (rc < 0) {
			/*
			 *  Kernel supports older (smaller) attr
			 *  but userspace supports newer (larger) attr,
			 *  so report this and pass it back up to re-do
			 *  the scheduling with a non SCHED_DEADLINE
			 *  scheduler that requires the larger attr
			 */
			if (errno == E2BIG)
				return -E2BIG;
			rc = -errno;
			if (!quiet)
				pr_inf("cannot set scheduler '%s': errno=%d (%s)\n",
					stress_get_sched_name(sched),
					errno, strerror(errno));
			return rc;
		}
		return 0;
		break;		/* Keep static analysers happy */
#endif
	default:
		param.sched_priority = 0;
		if (sched_priority != UNDEFINED)
			if (!quiet)
				pr_inf("ignoring priority level for "
					"scheduler class '%s'\n", name);
		if (!quiet)
			pr_dbg("sched: setting scheduler class '%s'\n", name);
		break;
	}
	rc = sched_setscheduler(pid, sched, &param);
	if (rc < 0) {
		rc = -errno;
		if (!quiet)
			pr_inf("cannot set scheduler '%s': errno=%d (%s)\n",
				stress_get_sched_name(sched),
				errno, strerror(errno));
		return rc;
	}
	return 0;
}
#endif

#if !defined(HAVE_STRESS_SET_SCHED)
#define HAVE_STRESS_SET_SCHED	(1)

/* No-op shim */
int stress_set_sched(
	const pid_t pid,
	const int sched,
	const int32_t sched_priority,
	const bool quiet)
{
	(void)pid;
	(void)sched;
	(void)sched_priority;
	(void)quiet;

	return 0;
}
#endif

#if !defined(HAVE_STRESS_SET_DEADLINE_SCHED)
#define HAVE_STRESS_SET_DEADLINE_SCHED	(1)

/* No-op shim */
int stress_set_deadline_sched(
	const pid_t pid,
	const uint64_t period,
	const uint64_t runtime,
	const uint64_t deadline,
	const bool quiet)
{
	(void)pid;
	(void)period;
	(void)runtime;
	(void)deadline;
	(void)quiet;

	return 0;
}
#endif

/*
 *  get_opt_sched()
 *	get scheduler policy
 */
int32_t stress_get_opt_sched(const char *const str)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(sched_types); i++) {
		if (!strcmp(sched_types[i].name, str))
			return sched_types[i].sched;
	}
	if (strcmp("which", str))
		(void)fprintf(stderr, "Invalid sched option: %s\n", str);
	if (SIZEOF_ARRAY(sched_types) == 0) {
		(void)fprintf(stderr, "No scheduler options are available\n");
	} else {
		(void)fprintf(stderr, "Available scheduler options are:");
		for (i = 0; i < SIZEOF_ARRAY(sched_types); i++) {
			(void)fprintf(stderr, " %s", sched_types[i].name);
		}
		(void)fprintf(stderr, "\n");
	}
	_exit(EXIT_FAILURE);
}
