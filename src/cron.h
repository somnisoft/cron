/**
 * @file
 * @brief Shared cron functions.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * These functions used by crontab and cron daemon.
 *
 * This software has been placed into the public domain using CC0.
 */
#ifndef CRON_COMMON_H
#define CRON_COMMON_H

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef CRON_TEST
/**
 * Declared some functions with extern linkage so that we can call those
 * functions from the test suite.
 */
# define CRON_LINKAGE extern
# include "../test/seams.h"
#else /* !(CRON_TEST) */
/**
 * Define all functions as static when not testing.
 */
# define CRON_LINKAGE static
/**
 * Redefine sigaction when testing because POSIX has a struct sigaction symbol.
 */
# define cron_sigaction sigaction
/**
 * Redefine stat when testing because POSIX has a struct stat symbol.
 */
# define cron_stat      stat
#endif /* CRON_TEST */

/**
 * Default buffer size used when reading files.
 */
#define CRON_READ_BUFFER_SZ 1000

/**
 * Add two size_t values and check for wrap.
 *
 * @param[in]  a      Add this value with @p b.
 * @param[in]  b      Add this value with @p a.
 * @param[out] result Save the result of the addition here.
 * @retval     1      Value wrapped.
 * @retval     0      Value did not wrap.
 */
int
si_add_size_t(const size_t a,
              const size_t b,
              size_t *const result);

/**
 * Multiply two size_t values and check for wrap.
 *
 * @param[in]  a      Multiply this value with @p b.
 * @param[in]  b      Multiply this value with @p a.
 * @param[out] result Save the result of the multiplication here.
 * @retval     1      Value wrapped.
 * @retval     0      Value did not wrap.
 */
int
si_mul_size_t(const size_t a,
              const size_t b,
              size_t *const result);

/**
 * Get default home path of the current user.
 *
 * This will first attempt to get the path from the HOME environment variable.
 * If that does not exist, then it will query getpwuid using the effective
 * user id of the current process.
 *
 * @retval char* User home path. The caller must free this when finished.
 * @retval NULL  Failed to get path.
 */
char *
cron_get_path_home(void);

/**
 * Get path to the crontab file of the current user.
 *
 * @retval char* Path to crontab file. The caller must free this when finished.
 * @retval NULL  Failed to get the crontab path.
 */
char *
cron_get_path_crontab(void);

#endif /* CRON_COMMON_H */

