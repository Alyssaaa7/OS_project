/* Measure hit rate of the buffer cache. */

#include <syscall.h>
#include <random.h>
#include "tests/lib.h"
#include "tests/main.h"

static char buf[1024];

void test_main(void) {
  int fd;
  size_t ofs;
  size_t size = sizeof(buf);
  char* file_name = "newfoo";

  CHECK(create(file_name, 1026), "create \"%s\"", file_name);
  CHECK((fd = open(file_name)) > 1, "open \"%s\"", file_name);
  random_bytes(buf, size);
  ofs = 0;
  msg("reading \"%s\"", file_name);
  while (ofs < size) {
    size_t block_size = 512;
    if (block_size > size - ofs)
      block_size = size - ofs;

    write(fd, buf + ofs, block_size);
    ofs += block_size;
  }

  int cold_hit = num_buffer_hit();
  int cold_access = num_buffer_access();

  seek(fd, 0);
  msg("setting fd position to 0");

  ofs = 0;
  msg("reading \"%s\"", file_name);
  while (ofs < size) {
    size_t block_size = 512;
    if (block_size > size - ofs)
      block_size = size - ofs;

    write(fd, buf + ofs, block_size);
    ofs += block_size;
  }
  msg("close \"%s\"", file_name);
  close(fd);

  int total_hit = num_buffer_hit();
  int total_access = num_buffer_access();

  int hot_hit = total_hit - cold_hit;
  int hot_access = total_access - cold_access;

  int result = hot_access * cold_hit - cold_access * hot_hit;
  if (result >= 0) {
    msg("hit rate does not improve");
  } else {
    msg("hit rate improves");
  }

  remove("newfoo");
}
