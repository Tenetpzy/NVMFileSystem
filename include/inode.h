#include "Common.h"

struct inode;

//根据ino获取struct inode
struct inode *inode_get(ino_t ino);

//释放struct inode ,当inode被所有持有者释放后可以被安全free
void inode_release(struct inode *self);


//获取文件具体信息，现略
