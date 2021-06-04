/* Test if SYS_SEEK is implemented correctly by specifying a point that seek jumps to,
and test whether it actually jumps to it or not. */

#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {

  char* file_name = "sample.txt";
  const void* buf = sample;
  size_t size = sample - 1;

  int fd;
  char block[5];
  CHECK((fd = open(file_name)) > 1, "open \"%s\" for verification", file_name);
  seek(fd, 5);
  read(fd, block, 5);
  if (block[0] != 'i')
    fail("seek the wrong position, should be i, but is %s\n", block[0]);
  msg("seek position 5 \"%s\"", file_name);
  msg("close \"%s\"", file_name);
  close(fd);
}
