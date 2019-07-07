/**
 * @file
 * @brief Test seams.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This software has been placed into the public domain using CC0.
 */
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"

/**
 * Maximum number of arguments used in the exec* functions.
 */
#define MAX_EXEC_ARGS (20)

/**
 * Specify the errno value when a function fails.
 */
int g_test_seam_err_force_errno = 0;

/**
 * Indicate if we should only decrement the error counter if inside the jobmon
 * fork.
 */
bool g_test_seam_err_req_fork_jobmon = false;

/**
 * Set to 1 if inside the jobmon fork.
 */
bool g_test_seam_err_in_fork_jobmon = false;

/**
 * Indicate if we should only decrement the error counter if inside the mailx
 * fork.
 */
bool g_test_seam_err_req_fork_mailx = false;

/**
 * Set to 1 if inside the mailx fork.
 */
bool g_test_seam_err_in_fork_mailx = false;

/**
 * Error counter for @ref test_seam_clock_gettime.
 */
int g_test_seam_err_ctr_clock_gettime = -1;

/**
 * Error counter for @ref test_seam_close.
 */
int g_test_seam_err_ctr_close = -1;

/**
 * Error counter for @ref test_seam_dup2.
 */
int g_test_seam_err_ctr_dup2 = -1;

/**
 * Error counter for @ref test_seam_execle.
 */
int g_test_seam_err_ctr_execle = -1;

/**
 * Error counter for @ref test_seam_execlp.
 */
int g_test_seam_err_ctr_execlp = -1;

/**
 * Error counter for @ref test_seam_fclose.
 */
int g_test_seam_err_ctr_fclose = -1;

/**
 * Error counter for @ref test_seam_ferror.
 */
int g_test_seam_err_ctr_ferror = -1;

/**
 * Error counter for @ref test_seam_fopen.
 */
int g_test_seam_err_ctr_fopen = -1;

/**
 * Error counter for @ref test_seam_fork.
 */
int g_test_seam_err_ctr_fork = -1;

/**
 * Error counter for @ref test_seam_getpwuid.
 */
int g_test_seam_err_ctr_getpwuid = -1;

/**
 * Error counter for @ref test_seam_localtime.
 */
int g_test_seam_err_ctr_localtime = -1;

/**
 * If not NULL, return this from @ref test_seam_localtime.
 */
struct tm *g_test_seam_localtime_tm = NULL;

/**
 * Error counter for @ref test_seam_malloc.
 */
int g_test_seam_err_ctr_malloc = -1;

/**
 * Error counter for @ref test_seam_mkdir.
 */
int g_test_seam_err_ctr_mkdir = -1;

/**
 * Error counter for @ref test_seam_open.
 */
int g_test_seam_err_ctr_open = -1;

/**
 * Error counter for @ref test_seam_pipe.
 */
int g_test_seam_err_ctr_pipe = -1;

/**
 * Error counter for @ref test_seam_read.
 */
int g_test_seam_err_ctr_read = -1;

/**
 * Error counter for @ref test_seam_realloc.
 */
int g_test_seam_err_ctr_realloc = -1;

/**
 * Error counter for @ref test_seam_remove.
 */
int g_test_seam_err_ctr_remove = -1;

/**
 * Error counter for @ref test_seam_rename.
 */
int g_test_seam_err_ctr_rename = -1;

/**
 * Error counter for @ref si_add_size_t.
 */
int g_test_seam_err_ctr_si_add_size_t = -1;

/**
 * Error counter for @ref si_mul_size_t.
 */
int g_test_seam_err_ctr_si_mul_size_t = -1;

/**
 * Error counter for @ref test_seam_sigaction.
 */
int g_test_seam_err_ctr_sigaction = -1;

/**
 * Error counter for @ref test_seam_sigemptyset.
 */
int g_test_seam_err_ctr_sigemptyset   = -1;

/**
 * Error counter for @ref test_seam_snprintf.
 */
int g_test_seam_err_ctr_snprintf = -1;

/**
 * Error counter for @ref test_seam_stat.
 */
int g_test_seam_err_ctr_stat = -1;

/**
 * Error counter for @ref test_seam_strdup.
 */
int g_test_seam_err_ctr_strdup = -1;

/**
 * Error counter for @ref test_seam_strndup.
 */
int g_test_seam_err_ctr_strndup = -1;

/**
 * Error counter for @ref test_seam_waitpid.
 */
int g_test_seam_err_ctr_waitpid = -1;

/**
 * Error counter for @ref test_seam_write.
 */
int g_test_seam_err_ctr_write = -1;

/**
 * Decrement an error counter until it reaches -1.
 *
 * After a counter reaches -1, it will return a true response. This gets
 * used by the test suite to denote when to cause a function to fail. For
 * example, the unit test might need to cause the malloc() function to fail
 * after calling it a third time. In that case, the counter should initially
 * get set to 2 and will get decremented every time this function gets called.
 *
 * @param[in,out] err_ctr Error counter to decrement.
 * @retval        true    The counter has reached -1.
 * @retval        false   The counter has been decremented, but did not
 *                        reach -1 yet.
 */
bool
test_seam_dec_err_ctr(int *const err_ctr){
  bool reached_end;

  reached_end = false;
  if(g_test_seam_err_req_fork_mailx == true &&
     g_test_seam_err_in_fork_mailx == false){
    /* Skip the decrement if not inside mailx fork and required. */
  }
  else if(g_test_seam_err_req_fork_jobmon == true &&
          g_test_seam_err_in_fork_jobmon == false){
    /* Skip the decrement if not inside jobmon fork and required. */
  }
  else{
    if(*err_ctr >= 0){
      *err_ctr -= 1;
      if(*err_ctr < 0){
        reached_end = true;
      }
    }
  }
  return reached_end;
}

/**
 * Set the errno value to the @ref g_test_seam_err_force_errno value if not 0.
 *
 * @param[in] errno_alternative Set errno to this value if
 *                              @ref g_test_seam_err_force_errno unset.
 */
static void
test_seam_force_errno(const int errno_alternative){
  if(g_test_seam_err_force_errno){
    errno = g_test_seam_err_force_errno;
  }
  else{
    errno = errno_alternative;
  }
}

/**
 * Control when clock_gettime() fails.
 *
 * @param[in]  clock_id Clock type.
 * @param[out] res      Store the time here.
 * @retval     0        Succesfully received time.
 * @retval     -1       Failed to get time.
 */
int
test_seam_clock_gettime(clockid_t clock_id,
                        struct timespec *res){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_clock_gettime)){
    test_seam_force_errno(EINVAL);
    rc = -1;
  }
  else{
    rc = clock_gettime(clock_id, res);
  }
  return rc;
}

/**
 * Control when close() fails.
 *
 * @param[in] fildes File descriptor to close.
 * @retval    0      File closed successfully.
 * @retval    -1     Failed to close file.
 */
int
test_seam_close(int fildes){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_close)){
    test_seam_force_errno(EBADF);
    rc = -1;
  }
  else{
    rc = close(fildes);
  }
  return rc;
}

/**
 * Control when dup2() fails.
 *
 * @param[in] fildes  Duplicate into @p fildes2.
 * @param[in] fildes2 Duplicated from @p fildes.
 * @retval    >=0     @p fildes2.
 * @retval    -1      Failed to duplicate file descriptor.
 */
int
test_seam_dup2(int fildes,
               int fildes2){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_dup2)){
    test_seam_force_errno(EBADF);
    rc = -1;
  }
  else{
    rc = dup2(fildes, fildes2);
  }
  return rc;
}

/**
 * Control when execle() fails.
 *
 * @param[in] path Path to executable file.
 * @param[in] arg0 List of arguments to pass to the executable program.
 * @return         This will return an error if unable to execute
 *                 program, otherwise this does not return.
 */
int
test_seam_execle(const char *path,
                 const char *arg0, ...){
  int rc;
  va_list ap;
  char **argv;
  char **envp;
  char *s;
  size_t argv_len;
  size_t envp_len;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_execle)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    argv = malloc(MAX_EXEC_ARGS * sizeof(*argv));
    assert(argv);
    envp = malloc(MAX_EXEC_ARGS * sizeof(*envp));
    assert(envp);
    argv_len = 0;
    assert(argv);
    argv[0] = strdup(arg0);
    assert(argv[0]);
    argv_len += 1;
    va_start(ap, arg0);
    while(1){
      s = va_arg(ap, char *);
      if(s){
        argv[argv_len] = strdup(s);
        assert(argv[argv_len]);
      }
      else{
        argv[argv_len] = NULL;
        break;
      }
      argv_len += 1;
    }
    envp_len = 0;
    while(1){
      s = va_arg(ap, char *);
      if(s){
        envp[envp_len] = strdup(s);
        assert(envp[envp_len]);
      }
      else{
        envp[envp_len] = NULL;
        break;
      }
      envp_len += 1;
    }
    va_end(ap);
    rc = execvpe(path, argv, envp);
    free(argv);
    free(envp);
  }
  return rc;
}

/**
 * Control when execlp() fails.
 *
 * @param[in] file Path to executable file.
 * @param[in] arg0 List of arguments to pass to the executable program.
 * @return         This will return an error if unable to execute
 *                 program, otherwise this does not return.
 */
int
test_seam_execlp(const char *file,
                 const char *arg0, ...){
  int rc;
  va_list ap;
  char **argv;
  char *s;
  size_t argv_len;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_execlp)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    argv = malloc(MAX_EXEC_ARGS * sizeof(*argv));
    assert(argv);
    argv_len = 0;
    assert(argv);
    argv[0] = strdup(arg0);
    assert(argv[0]);
    argv_len += 1;
    va_start(ap, arg0);
    while(1){
      s = va_arg(ap, char *);
      if(s){
        argv[argv_len] = strdup(s);
        assert(argv[argv_len]);
      }
      else{
        argv[argv_len] = NULL;
        break;
      }
      argv_len += 1;
    }
    va_end(ap);
    rc = execvp(file, argv);
    free(argv);
  }
  return rc;
}

/**
 * Control when fclose() fails.
 *
 * @param[in,out] stream File to close.
 * @retval        0      Closed the file.
 * @retval        EOF    Failed to close the file.
 */
int
test_seam_fclose(FILE *stream){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_fclose)){
    test_seam_force_errno(EBADF);
    rc = EOF;
  }
  else{
    rc = fclose(stream);
  }
  return rc;
}

/**
 * Control when ferror() fails.
 *
 * @param[in] stream File stream.
 * @retval    0      Error status not set.
 * @retval    !0     Error status set.
 */
int
test_seam_ferror(FILE *stream){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_ferror)){
    rc = 1;
  }
  else{
    rc = ferror(stream);
  }
  return rc;
}

/**
 * Control when fopen() fails.
 *
 * @param[in] pathname Path to file.
 * @param[in] mode     Access mode.
 * @retval    FILE*    File stream.
 * @retval    NULL     Failed to open file.
 */
FILE *
test_seam_fopen(const char *pathname,
                const char *mode){
  FILE *fp;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_fopen)){
    test_seam_force_errno(EACCES);
    fp = NULL;
  }
  else{
    fp = fopen(pathname, mode);
  }
  return fp;
}

/**
 * Control when fork() fails.
 *
 * @retval 0  Forked child process.
 * @retval >0 Child process passed to parent.
 * @retval -1 Failed to fork process.
 */
pid_t
test_seam_fork(void){
  pid_t pid;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_fork)){
    test_seam_force_errno(EAGAIN);
    pid = -1;
  }
  else{
    pid = fork();
  }
  return pid;
}

/**
 * Control when getpwuid() fails.
 *
 * @param[in] uid     User ID.
 * @retval    passwd* Password entry corresponding to @p uid.
 * @retval    NULL    Password entry does not exist.
 */
struct passwd *
test_seam_getpwuid(uid_t uid){
  struct passwd *passwd;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_getpwuid)){
    test_seam_force_errno(EIO);
    passwd = NULL;
  }
  else{
    passwd = getpwuid(uid);
  }
  return passwd;
}

/**
 * Control when localtime() fails.
 *
 * @param[in] timer Time to convert.
 * @retval    tm*   Converted time structure.
 * @retval    NULL  Failed to convert time.
 */
struct tm *
test_seam_localtime(const time_t *timer){
  struct tm *tm;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_localtime)){
    test_seam_force_errno(EOVERFLOW);
    tm = NULL;
  }
  else{
    if(g_test_seam_localtime_tm){
      tm = g_test_seam_localtime_tm;
    }
    else{
      tm = localtime(timer);
    }
  }
  return tm;
}

/**
 * Control when malloc() fails.
 *
 * @param[in] size  Number of bytes to allocate.
 * @retval    void* Pointer to new allocated memory.
 * @retval    NULL  Memory allocation failed.
 */
void *
test_seam_malloc(size_t size){
  void *mem;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_malloc)){
    test_seam_force_errno(ENOMEM);
    mem = NULL;
  }
  else{
    mem = malloc(size);
  }
  return mem;
}

/**
 * Control when mkdir() fails.
 *
 * @param[in] path  Path of new directory.
 * @param[in] mode  File permission bits.
 * @retval    0     Successfully created directory.
 * @retval    -1    Failed to create directory.
 */
int
test_seam_mkdir(const char *path,
                mode_t mode){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_mkdir)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    rc = mkdir(path, mode);
  }
  return rc;
}

/**
 * Control when open() fails.
 *
 * @param[in] path  Path to file.
 * @param[in] oflag File access flags.
 * @param[in] mode  Mode of file.
 * @retval    >0    File descriptor of opened file.
 * @retval    -1    Failed to open file.
 */
int
test_seam_open(const char *path,
               int oflag,
               mode_t mode){
  int fd;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_open)){
    errno = EACCES;
    fd = -1;
  }
  else{
    fd = open(path, oflag, mode);
  }
  return fd;
}

/**
 * Control when pipe() fails.
 *
 * @param[out] fildes File descriptors with read/write end of a pipe.
 * @retval     0      Successfully created a pipe.
 * @retval     -1     Failed to create a pipe.
 */
int
test_seam_pipe(int fildes[2]){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_pipe)){
    test_seam_force_errno(EMFILE);
    rc = -1;
  }
  else{
    rc = pipe(fildes);
  }
  return rc;
}

/**
 * Control when read() fails.
 *
 * @param[in]  fildes File Descriptor.
 * @param[out] buf    Store bytes read into this buffer.
 * @param[in]  nbyte  Number of bytes available in @p buf.
 * @retval     >=0    Number of bytes read.
 * @retval     -1     Failed to read file.
 */
ssize_t
test_seam_read(int fildes,
               void *buf,
               size_t nbyte){
  ssize_t bytes_read;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_read)){
    test_seam_force_errno(EBADF);
    bytes_read = -1;
  }
  else{
    bytes_read = read(fildes, buf, nbyte);
  }
  return bytes_read;
}

/**
 * Control when realloc() fails.
 *
 * @param[in,out] ptr   Pointer to an existing allocated memory region, or
 *                      NULL to allocate a new memory region.
 * @param[in]     size  Number of bytes to allocate.
 * @retval        void* Pointer to new allocated memory.
 * @retval        NULL  Memory allocation failed.
 */
void *
test_seam_realloc(void *ptr,
                  size_t size){
  void *mem;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_realloc)){
    test_seam_force_errno(ENOMEM);
    mem = NULL;
  }
  else{
    mem = realloc(ptr, size);
  }
  return mem;
}

/**
 * Control when remove() fails.
 *
 * @param[in] path Remove file at this path.
 * @retval    0    Successfully removed file.
 * @retval    -1   Failed to remove file.
 */
int
test_seam_remove(const char *path){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_remove)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    rc = remove(path);
  }
  return rc;
}

/**
 * Control when rename() fails.
 *
 * @param[in] old Rename this to @p new.
 * @param[in] new New file name.
 * @retval    0   Successfully renamed file.
 * @retval    -1  Failed to rename file.
 */
int
test_seam_rename(const char *old,
                 const char *new){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_rename)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    rc = rename(old, new);
  }
  return rc;
}

/**
 * Control when sigaction() fails.
 *
 * @param[in]  sig  Signal number.
 * @param[in]  act  New action.
 * @param[out] oact Old action.
 * @retval     0    Successfully changed signal action.
 * @retval     -1   Failed to change signal action.
 */
int
test_seam_sigaction(int sig,
                    const struct sigaction *act,
                    struct sigaction *oact){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_sigaction)){
    test_seam_force_errno(EINVAL);
    rc = -1;
  }
  else{
    rc = sigaction(sig, act, oact);
  }
  return rc;
}

/**
 * Control when sigemptyset() fails.
 *
 * @param[in,out] set Signal set.
 * @retval        0   Signal set cleared.
 * @retval        -1  Failed to clear signal set.
 */
int
test_seam_sigemptyset(sigset_t *set){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_sigemptyset)){
    test_seam_force_errno(ENOMEM);
    rc = -1;
  }
  else{
    rc = sigemptyset(set);
  }
  return rc;
}

/**
 * Store a formatted string into a buffer.
 *
 * @param[out] s      Buffer to store result.
 * @param[in]  n      Number of bytes available in @p s.
 * @param[in]  format Format string.
 * @retval     >=0    Number of bytes that would get output if @p s large
 *                    enough.
 * @retval     -1     Output error.
 */
int
test_seam_snprintf(char *s,
                   size_t n,
                   const char *format, ...){
  int rc;
  va_list ap;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_snprintf)){
    test_seam_force_errno(ENOMEM);
    rc = -1;
  }
  else{
    va_start(ap, format);
    rc = vsnprintf(s, n, format, ap);
    va_end(ap);
  }
  return rc;
}

/**
 * Get file information.
 *
 * @param[in]  path Path to file.
 * @param[out] buf  File information.
 * @retval     0    Successfully performed stat on file.
 * @retval     -1   Failed to stat file.
 */
int
test_seam_stat(const char *path,
               struct stat *buf){
  int rc;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_stat)){
    test_seam_force_errno(EACCES);
    rc = -1;
  }
  else{
    rc = stat(path, buf);
  }
  return rc;
}

/**
 * Control when strdup() fails.
 *
 * @param[in] s     String to duplicate.
 * @retval    char* Duplicated string.
 * @retval    NULL  Failed to duplicate string.
 */
char *
test_seam_strdup(const char *s){
  char *str;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_strdup)){
    test_seam_force_errno(ENOMEM);
    str = NULL;
  }
  else{
    str = strdup(s);
  }
  return str;
}

/**
 * Control when strndup() fails.
 *
 * @param[in] s     String to duplicate.
 * @param[in] size  Maximum number of bytes to copy.
 * @retval    char* Duplicated string.
 * @retval    NULL  Failed to duplicate string.
 */
char *
test_seam_strndup(const char *s,
                  size_t size){
  char *str;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_strndup)){
    test_seam_force_errno(ENOMEM);
    str = NULL;
  }
  else{
    str = strndup(s, size);
  }
  return str;
}

/**
 * Control when waitpid() fails.
 *
 * @param[in]  pid      Process ID to wait for.
 * @param[out] stat_loc Process status.
 * @param[in]  options  Wait flags.
 * @retval     !(-1)    Process ID.
 * @retval     -1       Failed to wait for process.
 */
pid_t
test_seam_waitpid(pid_t pid,
                  int *stat_loc,
                  int options){
  pid_t pid_wait;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_waitpid)){
    test_seam_force_errno(ECHILD);
    pid_wait = -1;
  }
  else{
    pid_wait = waitpid(pid, stat_loc, options);
  }
  return pid_wait;
}

/**
 * Control when write() fails.
 *
 * @param[in] fildes File descriptor.
 * @param[in] buf    Bytes to write to file.
 * @param[in] nbyte  Number of bytes to write in @p buf.
 * @retval    >=0    Number of bytes written.
 * @retval    -1     Failed to write to file.
 */
ssize_t
test_seam_write(int fildes,
                const void *buf,
                size_t nbyte){
  ssize_t bytes_written;

  if(test_seam_dec_err_ctr(&g_test_seam_err_ctr_write)){
    test_seam_force_errno(EACCES);
    bytes_written = -1;
  }
  else{
    bytes_written = write(fildes, buf, nbyte);
  }
  return bytes_written;
}

