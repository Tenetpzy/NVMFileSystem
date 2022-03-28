#pragma once

#include "Common.h"
#include "spin_lock.h"

struct inode;

struct dentry_list_node;
struct dentry_list {
    struct dentry_list_node *head;
    spinlock_t lock;
};

struct dentry_hash_key {
    ino_t fa_ino;
    const char *name;
};

#define DENTRY_NAME_MAX_LEN 128

struct dentry {
    unsigned long count;  // dentry引用计数
    spinlock_t lock;      // 保护dentry的自旋锁

    struct inode *d_inode;  // 关联到的索引节点
    ino_t d_ino;            // 关联索引节点号

    struct dentry *father;           // 父目录项
    struct dentry_list sub_list;     // 子目录项链表
    struct dentry_list *fa_sub_list;  // 父目录项的子目录项链表

    struct dentry_hash_key hash_key;
    unsigned long hash_pos_ptr;  // hash表的反向索引，指向hash表中存储它的位置

    char name[DENTRY_NAME_MAX_LEN];
};

void dentry_inc_count(struct dentry*);
void dentry_dec_count(struct dentry*);

int dentry_insert_hash_table(struct dentry*);