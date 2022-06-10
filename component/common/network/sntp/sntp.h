#ifndef __SNTP_H__
#define __SNTP_H__

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_AUDREY
enum sntp_stat {
	SNTP_STAT_READY = 0,
	SNTP_STAT_TIME_SET,
};

struct tm *get_local_tm(u32 *timep);
enum sntp_stat sntp_get_state(void);
void sntp_set_time_by_bt(time_t sec);
#endif

void sntp_init(void);
void sntp_stop(void);

/* Realtek added */
void sntp_get_lasttime(long *sec, long *usec, unsigned int *tick);
struct tm sntp_gen_system_time(int timezone);

#ifdef __cplusplus
}
#endif

#endif /* __SNTP_H__ */
