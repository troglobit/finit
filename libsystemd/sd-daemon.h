#ifndef SD_DAEMON_H_
#define SD_DAEMON_H_

#include <stdint.h>
#include <sys/types.h>

#define SD_LISTEN_FDS_START 3

/* systemd logging defines for stderr parsing by Finit */
#define SD_EMERG    "<0>"  /* system is unusable */
#define SD_ALERT    "<1>"  /* action must be taken immediately */
#define SD_CRIT     "<2>"  /* critical conditions */
#define SD_ERR      "<3>"  /* error conditions */
#define SD_WARNING  "<4>"  /* warning conditions */
#define SD_NOTICE   "<5>"  /* normal but significant condition */
#define SD_INFO     "<6>"  /* informational */
#define SD_DEBUG    "<7>"  /* debug-level messages */

/* Basic notification function */
int sd_notify(int unset_environment, const char *state);

/* Printf-style notification functions */
int sd_notifyf(int unset_environment, const char *format, ...);

/* PID-specific notification functions */
int sd_pid_notify(pid_t pid, int unset_environment, const char *state);
int sd_pid_notifyf(pid_t pid, int unset_environment, const char *format, ...);

/* Socket activation stubs */
int sd_listen_fds(int unset_environment);

/* Watchdog stub */
int sd_watchdog_enabled(int unset_environment, uint64_t *usec);

/* System detection */
int sd_booted(void);

#endif	/* SD_DAEMON_H_ */
