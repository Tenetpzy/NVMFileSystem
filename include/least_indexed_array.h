#include "Util.h"

typedef int lia_handle;

enum least_indexed_array_option { LIAOP_NONE = 0, LIAOP_CHECK = 1 << 0 };

struct least_indexed_array;

//总可用大小为L1_max_num * L2_max_num，第二级动态分配。
struct least_indexed_array *least_indexed_array_new(int L1_max_num, int L2_max_num);

void least_indexed_array_delete(struct least_indexed_array *lia);

// lia_handle 为数值类型。
//保证返回的handle是当前最小且未被占用的非负整数。可用此handle访问容器。线程安全
lia_handle least_indexed_array_alloc(struct least_indexed_array *lia);

//释放指定位置分配的handle，会被用于重新分配。不销毁此handle指向的位置的对象，需用户完成。线程安全。
//可指定是否进行检查（例如对没有分配却要求free的格子等）:LIA_CHECK
//成功返回1。失败返回0
int _least_indexed_array_free(struct least_indexed_array *lia, lia_handle handle);

int _least_indexed_array_free_with_option(struct least_indexed_array *lia, lia_handle handle, int option);

/// void least_indexed_array_free(struct least_indexed_array *lia, lia_handle handle, int option = 0);
#define least_indexed_array_free(lia, handle, ...)                                                    \
    OVERLOAD_0_1_ARG(_least_indexed_array_free_with_option, _least_indexed_array_free, ##__VA_ARGS__) \
    (lia, handle, ##__VA_ARGS__)

//访问handle对应位置容器。可以指定是否进行界限检查：LIA_CHECK
void **_least_indexed_array_access(struct least_indexed_array *lia, lia_handle handle);

void **_least_indexed_array_access_with_option(struct least_indexed_array *lia, lia_handle handle, int option);

/// void **_least_indexed_array_access(struct least_indexed_array *lia, lia_handle handle, int option = 0);
#define least_indexed_array_access(lia, handle, ...)                                                      \
    OVERLOAD_0_1_ARG(_least_indexed_array_access_with_option, _least_indexed_array_access, ##__VA_ARGS__) \
    (lia, handle, ##__VA_ARGS__)

//检查handle对应位置是否已经被分配出去了。可指定界限检查：LIA_CHECK
int _least_indexed_array_is_free(struct least_indexed_array *lia, lia_handle handle);

int _least_indexed_array_is_free_with_option(struct least_indexed_array *lia, lia_handle handle, int option);

#define least_indexed_array_is_free(lia, handle, ...)                                                       \
    OVERLOAD_0_1_ARG(_least_indexed_array_is_free_with_option, _least_indexed_array_is_free, ##__VA_ARGS__) \
    (lia, handle, ##__VA_ARGS__)

#ifdef LIA_DEBUG
// debug用。对lia进行一致性检查
void least_indexed_array_consistency_check(struct least_indexed_array *lia);
#endif