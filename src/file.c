#include "file.h"

struct file {
    struct dentry *dentry;
    off_t offset;
    mode_t mode;
};

// TODO
// struct file *file_new(ino_t ino, mode_t mode);

// TODO
// struct dentry *file_get_dentry(struct file *self);

mode_t file_get_mode(struct file *self) { return self->mode; }

off_t file_get_offset(struct file *self) { return self->offset; }

void file_set_offset(struct file *self, off_t noff) { self->offset = noff; }