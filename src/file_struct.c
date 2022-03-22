#include "file_struct.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

struct fileset
{
    file_struct_bitset_t bitset;
    struct file *files[64];
};

struct fd_array
{
    struct fileset fileset[L2FDARRAYSIZE];
    atomic_int_fast32_t counter;
};

#pragma region PRIVATE

static atomic_fd_array *get_fd_array_ptr(struct file_struct *self, int L1Index)
{
    return &self->fd_index_array[L1Index >> 6].fd_array[L1Index & 63];
}

static struct file **get_file_ptr(struct fd_array *self, int L2Index)
{
    return &self->fileset[L2Index >> 6].files[L2Index & 63];
}

static int find_available_fd_array_index(struct file_struct *self)
{
    struct fd_index_array *it;
    for (int i = 0; i < L1FDARRAYSIZE; i++)
    {
        it = &self->fd_index_array[i];
        file_struct_bitset_t bitset = it->bitset;
        if (bitset == UINT64_MAX)
        {
            // no empty slot
            continue;
        }
        int firstempty = RIGHT_MOST_ZERO_LL(bitset);
        return firstempty + (i << 6);
    }
    return -1;
}

static int fd_array_isfull(struct fd_array *self)
{
    assert(self->counter >= 0);
    assert(self->counter <= L2FDSIZE);
    return self->counter == L2FDSIZE;
}

static void file_struct_incre_fd_array_count(struct file_struct *self, int fd_array_idx)
{
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    atomic_fetch_add(&fd_array->counter, 1);
    assert(fd_array->counter <= L2FDSIZE);

    if (fd_array_isfull(fd_array))
    {
        int idx = fd_array_idx >> 6;
        uint64_t idxbit = 1ULL << (fd_array_idx & 63);
        file_struct_bitset_t exp_bitset = self->fd_index_array[idx].bitset;
        file_struct_bitset_t set_bitset = idxbit | exp_bitset;
        //弱保证：如果fd_array满，一般其bit会set，这里不做循环
        atomic_compare_exchange_strong(&self->fd_index_array[idx].bitset, &exp_bitset, set_bitset);
        //强保证：如果fd_array没有满，其bit一定unset
        //再次测试，没有满，去掉其bit
        while (!fd_array_isfull(fd_array))
        {
            exp_bitset = self->fd_index_array[idx].bitset;
            if ((exp_bitset & idxbit) == 0)
            {
                break;
            }
            set_bitset = exp_bitset & (~idxbit);
            if (atomic_compare_exchange_strong(&self->fd_index_array[idx].bitset, &exp_bitset, set_bitset))
            {
                break;
            }
        }
    }
}

static void file_struct_decre_fd_array_count(struct file_struct *self, int fd_array_idx)
{
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    atomic_fetch_sub(&fd_array->counter, 1);
    assert(fd_array->counter >= 0);
    int idx = fd_array_idx >> 6;
    uint64_t idxbit = 1ULL << (fd_array_idx & 63);
    file_struct_bitset_t exp_bitset;
    file_struct_bitset_t set_bitset;
    //去除bit
    while (1)
    {
        exp_bitset = self->fd_index_array[idx].bitset;
        set_bitset = exp_bitset & (~idxbit);
        if (exp_bitset == set_bitset)
        {
            break;
        }
        if (atomic_compare_exchange_weak(&self->fd_index_array[idx].bitset, &exp_bitset, set_bitset))
        {
            break;
        }
    }
}

static int file_struct_try_alloc_fd(struct file_struct *self, int fd_array_idx, int fileset_idx)
{
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    struct fileset *fileset = &fd_array->fileset[fileset_idx];
    while (1)
    {
        file_struct_bitset_t bitset = fileset->bitset;
        if (bitset == UINT64_MAX)
        {
            //满了，下一个
            return -1;
        }
        int firstempty = RIGHT_MOST_ZERO_LL(bitset);
        file_struct_bitset_t set_bitset = bitset | (1ULL << firstempty);
        if (atomic_compare_exchange_weak(&fileset->bitset, &bitset, set_bitset))
        {
            file_struct_incre_fd_array_count(self, fd_array_idx);
            return fd_array_idx * L2FDSIZE + (fileset_idx << 6) + firstempty;
        }
    }
}

static atomic_fd_array fd_array_new()
{
    atomic_fd_array ret = malloc(sizeof(struct fd_array));
    for (int i = 0; i < L2FDARRAYSIZE; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            ret->fileset[i].files[j] = NULL;
        }
        ret->fileset[i].bitset = 0;
    }
    ret->counter = 0;
    return ret;
}

static void fd_array_destory(struct fd_array *self)
{
    //这里没有释放所有资源，因为file 还没有定义
    if(self){
        //TODO destory file*
    }
    free(self);
}

#pragma endregion END PRIVATE

void file_struct_init(struct file_struct *self)
{
    for (int i = 0; i < L1FDARRAYSIZE; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            self->fd_index_array[i].fd_array[j] = NULL;
        }
        self->fd_index_array[i].bitset = 0;
    }
}

int file_struct_alloc_fd_slot(struct file_struct *self)
{
    while (1)
    {
        int fd_array_index = find_available_fd_array_index(self);
        //没找到任何有机会的位置
        if (fd_array_index == -1)
        {
            return -1;
        }
        atomic_fd_array *fd_array_ptr = get_fd_array_ptr(self, fd_array_index);

        //如果是空位置，初始化一个
        if (*fd_array_ptr == NULL)
        {
            //初始化
            atomic_fd_array new_fd_array = fd_array_new();
            atomic_fd_array expnull = NULL;
            if (!atomic_compare_exchange_strong(fd_array_ptr, &expnull, new_fd_array))
            {
                free(new_fd_array);
            }
        }
        for (int i = 0; i < L2FDARRAYSIZE; i++)
        {
            int ret = file_struct_try_alloc_fd(self, fd_array_index, i);
            if (ret != -1)
            {
                return ret;
            }
        }
    }
    return -1;
}

void file_struct_free_fd_slot(struct file_struct *self, int fd)
{
    if (fd < 0 || fd >= MAXFDSIZE)
    {
        return;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    atomic_fd_array fd_array = *get_fd_array_ptr(self, L1Index);
    if (fd_array == NULL)
    {
        return;
    }
    int fileset_idx = L2Index >> 6;
    uint64_t idxbit = 1ULL << (L2Index & 63);
    while(1)
    {
        file_struct_bitset_t exp_bitset;
        file_struct_bitset_t set_bitset;
        exp_bitset = fd_array->fileset[fileset_idx].bitset;
        set_bitset = exp_bitset & (~idxbit);
        if (exp_bitset == set_bitset)
        {
            return;
        }
        if (atomic_compare_exchange_weak(&fd_array->fileset[fileset_idx].bitset, &exp_bitset, set_bitset))
        {
            file_struct_decre_fd_array_count(self, L1Index);
            return;
        }
    }
}

struct file **file_struct_access_fd_slot(struct file_struct *self, int fd)
{
    if (fd < 0 || fd >= MAXFDSIZE)
    {
        return NULL;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    atomic_fd_array *tmp = get_fd_array_ptr(self, L1Index);
    if (*tmp == NULL)
    {
        return NULL;
    }
    return get_file_ptr(*tmp, L2Index);
}

void file_struct_destroy(struct file_struct *self)
{
    if (self)
    {
        for (int i = 0; i < L1FDARRAYSIZE; i++)
        {
            for (int j = 0; j < 64; j++)
            {
                fd_array_destory(self->fd_index_array[i].fd_array[j]);
            }
        }
    }
    free(self);
}
