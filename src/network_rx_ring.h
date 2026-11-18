#ifndef NETWORK_RX_RING_H
#define NETWORK_RX_RING_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

enum { NETWORK_RX_DEPTH = 32, NETWORK_RX_PAYLOAD_MAX = 64 };

typedef struct {
    uint32_t id;
    uint8_t flags;
    uint8_t length;
    uint8_t data[NETWORK_RX_PAYLOAD_MAX];
} NetworkRxFrame;

typedef struct {
    NetworkRxFrame frames[NETWORK_RX_DEPTH];
    atomic_uint head;
    atomic_uint tail;
    atomic_uint dropped;
} NetworkRxRing;

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "RX interrupt requires lock-free atomics");

/* One producer per ring. The producer never advances the consumer's tail. */
static inline bool NetworkRx_Push(NetworkRxRing *ring, const NetworkRxFrame *frame)
{
    unsigned head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    unsigned tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    if (head - tail >= NETWORK_RX_DEPTH)
    {
        atomic_fetch_add_explicit(&ring->dropped, 1U, memory_order_relaxed);
        return false;
    }

    ring->frames[head % NETWORK_RX_DEPTH] = *frame;
    atomic_store_explicit(&ring->head, head + 1U, memory_order_release);
    return true;
}

static inline bool NetworkRx_Pop(NetworkRxRing *ring, NetworkRxFrame *frame)
{
    unsigned tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    unsigned head = atomic_load_explicit(&ring->head, memory_order_acquire);
    if (tail == head)
        return false;

    *frame = ring->frames[tail % NETWORK_RX_DEPTH];
    atomic_store_explicit(&ring->tail, tail + 1U, memory_order_release);
    return true;
}

#endif
