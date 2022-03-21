#include "hash_table.h"
#include <stdint.h>

struct hash_bucket {
	struct hash_bucket_node *head;
};

typedef uint32_t bitset_t;
#pragma pack(CACHE_LINE_SIZE)
struct hash_bucket_node {
	void *value[ENTRY_NUMBER];
	struct HashBucketNode *next;
	bitset_t bitset;
};
#pragma pack()