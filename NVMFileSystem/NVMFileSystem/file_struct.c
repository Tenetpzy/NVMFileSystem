#include "file_struct.h"

struct fileset {
	uint64_t bitset;
	struct file *files[64];
};

struct fd_array {
	struct fileset fileset[ROUND_DOWN(L1FDSIZE, 64)]
};



struct file *file_struct_access_fd_slot(struct file_struct *self, int fd)
{
	int L1Index = fd / L2FDSIZE;
	int L2Index = fd % L2FDSIZE;
	return self->fd_index_array[L1Index / 64].fd_array[L1Index % 64]->fileset[L2Index / 64].files[L2Index % 64];
}