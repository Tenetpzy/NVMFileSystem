#include <gtest/gtest.h>

extern "C"
{
#include "file_struct.h"
}

class file_struct_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pfs = file_struct_new();
    }
    void TearDown() override
    {
        file_struct_free(pfs);
    }
    struct file_struct *pfs;
};

TEST_F(file_struct_Test, BasicAllocFreeFD)
{
    EXPECT_EQ(0, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(1, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(2, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(3, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(4, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(5, file_struct_alloc_fd_slot(pfs));
    file_struct_free_fd_slot(pfs, 3);
    EXPECT_EQ(3, file_struct_alloc_fd_slot(pfs));
    file_struct_free_fd_slot(pfs, 0);
    file_struct_free_fd_slot(pfs, 1);
    file_struct_free_fd_slot(pfs, 2);
    file_struct_free_fd_slot(pfs, 3);
    file_struct_free_fd_slot(pfs, 4);
    file_struct_free_fd_slot(pfs, 5);
}

TEST_F(file_struct_Test, ManyAllocFreeFD)
{
    for (int i = 0; i < 10000; i++)
    {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 2500; i < 5000; i++)
    {
        file_struct_free_fd_slot(pfs, i);
    }
    for (int i = 1000; i < 1500; i++)
    {
        file_struct_free_fd_slot(pfs, i);
    }
    for (int i = 6000; i < 7000; i++)
    {
        file_struct_free_fd_slot(pfs, i);
    }
    for(int i=1000;i<1500;i++)
    {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 2500; i < 5000; i++)
    {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 6000; i < 7000; i++)
    {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
}

