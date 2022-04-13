#include "least_indexed_array.h"
#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include "Common.h"
#include "Util.h"

#define INTERNAL_MAXSIZE 1000000000ULL
typedef uint64_t internal_bitset_t;
typedef int_fast32_t internal_counter_t;
typedef _Atomic(internal_bitset_t) bitset_t;
#define BITSET_BITNUM (sizeof(internal_bitset_t) * 8)
#define BITSET_FULL ((internal_bitset_t)((UINT64_MAX) >> (64 - BITSET_BITNUM)))
typedef _Atomic(internal_counter_t) counter_t;

struct LIA_L1_struct;
struct LIA_L2_struct;
struct LIA_L1_unit;
struct LIA_L2_unit;

typedef _Atomic(struct LIA_L2_struct *) LIA_L2_struct_atomic_ptr;

struct LIA_L1_unit {
    bitset_t bitset;
    LIA_L2_struct_atomic_ptr link[BITSET_BITNUM];
};
struct LIA_L2_unit {
    bitset_t bitset;
    void *data[BITSET_BITNUM];
};

struct LIA_L1_struct {
    struct LIA_L1_unit units[0];
};
struct LIA_L2_struct {
    counter_t counter;
    struct LIA_L2_unit units[0];
};

struct least_indexed_array {
    int L1_unit_num;
    int L2_unit_num;
    struct LIA_L1_struct *head;
};

static struct LIA_L1_struct *LIA_L1_struct_new(int L1_unit_num) {
    struct LIA_L1_struct *l1_struct = malloc(sizeof(struct LIA_L1_struct) + sizeof(struct LIA_L1_unit) * L1_unit_num);

    for (int i = 0; i < L1_unit_num; i++) {
        atomic_store(&l1_struct->units[i].bitset, 0);

        for (int j = 0; j < BITSET_BITNUM; j++) {
            atomic_store(&l1_struct->units[i].link[j], NULL);
        }
    }
    return l1_struct;
}

static struct LIA_L2_struct *LIA_L2_struct_new(int L2_unit_num) {
    struct LIA_L2_struct *l2_struct = malloc(sizeof(struct LIA_L2_struct) + sizeof(struct LIA_L2_unit) * L2_unit_num);

    atomic_store(&l2_struct->counter, 0);

    for (int i = 0; i < L2_unit_num; i++) {
        atomic_store(&l2_struct->units[i].bitset, 0);

        for (int j = 0; j < BITSET_BITNUM; j++) {
            l2_struct->units[i].data[j] = NULL;
        }
    }
    return l2_struct;
}

static void LIA_L2_struct_delete(struct LIA_L2_struct *l2_struct) { free(l2_struct); }

static void LIA_L1_struct_delete(int L1_unit_num, struct LIA_L1_struct *l1_struct) {
    if (l1_struct) {
        for (int i = 0; i < L1_unit_num; i++) {
            for (int j = 0; j < BITSET_BITNUM; j++) {
                LIA_L2_struct_delete(l1_struct->units[i].link[j]);
            }
        }
    }
    free(l1_struct);
}

static LIA_L2_struct_atomic_ptr *get_L2_struct_ref_by_L1_index(struct least_indexed_array *lia, int L1_index) {
    return &lia->head->units[L1_index / BITSET_BITNUM].link[L1_index % BITSET_BITNUM];
}

static int find_available_L1_index(struct least_indexed_array *lia) {
    for (int i = 0; i < lia->L1_unit_num; i++) {
        struct LIA_L1_unit *it = &lia->head->units[i];
        bitset_t bitset = atomic_load(&it->bitset);
        if (bitset == BITSET_FULL) {
            continue;
        }
        int first_empty = RIGHT_MOST_ZERO_LL(bitset);
        return first_empty + i * (int)BITSET_BITNUM;
    }
    return -1;
}

#define IS_L2_STRUCT_FULL(lia, L2_struct) \
    (atomic_load(&(L2_struct)->counter) >= (lia)->L2_unit_num * (int)BITSET_BITNUM)

static void least_indexed_array_set_L1_bit(struct least_indexed_array *lia, int L1_index) {
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1_index);
    int L1_unit_index = L1_index / BITSET_BITNUM;
    bitset_t index_bit = (internal_bitset_t)(1ULL << (L1_index % BITSET_BITNUM));
    //弱保证：如果fd_array满，一般其bit会set
    atomic_fetch_or(&lia->head->units[L1_unit_index].bitset, index_bit);
    //强保证：如果fd_array没有满，其bit一定unset
    //再次测试，没有满，去掉其bit
    if (!IS_L2_STRUCT_FULL(lia, L2_struct)) {
        atomic_fetch_and(&lia->head->units[L1_unit_index].bitset, ~index_bit);
    }
}

static void least_indexed_array_incre_L2_counter(struct least_indexed_array *lia, int L1_index) {
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1_index);
    counter_t prev_counter = atomic_fetch_add(&L2_struct->counter, 1);
    if (prev_counter + 1 >= lia->L2_unit_num * (int)BITSET_BITNUM) {
        least_indexed_array_set_L1_bit(lia, L1_index);
    }
}

static void least_indexed_array_decre_L2_counter(struct least_indexed_array *lia, int L1_index) {
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1_index);
    counter_t prev_counter = atomic_fetch_sub(&L2_struct->counter, 1);
    if (prev_counter >= lia->L2_unit_num * (int)BITSET_BITNUM) {
        int L1_unit_index = L1_index / BITSET_BITNUM;
        bitset_t index_bit = (internal_bitset_t)(1ULL << (L1_index % BITSET_BITNUM));
        atomic_fetch_and(&lia->head->units[L1_unit_index].bitset, ~index_bit);
    }
}

static int L2_unit_try_alloc_slot(struct LIA_L2_unit *l2_unit) {
    while (1) {
        bitset_t bitset = l2_unit->bitset;
        if (bitset == BITSET_FULL) {
            return -1;
        }
        int first_empty = RIGHT_MOST_ZERO_LL(bitset);
        bitset_t set_bitset = bitset | (1ULL << first_empty);
        if (atomic_compare_exchange_weak(&l2_unit->bitset, &bitset, set_bitset)) {
            return first_empty;
        }
    }
}

struct least_indexed_array *least_indexed_array_new(int L1_max_num, int L2_max_num) {
    if (L1_max_num <= 0 || L2_max_num <= 0) {
        return NULL;
    }
    int L1_unit_num = ROUND_UP(L1_max_num, BITSET_BITNUM);
    int L2_unit_num = ROUND_UP(L2_max_num, BITSET_BITNUM);
    //不能过大
    if (1ull * L1_unit_num * L2_unit_num * BITSET_BITNUM * BITSET_BITNUM > INTERNAL_MAXSIZE) {
        return NULL;
    }
    struct least_indexed_array *lia = malloc(sizeof(struct least_indexed_array));

    lia->L1_unit_num = L1_unit_num;
    lia->L2_unit_num = L2_unit_num;

    lia->head = LIA_L1_struct_new(lia->L1_unit_num);

    return lia;
}

void least_indexed_array_delete(struct least_indexed_array *lia) {
    LIA_L1_struct_delete(lia->L1_unit_num, lia->head);

    free(lia);
}

lia_handle least_indexed_array_alloc(struct least_indexed_array *lia) {
    while (1) {
        int L1_index = find_available_L1_index(lia);
        //没找到任何有机会的位置
        if (L1_index == -1) {
            return -1;
        }
        LIA_L2_struct_atomic_ptr *L2_struct_ref = get_L2_struct_ref_by_L1_index(lia, L1_index);
        //如果是空位置，初始化一个

        LIA_L2_struct_atomic_ptr L2_struct = atomic_load(L2_struct_ref);
        if (L2_struct == NULL) {
            LIA_L2_struct_atomic_ptr new_L2_struct = LIA_L2_struct_new(lia->L2_unit_num);
            LIA_L2_struct_atomic_ptr expnull = NULL;
            if (!atomic_compare_exchange_strong(L2_struct_ref, &expnull, new_L2_struct)) {
                LIA_L2_struct_delete(new_L2_struct);
            }
            L2_struct = atomic_load(L2_struct_ref);
        }

        for (int i = 0; i < lia->L2_unit_num; i++) {
            int L2_unit_slot = L2_unit_try_alloc_slot(&L2_struct->units[i]);
            if (L2_unit_slot == -1) {
                if (IS_L2_STRUCT_FULL(lia, L2_struct)) {
                    least_indexed_array_set_L1_bit(lia, L1_index);
                    break;
                }
                continue;
            }
            least_indexed_array_incre_L2_counter(lia, L1_index);
            return L2_unit_slot + i * BITSET_BITNUM + L1_index * (lia->L2_unit_num * BITSET_BITNUM);
        }
    }
}

int _least_indexed_array_free(struct least_indexed_array *lia, lia_handle handle) {
    int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
    int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
    int l2_unit_index = L2Index / BITSET_BITNUM;
    bitset_t index_mask = ~(internal_bitset_t)(1ULL << (L2Index % BITSET_BITNUM));
    atomic_fetch_and(&L2_struct->units[l2_unit_index].bitset, index_mask);
    least_indexed_array_decre_L2_counter(lia, L1Index);
    return 1;
}

int _least_indexed_array_free_with_option(struct least_indexed_array *lia, lia_handle handle, int option) {
    if (option & LIAOP_CHECK) {
        if (handle < 0 || handle >= lia->L1_unit_num * lia->L2_unit_num * BITSET_BITNUM * BITSET_BITNUM) {
            return 0;
        }
        int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
        int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
        LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
        if (L2_struct == NULL) {
            return 0;
        }
        int l2_unit_index = L2Index / BITSET_BITNUM;
        //之前没用也返回0
        if (!(atomic_load(&L2_struct->units[l2_unit_index].bitset) & (1ULL << (L2Index % BITSET_BITNUM)))) {
            return 0;
        }
        bitset_t index_mask = ~(internal_bitset_t)(1ULL << (L2Index % BITSET_BITNUM));
        atomic_fetch_and(&L2_struct->units[l2_unit_index].bitset, index_mask);
        least_indexed_array_decre_L2_counter(lia, L1Index);
        return 1;
    }
    return _least_indexed_array_free(lia, handle);
}

void **_least_indexed_array_access(struct least_indexed_array *lia, lia_handle handle) {
    int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
    int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
    return &L2_struct->units[L2Index / BITSET_BITNUM].data[L2Index % BITSET_BITNUM];
}
void **_least_indexed_array_access_with_option(struct least_indexed_array *lia, lia_handle handle, int option) {
    if (option & LIAOP_CHECK) {
        if (handle < 0 || handle >= lia->L1_unit_num * lia->L2_unit_num * BITSET_BITNUM * BITSET_BITNUM) {
            return NULL;
        }
        int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
        int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
        LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
        if (L2_struct == NULL) {
            return NULL;
        }
        //没分配也返回NULL
        if (!(atomic_load(&L2_struct->units[L2Index / BITSET_BITNUM].bitset) & (1ULL << (L2Index % BITSET_BITNUM)))) {
            return NULL;
        }
        return &L2_struct->units[L2Index / BITSET_BITNUM].data[L2Index % BITSET_BITNUM];
    }
    return _least_indexed_array_access(lia, handle);
}

int _least_indexed_array_is_free(struct least_indexed_array *lia, lia_handle handle) {
    int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
    int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
    LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
    if (L2_struct == NULL) {
        return 1;
    }
    return !(atomic_load(&L2_struct->units[L2Index / BITSET_BITNUM].bitset) & (1ULL << (L2Index % BITSET_BITNUM)));
}
int _least_indexed_array_is_free_with_option(struct least_indexed_array *lia, lia_handle handle, int option) {
    if (option & LIAOP_CHECK) {
        if (handle < 0 || handle >= lia->L1_unit_num * lia->L2_unit_num * BITSET_BITNUM * BITSET_BITNUM) {
            return 0;
        }
        int L1Index = handle / (lia->L2_unit_num * BITSET_BITNUM);
        int L2Index = handle % (lia->L2_unit_num * BITSET_BITNUM);
        LIA_L2_struct_atomic_ptr L2_struct = *get_L2_struct_ref_by_L1_index(lia, L1Index);
        if (L2_struct == NULL) {
            return 1;
        }
        return !(atomic_load(&L2_struct->units[L2Index / BITSET_BITNUM].bitset) & (1ULL << (L2Index % BITSET_BITNUM)));
    }
    return _least_indexed_array_is_free(lia, handle);
}

void least_indexed_array_consistency_check(struct least_indexed_array *lia) {
    for (int i = 0; i < lia->L1_unit_num; i++) {
        struct LIA_L1_unit *l1_unit = &lia->head->units[i];
        for (int j = 0; j < BITSET_BITNUM; j++) {
            LIA_L2_struct_atomic_ptr L2_struct_ptr = atomic_load(&l1_unit->link[j]);
            if (L2_struct_ptr == NULL) {
                continue;
            }
            int cnt = 0;
            for (int k = 0; k < lia->L2_unit_num; k++) {
                cnt += COUNT_ONE_LL(L2_struct_ptr->units[k].bitset);
            }
            assert(atomic_load(&L2_struct_ptr->counter) == cnt);
            if (cnt != lia->L2_unit_num * BITSET_BITNUM) {
                assert((atomic_load(&l1_unit->bitset) & (1ULL << j)) == 0);
            }
        }
    }
}