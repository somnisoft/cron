/**
 * @file
 * @brief Test seams.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This software has been placed into the public domain using CC0.
 */
#ifndef CRON_TEST_SEAMS_H
#define CRON_TEST_SEAMS_H

#include "test.h"

/*
 * Redefine these functions to internal test seams.
 */
#undef clock_gettime
#undef close
#undef dup2
#undef execle
#undef execlp
#undef fclose
#undef ferror
#undef fopen
#undef fork
#undef getpwuid
#undef localtime
#undef malloc
#undef mkdir
#undef open
#undef pipe
#undef read
#undef realloc
#undef remove
#undef rename
#undef cron_sigaction
#undef sigemptyset
#undef snprintf
#undef cron_stat
#undef strdup
#undef strndup
#undef waitpid
#undef write

/**
 * Inject a test seam to replace clock_gettime().
 */
#define clock_gettime  test_seam_clock_gettime

/**
 * Inject a test seam to replace close().
 */
#define close          test_seam_close

/**
 * Inject a test seam to replace dup2().
 */
#define dup2           test_seam_dup2

/**
 * Inject a test seam to replace execle().
 */
#define execle         test_seam_execle

/**
 * Inject a test seam to replace execlp().
 */
#define execlp         test_seam_execlp

/**
 * Inject a test seam to replace fclose().
 */
#define fclose         test_seam_fclose

/**
 * Inject a test seam to replace ferror().
 */
#define ferror         test_seam_ferror

/**
 * Inject a test seam to replace fopen().
 */
#define fopen          test_seam_fopen

/**
 * Inject a test seam to replace fork().
 */
#define fork           test_seam_fork

/**
 * Inject a test seam to replace getpwuid().
 */
#define getpwuid       test_seam_getpwuid

/**
 * Inject a test seam to replace localtime().
 */
#define localtime      test_seam_localtime

/**
 * Inject a test seam to replace malloc().
 */
#define malloc         test_seam_malloc

/**
 * Inject a test seam to replace mkdir().
 */
#define mkdir          test_seam_mkdir

/**
 * Inject a test seam to replace open().
 */
#define open           test_seam_open

/**
 * Inject a test seam to replace pipe().
 */
#define pipe           test_seam_pipe

/**
 * Inject a test seam to replace read().
 */
#define read           test_seam_read

/**
 * Inject a test seam to replace realloc().
 */
#define realloc        test_seam_realloc

/**
 * Inject a test seam to replace remove().
 */
#define remove         test_seam_remove

/**
 * Inject a test seam to replace rename().
 */
#define rename         test_seam_rename

/**
 * Inject a test seam to replace sigaction().
 */
#define cron_sigaction test_seam_sigaction

/**
 * Inject a test seam to replace sigemptyset().
 */
#define sigemptyset    test_seam_sigemptyset

/**
 * Inject a test seam to replace snprintf().
 */
#define snprintf       test_seam_snprintf

/**
 * Inject a test seam to replace stat().
 */
#define cron_stat      test_seam_stat

/**
 * Inject a test seam to replace strdup().
 */
#define strdup         test_seam_strdup

/**
 * Inject a test seam to replace strndup().
 */
#define strndup        test_seam_strndup

/**
 * Inject a test seam to replace waitpid().
 */
#define waitpid        test_seam_waitpid

/**
 * Inject a test seam to replace write().
 */
#define write          test_seam_write

#endif /* CRON_TEST_SEAMS_H */

