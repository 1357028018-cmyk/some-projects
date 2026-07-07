/**
 * \file mtb_ml_rtos_rtthread.c
 * \brief Empty stub implementations for MTB ML middleware RTOS functions
 *
 * These functions provide empty implementations to satisfy the CY_RTOS_AWARE
 * code path in the Ethos-U NPU driver. Since the NPU inference runs in a
 * single-threaded context, actual RTOS synchronization is not required.
 *
 * This approach avoids type mismatches between RT-Thread RTOS types and
 * the FreeRTOS types expected by cy_rtos_* functions.
 */

#include <rtthread.h>

/*
 * Empty stub implementations for mutex and semaphore.
 * Used by Ethos-U NPU driver when CY_RTOS_AWARE is defined.
 * These are no-op functions to avoid type mismatch issues.
 */

void *mtb_ml_mutex_create(void)
{
    return (void *)1;
}

int mtb_ml_mutex_lock(void *mutex)
{
    (void)mutex;
    return 0;
}

int mtb_ml_mutex_unlock(void *mutex)
{
    (void)mutex;
    return 0;
}

void mtb_ml_mutex_destroy(void *mutex)
{
    (void)mutex;
}

void *mtb_ml_sem_create(void)
{
    return (void *)1;
}

int mtb_ml_sem_take(void *sem, uint64_t timeout)
{
    (void)sem;
    (void)timeout;
    return 0;
}

void mtb_ml_sem_destroy(void *sem)
{
    (void)sem;
}
