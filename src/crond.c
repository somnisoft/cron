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

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>

#include "cron.h"
#include "crond.h"

/**
 * Set to 1 if SIGTERM signal caught.
 *
 * SIGTERM would normally kill the process, but catching this signal
 * allows crond to cleanly shut down.
 */
static volatile sig_atomic_t
g_signal_sigterm = 0;

/**
 * Set to 1 if SIGINT signal caught.
 *
 * SIGINT would normally kill the process, but catching this signal
 * allows crond to cleanly shut down.
 */
static volatile sig_atomic_t
g_signal_sigint = 0;

/**
 * Reallocate memory with an unsigned wrap check.
 *
 * @param[in,out] ptr   Existing allocated memory, or NULL when allocating
 *                      a new buffer.
 * @param[in]     nmemb Number of elements to allocate.
 * @param[in]     size  Size of each element in @p nmemb.
 * @retval        void* Pointer to a reallocated buffer containing
 *                      @p nmemb * @p size bytes.
 * @retval        NULL  Failed to reallocate memory.
 */
static void *
crond_reallocarray(void *const ptr,
                   const size_t nmemb,
                   const size_t size){
  void *alloc;
  size_t size_mul;

  if(si_mul_size_t(nmemb, size, &size_mul)){
    alloc = NULL;
    errno = ENOMEM;
  }
  else{
    alloc = realloc(ptr, size_mul);
  }
  return alloc;
}

/**
 * Print a formatted message to stderr surrounded by a crond prefix
 * and newline character.
 *
 * @param[in] fmt Format string sent to vfprintf.
 * @param[in] ap  Format arguments sent to vfprintf.
 */
static void
crond_vfprintf_stderr(const char *const fmt,
                      va_list ap){
  fputs("crond: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
}

/**
 * Print a formatted message to stderr.
 *
 * See @ref crond_vfprintf_stderr.
 *
 * @param[in] fmt Format string used by vfprintf.
 */
static void
crond_fprintf_stderr(const char *const fmt, ...){
  va_list ap;

  va_start(ap, fmt);
  crond_vfprintf_stderr(fmt, ap);
  va_end(ap);
}

/**
 * Print a verbose message to stderr.
 *
 * See @ref crond_fprintf_stderr.
 *
 * @param[in] crond See @ref crond.
 * @param[in] fmt   Format string used by vfprintf.
 */
static void
crond_verbose(const struct crond *const crond,
              const char *const fmt, ...){
  va_list ap;

  if(crond->flags & CROND_FLAG_VERBOSE){
    va_start(ap, fmt);
    crond_vfprintf_stderr(fmt, ap);
    va_end(ap);
  }
}

/**
 * Print a verbose message to stderr and set an error status code.
 *
 * @param[in,out] crond See @ref crond.
 * @param[in]     fmt   Format string used by vfprintf.
 */
static void
crond_errx_noexit(struct crond *const crond,
                  const char *const fmt, ...){
  va_list ap;

  crond->status_code = EXIT_FAILURE;
  va_start(ap, fmt);
  crond_vfprintf_stderr(fmt, ap);
  va_end(ap);
}

/**
 * Check if the crontab file has been modified since the last check.
 *
 * This considers the crontab modified if one of the following changes occurred:
 *   - New crontab file created
 *   - Crontab file removed
 *   - Crontab modification time updated
 *
 * @param[in,out] crond See @ref crond.
 * @retval        true  Crontab changed.
 * @retval        false Crontab unchanged.
 */
static bool
crond_crontab_has_changed(struct crond *const crond){
  bool has_changed;
  struct stat sb;

  has_changed = false;
  if(cron_stat(crond->path_crontab, &sb) == 0){
    if(memcmp(&crond->mtime_crontab,
              &sb.st_mtim,
              sizeof(crond->mtime_crontab)) != 0){
      has_changed = true;
      memcpy(&crond->mtime_crontab,
             &sb.st_mtim,
             sizeof(crond->mtime_crontab));
    }
  }
  else{
    if(errno == ENOENT){
      if(crond->mtime_crontab.tv_sec  != 0 ||
         crond->mtime_crontab.tv_nsec != 0){
        has_changed = true;
        memset(&crond->mtime_crontab, 0, sizeof(crond->mtime_crontab));
      }
    }
    else{
      crond_errx_noexit(crond, "stat: %s", crond->path_crontab);
    }
  }
  return has_changed;
}

/**
 * Free a @ref crond_job.
 *
 * @param[in,out] job See @ref crond_job.
 */
static void
crond_job_free(struct crond_job *const job){
  free(job->command);
  free(job->stdin_lines);
}

/**
 * Free all jobs in @ref crond::job_list.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_job_list_free(struct crond *const crond){
  size_t job_i;

  for(job_i = 0; job_i < crond->num_jobs; job_i++){
    crond_job_free(&crond->job_list[job_i]);
  }
  free(crond->job_list);
  crond->job_list = NULL;
  crond->num_jobs = 0;
}

/**
 * Skip blank characters at the beginning of the string.
 *
 * @param[in]     line     String to check for blank characters.
 * @param[in,out] line_idx The position to start searching for blank
 *                         characters. Updated by this function to point to
 *                         character after the blanks.
 * @return                 Number of blank characters skipped.
 */
static size_t
crond_crontab_parse_blank(const char *const line,
                          size_t *const line_idx){
  size_t num_blanks;

  for(num_blanks = 0; isblank(line[*line_idx]); num_blanks++){
    *line_idx += 1;
  }
  return num_blanks;
}

/**
 * Convert a string (max 2 characters) to an unsigned integer.
 *
 * The caller must ensure that str has either one or two digit characters.
 *
 * @param[in] str One or two character string of digits [0-99].
 * @return        Unsigned int representation of @p str.
 */
static unsigned int
crond_strtoul_field(const unsigned char *const str){
  unsigned int ul;

  if(str[1] == '\0'){
    ul = str[0] - '0';
  }
  else{
    ul = 10 * (str[0] - '0') +
          1 * (str[1] - '0');
  }
  return ul;
}

/**
 * Enable a job to run over a time range for a given field.
 *
 * @param[in,out] field Job field.
 * @param[in]     start Starting index.
 * @param[in]     end   Ending index.
 */
static void
crond_set_field_range(bool *const field,
                      const size_t start,
                      const size_t end){
  size_t i;

  for(i = start; i <= end; i++){
    field[i] = true;
  }
}

/**
 * Parse an integer value or range in one of the crontab fields.
 *
 * @param[in]     line      Crontab line to parse.
 * @param[in,out] line_idx  Index into @p line which will get updated as it
 *                          parses the next field.
 * @param[out]    field     Job field to populate integer data. For example,
 *                          @ref crond_job::minute.
 * @param[in]     field_len Maximum number of entries in @p field.
 * @param[in]     offset    Subtract this offset when working on fields with
 *                          1-based indexing.
 * @retval        0         Successfully parsed field.
 * @retval        -1        Failed to parse field.
 */
static int
crond_parse_field_int(const char *const line,
                      size_t *const line_idx,
                      bool *const field,
                      const size_t field_len,
                      const size_t offset){
  int rc;
  bool has_comma;
  size_t i;
  unsigned char digit_1[3];
  unsigned char digit_2[3];
  size_t d1;
  size_t d2;
  size_t swap;

  rc = 0;
  if(line[*line_idx] == '*'){
    *line_idx += 1;
    crond_set_field_range(field, 0, field_len);
  }
  else{
    do{
      has_comma = false;
      memset(digit_1, 0, sizeof(digit_1));
      memset(digit_2, 0, sizeof(digit_2));
      for(i = 0; i < 2 && isdigit(line[*line_idx]); i++){
        digit_1[i] = (unsigned char)line[*line_idx];
        *line_idx += 1;
      }
      if(i == 0){
        rc = -1;
        break;
      }
      if(line[*line_idx] == '-'){
        *line_idx += 1;
        for(i = 0; i < 2 && isdigit(line[*line_idx]); i++){
          digit_2[i] = (unsigned char)line[*line_idx];
          *line_idx += 1;
        }
        if(i == 0){
          rc = -1;
          break;
        }
        d2 = crond_strtoul_field(digit_2) - offset;
      }
      else{
        d2 = SIZE_MAX;
      }
      d1 = crond_strtoul_field(digit_1) - offset;
      if(d1 >= field_len){
        rc = -1;
      }
      else if(d2 == SIZE_MAX){
        crond_set_field_range(field, d1, d1);
      }
      else{
        if(d1 > d2){
          swap = d1;
          d1 = d2;
          d2 = swap;
        }
        if(d2 >= field_len){
          d2 = field_len;
        }
        crond_set_field_range(field, d1, d2);
      }
      if(line[*line_idx] == ','){
        *line_idx += 1;
        has_comma = true;
      }
    } while(has_comma);
  }
  if(crond_crontab_parse_blank(line, line_idx) == 0){
    rc = -1;
  }
  return rc;
}

/**
 * Parse the command string in the crontab file.
 *
 * @param[in]  line     Crontab line.
 * @param[in]  line_idx Index into @p line where the parsing should begin.
 * @param[out] job      Store the command and stdin lines here.
 * @retval     true     Succeeded.
 * @retval     false    Failed to allocate memory.
 */
static bool
crond_crontab_parse_command(const char *const line,
                            const size_t line_idx,
                            struct crond_job *const job){
  bool parsed;
  size_t i;
  size_t i_fwd;
  size_t stdin_start_idx;

  job->stdin_lines_len = 0;
  stdin_start_idx = SIZE_MAX;
  parsed = true;
  for(i = line_idx; line[i]; i++){
    if(line[i] == '%' && i > 0 && line[i - 1] != '\\'){
      stdin_start_idx = i + 1;
      break;
    }
  }
  job->command = strndup(&line[line_idx], i - line_idx);
  if(job->command == NULL){
    parsed = false;
  }
  else{
    if(stdin_start_idx != SIZE_MAX){
      job->stdin_lines = strdup(&line[stdin_start_idx]);
      if(job->stdin_lines == NULL){
        parsed = false;
        free(job->command);
        job->command = NULL;
      }
      else{
        i = 0;
        for(i_fwd = 0; job->stdin_lines[i_fwd]; i_fwd++){
          if(job->stdin_lines[i_fwd] == '\\' && job->stdin_lines[i_fwd + 1]){
            i_fwd += 1;
          }
          else if(job->stdin_lines[i_fwd] == '%'){
            job->stdin_lines[i_fwd] = '\n';
          }
          job->stdin_lines[i] = job->stdin_lines[i_fwd];
          i += 1;
        }
        /* Replace the null-terminator with a newline character. */
        job->stdin_lines[i] = '\n';
        i += 1;
        job->stdin_lines_len = i;
      }
    }
  }
  return parsed;
}

/**
 * Append a new job to the job list.
 *
 * @param[in,out] crond See @ref crond.
 * @param[in]     job   Append this job to the job list.
 * @retval        true  Job has been appended.
 * @retval        false Failed to append job.
 */
static bool
crond_job_append(struct crond *const crond,
                 const struct crond_job *const job){
  struct crond_job *new_job_list;
  bool appended;

  new_job_list = crond_reallocarray(crond->job_list,
                                    crond->num_jobs + 1,
                                    sizeof(*crond->job_list));
  if(new_job_list == NULL){
    crond_errx_noexit(crond, "reallocarray");
    appended = false;
  }
  else{
    crond->job_list = new_job_list;
    memcpy(&crond->job_list[crond->num_jobs], job, sizeof(*job));
    crond->num_jobs += 1;
    appended = true;
  }
  return appended;
}

/**
 * Parse a single crontab line and append to the job list.
 *
 * @param[in,out] crond See @ref crond.
 * @param[in]     line  Crontab line to parse.
 */
CRON_LINKAGE void
crond_crontab_parse_line(struct crond *const crond,
                         const char *line){
  const char *const STR_YEARLY = "yearly";
  const char *const STR_ANNUALLY = "annually";
  const char *const STR_MONTHLY = "monthly";
  const char *const STR_WEEKLY = "weekly";
  const char *const STR_DAILY = "daily";
  const char *const STR_MIDNIGHT = "midnight";
  const char *const STR_HOURLY = "hourly";
  size_t STRLEN_YEARLY;
  size_t STRLEN_ANNUALLY;
  size_t STRLEN_MONTHLY;
  size_t STRLEN_WEEKLY;
  size_t STRLEN_DAILY;
  size_t STRLEN_MIDNIGHT;
  size_t STRLEN_HOURLY;
  size_t i;
  struct crond_job job;
  const char *cmd_special;
  bool valid_line;

  i = 0;
  valid_line = true;
  crond_crontab_parse_blank(line, &i);
  if(line[i] && line[i] != '#'){
    memset(&job, 0, sizeof(job));
    if(line[i] == '@'){
      i += 1;
      cmd_special = &line[i];
      STRLEN_YEARLY   = strlen(STR_YEARLY);
      STRLEN_ANNUALLY = strlen(STR_ANNUALLY);
      STRLEN_MONTHLY  = strlen(STR_MONTHLY);
      STRLEN_WEEKLY   = strlen(STR_WEEKLY);
      STRLEN_DAILY    = strlen(STR_DAILY);
      STRLEN_MIDNIGHT = strlen(STR_MIDNIGHT);
      STRLEN_HOURLY   = strlen(STR_HOURLY);
      if(strncmp(cmd_special, STR_YEARLY  , STRLEN_YEARLY  ) == 0 ||
         strncmp(cmd_special, STR_ANNUALLY, STRLEN_ANNUALLY) == 0){
        /* 0 0 1 1 * */
        crond_set_field_range(job.minute , 0, 0                  );
        crond_set_field_range(job.hour   , 0, 0                  );
        crond_set_field_range(job.day    , 0, 0                  );
        crond_set_field_range(job.month  , 0, 0                  );
        crond_set_field_range(job.weekday, 0, sizeof(job.weekday));
        if(cmd_special[0] == STR_YEARLY[0]){
          i += STRLEN_YEARLY;
        }
        else{
          i += STRLEN_ANNUALLY;
        }
      }
      else if(strncmp(cmd_special, STR_MONTHLY, STRLEN_MONTHLY) == 0){
        /* 0 0 1 * * */
        crond_set_field_range(job.minute , 0, 0                  );
        crond_set_field_range(job.hour   , 0, 0                  );
        crond_set_field_range(job.day    , 0, 0                  );
        crond_set_field_range(job.month  , 0, sizeof(job.month)  );
        crond_set_field_range(job.weekday, 0, sizeof(job.weekday));
        i += STRLEN_MONTHLY;
      }
      else if(strncmp(cmd_special, STR_WEEKLY, STRLEN_WEEKLY) == 0){
        /* 0 0 * * 0 */
        crond_set_field_range(job.minute , 0, 0                );
        crond_set_field_range(job.hour   , 0, 0                );
        crond_set_field_range(job.day    , 0, sizeof(job.day)  );
        crond_set_field_range(job.month  , 0, sizeof(job.month));
        crond_set_field_range(job.weekday, 0, 0                );
        i += STRLEN_WEEKLY;
      }
      else if(strncmp(cmd_special, STR_DAILY   , STRLEN_DAILY) == 0 ||
              strncmp(cmd_special, STR_MIDNIGHT, STRLEN_MIDNIGHT) == 0){
        /* 0 0 * * * */
        crond_set_field_range(job.minute , 0, 0                  );
        crond_set_field_range(job.hour   , 0, 0                  );
        crond_set_field_range(job.day    , 0, sizeof(job.day)    );
        crond_set_field_range(job.month  , 0, sizeof(job.month)  );
        crond_set_field_range(job.weekday, 0, sizeof(job.weekday));
        if(cmd_special[0] == STR_DAILY[0]){
          i += STRLEN_DAILY;
        }
        else{
          i += STRLEN_MIDNIGHT;
        }
      }
      else if(strncmp(cmd_special, STR_HOURLY, STRLEN_HOURLY) == 0){
        /* 0 * * * * */
        crond_set_field_range(job.minute , 0, 0                  );
        crond_set_field_range(job.hour   , 0, sizeof(job.hour)   );
        crond_set_field_range(job.day    , 0, sizeof(job.day)    );
        crond_set_field_range(job.month  , 0, sizeof(job.month)  );
        crond_set_field_range(job.weekday, 0, sizeof(job.weekday));
        i += STRLEN_HOURLY;
      }
      else{
        crond_verbose(crond, "invalid special command: %s", &line[i]);
        valid_line = false;
      }
    }
    else{
      if(crond_parse_field_int(line, &i, job.minute , sizeof(job.minute ), 0) < 0 ||
         crond_parse_field_int(line, &i, job.hour   , sizeof(job.hour   ), 0) < 0 ||
         crond_parse_field_int(line, &i, job.day    , sizeof(job.day    ), 1) < 0 ||
         crond_parse_field_int(line, &i, job.month  , sizeof(job.month  ), 1) < 0 ||
         crond_parse_field_int(line, &i, job.weekday, sizeof(job.weekday), 0) < 0){
        valid_line = false;
      }
    }
    if(valid_line == true){
      crond_crontab_parse_blank(line, &i);
      if(crond_crontab_parse_command(line, i, &job) == false ||
         crond_job_append(crond, &job) == false){
        crond_job_free(&job);
      }
    }
  }
}

/**
 * Check if the crontab has changed and reparse if it has.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_crontab_reparse(struct crond *const crond){
  FILE *fp;
  size_t len;
  ssize_t read;
  char *line;

  if(crond_crontab_has_changed(crond)){
    crond_job_list_free(crond);
    fp = fopen(crond->path_crontab, "r");
    if(fp){
      line = NULL;
      len = 0;
      while((read = getline(&line, &len, fp)) != -1){
        /* Remove the newline character. */
        if(read){
          line[read - 1] = '\0';
        }
        crond_crontab_parse_line(crond, line);
      }
      free(line);
      if(ferror(fp)){
        crond_errx_noexit(crond, "ferror: %s", crond->path_crontab);
        crond_job_list_free(crond);
      }
      else if(fclose(fp) != 0){
        crond_errx_noexit(crond, "fclose: %s", crond->path_crontab);
        crond_job_list_free(crond);
      }
    }
  }
}

/**
 * Check if a job should run based on the current time.
 *
 * @param[in] crond See @ref crond.
 * @param[in] job   See @ref crond_job.
 * @retval    true  Run the job.
 * @retval    false Do not run the job.
 */
static bool
crond_job_should_run(const struct crond *const crond,
                     const struct crond_job *const job){
  bool should_run;

  if(job->weekday[crond->tm->tm_wday    ] &&
     job->month  [crond->tm->tm_mon  - 1] &&
     job->day    [crond->tm->tm_mday - 1] &&
     job->hour   [crond->tm->tm_hour    ] &&
     job->minute [crond->tm->tm_min     ]){
    should_run = true;
  }
  else{
    should_run = false;
  }
  return should_run;
}

/**
 * Wait for a process to exit.
 *
 * @param[in] crond See @ref crond.
 * @param[in] pid   Process ID to wait for.
 */
static void
crond_waitpid(const struct crond *const crond,
              const pid_t pid){
  while(waitpid(pid, NULL, 0) == -1){
    if(errno != EINTR){
      crond_verbose(crond, "waitpid");
      exit(EXIT_FAILURE);
    }
  }
}

/**
 * Reap zombie jobmon processes.
 */
static void
crond_reap_jobmon(void){
  while(waitpid(-1, NULL, WNOHANG) > 0){
  }
}

/**
 * Write to a child process through a pipe.
 *
 * This function closes the pipe file descriptors when no longer needed. It
 * will also exit if an error occurs at any point during this process.
 *
 * @param[in] pipe_write File descriptors created by pipe(). This function will
 *                       close both file descriptors.
 * @param[in] data       Data to write through the pipe.
 * @param[in] datasz     Number of bytes in @p data.
 */
static void
crond_fd_write(const int pipe_write[2],
               const char *const data,
               const size_t datasz){
  size_t bytes_to_write;
  ssize_t bytes_written;

  if(close(pipe_write[0]) != 0){
    exit(EXIT_FAILURE);
  }
  bytes_to_write = datasz;
  while(bytes_to_write){
    bytes_written = write(pipe_write[1],
                          &data[datasz - bytes_to_write],
                          bytes_to_write);
    if(bytes_written < 0){
      if(errno != EINTR){
        exit(EXIT_FAILURE);
      }
    }
    else{
      bytes_to_write -= (size_t)bytes_written;
    }
  }
  if(close(pipe_write[1]) != 0){
    exit(EXIT_FAILURE);
  }
}

/**
 * Send mail of job output to the user.
 *
 * @param[in] crond       See @ref crond.
 * @param[in] command_str The shell command that ran when launching the job.
 * @param[in] body        Program output to send in the mail body.
 * @param[in] bodysz      Number of bytes in @p body.
 */
static void
crond_mailx(const struct crond *const crond,
            const char *const command_str,
            const char *const body,
            const size_t bodysz){
  char subject[CROND_MAX_SUBJECT_LEN];
  pid_t pid_mailx;
  int pipe_write[2];

  /* Allow subject to get truncated. */
  if(snprintf(subject,
              sizeof(subject),
              "Cron <%s> %s",
              crond->email_to,
              command_str) < 0){
    exit(EXIT_FAILURE);
  }

  if(pipe(pipe_write) != 0){
    exit(EXIT_FAILURE);
  }
  pid_mailx = fork();
  if(pid_mailx == -1){
    exit(EXIT_FAILURE);
  }
  else if(pid_mailx == 0){
#ifdef CRON_TEST
    g_test_seam_err_in_fork_mailx = true;
#endif /* CRON_TEST */
    if(dup2(pipe_write[0], STDIN_FILENO) >= 0 &&
       close(pipe_write[0])              == 0 &&
       close(pipe_write[1])              == 0){
      execlp("mailx", "mailx", "-s", subject, crond->email_to, NULL);
    }
    exit(EXIT_FAILURE);
  }
  else{
    crond_fd_write(pipe_write, body, bodysz);
    crond_waitpid(crond, pid_mailx);
  }
}

/**
 * Launch the job in a new process.
 *
 * This will create two child processes:
 *   - A monitor process that checks for STDOUT and STDERR output from
 *     the command. If it does generate output, then that will get mailed to
 *     the user.
 *   - A command process that runs the job in the shell.
 *
 * @verbatim
   <crond>----------------------------> Continue processing next job and sleep
        |      |       |
        |      |       |
        |      |       <jobmon>----------------> No email if no STDOUT/STDERR
        |      |              |
        |      |              |
        |      |              |
        |      |              |
        |      |              <command>--------> /bin/sh
        |      |
        |      |
        |      <jobmon>------------------------> Send email with STDOUT/STDERR
        |             |         ^         ^
        |             |         |         |
        |             |         | STDOUT  | STDERR
        |             |         |         |
        |             <command>----------------> /bin/sh
        |
        |
        <jobmon>-----------------------------> Send email with STDOUT/STDERR
              |         ^         ^
              |         |         |
              |         | STDOUT  | STDERR
              |         |         |
              <command>----------------------> /bin/sh
 * @endverbatim
 *
 * @param[in] crond See @ref crond.
 * @param[in] job   See @ref crond_job.
 */
static void
crond_job_run(const struct crond *const crond,
              const struct crond_job *const job){
  pid_t pid_jobmon;
  pid_t pid_cmd;
  int pipe_read[2];
  int pipe_write[2];
  ssize_t bytes_read;
  char read_buf[CRON_READ_BUFFER_SZ];
  char *mail_body;
  size_t mail_body_sz;
  size_t old_mail_body_sz;
  char *new_mail_body;

  crond_verbose(crond, "running job: %s", job->command);
  pid_jobmon = fork();
  if(pid_jobmon == -1){
    crond_verbose(crond, "failed to execute job");
  }
  else if(pid_jobmon == 0){
#ifdef CRON_TEST
    g_test_seam_err_in_fork_jobmon = true;
#endif /* CRON_TEST */
    if(pipe(pipe_read ) != 0 ||
       pipe(pipe_write) != 0){
      exit(EXIT_FAILURE);
    }
    pid_cmd = fork();
    if(pid_cmd == -1){
      exit(EXIT_FAILURE);
    }
    else if(pid_cmd == 0){
      if(dup2(pipe_read[1] , STDOUT_FILENO) >= 0 &&
         dup2(pipe_read[1] , STDERR_FILENO) >= 0 &&
         dup2(pipe_write[0], STDIN_FILENO ) >= 0 &&
         close(pipe_read[0])                == 0 &&
         close(pipe_read[1])                == 0 &&
         close(pipe_write[0])               == 0 &&
         close(pipe_write[1])               == 0){
        execle(crond->path_shell,
               crond->path_shell,
               "-c",
               job->command,
               NULL,
               NULL);
      }
      exit(EXIT_FAILURE);
    }
    if(close(pipe_read[1]) != 0){
      exit(EXIT_FAILURE);
    }
    crond_fd_write(pipe_write, job->stdin_lines, job->stdin_lines_len);
    mail_body = NULL;
    mail_body_sz = 0;
    do{
      bytes_read = read(pipe_read[0], read_buf, sizeof(read_buf));
      if(bytes_read < 0){
        if(errno != EINTR){
          exit(EXIT_FAILURE);
        }
      }
      else if(bytes_read){
        old_mail_body_sz = mail_body_sz;
        if(si_add_size_t(mail_body_sz, (size_t)bytes_read, &mail_body_sz)){
          exit(EXIT_FAILURE);
        }
        else{
          new_mail_body = realloc(mail_body, mail_body_sz);
          if(new_mail_body == NULL){
            free(mail_body);
            exit(EXIT_FAILURE);
          }
          mail_body = new_mail_body;
          memcpy(&mail_body[old_mail_body_sz], read_buf, (size_t)bytes_read);
        }
      }
    } while(bytes_read);
    if(close(pipe_read[0]) != 0){
      exit(EXIT_FAILURE);
    }
    crond_waitpid(crond, pid_cmd);
    if(mail_body){
      crond_mailx(crond,
                  job->command,
                  mail_body,
                  mail_body_sz);
    }
    exit(EXIT_SUCCESS);
  }
}

/**
 * Check if each job needs to run and execute the job if it does.
 *
 * @param[in] crond See @ref crond.
 */
static void
crond_job_list_run(const struct crond *const crond){
  size_t i;
  const struct crond_job *job;

  for(i = 0; i < crond->num_jobs; i++){
    job = &crond->job_list[i];
    if(crond_job_should_run(crond, job)){
      crond_job_run(crond, job);
    }
  }
}

/**
 * Get path to the crond lock file.
 *
 * @param[in] path_crontab Path to crontab file.
 * @retval    char*        Path to the lock file. The caller must free this
 *                         when finished.
 * @retval    NULL         Memory allocation failed.
 */
CRON_LINKAGE char *
crond_get_path_lock_file(const char *const path_crontab){
  const char *const lock_file_suffix = ".lock";
  char *path_lock_file;
  size_t path_crontab_len;
  size_t lock_file_suffix_len;
  size_t alloc_len;
  char *copy_ptr;

  path_lock_file = NULL;
  if(path_crontab){
    path_crontab_len = strlen(path_crontab);
    lock_file_suffix_len = strlen(lock_file_suffix);
    if(si_add_size_t(path_crontab_len,
                     lock_file_suffix_len + 1,
                     &alloc_len) == 0){
      path_lock_file = malloc(alloc_len);
      if(path_lock_file){
        copy_ptr = stpcpy(path_lock_file, path_crontab);
        stpcpy(copy_ptr, lock_file_suffix);
      }
    }
  }
  return path_lock_file;
}

/**
 * Create a lock file for this user.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_lock_file_create(struct crond *const crond){
  const int oflag = O_CREAT | O_EXCL | O_WRONLY | O_TRUNC | O_CLOEXEC;
  const mode_t mode = S_IWUSR;

  crond->path_lock_file = crond_get_path_lock_file(crond->path_crontab);
  if(crond->path_lock_file == NULL){
    crond_errx_noexit(crond, "failed to get lock file path");
  }
  else{
    crond->fd_lock_file = open(crond->path_lock_file, oflag, mode);
    if(crond->fd_lock_file < 0){
      if(errno == EEXIST){
        crond_errx_noexit(crond,
                          "crond already running: %s",
                          crond->path_lock_file);
      }
      else{
        crond_errx_noexit(crond, "failed to create lock file");
      }
    }
  }
}

/**
 * Remove the lock file.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_lock_file_delete(struct crond *const crond){
  if(crond->fd_lock_file > 0){
    if(close(crond->fd_lock_file) != 0){
      crond_errx_noexit(crond, "failed to close lock file");
    }
    if(remove(crond->path_lock_file) != 0){
      crond_fprintf_stderr("failed to remove lock file: %s",
                           crond->path_lock_file);
    }
  }
}

/**
 * Set the current time in @ref crond::tm.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_gettime(struct crond *const crond){
  struct timespec timespec;

  if(clock_gettime(CLOCK_REALTIME, &timespec) != 0){
    crond_errx_noexit(crond, "clock_gettime");
  }
  else{
    crond->tm = localtime(&timespec.tv_sec);
    if(crond->tm == NULL){
      crond_errx_noexit(crond, "localtime_r");
    }
  }
}

/**
 * Catch the SIGTERM/SIGINT signals and set the global indicator.
 *
 * @param[in] signum SIGTERM or SIGHUP.
 */
static void
crond_signal_handler(const int signum){
  if(signum == SIGTERM){
    g_signal_sigterm = 1;
  }
  else if(signum == SIGINT){
    g_signal_sigint = 1;
  }
}

/**
 * Set up the default signal handlers.
 *
 *   - SIGHUP : Cron will catch this and reload the crontab file.
 *   - SIGINT : Cron will catch this and cleanly exit.
 *   - SIGTERM: Cron will catch this and cleanly exit.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_signal_set(struct crond *const crond){
  struct sigaction sact;

  sact.sa_handler = crond_signal_handler;
  sact.sa_flags = SA_RESTART;
  if(sigemptyset(&sact.sa_mask) != 0 ||
     cron_sigaction(SIGHUP , &sact, NULL) != 0 ||
     cron_sigaction(SIGINT , &sact, NULL) != 0 ||
     cron_sigaction(SIGTERM, &sact, NULL) != 0){
    crond_errx_noexit(crond, "signal set");
  }
}

/**
 * Set the shell path in @ref crond::path_shell.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_get_shell(struct crond *const crond){
  const char *path_shell;

  path_shell = getenv("SHELL");
  if(path_shell == NULL){
    path_shell = "/bin/sh";
  }
  crond->path_shell = path_shell;
}

/**
 * Get the current user name.
 *
 * A long user name might get truncated.
 *
 * @param[out] user_name    Store the user name in this buffer.
 * @param[in]  user_name_sz Number of bytes available in @p user_name.
 */
static void
crond_get_user_name(char *const user_name,
                    const size_t user_name_sz){
  const char *env_logname;
  const struct passwd *pwd;
  const char *user_name_copy;

  env_logname = getenv("LOGNAME");
  if(env_logname){
    user_name_copy = env_logname;
  }
  else{
    pwd = getpwuid(geteuid());
    if(pwd){
      user_name_copy = pwd->pw_name;
    }
    else{
      user_name_copy = "";
    }
  }
  strncpy(user_name, user_name_copy, user_name_sz);
  user_name[user_name_sz - 1] = '\0';
}

/**
 * Set the email to in @ref crond::email_to.
 *
 * @param[in,out] crond See @ref crond.
 */
static void
crond_get_email_to(struct crond *const crond){
  char host_name[CROND_MAX_HOST_NAME_SZ];
  char user_name[CROND_MAX_USER_NAME];
  char *email_to;

  /*
   * Do not care if this truncates since we will force a null-terminator
   * after the call.
   */
  gethostname(host_name, sizeof(host_name));
  host_name[sizeof(host_name) - 1] = '\0';

  crond_get_user_name(user_name, sizeof(user_name));

  email_to = stpcpy(crond->email_to, user_name);
  email_to = stpcpy(email_to, "@");
  stpcpy(email_to, host_name);
}

/**
 * Determine if cron should cleanly shut down.
 *
 * This should only happen if:
 *   - An error occurred causing the exit status code to get set.
 *   - SIGTERM signal caught.
 *
 * @param[in] crond See @ref crond.
 * @retval    true  Cron should exit.
 * @retval    false Cron should not exit.
 */
static bool
crond_should_exit(const struct crond *const crond){
  bool should_exit;

  if(crond->status_code == 0 &&
     g_signal_sigterm   == 0 &&
     g_signal_sigint    == 0){
    should_exit = false;
  }
  else{
    should_exit = true;
  }
  return should_exit;
}

/**
 * Main entry point for cron.
 *
 * Usage: crond [-v]
 *
 * @param[in]     argc         Number of arguments in @p argv.
 * @param[in,out] argv         Argument list.
 * @retval        EXIT_SUCCESS Successful.
 * @retval        EXIT_FAILURE Error occurred.
 */
CRON_LINKAGE int
crond_main(const int argc,
           char *const argv[]){
  unsigned int sleep_sec;
  struct crond crond;
  int c;

  memset(&crond, 0, sizeof(crond));
  while((c = getopt(argc, argv, "v")) != -1){
    switch(c){
      case 'v':
        crond.flags |= CROND_FLAG_VERBOSE;
        break;
      default:
        crond_errx_noexit(&crond, "invalid argument: %s", optarg);
        break;
    }
  }

  crond.path_crontab = cron_get_path_crontab();
  if(crond.path_crontab == NULL){
    crond_errx_noexit(&crond, "failed to get crontab path");
  }

  crond_get_shell(&crond);
  crond_get_email_to(&crond);
  crond_signal_set(&crond);
  crond_lock_file_create(&crond);
  while(crond_should_exit(&crond) == false){
    crond_crontab_reparse(&crond);
    crond_gettime(&crond);
    crond_job_list_run(&crond);
    crond_gettime(&crond);

    if(crond_should_exit(&crond) == false){
      /* tm_sec = [0,60] */
      sleep_sec = 60 - (unsigned int)crond.tm->tm_sec;
      if(sleep_sec == 0){
        sleep_sec += 1;
      }
      crond_verbose(&crond, "sleeping for %u seconds", sleep_sec);
      sleep(sleep_sec);
    }
    crond_reap_jobmon();
  }
  crond_job_list_free(&crond);
  crond_lock_file_delete(&crond);
  free(crond.path_lock_file);
  free(crond.path_crontab);
  return crond.status_code;
}

#ifndef CRON_NO_MAIN
/**
 * Main program entry point.
 *
 * @param[in]     argc See @ref crond_main.
 * @param[in,out] argv See @ref crond_main.
 * @return             See @ref crond_main.
 */
int
main(const int argc,
     char *const argv[]){
  return crond_main(argc, argv);
}
#endif /* CRON_NO_MAIN */

