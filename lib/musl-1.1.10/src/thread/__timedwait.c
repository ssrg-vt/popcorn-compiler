#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "futex.h"
#include "syscall.h"
#include "pthread_impl.h"

/* Return negative, zero, positive if A < B, A == B, A > B, respectively.
   Assume the nanosecond components are in range, or close to it.  */
static inline int
timespec_cmp (struct timespec a, struct timespec b)
{
  return (a.tv_sec < b.tv_sec ? -1
        : a.tv_sec > b.tv_sec ? 1
        : a.tv_nsec - b.tv_nsec);
}

int __pthread_setcancelstate(int, int *);
int __clock_gettime(clockid_t, struct timespec *);

int __timedwait_cp(volatile int *addr, int val,
	clockid_t clk, const struct timespec *at, int priv)
{
	/*OLD
	int r;
	struct timespec to, *top=0;*/
	//NEW
	int r =0;
	struct timespec to, *top=0, tcur;

	if (priv) priv = 128;

	if (at) {
		if (at->tv_nsec >= 1000000000UL) return EINVAL;
		if (__clock_gettime(clk, &to)) return EINVAL;
		to.tv_sec = at->tv_sec - to.tv_sec;
		if ((to.tv_nsec = at->tv_nsec - to.tv_nsec) < 0) {
			to.tv_sec--;
			to.tv_nsec += 1000000000;
		}
		if (to.tv_sec < 0) return ETIMEDOUT;
		top = &to;
	}

	/*r = -__syscall_cp(SYS_futex, addr, FUTEX_WAIT|priv, val, top);
	if (r == ENOSYS) r = -__syscall_cp(SYS_futex, addr, FUTEX_WAIT, val, top);
	if (r != EINTR && r != ETIMEDOUT && r != ECANCELED) r = 0;*/
	while (*addr==val) {
		if (at) {
			__clock_gettime(clk, &tcur);
			if (timespec_cmp(to, tcur) > 0) {
				r = ETIMEDOUT;
				break;
			}
		}
	}
#warning "__timedwait_cp doesn't support signals (EINTR), and cancellations (ECANCELED)."
	if (r != EINTR && r != ETIMEDOUT && r != ECANCELED){
		//EINTR: FUTEX_WAIT operation was interupted by signal
		//ETIMEDOUT: Timeout during FUTEX_WAIT operation
		//ECANCELED: ???
		r = 0;
	}

	return r;
}

int __timedwait(volatile int *addr, int val,
	clockid_t clk, const struct timespec *at, int priv)
{
	int cs, r;
	__pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);
	r = __timedwait_cp(addr, val, clk, at, priv);
	__pthread_setcancelstate(cs, 0);
	return r;
}
