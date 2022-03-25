#include "Common.h"

struct dentry;

struct file;

//新建file，以给定mode打开
struct file *file_new(ino_t ino, mode_t mode);

//获取对应dentry
struct dentry *file_get_dentry(struct file *self);

//获取对应mode
mode_t file_get_mode(struct file *self);

//获取对应offset
off_t file_get_offset(struct file *self);

//设置offset
void file_set_offset(struct file *self, off_t noff);