#define ENAME MutexInterestingEvent
EBEGIN_DERIVED(uint8_t)
EPN_BIT_FLAG(Lock, 0)
EPN_BIT_FLAG(ContendedLock, 1)
EEND()
#undef ENAME
