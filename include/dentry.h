#pragma once

#include "Common.h"
#include "spin_lock.h"
#include "stdatomic.h"

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
    atomic_llong ref_count;  // dentry引用计数

    struct inode *d_inode;  // 关联到的索引节点
    ino_t d_ino;            // 关联索引节点号

    struct dentry *father;           // 父目录项
    struct dentry_list sub_list;     // 子目录项链表
    struct dentry_list *fa_sub_list;  // 父目录项的子目录项链表

    struct dentry_hash_key hash_key;

    char name[DENTRY_NAME_MAX_LEN];
};

void dentry_inc_count(struct dentry*);
void dentry_dec_count(struct dentry*);

struct dentry* alloc_dentry();
void free_dentry(struct dentry* dt);

int dentry_hash_table_init();
int dentry_hash_table_insert(struct dentry*);
struct dentry* dentry_hash_table_lookup(struct dentry_hash_key*);
int dentry_hash_table_remove(struct dentry*);