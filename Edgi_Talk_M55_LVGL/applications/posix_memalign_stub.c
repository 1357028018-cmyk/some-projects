#include <rtthread.h>
#include <stdlib.h>

int posix_memalign(void **memptr, size_t align, size_t size)
{
    void *ptr = rt_malloc_align(size, align);
    if (ptr)
    {
        *memptr = ptr;
        return 0;
    }
    return 12;
}
