/* Checks if buffer cache can coalesce writes to the same sector. */

#include <syscall.h>
#include <random.h>
#include "tests/lib.h"
#include "tests/main.h"

static char buf[1];

void test_main(void) {
  int fd;
  char* file_name = "foonew";
  size_t ofs = 0;
  size_t size = sizeof(buf);
  unsigned long long start_write_cnt = disk_write_cnt();

  random_bytes(buf, size);
  CHECK(create(file_name, 1026), "create \"%s\"", file_name);
  CHECK((fd = open(file_name)) > 1, "open \"%s\"", file_name);

  msg("writing \"%s\"", file_name);
  while (ofs < 1024 * 64) {
    write(fd, buf, 1);
    ofs += 1;
  }

  seek(fd, 0);
  msg("setting fd position to 0");
  ofs = 0;
  msg("reading \"%s\"", file_name);
  while (ofs < 1024 * 64) {
    read(fd, buf, 1);
    ofs += 1;
  }

  unsigned long long total_writes = disk_write_cnt() - start_write_cnt;
  if (total_writes > 70 && total_writes < 200)
    msg("reasonable write count (about 128)");
  else
    msg("unreasonable write count");

  msg("close \"%s\"", file_name);
  close(fd);
}
