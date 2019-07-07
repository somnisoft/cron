/**
 * @file
 * @brief Fuzz testing.
 * @author James Humphrey (humphreyj@somnisoft.com)
 *
 * This software has been placed into the public domain using CC0.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/crond.h"

/**
 * Fuzz test the crond crontab line parser.
 *
 * Usage: fuzz-driver
 *
 * @retval 0 All tests passed.
 */
int
main(void){
  struct crond crond;
  FILE *fp;
  char *buf;
  size_t bufsz;
  size_t buflen;
  size_t bytes_read;

  memset(&crond, 0, sizeof(crond));
  fp = stdin;
  buf = NULL;
  bufsz = 0;
  buflen = 0;
  while(!feof(fp)){
    bufsz += 1000;
    buf = realloc(buf, bufsz);
    assert(buf);
    bytes_read = fread(&buf[buflen], 1, bufsz - buflen, fp);
    buflen += bytes_read;
    assert(ferror(fp) == 0);
  }
  buf[buflen] = '\0';
  crond_crontab_parse_line(&crond, buf);

  return 0;
}

