#include "filesys/directory.h"
#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/block.h"

/* Partition that contains the file system. */
struct block* fs_device;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
  fs_device = block_get_role(BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC("No file system device found, can't initialize file system.");

  buffer_init();
  inode_init();
  free_map_init();
  thread_current()->cwd = dir_open_root();

  if (format)
    do_format();

  free_map_open();
}

unsigned long long get_disk_write_cnt(void) { return fs_device->write_cnt; }

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done(void) {
  buffer_flush();
  free_map_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create(const char* name, off_t initial_size, bool is_dir) {
  block_sector_t inode_sector = 0;
  bool success = false;
  struct dir* dir;

  if (!is_dir) {
    char filename[NAME_MAX + 1];
    dir = get_dir_path(name, filename);
    if (dir == NULL)
      return false;
    success =
        (dir != NULL && free_map_allocate(1, &inode_sector) &&
         inode_create(inode_sector, initial_size, 0) && dir_add(dir, filename, inode_sector, 0));
    dir->file_dir_count += 1;
  } else {
    char* last_part = malloc(NAME_MAX + 1);
    dir = get_dir_path(name, last_part);
    success =
        (dir != NULL && free_map_allocate(1, &inode_sector) &&
         inode_create(inode_sector, initial_size, 1) && dir_add(dir, last_part, inode_sector, 1));
    if (success) {
      dir->file_dir_count += 1;
      struct inode* new_inode;
      dir_lookup(dir, last_part, &new_inode);
      struct dir* new_dir = dir_open(new_inode);
      new_dir->file_dir_count = 0;
      create_parent_dir(dir, new_dir);
      dir_close(new_dir);
    }
  }

  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);
  dir_close(dir);

  return success;
}

/* Opens the file with the given NAME in DIR.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file* filesys_open(const char* name, struct dir* dir) {
  struct inode* inode = NULL;

  if (dir != NULL)
    dir_lookup(dir, name, &inode);
  dir_close(dir);

  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove(const char* name, struct dir* dir) {
  bool success = dir != NULL && dir_remove(dir, name);
  dir_close(dir);

  return success;
}

/* Formats the file system. */
static void do_format(void) {
  printf("Formatting file system...");
  free_map_create();
  if (!dir_create(ROOT_DIR_SECTOR, 16))
    PANIC("root directory creation failed");
  free_map_close();
  printf("done.\n");
}
