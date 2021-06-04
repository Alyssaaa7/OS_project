#include <syscall.h>
#include <stdio.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  char child_cmd[128];
  int handle;
  CHECK((handle = open("sample.txt")) > 1, "open \"sample.txt\"");
  snprintf(child_cmd, sizeof child_cmd, "c-open-close sample.txt");
  msg("wait(exec()) = %d", wait(exec(child_cmd)));
  msg("close \"sample.txt\" in parent");
  close(handle);
}
