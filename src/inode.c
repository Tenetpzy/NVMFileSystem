#include "inode.h"
#include <stdatomic.h>

struct inode
{
  ino_t ino;
  atomic_int ref_count;
  atomic_int hard_ref_count;
  struct dentry * dentry;
  /*
  文件信息
  */
};