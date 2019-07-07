/**
 * @file
 * @brief View or update the schedule file used by crond.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This implementation only supports modifying a crontab file defined
 * in the user home directory, and therefore does not require running a
 * process with setuid or root privileges.
 *
 * This software has been placed into the public domain using CC0.
 */

#include "cron.h"

/**
 * @defgroup crontab_flag Crontab flags
 *
 * Option flags used when running the crontab program.
 */

/**
 * Edit an existing crontab or create a new one (@ref crontab_edit).
 *
 * @ingroup crontab_flag
 */
#define CRONTAB_OPTION_EDIT   (1 << 0)

/**
 * Print an existing crontab to STDOUT (@ref crontab_list).
 *
 * @ingroup crontab_flag
 */
#define CRONTAB_OPTION_LIST   (1 << 1)

/**
 * Remove an existing crontab file (@ref crontab_remove).
 *
 * @ingroup crontab_flag
 */
#define CRONTAB_OPTION_REMOVE (1 << 2)

/**
 * Crontab context.
 */
struct crontab{
  /**
   * Path to the crontab file.
   */
  char *path_crontab;

  /**
   * Path to a temporary crontab file.
   */
  char *path_crontab_tmp;

  /**
   * Program exit status set to one of the following values.
   *   - EXIT_SUCCESS
   *   - EXIT_FAILURE
   */
  int status_code;

  /**
   * See @ref crontab_flag.
   */
  unsigned int flags;
};

/**
 * Set an error status code and print an error message.
 *
 * @param[in,out] crontab See @ref crontab.
 * @param[in]     fmt     Format string used by vfprintf.
 */
static void
crontab_errx_noexit(struct crontab *const crontab,
                    const char *fmt, ...){
  va_list ap;

  crontab->status_code = EXIT_FAILURE;
  fputs("crontab error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

/**
 * Get path to a temporary crontab file which will get used to update the
 * crontab file.
 *
 * @param[in,out] crontab See @ref crontab.
 */
static void
crontab_get_temp_file_path(struct crontab *const crontab){
  const char *const edit_suffix = ".edit";
  size_t path_crontab_len;
  size_t edit_suffix_len;
  size_t alloc_len;
  char *copy_ptr;

  path_crontab_len = strlen(crontab->path_crontab);
  edit_suffix_len = strlen(edit_suffix);
  if(si_add_size_t(path_crontab_len, edit_suffix_len + 1, &alloc_len)){
    crontab_errx_noexit(crontab, "si_add_size_t");
  }
  else{
    crontab->path_crontab_tmp = malloc(alloc_len);
    if(crontab->path_crontab_tmp == NULL){
      crontab_errx_noexit(crontab, "malloc: %zu", alloc_len);
    }
    else{
      copy_ptr = stpcpy(crontab->path_crontab_tmp, crontab->path_crontab);
      stpcpy(copy_ptr, edit_suffix);
    }
  }
}

/**
 * Get the crontab path and temporary crontab file paths.
 *
 * @param[in,out] crontab See @ref crontab.
 */
static void
crontab_set_paths(struct crontab *const crontab){
  crontab->path_crontab = cron_get_path_crontab();
  if(crontab->path_crontab == NULL){
    crontab_errx_noexit(crontab, "Failed to get crontab path");
  }
  else{
    crontab_get_temp_file_path(crontab);
  }
}

/**
 * Gets the default editor program.
 *
 * @return Value of EDITOR environment variable or "vi" if unset.
 */
static const char *
crontab_get_default_editor(void){
  const char *editor;

  editor = getenv("EDITOR");
  if(editor == NULL){
    editor = "vi";
  }
  return editor;
}

/**
 * Launch default editor to edit the temporary crontab file.
 *
 * @param[in,out] crontab See @ref crontab.
 * @return                See @ref crontab::status_code.
 */
static int
crontab_edit_process(struct crontab *const crontab){
  int exit_status;
  pid_t pid;
  int status;
  const char *editor;

  editor = crontab_get_default_editor();

  exit_status = -1;
  pid = fork();
  if(pid == -1){
    crontab_errx_noexit(crontab, "fork");
  }
  else if(pid == 0){
    execlp(editor, editor, crontab->path_crontab_tmp, NULL);
    exit(EXIT_FAILURE);
  }
  while(waitpid(pid, &status, 0) == -1){
    if(errno != EINTR){
      crontab_errx_noexit(crontab, "waitpid");
      break;
    }
  }
  if(crontab->status_code == 0){
    if(WIFEXITED(status)){
      exit_status = WEXITSTATUS(status);
    }
    else{
      crontab_errx_noexit(crontab, "WIFEXITED");
    }
    if(exit_status != 0){
      crontab_errx_noexit(crontab, "Editor did not exit with 0 status code");
    }
  }
  return crontab->status_code;
}

/**
 * Copy a file.
 *
 * @param[in,out] crontab See @ref crontab.
 * @param[in,out] fp_in   Copy this file to @p fp_out.
 * @param[in,out] fp_out  Destination file copied from @p fp_in.
 */
static void
crontab_copy_file(struct crontab *const crontab,
                  FILE *const fp_in,
                  FILE *const fp_out){
  size_t bytes_read;
  char read_buf[CRON_READ_BUFFER_SZ];

  do{
    bytes_read = fread(read_buf, 1, sizeof(read_buf), fp_in);
    if(ferror(fp_in)){
      crontab_errx_noexit(crontab, "ferror: in");
    }
    else{
      fwrite(read_buf, 1, bytes_read, fp_out);
      if(ferror(fp_out)){
        crontab_errx_noexit(crontab, "ferror: out");
      }
    }
  } while(!feof(fp_in));
}

/**
 * Copy the existing crontab file to a temporary file if it exists.
 *
 * @param[in,out] crontab See @ref crontab.
 * @return                See @ref crontab::status_code.
 */
static int
crontab_copy_existing_crontab_to_tmp(struct crontab *const crontab){
  FILE *fp_in;
  FILE *fp_out;
  int rc_close;

  fp_in = fopen(crontab->path_crontab, "r");
  if(fp_in){
    fp_out = fopen(crontab->path_crontab_tmp, "w");
    if(fp_out == NULL){
      crontab_errx_noexit(crontab, "fopen: %s", crontab->path_crontab_tmp);
    }
    else{
      crontab_copy_file(crontab, fp_in, fp_out);
      rc_close = fclose(fp_out);
      if(rc_close == EOF){
        crontab_errx_noexit(crontab, "fclose: %s", crontab->path_crontab_tmp);
      }
    }
    rc_close = fclose(fp_in);
    if(rc_close == EOF){
      crontab_errx_noexit(crontab, "fclose: %s", crontab->path_crontab);
    }
  }
  return crontab->status_code;
}

/**
 * Rename a file.
 *
 * @param[in,out] crontab  See @ref crontab.
 * @param[in]     path_old Rename this to @p path_new.
 * @param[in]     path_new Rename @p path_old to this new name.
 * @return                 See @ref crontab::status_code.
 */
static int
crontab_rename_file(struct crontab *const crontab,
                    const char *const path_old,
                    const char *const path_new){
  if(rename(path_old, path_new) != 0){
    crontab_errx_noexit(crontab, "rename %s -> %s", path_old, path_new);
  }
  return crontab->status_code;
}

/**
 * Create the config directory if it does not already exist.
 *
 * @param[in] crontab See @ref crontab.
 * @retval            See @ref crontab::status_code.
 */
static int
crontab_create_config_dir(struct crontab *const crontab){
  const char *const PATH_APPEND_CONFIG = "/.config";
  char *path_home;
  char *path_config;
  char *path_copy;
  size_t path_home_len;
  size_t path_append_config_len;
  size_t alloc_len;

  path_home = cron_get_path_home();
  if(path_home == NULL){
    crontab_errx_noexit(crontab, "failed to get home path");
  }
  else{
    path_home_len = strlen(path_home);
    path_append_config_len = strlen(PATH_APPEND_CONFIG);
    if(si_add_size_t(path_home_len,
                     path_append_config_len + 1,
                     &alloc_len)){
      crontab_errx_noexit(crontab, "si_add_size_t");
    }
    else{
      path_config = malloc(alloc_len);
      if(path_config == NULL){
        crontab_errx_noexit(crontab, "malloc: %zu", alloc_len);
      }
      else{
        path_copy = stpcpy(path_config, path_home);
        stpcpy(path_copy, PATH_APPEND_CONFIG);
        if(mkdir(path_config, 0700) < 0){
          if(errno != EEXIST){
            crontab_errx_noexit(crontab,
                                "failed to create directory: %s",
                                path_config);
          }
        }
        free(path_config);
      }
    }
    free(path_home);
  }
  return crontab->status_code;
}

/**
 * Edit an existing crontab file or create a new one if an existing crontab
 * does not already exist.
 *
 * @param[in,out] crontab See @ref crontab.
 */
static void
crontab_edit(struct crontab *const crontab){
  if(crontab_create_config_dir(crontab) == 0 &&
     crontab_copy_existing_crontab_to_tmp(crontab) == 0 &&
     crontab_edit_process(crontab) == 0 &&
     crontab_rename_file(crontab,
                         crontab->path_crontab_tmp,
                         crontab->path_crontab) == 0){
    /* Successfully edited file. */
  }
}

/**
 * Print crontab file to STDOUT.
 *
 * @param[in,out] crontab See @ref crontab.
 */
static void
crontab_list(struct crontab *const crontab){
  FILE *fp_in;
  int rc_close;

  fp_in = fopen(crontab->path_crontab, "r");
  if(fp_in == NULL){
    crontab_errx_noexit(crontab,
                        "no crontab: %s",
                        crontab->path_crontab);
  }
  else{
    crontab_copy_file(crontab, fp_in, stdout);
    rc_close = fclose(fp_in);
    if(rc_close == EOF){
      crontab_errx_noexit(crontab, "fclose: %s", crontab->path_crontab);
    }
  }
}

/**
 * Remove an existing crontab file.
 *
 * @param[in,out] crontab See @ref crontab.
 */
static void
crontab_remove(struct crontab *const crontab){
  if(remove(crontab->path_crontab) != 0){
    crontab_errx_noexit(crontab, "remove: %s", crontab->path_crontab);
  }
}

/**
 * Set the crontab file to the contents of a file pointer.
 *
 * @param[in,out] crontab See @ref crontab.
 * @param[in,out] fp_in   Use this file pointer as the new crontab.
 */
static void
crontab_file_set(struct crontab *const crontab,
                 FILE *const fp_in){
  FILE *fp_out;
  int rc_close;

  if(crontab_create_config_dir(crontab) == 0){
    fp_out = fopen(crontab->path_crontab_tmp, "w");
    if(fp_out == NULL){
      crontab_errx_noexit(crontab, "fopen: %s", crontab->path_crontab_tmp);
    }
    else{
      crontab_copy_file(crontab, fp_in, fp_out);
      rc_close = fclose(fp_out);
      if(rc_close == EOF){
        crontab_errx_noexit(crontab, "fclose: %s", crontab->path_crontab_tmp);
      }
      else{
        crontab_rename_file(crontab,
                            crontab->path_crontab_tmp,
                            crontab->path_crontab);
      }
    }
  }
}

/**
 * Main entry point for crontab.
 *
 * Usage: crontab [file]
 *
 * Usage: crontab [-e|-l|-r]
 *
 * @param[in]     argc         Number of arguments in @p argv.
 * @param[in,out] argv         Argument list.
 * @retval        EXIT_SUCCESS Successful.
 * @retval        EXIT_FAILURE Error occurred.
 */
CRON_LINKAGE int
crontab_main(int argc,
             char *const argv[]){
  struct crontab crontab;
  int c;
  FILE *fp_in;

  memset(&crontab, 0, sizeof(crontab));
  while((c = getopt(argc, argv, "elr")) != -1){
    switch(c){
      case 'e':
        crontab.flags |= CRONTAB_OPTION_EDIT;
        break;
      case 'l':
        crontab.flags |= CRONTAB_OPTION_LIST;
        break;
      case 'r':
        crontab.flags |= CRONTAB_OPTION_REMOVE;
        break;
      default:
        crontab_errx_noexit(&crontab, "Invalid option: %s", optarg);
        break;
    }
  }
  argc -= optind;
  argv += optind;

  crontab_set_paths(&crontab);
  if(crontab.status_code == 0){
    if(crontab.flags == CRONTAB_OPTION_EDIT){
      crontab_edit(&crontab);
    }
    else if(crontab.flags == CRONTAB_OPTION_LIST){
      crontab_list(&crontab);
    }
    else if(crontab.flags == CRONTAB_OPTION_REMOVE){
      crontab_remove(&crontab);
    }
    else if(crontab.flags == 0){
      if(argc == 0){
        crontab_file_set(&crontab, stdin);
      }
      else if(argc == 1){
        fp_in = fopen(argv[0], "r");
        if(fp_in == NULL){
          crontab_errx_noexit(&crontab, "fopen: %s", argv[0]);
        }
        else{
          crontab_file_set(&crontab, fp_in);
          /*
           * Do not care if this fails because the crontab has already
           * been updated at this point or an error has occured.
           */
          fclose(fp_in);
        }
      }
      else{
        crontab_errx_noexit(&crontab, "Too many files");
      }
    }
    else{
      crontab_errx_noexit(&crontab, "Incorrect usage of flags");
    }
  }
  free(crontab.path_crontab);
  free(crontab.path_crontab_tmp);
  return crontab.status_code;
}

#ifndef CRON_NO_MAIN
/**
 * Main program entry point.
 *
 * @param[in]     argc See @ref crontab_main.
 * @param[in,out] argv See @ref crontab_main.
 * @return             See @ref crontab_main.
 */
int
main(const int argc,
     char *const argv[]){
  return crontab_main(argc, argv);
}
#endif /* !(CRON_NO_MAIN) */

