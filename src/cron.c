/**
 * @file
 * @brief Shared cron functions.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * These functions used by crontab and cron daemon.
 *
 * This software has been placed into the public domain using CC0.
 */

#include <pwd.h>

#include "cron.h"

int
si_add_size_t(const size_t a,
              const size_t b,
              size_t *const result){
  int wraps;

#ifdef CRON_TEST
  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_si_add_size_t)){
    return 1;
  }
#endif /* CRON_TEST */
  *result = a + b;
  if(*result < a){
    wraps = 1;
  }
  else{
    wraps = 0;
  }
  return wraps;
}

int
si_mul_size_t(const size_t a,
              const size_t b,
              size_t *const result){
  int wraps;

#ifdef CRON_TEST
  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_si_mul_size_t)){
    return 1;
  }
#endif /* CRON_TEST */

  *result = a * b;
  if(b != 0 && a > SIZE_MAX / b){
    wraps = 1;
  }
  else{
    wraps = 0;
  }
  return wraps;
}

char *
cron_get_path_home(void){
  const char *env_home;
  char *path_home;
  const struct passwd *pwd;

  env_home = getenv("HOME");
  if(env_home){
    path_home = strdup(env_home);
  }
  else{
    pwd = getpwuid(geteuid());
    if(pwd){
      path_home = strdup(pwd->pw_dir);
    }
    else{
      path_home = NULL;
    }
  }
  return path_home;
}

char *
cron_get_path_crontab(void){
  const char *const PATH_CRONTAB = "/.config/.crontab";
  char *path_home;
  size_t path_home_len;
  size_t path_crontab_rel_len;
  size_t alloc_len;
  char *path_crontab;
  char *path_copy;

  path_crontab = NULL;
  path_home = cron_get_path_home();
  if(path_home){
    path_home_len = strlen(path_home);
    path_crontab_rel_len = strlen(PATH_CRONTAB);
    if(si_add_size_t(path_home_len,
                     path_crontab_rel_len + 1,
                     &alloc_len) == 0){
      path_crontab = malloc(alloc_len);
      if(path_crontab){
        path_copy = stpcpy(path_crontab, path_home);
        stpcpy(path_copy, PATH_CRONTAB);
      }
    }
    free(path_home);
  }
  return path_crontab;
}

