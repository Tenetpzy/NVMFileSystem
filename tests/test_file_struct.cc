#include <gtest/gtest.h>

extern "C"
{
#include "file_struct.h"
}



TEST(file_struct,BasicAllocFreeFD)
{
  struct file_struct fs;
  file_struct_init(&fs);
  EXPECT_EQ(0, file_struct_alloc_fd_slot(&fs));
  EXPECT_EQ(1, file_struct_alloc_fd_slot(&fs));
  EXPECT_EQ(2, file_struct_alloc_fd_slot(&fs));
  EXPECT_EQ(3, file_struct_alloc_fd_slot(&fs));
  EXPECT_EQ(4, file_struct_alloc_fd_slot(&fs));
  EXPECT_EQ(5, file_struct_alloc_fd_slot(&fs));
  file_struct_free_fd_slot(&fs, 3);
  EXPECT_EQ(3, file_struct_alloc_fd_slot(&fs));
  file_struct_free_fd_slot(&fs, 0);
  file_struct_free_fd_slot(&fs, 1);
  file_struct_free_fd_slot(&fs, 2);
  file_struct_free_fd_slot(&fs, 3);
  file_struct_free_fd_slot(&fs, 4);
  file_struct_free_fd_slot(&fs, 5);
  file_struct_destroy(&fs);
}

