#include "file_struct.h"

struct fileset {
	uint64_t bitset;
	struct file *files[64];
};

struct fd_array {
	struct fileset fileset[ROUND_DOWN(L1FDSIZE, 64)]
};

static struct fd_array** find_fd_array(struct file_struct* self, int L1Index) {
	return &self->fd_index_array[L1Index >> 6].fd_array[L1Index & 63];
}

static struct file** find_file(struct fd_array* self, int L2Index) {
	return &self->fileset[L2Index >> 6].files[L2Index & 63];
}

struct file **file_struct_access_fd_slot(struct file_struct *self, int fd)
{
	if (fd < 0 || fd >= MAXFDSIZE) {
		return NULL;
	}
	int L1Index = fd / L2FDSIZE;
	int L2Index = fd % L2FDSIZE;
	struct fd_array** tmp = find_fd_array(self, L1Index);
	if (tmp == NULL) {
		return NULL;
	}
	return find_file(tmp, L2Index);
}