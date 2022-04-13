#include "file_struct.h"

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

typedef _Atomic(uint64_t) file_struct_bitset_t;
typedef _Atomic(struct fd_array *) atomic_fd_array;

struct file;

struct fileset {
    file_struct_bitset_t bitset;
    struct file *files[64];
};

struct fd_array {
    struct fileset fileset[L2FDARRAYSIZE];
    atomic_int_fast32_t counter;
};
struct fd_index_array {
    file_struct_bitset_t bitset;
    atomic_fd_array fd_array[64];
};

struct file_struct {
    struct fd_index_array fd_index_array[L1FDARRAYSIZE];
};

#pragma region PRIVATE

static atomic_fd_array *get_fd_array_ptr(struct file_struct *self, int L1Index) {
    return &self->fd_index_array[L1Index >> 6].fd_array[L1Index & 63];
}

static struct file **get_file_ptr(struct fd_array *self, int L2Index) {
    return &self->fileset[L2Index >> 6].files[L2Index & 63];
}

static int fd_array_is_fd_slot_empty(struct fd_array *self, int L2Index) {
    return !(atomic_load(&self->fileset[L2Index >> 6].bitset) & (1ULL << (L2Index & 63)));
}

static int find_available_fd_array_index(struct file_struct *self) {
    struct fd_index_array *it;
    for (int i = 0; i < L1FDARRAYSIZE; i++) {
        it = &self->fd_index_array[i];
        file_struct_bitset_t bitset = it->bitset;
        if (bitset == UINT64_MAX) {
            // no empty slot
            continue;
        }
        int firstempty = RIGHT_MOST_ZERO_LL(bitset);
        return firstempty + (i << 6);
    }
    return -1;
}

static int fd_array_isfull(struct fd_array *self) {
    // assert(self->counter >= 0);
    // assert(self->counter <= L2FDSIZE);
    return atomic_load(&self->counter) >= L2FDSIZE;
}

//检查是否需要将某二级页表在一级页表中表示为已满
static void file_struct_set_L1_bit(struct file_struct *self, int fd_array_idx) {
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    int idx = fd_array_idx >> 6;
    uint64_t idxbit = 1ULL << (fd_array_idx & 63);
    //弱保证：如果fd_array满，一般其bit会set，这里不做循环
    atomic_fetch_or(&self->fd_index_array[idx].bitset, idxbit);
    //强保证：如果fd_array没有满，其bit一定unset
    //再次测试，没有满，去掉其bit
    if (!fd_array_isfull(fd_array)) {
        file_struct_bitset_t bitset = self->fd_index_array[idx].bitset;
        if ((bitset & idxbit) != 0) {
            atomic_fetch_and(&self->fd_index_array[idx].bitset, ~idxbit);
        }
    }
}

static void file_struct_incre_fd_array_count(struct file_struct *self, int fd_array_idx) {
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    atomic_fetch_add(&fd_array->counter, 1);
    // int_fast32_t counter = atomic_load(&fd_array->counter);
    // assert(counter <= L2FDSIZE);
    if (fd_array_isfull(fd_array)) {
        file_struct_set_L1_bit(self, fd_array_idx);
    }
}

static void file_struct_decre_fd_array_count(struct file_struct *self, int fd_array_idx) {
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    atomic_fetch_sub(&fd_array->counter, 1);
    // int_fast32_t counter = atomic_load(&fd_array->counter);
    // assert(counter >= 0);
    int idx = fd_array_idx >> 6;
    uint64_t idxbit = 1ULL << (fd_array_idx & 63);

    //去除bit
    atomic_fetch_and(&self->fd_index_array[idx].bitset, ~idxbit);
}

static int file_struct_try_alloc_fd(struct file_struct *self, int fd_array_idx, int fileset_idx) {
    struct fd_array *fd_array = *get_fd_array_ptr(self, fd_array_idx);
    struct fileset *fileset = &fd_array->fileset[fileset_idx];
    while (1) {
        file_struct_bitset_t bitset = fileset->bitset;
        if (bitset == UINT64_MAX) {
            //满了，下一个
            file_struct_set_L1_bit(self, fd_array_idx);
            return -1;
        }
        int firstempty = RIGHT_MOST_ZERO_LL(bitset);
        file_struct_bitset_t set_bitset = bitset | (1ULL << firstempty);
        if (atomic_compare_exchange_weak(&fileset->bitset, &bitset, set_bitset)) {
            file_struct_incre_fd_array_count(self, fd_array_idx);
            return fd_array_idx * L2FDSIZE + (fileset_idx << 6) + firstempty;
        } else {
            if (fd_array_isfull(fd_array)) {
                file_struct_set_L1_bit(self, fd_array_idx);
            }
        }
    }
}

static atomic_fd_array fd_array_new() {
    atomic_fd_array ret = malloc(sizeof(struct fd_array));
    for (int i = 0; i < L2FDARRAYSIZE; i++) {
        for (int j = 0; j < 64; j++) {
            ret->fileset[i].files[j] = NULL;
        }
        ret->fileset[i].bitset = 0;
    }
    ret->counter = 0;
    return ret;
}

static void fd_array_delete(struct fd_array *self) {
    //这里没有释放所有资源，因为file 还没有定义
    if (self) {
        // TODO destory file*
    }
    free(self);
}

#pragma endregion END PRIVATE

struct file_struct *file_struct_new() {
    struct file_struct *self = malloc(sizeof(struct file_struct));
    for (int i = 0; i < L1FDARRAYSIZE; i++) {
        for (int j = 0; j < 64; j++) {
            self->fd_index_array[i].fd_array[j] = NULL;
        }
        self->fd_index_array[i].bitset = 0;
    }
    return self;
}

int file_struct_alloc_fd_slot(struct file_struct *self) {
    while (1) {
        int fd_array_index = find_available_fd_array_index(self);
        //没找到任何有机会的位置
        if (fd_array_index == -1) {
            return -1;
        }
        atomic_fd_array *fd_array_ptr = get_fd_array_ptr(self, fd_array_index);

        //如果是空位置，初始化一个
        if (*fd_array_ptr == NULL) {
            //初始化
            atomic_fd_array new_fd_array = fd_array_new();
            atomic_fd_array expnull = NULL;
            if (!atomic_compare_exchange_strong(fd_array_ptr, &expnull, new_fd_array)) {
                fd_array_delete(new_fd_array);
            }
        }
        for (int i = 0; i < L2FDARRAYSIZE; i++) {
            int ret = file_struct_try_alloc_fd(self, fd_array_index, i);
            if (ret != -1) {
                return ret;
            }
        }
    }
    return -1;
}

void file_struct_free_fd_slot(struct file_struct *self, int fd) {
    if (fd < 0 || fd >= MAXFDSIZE) {
        return;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    atomic_fd_array fd_array = *get_fd_array_ptr(self, L1Index);
    if (fd_array == NULL) {
        return;
    }
    int fileset_idx = L2Index >> 6;
    uint64_t idxbit = 1ULL << (L2Index & 63);
    atomic_fetch_and(&fd_array->fileset[fileset_idx].bitset, ~idxbit);
    file_struct_decre_fd_array_count(self, L1Index);
}

struct file **file_struct_access_fd_slot(struct file_struct *self, int fd) {
    if (fd < 0 || fd >= MAXFDSIZE) {
        return NULL;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    atomic_fd_array *tmp = get_fd_array_ptr(self, L1Index);
    if (*tmp == NULL) {
        return NULL;
    }
    return get_file_ptr(*tmp, L2Index);
}

int file_struct_is_fd_slot_empty(struct file_struct *self, int fd) {
    if (fd < 0 || fd >= MAXFDSIZE) {
        return 0;
    }
    int L1Index = fd / L2FDSIZE;
    int L2Index = fd % L2FDSIZE;
    atomic_fd_array *tmp = get_fd_array_ptr(self, L1Index);
    if (*tmp == NULL) {
        return 1;
    }
    return fd_array_is_fd_slot_empty(*tmp, L2Index);
}

void file_struct_consistency_validation(struct file_struct *self) {
    for (int i = 0; i < L1FDARRAYSIZE; i++) {
        struct fd_index_array *fd_index_array = &self->fd_index_array[i];
        for (int j = 0; j < 64; j++) {
            atomic_fd_array fd_array = fd_index_array->fd_array[j];
            if (fd_array == NULL) {
                continue;
            }
            int cnt = 0;
            for (int k = 0; k < L2FDARRAYSIZE; k++) {
                struct fileset *fileset = &fd_array->fileset[k];
                cnt += COUNT_ONE_LL(fileset->bitset);
            }
            assert(atomic_load(&fd_array->counter) == cnt);
            if (cnt != L2FDSIZE) {
                assert((atomic_load(&fd_index_array->bitset) & (1ULL << j)) == 0);
            }
        }
    }
}

void file_struct_delete(struct file_struct *self) {
    if (self) {
        for (int i = 0; i < L1FDARRAYSIZE; i++) {
            for (int j = 0; j < 64; j++) {
                fd_array_delete(self->fd_index_array[i].fd_array[j]);
            }
        }
    }
    free(self);
}
