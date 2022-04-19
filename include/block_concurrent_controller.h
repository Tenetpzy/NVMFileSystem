#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct block_concurrent_controller block_concurrent_controller;

typedef struct block_concurrent_controller_interface
{
    block_concurrent_controller* (*alloc_block_concurrent_controller)();
    void (*free_block_concurrent_controller)(block_concurrent_controller*);

    int (*block_concurrent_controller_init)(block_concurrent_controller*);

    void (*writer_enter)(block_concurrent_controller*);
    void (*writer_exit)(block_concurrent_controller*);
    void (*reader_enter)(block_concurrent_controller*);
    void (*reader_exit)(block_concurrent_controller*);

}block_concurrent_controller_interface;

// 在可能较长时间无法进行读写时睡眠
typedef struct block_concurrent_controller_using_sleep
{
    /* 块内并发状态，允许同时存在多个读者，单个写者
     * 15: 写者独占位，当前有活跃的写者则置1
     * [10:14]: 读者计数，当前活跃的读者数量
     * [5:9]: 写者挂起计数，挂起等待当前块的写者数量
     * [0:4]: 读者挂起计数，挂起等待当前块的读者数量
    */
    uint16_t state;
    pthread_mutex_t state_lock;
    pthread_cond_t reader_pending_queue;
    pthread_cond_t writer_pending_queue;
}block_concurrent_controller_using_sleep;

// 在无法进行读写时始终自旋等待
typedef struct block_concurrent_controller_using_spin
{
    /*
     * 7: 写者独占位，当前有活跃的写者则置1
     * 6: 写者等待位，作为调度的建议。若为1，一定有写者在自旋等待；若为0，可能还有写者在自旋等待
     * [0:5]: 读者计数，当前正在活跃的读者数量(max 64)
    */
    atomic_uchar state;
}block_concurrent_controller_using_spin;



block_concurrent_controller* alloc_block_concurrent_controller_sleep();
void free_block_concurrent_controller_sleep(block_concurrent_controller*);

int block_concurrent_controller_init_sleep(block_concurrent_controller*);

void writer_enter_sleep(block_concurrent_controller*);
void writer_exit_sleep(block_concurrent_controller*);
void reader_enter_sleep(block_concurrent_controller*);
void reader_exit_sleep(block_concurrent_controller*);



block_concurrent_controller* alloc_block_concurrent_controller_spin();
void free_block_concurrent_controller_spin(block_concurrent_controller*);

int block_concurrent_controller_init_spin(block_concurrent_controller*);

void writer_enter_spin(block_concurrent_controller*);
void writer_exit_spin(block_concurrent_controller*);
void reader_enter_spin(block_concurrent_controller*);
void reader_exit_spin(block_concurrent_controller*);