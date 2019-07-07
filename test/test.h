/**
 * @file
 * @brief Test crontab and crond.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This software has been placed into the public domain using CC0.
 */
#ifndef CRON_TEST_H
#define CRON_TEST_H

#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

int
crond_main(const int argc,
           char *const argv[]);

int
crontab_main(int argc,
             char *const argv[]);

char *
crond_get_path_lock_file(const char *const path_crontab);

bool
test_seam_dec_err_ctr(int *const err_ctr);

int
test_seam_clock_gettime(clockid_t clock_id,
                        struct timespec *res);

int
test_seam_close(int fildes);

int
test_seam_dup2(int fildes,
               int fildes2);

int
test_seam_execle(const char *path,
                 const char *arg0, ...);

int
test_seam_execlp(const char *file,
                 const char *arg0, ...);

int
test_seam_fclose(FILE *stream);

int
test_seam_ferror(FILE *stream);

FILE *
test_seam_fopen(const char *pathname,
                const char *mode);

pid_t
test_seam_fork(void);

struct passwd *
test_seam_getpwuid(uid_t uid);

struct tm *
test_seam_localtime(const time_t *timer);

void *
test_seam_malloc(size_t size);

int
test_seam_mkdir(const char *path,
                mode_t mode);

int
test_seam_open(const char *path,
               int oflag,
               mode_t mode);

int
test_seam_pipe(int fildes[2]);

ssize_t
test_seam_read(int fildes,
               void *buf,
               size_t nbyte);

void *
test_seam_realloc(void *ptr,
                  size_t size);

int
test_seam_remove(const char *path);

int
test_seam_rename(const char *old,
                 const char *new);

int
test_seam_sigaction(int sig,
                    const struct sigaction *act,
                    struct sigaction *oact);

int
test_seam_sigemptyset(sigset_t *set);

int
test_seam_snprintf(char *s,
                   size_t n,
                   const char *format, ...);

int
test_seam_stat(const char *path,
               struct stat *buf);

char *
test_seam_strdup(const char *s);

char *
test_seam_strndup(const char *s,
                  size_t size);

pid_t
test_seam_waitpid(pid_t pid,
                  int *stat_loc,
                  int options);

ssize_t
test_seam_write(int fildes,
                const void *buf,
                size_t nbyte);

extern int g_test_seam_err_force_errno;
extern bool g_test_seam_err_req_fork_jobmon;
extern bool g_test_seam_err_in_fork_jobmon;
extern bool g_test_seam_err_req_fork_mailx;
extern bool g_test_seam_err_in_fork_mailx;

extern int g_test_seam_err_ctr_clock_gettime;
extern int g_test_seam_err_ctr_close;
extern int g_test_seam_err_ctr_dup2;
extern int g_test_seam_err_ctr_execle;
extern int g_test_seam_err_ctr_execlp;
extern int g_test_seam_err_ctr_fclose;
extern int g_test_seam_err_ctr_ferror;
extern int g_test_seam_err_ctr_fopen;
extern int g_test_seam_err_ctr_fork;
extern int g_test_seam_err_ctr_getpwuid;
extern int g_test_seam_err_ctr_localtime;
extern struct tm *g_test_seam_localtime_tm;
extern int g_test_seam_err_ctr_malloc;
extern int g_test_seam_err_ctr_mkdir;
extern int g_test_seam_err_ctr_open;
extern int g_test_seam_err_ctr_pipe;
extern int g_test_seam_err_ctr_read;
extern int g_test_seam_err_ctr_realloc;
extern int g_test_seam_err_ctr_remove;
extern int g_test_seam_err_ctr_rename;
extern int g_test_seam_err_ctr_si_add_size_t;
extern int g_test_seam_err_ctr_si_mul_size_t;
extern int g_test_seam_err_ctr_sigaction;
extern int g_test_seam_err_ctr_sigemptyset;
extern int g_test_seam_err_ctr_snprintf;
extern int g_test_seam_err_ctr_stat;
extern int g_test_seam_err_ctr_strdup;
extern int g_test_seam_err_ctr_strndup;
extern int g_test_seam_err_ctr_waitpid;
extern int g_test_seam_err_ctr_write;

#endif /* CRON_TEST_H */

