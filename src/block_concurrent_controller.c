#include "block_concurrent_controller.h"
#include <stdlib.h>

/* block_concurrent_controller state micro */
/* state result type: uint16_t */

#define READER_OCCUPIED 0x01U
#define READER_FULL 0x02U
#define HAS_PENDING_WRITER 0x04U
#define PENDING_WRITER_FULL 0x08U
#define WRITER_OCCUPIED 0x10U
#define PENDING_READER_FULL 0x20U


static inline int is_in_state(uint16_t state_result, uint16_t state_type)
{
    return state_result & state_type;
}

/* ************************************************** */

block_concurrent_controller* alloc_block_concurrent_controller_sleep()
{
    return (block_concurrent_controller*)malloc(sizeof(block_concurrent_controller_using_sleep));
}

void free_block_concurrent_controller_sleep(block_concurrent_controller* self)
{
    free(self);
}

int block_concurrent_controller_init_sleep(block_concurrent_controller* self)
{
    block_concurrent_controller_using_sleep *p = (block_concurrent_controller_using_sleep*)self;
    int res[3] = {0, 0, 0};

    p->state = 0;
    res[0] = pthread_mutex_init(&p->state_lock, NULL);
    res[1] = pthread_cond_init(&p->writer_pending_queue, NULL);
    res[2] = pthread_cond_init(&p->reader_pending_queue, NULL);
    
    for (int i = 0; i < 3; ++i)
        if (res[i] != 0)
            return res[i];
    return 0;
}

/* block_concurrent_controller_using_sleep */
/* ******************************************************** */



/* ******************************************************** */
/* block_concurrent_controller_using_spin */

static inline int get_reader_number_spin(uint8_t state)
{
    return state & 63U;
}

static inline int is_writer_pending_spin(uint8_t state)
{
    return state & (1U << 6);
}

static inline int is_writer_occupied_spin(uint8_t state)
{
    return state & (1U << 7);
}

static uint16_t decode_state_spin(uint8_t state)
{
    uint16_t state_result = 0;

    if (is_writer_occupied_spin(state))
        state_result |= WRITER_OCCUPIED;
    else
    {
        int reader = get_reader_number_spin(state);
        if (reader > 0)
        {
            state_result |= READER_OCCUPIED;
            if (reader == 63)
                state_result |= READER_FULL;
        }
    }

    if (is_writer_pending_spin(state))
        state_result |= HAS_PENDING_WRITER;

    return state_result;
}

block_concurrent_controller* alloc_block_concurrent_controller_spin()
{
    return (block_concurrent_controller*)malloc(sizeof(block_concurrent_controller_using_spin));
}

void free_block_concurrent_controller_spin(block_concurrent_controller* self)
{
    free(self);
}

int block_concurrent_controller_init_spin(block_concurrent_controller* self)
{
    ((block_concurrent_controller_using_spin*)self)->state = 0;
}

void writer_enter_spin(block_concurrent_controller* self)
{
    block_concurrent_controller_using_spin *p = (block_concurrent_controller_using_spin*)self;

    while (1)
    {
        uint16_t state = p->state;
        uint16_t state_result = decode_state_spin(state);

        if (is_in_state(state_result, WRITER_OCCUPIED))
        {
            
        }
    }
}