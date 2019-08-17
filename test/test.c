/**
 * @file
 * @brief Test crontab and crond.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This software has been placed into the public domain using CC0.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/cron.h"
#include "test.h"

/**
 * Path to the simple crontab file.
 */
#define PATH_CRONTAB_SIMPLE "test/crontabs/simple.txt"

/**
 * Path to the file created by the command in the simple crontab.
 */
#define PATH_TMP_SIMPLE "/tmp/test-cron-simple.txt"

/**
 * Path to the default crontab file retrieved from @ref cron_get_path_crontab.
 */
static char *
g_path_crontab = NULL;

/**
 * Number of arguments in @ref g_argv.
 */
static int
g_argc;

/**
 * Argument list to pass to the crontab and crond programs.
 */
static char **
g_argv;

/**
 * Print a message to STDERR before running a unit test.
 *
 * @param[in] description Description of test case.
 */
static void
test_describe(const char *const description){
  assert(fprintf(stderr, "%s\n", description) >= 0);
}

/**
 * Test harness for @ref si_add_size_t and @ref si_mul_size_t.
 *
 * @param[in] si_op_size  Set to @ref si_add_size_t or @ref si_mul_size_t.
 * @param[in] a           First operand value.
 * @param[in] b           Second operand value.
 * @param[in] expect_calc Expected result of calculation.
 * @param[in] expect_rc   Expected wrap return code.
 */
static void
test_unit_si_op_size(int (*si_op_size)(const size_t a,
                                       const size_t b,
                                       size_t *const result),
                     const size_t a,
                     const size_t b,
                     const size_t expect_calc,
                     const int expect_rc){
  int rc;
  size_t result;

  rc = si_op_size(a, b, &result);
  assert(rc == expect_rc);
  if(expect_rc == 0){
    assert(result == expect_calc);
  }
}

/**
 * Run all test cases for the si_* functions.
 */
static void
test_unit_si_all(void){
  test_unit_si_op_size(si_add_size_t, 0, 1, 1, 0);
  test_unit_si_op_size(si_add_size_t, SIZE_MAX, 1, 0, 1);

  test_unit_si_op_size(si_mul_size_t, 2, 2, 4, 0);
  test_unit_si_op_size(si_mul_size_t, SIZE_MAX / 2, 2, SIZE_MAX - 1, 0);
  test_unit_si_op_size(si_mul_size_t, SIZE_MAX, 2, SIZE_MAX / 2, 1);
}

/**
 * Run all unit tests.
 */
static void
test_unit_all(void){
  test_unit_si_all();
}

/**
 * Sleep for a maximum amount of time required for the touch program to fork
 * and create the test file.
 *
 * This should give the child enough time to install signal handlers, parse
 * the crontab, and fork the process that creates the test files.
 */
static void
test_sleep_max_file(void){
  struct timespec timespec;
  int rc;

  timespec.tv_sec  = 0;
  timespec.tv_nsec = 500000000; /* 500 milliseconds */
  rc = clock_nanosleep(CLOCK_REALTIME, 0, &timespec, NULL);
  assert(rc == 0);
}

/**
 * Remove the cron daemon lock file if it exists.
 */
static void
test_crond_remove_lock_file(void){
  char *path_crontab;
  char *path_lock_file;

  path_crontab = cron_get_path_crontab();
  assert(path_crontab);
  path_lock_file = crond_get_path_lock_file(path_crontab);
  assert(path_lock_file);
  remove(path_lock_file);
  free(path_crontab);
  free(path_lock_file);
}

/**
 * Check if a file exists.
 *
 * @param[in] path  Path to file.
 * @retval    true  File exists.
 * @retval    false File does not exist.
 */
static bool
test_file_exists(const char *const path){
  bool exists;

  if(access(path, R_OK) == 0){
    exists = true;
  }
  else{
    exists = false;
  }
  return exists;
}

/**
 * Check if the @ref PATH_TMP_SIMPLE file exists and remove it if it does.
 *
 * @param[in] should_exist Set to true if the file should exist.
 */
static void
test_simple_file_verify_remove(const bool should_exist){
  bool file_exists;

  file_exists = test_file_exists(PATH_TMP_SIMPLE);
  assert(file_exists == should_exist);
  if(file_exists){
    assert(remove(PATH_TMP_SIMPLE) == 0);
  }
}

/**
 * Check if the crontab file exists on the file system.
 *
 * @retval true  Crontab file exists.
 * @retval false Crontab file does not exist.
 */
static bool
test_crontab_exists(void){
  return test_file_exists(g_path_crontab);
}

/**
 * Set the EDITOR environment variable.
 *
 * @param[in] editor Character string pointing to an editor program, or
 *                   NULL to unset the EDITOR variable.
 */
static void
test_crontab_set_editor(const char *const editor){
  if(editor){
    assert(setenv("EDITOR", editor, 1) == 0);
  }
  else{
    assert(unsetenv("EDITOR") == 0);
  }
}

/**
 * Call @ref crond_main with the preset global argument parameters.
 *
 * @param[in] expect_exit_status Expected exit code from @ref crond_main.
 */
static void
test_crond_main(const int expect_exit_status){
  int rc;

  optind = 0;
  rc = crond_main(g_argc, g_argv);
  assert(rc == expect_exit_status);
}

/**
 * Call @ref crontab_main with the preset global argument parameters.
 *
 * @param[in] expect_exit_status Expected exit code from @ref crontab_main.
 */
static void
test_crontab_main(const int expect_exit_status){
  int rc;

  optind = 0;
  rc = crontab_main(g_argc, g_argv);
  assert(rc == expect_exit_status);
}

/**
 * Add a file to the crontab.
 *
 * Usage: crontab [file]
 *
 * @param[in] file               Set to file path to read from that file
 *                               or NULL to read crontab from stdin.
 * @param[in] expect_exit_status Expected exit code from @ref crontab_main.
 */
static void
test_crontab_add(const char *const file,
                 const int expect_exit_status){
  if(file){
    g_argc = 2;
    strcpy(g_argv[1], file);
  }
  else{
    g_argc = 1;
    g_argv[1][0] = '\0';
  }
  test_crontab_main(expect_exit_status);
  if(expect_exit_status == EXIT_SUCCESS){
    assert(test_crontab_exists() == true);
  }
}

/**
 * Edit an existing crontab or create a new one.
 *
 * Usage: crontab -e
 *
 * @param[in] expect_exit_status Expected exit code from @ref crontab_main.
 */
static void
test_crontab_edit(const int expect_exit_status){
  g_argc = 2;
  strcpy(g_argv[1], "-e");
  test_crontab_main(expect_exit_status);
}

/**
 * Print the existing crontab to STDOUT.
 *
 * Usage: crontab -l
 *
 * @param[in] expect_exit_status Expected exit code from @ref crontab_main.
 */
static void
test_crontab_list(const int expect_exit_status){
  g_argc = 2;
  strcpy(g_argv[1], "-l");
  test_crontab_main(expect_exit_status);
}

/**
 * Remove an existing crontab.
 *
 * Usage: crontab -r
 *
 * @param[in] expect_exit_status Expected exit code from @ref crontab_main.
 */
static void
test_crontab_remove(const int expect_exit_status){
  g_argc = 2;
  strcpy(g_argv[1], "-r");
  test_crontab_main(expect_exit_status);
  assert(test_crontab_exists() == false);
}

/**
 * Compare the contents of the current crontab with another file.
 *
 * @param[in] path_cmp Compare this file with the crontab.
 */
static void
test_crontab_list_check_file(const char *const path_cmp){
  const char *const PATH_TMP_LIST_CMP = "/tmp/crontab.list";
  pid_t pid;
  int wstatus;
  FILE *fp;
  char cmd_cmp[1000];

  pid = fork();
  assert(pid >= 0);
  if(pid == 0){
    fp = freopen(PATH_TMP_LIST_CMP, "w", stdout);
    assert(fp);
    test_crontab_list(EXIT_SUCCESS);
    exit(EXIT_SUCCESS);
  }
  assert(waitpid(pid, &wstatus, 0) == pid);
  assert(WEXITSTATUS(wstatus) == EXIT_SUCCESS);

  sprintf(cmd_cmp, "cmp \'%s\' \'%s\'", path_cmp, PATH_TMP_LIST_CMP);
  assert(system(cmd_cmp) == 0);
}

/**
 * Test scenario where crontab creates the config directory if it does
 * not already exist.
 */
static void
test_crontab_mkdir(void){
  const char *old_env;
  int rc;

  /* Remove the test config directory if it exists. */
  remove("/tmp/.config/.crontab");
  rc = rmdir("/tmp/.config");
  assert(rc == 0 || errno == ENOENT);

  old_env = getenv("HOME");
  assert(old_env);
  assert(setenv("HOME", "/tmp", 1) == 0);

  /* Add a crontab when the config directory does not exist. */
  g_argc = 2;
  strcpy(g_argv[1], PATH_CRONTAB_SIMPLE);
  test_crontab_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/.config/.crontab") == true);
  assert(remove("/tmp/.config/.crontab") == 0);
  assert(rmdir("/tmp/.config") == 0);

  /* Edit a new crontab when the config directory does not exist. */
  test_crontab_set_editor("test/crontab-editor-touch.sh");
  test_crontab_edit(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/.config/.crontab") == true);
  assert(remove("/tmp/.config/.crontab") == 0);
  assert(rmdir("/tmp/.config") == 0);
  test_crontab_set_editor(NULL);

  assert(setenv("HOME", old_env, 1) == 0);
}

/**
 * Run all test cases for the crontab program.
 */
static void
test_crontab_all(void){
  int i;

  /* Try to remove crontab file that does not exist. */
  test_crontab_remove(EXIT_FAILURE);

  /* Invalid argument. */
  g_argc = 2;
  strcpy(g_argv[1], "-a");
  test_crontab_main(EXIT_FAILURE);

  /* Error if multiple file parameters given. */
  g_argc = 3;
  strcpy(g_argv[1], PATH_CRONTAB_SIMPLE);
  strcpy(g_argv[2], PATH_CRONTAB_SIMPLE);
  test_crontab_main(EXIT_FAILURE);

  /* Multiple incompatible flags given. */
  g_argc = 4;
  strcpy(g_argv[1], "-e");
  strcpy(g_argv[2], "-l");
  strcpy(g_argv[3], "-r");
  test_crontab_main(EXIT_FAILURE);

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_si_add_size_t = i;
    test_crontab_edit(EXIT_FAILURE);
    g_test_seam_err_ctr_si_add_size_t = -1;
  }

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_malloc = i;
    test_crontab_edit(EXIT_FAILURE);
    g_test_seam_err_ctr_malloc = -1;
  }

  for(i = 0; i < 3; i++){
    g_test_seam_err_ctr_si_add_size_t = i;
    test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
    g_test_seam_err_ctr_si_add_size_t = -1;
  }

  for(i = 0; i < 3; i++){
    g_test_seam_err_ctr_malloc = i;
    test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
    g_test_seam_err_ctr_malloc = -1;
  }

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_strdup = i;
    test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
    g_test_seam_err_ctr_strdup = -1;
  }

  g_test_seam_err_ctr_mkdir = 0;
  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
  g_test_seam_err_ctr_mkdir = -1;

  g_test_seam_err_ctr_mkdir = 0;
  test_crontab_edit(EXIT_FAILURE);
  g_test_seam_err_ctr_mkdir = -1;

  test_crontab_mkdir();

  /* crontab_edit_process - fork */
  g_test_seam_err_ctr_fork = 0;
  test_crontab_edit(EXIT_FAILURE);
  g_test_seam_err_ctr_fork = -1;

  /* crontab_edit_process - execlp */
  g_test_seam_err_ctr_execlp = 0;
  test_crontab_edit(EXIT_FAILURE);
  g_test_seam_err_ctr_execlp = -1;

  /* crontab_edit_process - waitpid != EINTR */
  test_crontab_set_editor("test/crontab-editor-touch.sh");
  g_test_seam_err_ctr_waitpid = 0;
  test_crontab_edit(EXIT_FAILURE);
  g_test_seam_err_ctr_waitpid = -1;

  /* crontab_edit_process - waitpid == EINTR */
  test_crontab_set_editor("test/crontab-editor-touch.sh");
  g_test_seam_err_force_errno = EINTR;
  g_test_seam_err_ctr_waitpid = 0;
  test_crontab_edit(EXIT_SUCCESS);
  g_test_seam_err_ctr_waitpid = -1;
  g_test_seam_err_force_errno = 0;

  /* crontab editor exits with non-0 status. */
  test_crontab_set_editor("test/crontab-editor-exit-bad.sh");
  test_crontab_edit(EXIT_FAILURE);

  /* WIFEXITED */
  test_crontab_set_editor("test/crontab-editor-sigkill.sh");
  test_crontab_edit(EXIT_FAILURE);

  /* Try to add a crontab that does not exist. */
  test_crontab_add("test/crontab/noexist.txt", EXIT_FAILURE);

  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_SUCCESS);
  test_crontab_list_check_file(PATH_CRONTAB_SIMPLE);

  /* crontab_file_set - fopen */
  g_test_seam_err_ctr_fopen = 1;
  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
  g_test_seam_err_ctr_fopen = -1;

  /* crontab_file_set - fclose */
  g_test_seam_err_ctr_fclose = 0;
  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_FAILURE);
  g_test_seam_err_ctr_fclose = -1;

  g_test_seam_err_ctr_ferror = 0;
  test_crontab_list(EXIT_FAILURE);
  g_test_seam_err_ctr_ferror = -1;

  g_test_seam_err_ctr_ferror = 1;
  test_crontab_list(EXIT_FAILURE);
  g_test_seam_err_ctr_ferror = -1;

  g_test_seam_err_ctr_fclose = 0;
  test_crontab_list(EXIT_FAILURE);
  g_test_seam_err_ctr_fclose = -1;

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_fopen = i;
    test_crontab_edit(EXIT_FAILURE);
    g_test_seam_err_ctr_fopen = -1;
  }
  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_fclose = i;
    test_crontab_edit(EXIT_FAILURE);
    g_test_seam_err_ctr_fclose = -1;
  }

  test_crontab_set_editor("test/crontab-editor-touch.sh");
  g_test_seam_err_ctr_rename = 0;
  test_crontab_edit(EXIT_FAILURE);
  g_test_seam_err_ctr_rename = -1;

  test_crontab_remove(EXIT_SUCCESS);
  test_crontab_list(EXIT_FAILURE);

  test_crontab_set_editor("test/crontab-editor-touch.sh");
  test_crontab_edit(EXIT_SUCCESS);

  /* Add a crontab using STDIN. */
  assert(system("build/debug/crontab < " PATH_CRONTAB_SIMPLE) == 0);
  test_crontab_list_check_file(PATH_CRONTAB_SIMPLE);
}

/**
 * Fork a new crond process.
 *
 * @return Child process ID.
 */
static pid_t
test_crond_fork(void){
  pid_t pid;
  int exit_status;

  pid = fork();
  assert(pid >= 0);
  if(pid == 0){
    g_argc = 2;
    strcpy(g_argv[0], "crond");
    strcpy(g_argv[1], "-v");
    exit_status = crond_main(g_argc, g_argv);
    exit(exit_status);
  }
  return pid;
}

/**
 * Wait for the crond process to exit.
 *
 * @param[in] pid                Process ID to wait for.
 * @param[in] expect_exit_status Expected process exit code.
 */
static void
test_crond_wait(const pid_t pid,
                const int expect_exit_status){
  int status;

  assert(waitpid(pid, &status, 0) == pid);
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == expect_exit_status);
}

/**
 * Set the localtime() return value.
 *
 * @param[in] tm_min  Minute.
 * @param[in] tm_hour Hour
 * @param[in] tm_mday Day of the month.
 * @param[in] tm_mon  Month of the year.
 * @param[in] tm_wday Day of the week.
 */
static void
test_crond_set_tm(const int tm_min,
                  const int tm_hour,
                  const int tm_mday,
                  const int tm_mon,
                  const int tm_wday){
  static struct tm g_test_tm;

  memset(&g_test_tm, 0, sizeof(g_test_tm));
  g_test_tm.tm_min  = tm_min;
  g_test_tm.tm_hour = tm_hour;
  g_test_tm.tm_mday = tm_mday;
  g_test_tm.tm_mon  = tm_mon;
  g_test_tm.tm_wday = tm_wday;
  g_test_seam_localtime_tm = &g_test_tm;
}

/**
 * Fork a new crond process, sleep for a small time frame, and then shut down
 * the process and wait for it to exit.
 *
 * @param[in] expect_exit_status Expected return code of crond process.
 */
static void
test_crond_fork_main(const int expect_exit_status){
  pid_t pid;

  pid = test_crond_fork();
  test_sleep_max_file();
  assert(kill(pid, SIGTERM) == 0);
  g_test_seam_err_ctr_waitpid = -1;
  test_crond_wait(pid, expect_exit_status);
}

/**
 * Run crond and verify that it created the file.
 *
 * @param[in] path File to check and remove.
 */
static void
test_crond_verify_file_create(const char *const path){
  if(test_file_exists(path)){
    assert(remove(path) == 0);
  }
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists(path));
  assert(remove(path) == 0);
}

/**
 * Test scenario where we remove a crontab file so that the jobs no longer
 * run.
 *   1. Create the crontab.
 *   2. The crond job creates a file.
 *   3. Remove the crontab.
 *   4. Remove the file.
 *   5. Ensure that the job does not run by checking if the file has been
 *      created again.
 */
static void
test_crond_remove_crontab(void){
  pid_t pid;

  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_SUCCESS);

  test_crond_set_tm(1, 1, 1, 1, 1);
  pid = test_crond_fork();
  test_sleep_max_file();
  test_simple_file_verify_remove(true);
  test_crontab_remove(EXIT_SUCCESS);
  assert(kill(pid, SIGHUP) == 0);
  test_sleep_max_file();
  test_simple_file_verify_remove(false);
  assert(kill(pid, SIGTERM) == 0);
  test_crond_wait(pid, EXIT_SUCCESS);

  g_test_seam_localtime_tm = NULL;
}

/**
 * Test scenarios where we send stdin lines to the job command using '%'.
 */
static void
test_crond_stdin_lines(void){
  test_crontab_add("test/crontabs/stdin-lines.txt", EXIT_SUCCESS);

  test_crond_set_tm(1, 1, 1, 1, 1);
  test_crond_verify_file_create("/tmp/test-cron-stdin-1.txt");

  test_crond_set_tm(2, 2, 2, 2, 2);
  test_crond_verify_file_create("/tmp/test-cron-stdin-1.txt");
  assert(test_file_exists("/tmp/test-cron-stdin-2.txt"));
  assert(remove("/tmp/test-cron-stdin-2.txt") == 0);

  test_crond_set_tm(3, 3, 3, 3, 3);
  test_crond_verify_file_create("/tmp/test-cron-stdin-1.txt");
  assert(test_file_exists("/tmp/test-cron-stdin-2.txt"));
  assert(remove("/tmp/test-cron-stdin-2.txt") == 0);
  assert(test_file_exists("/tmp/test-cron-stdin-3.txt"));
  assert(remove("/tmp/test-cron-stdin-3.txt") == 0);

  test_crond_set_tm(4, 4, 4, 4, 4);
  test_crond_verify_file_create("/tmp/test-cron-stdin-%\\.txt");
  assert(test_file_exists("/tmp/test-cron-stdin-2.txt"));
  assert(remove("/tmp/test-cron-stdin-2.txt") == 0);

  test_crond_set_tm(5, 5, 5, 5, 5);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-stdin-1.txt") == false);

  g_test_seam_err_ctr_strdup = 1;
  test_crond_set_tm(1, 1, 1, 1, 1);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-stdin-1.txt") == false);
  g_test_seam_err_ctr_strdup = -1;

  test_describe("failed to write data to child process");
  g_test_seam_err_force_errno = ENOMEM;
  g_test_seam_err_ctr_write = 0;
  test_crond_set_tm(1, 1, 1, 1, 1);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-stdin-1.txt") == false);
  g_test_seam_err_ctr_write = -1;
  g_test_seam_err_force_errno = 0;

  test_describe("simulate an interrupt during write but allow the next write to proceed");
  g_test_seam_err_force_errno = EINTR;
  g_test_seam_err_ctr_write = 0;
  test_crond_set_tm(1, 1, 1, 1, 1);
  test_crond_verify_file_create("/tmp/test-cron-stdin-1.txt");
  g_test_seam_err_ctr_write = -1;
  g_test_seam_err_force_errno = 0;

  g_test_seam_localtime_tm = NULL;
}

/**
 * Test scenarios that send job output through mailx.
 *
 * @todo Verify mailx output.
 */
static void
test_crond_mailx(void){
  int i;
  const char *old_env;

  test_crontab_add("test/crontabs/mailx.txt", EXIT_SUCCESS);

  test_crond_set_tm(4, 4, 4, 4, 4);

  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");

  test_describe("(1) Get username using getpwuid instead of env variable");
  test_describe("(2) Test getpwuid does not return an entry (empty username)");
  old_env = getenv("LOGNAME");
  assert(old_env);
  assert(unsetenv("LOGNAME") == 0);
  /* (1) */
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  /* (2) */
  g_test_seam_err_ctr_getpwuid = 0;
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  g_test_seam_err_ctr_getpwuid = -1;
  assert(setenv("LOGNAME", old_env, 1) == 0);

  g_test_seam_err_force_errno = ENOMEM;
  g_test_seam_err_ctr_read = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_read = -1;
  g_test_seam_err_force_errno = 0;

  g_test_seam_err_force_errno = EINTR;
  g_test_seam_err_ctr_read = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_read = -1;
  g_test_seam_err_force_errno = 0;

  g_test_seam_err_ctr_si_add_size_t = 2;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_si_add_size_t = -1;

  g_test_seam_err_ctr_realloc = 1;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_realloc = -1;

  g_test_seam_err_ctr_snprintf = 0;
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  g_test_seam_err_ctr_snprintf = -1;

  test_describe("failed to create write pipe for mailx process");
  g_test_seam_err_ctr_pipe = 2;
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  g_test_seam_err_ctr_pipe = -1;

  test_describe("failed to fork mailx process");
  g_test_seam_err_ctr_fork = 3;
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  g_test_seam_err_ctr_fork = -1;

  g_test_seam_err_req_fork_mailx = true;
  g_test_seam_err_ctr_dup2 = 0;
  test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
  g_test_seam_err_ctr_dup2 = -1;
  g_test_seam_err_req_fork_mailx = false;

  for(i = 0; i < 2; i++){
    g_test_seam_err_req_fork_mailx = true;
    g_test_seam_err_ctr_close = i;
    test_crond_verify_file_create("/tmp/test-cron-echo-output.txt");
    g_test_seam_err_ctr_close = -1;
    g_test_seam_err_req_fork_mailx = false;
  }

  g_test_seam_err_ctr_execle = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_execle = -1;

  g_test_seam_err_ctr_execlp = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  g_test_seam_err_ctr_execlp = -1;

  g_test_seam_localtime_tm = NULL;
}

/**
 * Ensure the special string commands get executed.
 */
static void
test_crond_special_strings(void){
  test_crontab_add("test/crontabs/special-strings.txt", EXIT_SUCCESS);

  test_describe("@yearly / @annually -> 0 0 1 1 *");
  test_crond_set_tm(0, 0, 1, 1, 0);
  test_crond_verify_file_create("/tmp/test-cron-yearly.txt");
  assert(test_file_exists("/tmp/test-cron-annually.txt"));
  assert(remove("/tmp/test-cron-annually.txt") == 0);

  test_describe("@monthly -> 0 0 1 * *");
  test_crond_set_tm(0, 0, 1, 5, 5);
  test_crond_verify_file_create("/tmp/test-cron-monthly.txt");

  test_describe("@weekly -> 0 0 * * 0");
  test_crond_set_tm(0, 0, 1, 5, 0);
  test_crond_verify_file_create("/tmp/test-cron-weekly.txt");

  test_describe("@daily / @midnight -> 0 0 * * *");
  test_crond_set_tm(0, 0, 2, 2, 2);
  test_crond_verify_file_create("/tmp/test-cron-daily.txt");
  assert(test_file_exists("/tmp/test-cron-midnight.txt"));
  assert(remove("/tmp/test-cron-midnight.txt") == 0);

  test_describe("@hourly -> 0 * * * *");
  test_crond_set_tm(0, 0, 0, 0, 0);
  test_crond_verify_file_create("/tmp/test-cron-hourly.txt");

  test_describe("@invalid");
  assert(test_file_exists("/tmp/test-cron-invalid.txt") == false);

  g_test_seam_localtime_tm = NULL;
}

/**
 * Test different variations of specifying integers in the time fields.
 *   - Single values
 *   - Commas
 *   - Dashes
 */
static void
test_crond_field_ints(void){
  int i;

  test_crontab_add("test/crontabs/field-ints.txt", EXIT_SUCCESS);

  test_describe("(1) Only integers");
  test_crond_set_tm(0, 10, 2, 3, 4);
  test_crond_verify_file_create("/tmp/test-cron-field-1.txt");

  test_describe("(2) Commas");
  for(i = 1; i < 6; i++){
    test_crond_set_tm(1, 2, 3, 4, i);
    test_crond_verify_file_create("/tmp/test-cron-field-2.txt");
  }

  test_describe("(3) Dashes");
  for(i = 2; i < 6; i++){
    test_crond_set_tm(i, 3, 4, 5, 6);
    test_crond_verify_file_create("/tmp/test-cron-field-3.txt");
  }

  test_describe("(4) Invalid dash");
  test_crond_set_tm(1, 1, 1, 1, 1);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-field-4.txt") == false);

  test_describe("(5) Value too high");
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-field-5.txt") == false);

  test_describe("(6) Dash value swapped");
  for(i = 1; i < 3; i++){
    test_crond_set_tm(i, 2, 3, 4, 5);
    test_crond_verify_file_create("/tmp/test-cron-field-6.txt");
  }

  test_describe("(7) Invalid character");
  test_crond_set_tm(1, 3, 6, 2, 0);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-field-7.txt") == false);

  test_describe("(8) Range value too high");
  for(i = 55; i < 60; i++){
    test_crond_set_tm(i, 2, 3, 4, 5);
    test_crond_verify_file_create("/tmp/test-cron-field-8.txt");
  }

  test_describe("(9) Range value the same");
  test_crond_set_tm(2, 3, 4, 5, 6);
  test_crond_verify_file_create("/tmp/test-cron-field-9.txt");

  test_describe("(10) Invalid character");
  test_crond_set_tm(1, 2, 3, 4, 5);
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists("/tmp/test-cron-field-10.txt") == false);

  g_test_seam_localtime_tm = NULL;
}

/**
 * Test cases with the simple crontab file.
 */
static void
test_crond_simple_file(void){
  int i;
  const char *old_env;

  test_crontab_add(PATH_CRONTAB_SIMPLE, EXIT_SUCCESS);
  remove(PATH_TMP_SIMPLE);

  test_describe("parse crontab file - ferror");
  g_test_seam_err_ctr_ferror = 0;
  test_crond_fork_main(EXIT_FAILURE);
  g_test_seam_err_ctr_ferror = -1;

  test_describe("parse crontab file - fclose");
  g_test_seam_err_ctr_fclose = 0;
  test_crond_fork_main(EXIT_FAILURE);
  g_test_seam_err_ctr_fclose = -1;

  test_describe("reallocarry call failed when appending jobs");
  g_test_seam_err_ctr_si_mul_size_t = 0;
  test_crond_fork_main(EXIT_FAILURE);
  g_test_seam_err_ctr_si_mul_size_t = -1;

  test_describe("fail to fork job monitor process");
  g_test_seam_err_ctr_fork = 1;
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(false);
  g_test_seam_err_ctr_fork = -1;

  test_describe("failed to create pipe in job monitor process");
  g_test_seam_err_ctr_pipe = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(false);
  g_test_seam_err_ctr_pipe = -1;

  test_describe("fail to fork job process");
  g_test_seam_err_ctr_fork = 2;
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(false);
  g_test_seam_err_ctr_fork = -1;

  test_describe("fail to dup2 descriptors in job process");
  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_dup2 = i;
    test_crond_fork_main(EXIT_SUCCESS);
    test_simple_file_verify_remove(false);
    g_test_seam_err_ctr_dup2 = -1;
  }

  test_describe("fail to close dup2'ed file descriptors in job process");
  for(i = 0; i < 4; i++){
    g_test_seam_err_req_fork_jobmon = true;
    g_test_seam_err_ctr_close = i;
    test_crond_fork_main(EXIT_SUCCESS);
    test_simple_file_verify_remove(false);
    g_test_seam_err_ctr_close = -1;
    g_test_seam_err_req_fork_jobmon = false;
  }

  test_describe("failed to wait for child process to complete");
  g_test_seam_err_force_errno = ENOMEM;
  g_test_seam_err_ctr_waitpid = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(true);
  g_test_seam_err_ctr_waitpid = -1;
  g_test_seam_err_force_errno = 0;

  test_describe("simulate a child waitpid getting interrupted");
  g_test_seam_err_force_errno = EINTR;
  g_test_seam_err_ctr_waitpid = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(true);
  g_test_seam_err_ctr_waitpid = -1;
  g_test_seam_err_force_errno = 0;

  test_describe("use the default shell if not provided in the environment");
  old_env = getenv("SHELL");
  assert(old_env);
  assert(unsetenv("SHELL") == 0);
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(true);
  assert(setenv("SHELL", old_env, 1) == 0);

  /*
   * (1) Get home path using getpwuid instead of env variable.
   * (2) Test scenario where getpwuid does not return an entry (empty home).
   */
  old_env = getenv("HOME");
  assert(old_env);
  assert(unsetenv("HOME") == 0);
  /* (1) */
  test_crond_fork_main(EXIT_SUCCESS);
  test_simple_file_verify_remove(true);
  /* (2) */
  g_test_seam_err_ctr_getpwuid = 0;
  test_crond_fork_main(EXIT_FAILURE);
  test_simple_file_verify_remove(false);
  g_test_seam_err_ctr_getpwuid = -1;
  assert(setenv("HOME", old_env, 1) == 0);

  test_describe("command parsing failed - strndup");
  g_test_seam_err_ctr_strndup = 0;
  test_crond_fork_main(EXIT_SUCCESS);
  assert(test_file_exists(PATH_TMP_SIMPLE) == false);
  g_test_seam_err_ctr_strndup = -1;
}

/**
 * Run all test cases for crond.
 */
static void
test_crond_all(void){
  int i;
  pid_t pid;

  test_crond_remove_lock_file();
  test_crontab_remove(EXIT_SUCCESS);

  test_describe("invalid argument");
  g_argc = 2;
  strcpy(g_argv[0], "crond");
  strcpy(g_argv[1], "-a");
  test_crond_main(EXIT_FAILURE);
  strcpy(g_argv[1], "-v");

  g_test_seam_err_ctr_sigemptyset = 0;
  test_crond_main(EXIT_FAILURE);
  g_test_seam_err_ctr_sigemptyset = -1;
  for(i = 0; i < 3; i++){
    g_test_seam_err_ctr_sigaction = i;
    test_crond_main(EXIT_FAILURE);
    g_test_seam_err_ctr_sigaction = -1;
  }

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_si_add_size_t = i;
    test_crond_main(EXIT_FAILURE);
    g_test_seam_err_ctr_si_add_size_t = -1;
  }

  for(i = 0; i < 2; i++){
    g_test_seam_err_ctr_malloc = i;
    test_crond_main(EXIT_FAILURE);
    g_test_seam_err_ctr_malloc = -1;
  }

  g_test_seam_err_ctr_clock_gettime = 0;
  test_crond_main(EXIT_FAILURE);
  g_test_seam_err_ctr_clock_gettime = -1;

  g_test_seam_err_ctr_localtime = 0;
  test_crond_main(EXIT_FAILURE);
  g_test_seam_err_ctr_localtime = -1;

  test_describe("fail to create lock file");
  g_test_seam_err_ctr_open = 0;
  test_crond_main(EXIT_FAILURE);
  g_test_seam_err_ctr_open = -1;

  test_describe("fail to remove lock file");
  g_test_seam_err_ctr_localtime = 0;
  g_test_seam_err_ctr_remove = 0;
  test_crond_main(EXIT_FAILURE);
  g_test_seam_err_ctr_remove = -1;
  g_test_seam_err_ctr_localtime = -1;

  test_describe("fail because the lock file still exists (from previous step)");
  test_crond_main(EXIT_FAILURE);

  test_crond_remove_lock_file();

  test_describe("failed to close lock file");
  g_test_seam_err_ctr_close = 0;
  test_crond_fork_main(EXIT_FAILURE);
  g_test_seam_err_ctr_close = -1;
  test_crond_remove_lock_file();

  test_describe("send a SIGTERM kill signal to crond");
  pid = test_crond_fork();
  test_sleep_max_file();
  assert(kill(pid, SIGTERM) == 0);
  test_crond_wait(pid, EXIT_SUCCESS);
  test_crond_remove_lock_file();

  test_describe("send a SIGINT kill signal to crond");
  pid = test_crond_fork();
  test_sleep_max_file();
  assert(kill(pid, SIGINT) == 0);
  test_crond_wait(pid, EXIT_SUCCESS);
  test_crond_remove_lock_file();

  test_describe("stat call failed");
  g_test_seam_err_ctr_stat = 0;
  g_test_seam_err_force_errno = ENOMEM;
  test_crond_fork_main(EXIT_FAILURE);
  g_test_seam_err_force_errno = 0;
  g_test_seam_err_ctr_stat = -1;

  test_crond_simple_file();
  test_crond_remove_crontab();
  test_crond_stdin_lines();
  test_crond_mailx();
  test_crond_special_strings();
  test_crond_field_ints();
}

/**
 * Run all test cases for crontab and crond.
 */
static void
test_cron_all(void){
  test_unit_all();
  test_crontab_all();
  test_crond_all();
}

/**
 * Test crontab and crond.
 *
 * Usage: test
 *
 * @retval 0 All tests passed.
 */
int
main(void){
  const size_t MAX_ARGS = 4;
  const size_t MAX_ARG_LENGTH = 255;
  size_t i;

  g_path_crontab = cron_get_path_crontab();
  assert(g_path_crontab);

  g_argv = malloc(MAX_ARGS * sizeof(g_argv));
  assert(g_argv);
  for(i = 0; i < MAX_ARGS; i++){
    g_argv[i] = malloc(MAX_ARG_LENGTH * sizeof(*g_argv));
    assert(g_argv[i]);
  }
  strcpy(g_argv[0], "crontab");

  /* Remove crontab file if it exists. */
  if(test_crontab_exists()){
    assert(remove(g_path_crontab) == 0);
  }

  test_cron_all();

  free(g_path_crontab);

  for(i = 0; i < MAX_ARGS; i++){
    free(g_argv[i]);
  }
  free(g_argv);

  return 0;
}

