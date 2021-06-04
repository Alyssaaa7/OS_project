#include "filesys/inode.h"
#include <stdbool.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/directory.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Global lock for free_map */
struct lock free_map_lock;

/* Specifies length of buffer cache. */
#define NUM_BLOCKS 64

/* A global lock for the buffer cache. */
static struct lock buffer_cache_lock;

/* Buffer cache consist of buffer blocks. */
static buffer_block buffer_cache[NUM_BLOCKS];

/* Clock hand for the clock algorithm. */
static int clock_hand;

/* # of hit in the buffer cache. */
static int num_hit;

/* # of buffer cache accesses. */
static int num_access;

/* Initialize buffer cache. */
void buffer_init(void) {
  lock_init(&buffer_cache_lock);
  num_hit = 0;
  num_access = 0;
  for (int i = 0; i < NUM_BLOCKS; i++) {
    buffer_block* block = &buffer_cache[i];
    block->data = malloc(BLOCK_SECTOR_SIZE);
    memset(block->data, 0, BLOCK_SECTOR_SIZE);
    block->free = true;
    block->dirty = false;
    block->accessed = false;
    block->sector = 0;
    lock_init(&block->lock);
  }
}

/* Evict a block in buffer cache according to the clock algorithm. */
static int buffer_evict(void) {
  while (buffer_cache[clock_hand].accessed) {
    buffer_cache[clock_hand].accessed = false;
    // Advance clock hand
    clock_hand = (clock_hand + 1) % NUM_BLOCKS;
  }
  buffer_block* block = &buffer_cache[clock_hand];

  // flush the block to disk if dirty
  if (block->dirty)
    block_write(fs_device, block->sector, block->data);
  block->free = true;
  block->sector = 0;
  block->dirty = false;
  memset(block->data, 0, BLOCK_SECTOR_SIZE);
  return clock_hand;
}

/* Search and return the buffer block with desired sector.
 * Return -1 if sector is not in buffer cache. */
static int buffer_search(block_sector_t sector) {
  for (int i = 0; i < NUM_BLOCKS; i++) {
    if (buffer_cache[i].sector == sector && !buffer_cache[i].free)
      return i;
  }
  return -1;
}

/* Search for a free block in the buffer cache. Calls
 * buffer_evict if there is no free block. */
static int buffer_search_free(void) {
  for (int i = 0; i < NUM_BLOCKS; i++) {
    if (buffer_cache[i].free)
      return i;
  }
  return buffer_evict();
}

/* Read SECTOR into buffer_cache. Copy SIZE bytes of content starting
 * from OFFSET into BUFFER. */
void buffer_read(block_sector_t sector, void* buffer, int offset, int size) {
  int index;
  bool in_buffer;

  while (true) {
    in_buffer = true;
    lock_acquire(&buffer_cache_lock);
    num_access++;

    /* search for the desired sector in buffer cache */
    index = buffer_search(sector);
    if (index < 0) {
      /* if not present, prepare a free block */
      index = buffer_search_free();
      in_buffer = false;
    } else {
      num_hit++;
    }

    /* check if the selected block has been modified */
    lock_release(&buffer_cache_lock);
    lock_acquire(&(buffer_cache[index].lock));
    if (!in_buffer && buffer_cache[index].free)
      break;
    if (buffer_cache[index].sector == sector && !buffer_cache[index].free)
      break;
    lock_release(&(buffer_cache[index].lock));
  }

  buffer_block* block = &buffer_cache[index];
  /* If the target sector is not in buffer, read into the free block */
  if (!in_buffer) {
    block_read(fs_device, sector, block->data);
    block->free = false;
    block->sector = sector;
  }

  if (buffer != NULL)
    memcpy(buffer, block->data + offset, size);
  block->accessed = true;
  lock_release(&(buffer_cache[index].lock));
}

/* Write SIZE bytes of SECTOR into buffer_cache (write-back), starting
 * from OFFSET, which gets flushed onto disk later on. */
void buffer_write(block_sector_t sector, void* buffer, int offset, int size) {
  lock_acquire(&buffer_cache_lock);
  num_access++;

  // search for specified sector
  int index = buffer_search(sector);
  if (index < 0) {
    index = buffer_search_free();
    buffer_cache[index].free = false;
    buffer_cache[index].sector = sector;
  } else {
    num_hit++;
  }
  buffer_block* block = &buffer_cache[index];
  lock_acquire(&block->lock);
  lock_release(&buffer_cache_lock);

  // write data to buffer cache
  memcpy(block->data + offset, buffer, size);
  block->dirty = true;
  block->accessed = true;
  lock_release(&block->lock);
}

/* Flush the entire buffer cache to disk. */
void buffer_flush(void) {
  lock_acquire(&buffer_cache_lock);
  for (int i = 0; i < NUM_BLOCKS; i++) {
    if (buffer_cache[i].dirty) {
      block_write(fs_device, buffer_cache[i].sector, buffer_cache[i].data);
      buffer_cache[i].dirty = false;
    }
  }
  lock_release(&buffer_cache_lock);
}

/* Get number of buffer cache hits. */
int get_buffer_hit(void) { return num_hit; }

/* Get number of buffer cache accesses. */
int get_buffer_ac(void) { return num_access; }

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t bytes_to_sectors(off_t size) { return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE); }

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector(const struct inode* inode, off_t pos) {
  ASSERT(inode != NULL);

  block_sector_t result;
  /* Get inode_disk from inode */
  struct inode_disk* inode_disk = malloc(sizeof *inode_disk);
  buffer_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);

  /* Number of sectors up until (and including) the sector that POS(index) is. */
  int target_sector_idx = bytes_to_sectors(pos + 1);

  /* Lookup the sector number that contains POS. */
  if (target_sector_idx <= 123) {
    result = inode_disk->direct[target_sector_idx - 1];
  } else if (target_sector_idx <= 123 + 128) {
    block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
    buffer_read(inode_disk->indirect, direct_arr, 0, BLOCK_SECTOR_SIZE);
    result = direct_arr[target_sector_idx - 124];
    free(direct_arr);
  } else if (target_sector_idx <= 123 + 128 + 128 * 128) {
    block_sector_t* indirect_arr = malloc(128 * sizeof(block_sector_t));
    buffer_read(inode_disk->doubly_indirect, indirect_arr, 0, BLOCK_SECTOR_SIZE);

    int idx = (target_sector_idx - 123 - 128 - 1) / 128;
    int offset = (target_sector_idx - 123 - 128 - 1) % 128;

    block_sector_t indirect_sector = indirect_arr[idx];
    free(indirect_arr);

    block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
    buffer_read(indirect_sector, direct_arr, 0, BLOCK_SECTOR_SIZE);
    result = direct_arr[offset];
    free(direct_arr);
  }

  free(inode_disk);
  return result;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
static struct lock open_inodes_lock;

/* Initializes the inode module. */
void inode_init(void) {
  list_init(&open_inodes);
  lock_init(&open_inodes_lock);
}

bool inode_is_dir(struct inode* inode) {
  bool result = false;
  struct inode_disk* inode_disk = malloc(sizeof *inode_disk);
  buffer_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);
  if (inode_disk->is_dir) {
    result = true;
  }
  free(inode_disk);
  return result;
}

/* Release from cur_block to starting_block. */
static void inode_release(struct inode_disk* inode_disk, int starting_block, int end_block) {
  if (end_block == starting_block) {
    return;
  }
  int cur_block = starting_block;

  /* Release direct pointers first. */
  while (cur_block <= 122) {
    free_map_release(inode_disk->direct[cur_block], 1);
    cur_block++;
    if (cur_block == end_block) {
      return;
    }
  }

  /* Release indirect pointer. */
  block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
  buffer_read(inode_disk->indirect, direct_arr, 0, BLOCK_SECTOR_SIZE);

  if (starting_block <= 123) {
    /* Release indirect pointer. */
    free_map_release(inode_disk->indirect, 1);
  }

  while (cur_block <= 122 + 128) {
    free_map_release(direct_arr[cur_block - 123], 1);
    cur_block++;

    if (cur_block == end_block) {
      free(direct_arr);
      return;
    }
  }
  free(direct_arr);

  /* Read in doubly indirect pointer. */
  block_sector_t* indirect_arr = malloc(128 * sizeof(block_sector_t));
  buffer_read(inode_disk->doubly_indirect, indirect_arr, 0, BLOCK_SECTOR_SIZE);

  if (starting_block <= 123 + 128) {
    /* Release doubly-indirect pointer. */
    free_map_release(inode_disk->doubly_indirect, 1);
  }

  int indirect_idx = (cur_block - 123 - 128) / 128;
  int offset = (cur_block - 123 - 128) % 128;

  while (cur_block <= 122 + 128 + 128 * 128) {
    block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
    buffer_read(indirect_arr[indirect_idx], direct_arr, 0, BLOCK_SECTOR_SIZE);

    if (starting_block <= 123 + 128 + indirect_idx * 128) {
      /* Release doubly-indirect pointer. */
      free_map_release(indirect_arr[indirect_idx], 1);
    }

    while (offset < 128) {
      ASSERT(offset == offset % 128);
      free_map_release(direct_arr[offset], 1);

      cur_block++;
      offset++;

      if (cur_block == end_block) {
        free(direct_arr);
        free(indirect_arr);
        return;
      }
    }

    free(direct_arr);
    indirect_idx++;
    offset = 0;
  }
  free(indirect_arr);
}

/* Grow file's inode_disk to SIZE */
static bool inode_grow_file(struct inode_disk* inode_disk, off_t end_size) {

  /* Current number of blocks in the inode_disk. */
  int starting_block = bytes_to_sectors(inode_disk->length);
  /* Desired number of blocks in the inode_disk. */
  int end_block = bytes_to_sectors(end_size);
  if (starting_block == end_block) {
    return true;
  }
  if (end_block > 123 + 128 + 128 * 128) {
    return false;
  }

  int cur_block = starting_block;
  /* Allocate direct pointers first. */
  while (cur_block <= 122) {
    ASSERT(inode_disk->direct != NULL);
    if (!free_map_allocate(1, &inode_disk->direct[cur_block])) {
      inode_release(inode_disk, starting_block, cur_block);
      return false;
    }

    char* buffer = calloc(1, BLOCK_SECTOR_SIZE);
    buffer_write(inode_disk->direct[cur_block], buffer, 0, BLOCK_SECTOR_SIZE);
    free(buffer);
    cur_block++;

    if (cur_block == end_block) {
      return true;
    }
  }

  /* Allocate indirect pointer. */
  block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
  if (!inode_disk->indirect) {
    if (!free_map_allocate(1, &inode_disk->indirect)) {
      inode_release(inode_disk, starting_block, cur_block);
      return false;
    }
  } else {
    buffer_read(inode_disk->indirect, direct_arr, 0, BLOCK_SECTOR_SIZE);
  }

  while (cur_block <= 122 + 128) {
    if (!free_map_allocate(1, &direct_arr[cur_block - 123])) {
      inode_release(inode_disk, starting_block, cur_block);
      free(direct_arr);
      return false;
    }

    char* buffer = calloc(1, BLOCK_SECTOR_SIZE);
    buffer_write(direct_arr[cur_block - 123], buffer, 0, BLOCK_SECTOR_SIZE);
    free(buffer);
    cur_block++;

    if (cur_block == end_block) {
      buffer_write(inode_disk->indirect, direct_arr, 0, BLOCK_SECTOR_SIZE);
      free(direct_arr);
      return true;
    }
  }

  buffer_write(inode_disk->indirect, direct_arr, 0, BLOCK_SECTOR_SIZE);
  free(direct_arr);

  /* Allocate doubly indirect pointer. */
  block_sector_t* indirect_arr = malloc(128 * sizeof(block_sector_t));
  if (!inode_disk->doubly_indirect) {
    if (!free_map_allocate(1, &inode_disk->doubly_indirect)) {
      inode_release(inode_disk, starting_block, cur_block);
      return false;
    }
  } else {
    buffer_read(inode_disk->doubly_indirect, indirect_arr, 0, BLOCK_SECTOR_SIZE);
  }

  int indirect_idx = (cur_block - 123 - 128) / 128;
  int offset = (cur_block - 123 - 128) % 128;

  while (cur_block <= 122 + 128 + 128 * 128) {

    block_sector_t* direct_arr = malloc(128 * sizeof(block_sector_t));
    if (starting_block <= 123 + 128 + indirect_idx * 128) {
      /* Allocate indirect index */
      if (!free_map_allocate(1, &indirect_arr[indirect_idx])) {
        inode_release(inode_disk, starting_block, cur_block);
        return false;
      }
    } else {
      buffer_read(indirect_arr[indirect_idx], direct_arr, 0, BLOCK_SECTOR_SIZE);
    }

    while (offset < 128) {
      ASSERT(offset == offset % 128);
      if (!free_map_allocate(1, &direct_arr[offset])) {
        inode_release(inode_disk, starting_block, cur_block);
        free(direct_arr);
        free(indirect_arr);
        return false;
      }
      char* buffer = calloc(1, BLOCK_SECTOR_SIZE);
      buffer_write(direct_arr[offset], buffer, 0, BLOCK_SECTOR_SIZE);
      free(buffer);
      cur_block++;
      offset++;

      if (cur_block == end_block) {
        buffer_write(indirect_arr[indirect_idx], direct_arr, 0, BLOCK_SECTOR_SIZE);
        free(direct_arr);
        buffer_write(inode_disk->doubly_indirect, indirect_arr, 0, BLOCK_SECTOR_SIZE);
        free(indirect_arr);
        return true;
      }
    }

    buffer_write(indirect_arr[indirect_idx], direct_arr, 0, BLOCK_SECTOR_SIZE);
    free(direct_arr);

    indirect_idx++;
    offset = 0;
  }

  buffer_write(inode_disk->doubly_indirect, indirect_arr, 0, BLOCK_SECTOR_SIZE);
  free(indirect_arr);

  /* Should never reach here.*/
  return true;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, int is_dir) {
  struct inode_disk* disk_inode = NULL;
  bool success = false;

  ASSERT(length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    disk_inode->is_dir = is_dir;
    disk_inode->length = 0;
    disk_inode->magic = INODE_MAGIC;

    off_t actual_length = length;
    if (is_dir)
      actual_length += 2 * sizeof(struct dir_entry);

    // TODO: initialize the pointers.
    if (sector != FREE_MAP_SECTOR) {
      lock_acquire(&free_map_lock);
    }
    if (inode_grow_file(disk_inode, actual_length)) {
      disk_inode->length = actual_length;
      buffer_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
      success = true;
    } else {
      success = false;
    }

    if (sector != FREE_MAP_SECTOR) {
      lock_release(&free_map_lock);
    }
    free(disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode* inode_open(block_sector_t sector) {
  struct list_elem* e;
  struct inode* inode;

  /* Check whether this inode is already open. */
  lock_acquire(&open_inodes_lock);
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e = list_next(e)) {
    inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector) {
      inode_reopen(inode);
      lock_release(&open_inodes_lock);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);
  if (inode == NULL) {
    lock_release(&open_inodes_lock);
    return NULL;
  }

  /* Initialize. */
  lock_init(&inode->inode_lock);
  list_push_front(&open_inodes, &inode->elem);
  lock_release(&open_inodes_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode* inode_reopen(struct inode* inode) {
  if (inode != NULL) {
    lock_acquire(&inode->inode_lock);
    inode->open_cnt++;
    lock_release(&inode->inode_lock);
  }
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode* inode) { return inode->sector; }

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode* inode) {
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  lock_acquire(&inode->inode_lock);
  if (--inode->open_cnt == 0) {
    /* Remove from inode list and release lock. */
    lock_acquire(&open_inodes_lock);
    list_remove(&inode->elem);
    lock_release(&open_inodes_lock);

    /* Deallocate blocks if removed. */
    if (inode->removed) {
      struct inode_disk* inode_disk = malloc(sizeof *inode_disk);
      buffer_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);
      inode_release(inode_disk, 0, bytes_to_sectors(inode_disk->length));
      free(inode_disk);
      free_map_release(inode->sector, 1);
    }
    lock_release(&inode->inode_lock);
    free(inode);
    return;
  } else {
    lock_release(&inode->inode_lock);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode* inode) {
  lock_acquire(&inode->inode_lock);
  ASSERT(inode != NULL);
  inode->removed = true;
  lock_release(&inode->inode_lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
  uint8_t* buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    buffer_read(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset) {
  const uint8_t* buffer = buffer_;
  off_t bytes_written = 0;

  lock_acquire(&inode->inode_lock);
  if (inode->deny_write_cnt) {
    lock_release(&inode->inode_lock);
    return 0;
  }
  lock_release(&inode->inode_lock);

  /* Grow file if necessary */

  if (inode->sector != FREE_MAP_SECTOR) {

    struct inode_disk* inode_disk = malloc(sizeof *inode_disk);
    buffer_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);
    if (offset + size > inode_disk->length) {
      /* Need to grow file. */
      lock_acquire(&free_map_lock);
      if (!inode_grow_file(inode_disk, offset + size)) {
        lock_release(&free_map_lock);
        free(inode_disk);
        return 0; // Growing file failed.
      } else {
        inode_disk->length = offset + size;
        buffer_write(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);
      }
      lock_release(&free_map_lock);
    }
    free(inode_disk);
  }

  while (size > 0) {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;
    buffer_write(sector_idx, (void*)buffer + bytes_written, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode* inode) {
  lock_acquire(&inode->inode_lock);
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  lock_release(&inode->inode_lock);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode* inode) {
  lock_acquire(&inode->inode_lock);
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  lock_release(&inode->inode_lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode* inode) {
  struct inode_disk* inode_disk = malloc(sizeof(struct inode_disk));
  buffer_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE);
  off_t result = inode_disk->length;
  free(inode_disk);
  return result;
}
