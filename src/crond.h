/**
 * @file
 * @brief Cron daemon.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * Cron daemon that reads a configuration file in the user home directory and
 * executes jobs.
 *
 * This software has been placed into the public domain using CC0.
 */
#ifndef CROND_H
#define CROND_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/**
 * Maximum host name size allowed by cron.
 */
#define CROND_MAX_HOST_NAME_SZ (256)

/**
 * Maximum user name size allowed by cron.
 */
#define CROND_MAX_USER_NAME    (256)

/**
 * Maximum length of email subject line sent to mailx.
 *
 * The subject will get truncated if it goes beyond this maximum length.
 */
#define CROND_MAX_SUBJECT_LEN  (80)

/**
 * @defgroup crond_flag crond flags
 *
 * Option flags when running crond.
 */

/**
 * Print verbose messages to STDERR.
 *
 * @ingroup crond_flag
 */
#define CROND_FLAG_VERBOSE (1 << 0)

/**
 * Cron daemon job.
 */
struct crond_job{
  /**
   * Shell command to execute.
   */
  char *command;

  /**
   * If set, pass this to the command through STDIN.
   */
  char *stdin_lines;

  /**
   * Number of bytes in @ref stdin_lines.
   */
  size_t stdin_lines_len;

  /**
   * The minutes to run the job.
   */
  bool minute[60];

  /**
   * The hours to run the job.
   */
  bool hour[24];

  /**
   * The days of the month to run the job.
   */
  bool day[31];

  /**
   * The months of the year to run the job.
   */
  bool month[12];

  /**
   * The days of the week to run the job.
   */
  bool weekday[7];

  /**
   * Padding for alignment.
   */
  bool pad[2];
};

/**
 * Cron daemon context.
 */
struct crond{
  /**
   * Program exit status set to one of the following values.
   *   - EXIT_SUCCESS
   *   - EXIT_FAILURE
   */
  int status_code;

  /**
   * See @ref crond_flag.
   */
  unsigned int flags;

  /**
   * File descriptor of the lock file.
   */
  int fd_lock_file;

  /**
   * Padding for alignment.
   */
  char pad_1[4];

  /**
   * Lock file path.
   *
   * This file gets created when crond starts up and prevents simultaneous
   * crond processes from running.
   */
  char *path_lock_file;

  /**
   * Shell used to execute each job.
   */
  const char *path_shell;

  /**
   * Path to crontab file.
   */
  char *path_crontab;

  /**
   * Current time.
   */
  struct tm *tm;

  /**
   * Previous modification time of the crontab file.
   *
   * See @ref path_crontab.
   */
  struct timespec mtime_crontab;

  /**
   * List of jobs to execute.
   *
   * See @ref crond_job.
   */
  struct crond_job *job_list;

  /**
   * Number of jobs in @ref job_list.
   */
  size_t num_jobs;

  /**
   * Send email with job output to this address.
   */
  char email_to[CROND_MAX_HOST_NAME_SZ + CROND_MAX_USER_NAME + 1];

  /**
   * Padding for alignment.
   */
  char pad_2[7];
};

#ifdef CRON_TEST
void
crond_crontab_parse_line(struct crond *const crond,
                         const char *line);
#endif /* CRON_TEST */

#endif /* CROND_H */

