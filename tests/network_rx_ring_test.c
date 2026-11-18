#include "network_rx_ring.h"
#include <assert.h>
#include <limits.h>

int main(void)
{
    NetworkRxRing ring = {0};
    atomic_store(&ring.head, UINT_MAX - 15U);
    atomic_store(&ring.tail, UINT_MAX - 15U);
    NetworkRxFrame frame = {0};
    assert(!NetworkRx_Pop(&ring, &frame));

    for (unsigned i = 0; i < NETWORK_RX_DEPTH; ++i)
    {
        frame.id = i;
        frame.data[0] = (uint8_t)i;
        assert(NetworkRx_Push(&ring, &frame));
    }
    unsigned tail = atomic_load(&ring.tail);
    assert(!NetworkRx_Push(&ring, &frame));
    assert(atomic_load(&ring.tail) == tail && atomic_load(&ring.dropped) == 1);

    for (unsigned i = 0; i < 2 * NETWORK_RX_DEPTH; ++i)
    {
        assert(NetworkRx_Pop(&ring, &frame));
        assert(frame.id == i && frame.data[0] == i);
        if (i < NETWORK_RX_DEPTH)
        {
            frame.id = NETWORK_RX_DEPTH + i;
            frame.data[0] = (uint8_t)frame.id;
            assert(NetworkRx_Push(&ring, &frame));
        }
    }
    assert(!NetworkRx_Pop(&ring, &frame));
}
