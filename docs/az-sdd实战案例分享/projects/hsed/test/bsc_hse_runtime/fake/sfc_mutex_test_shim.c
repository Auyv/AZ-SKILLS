/**
 * @file        sfc_mutex_test_shim.c
 * @brief       Linux 主机测试使用的 SFC 临界区最小替身
 * @note        仅用于 x86 主机测试，不得进入生产目标
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "ofc/ofc_object.h"
#include "sfc/sfc_mutex.h"

#include <string.h>

static void L_DestroyCriticalSection(TOfObject *object)
{
    TSfCriticalSection *self = (TSfCriticalSection *)object;

    if (self != NULL)
    {
        (void)pthread_mutex_destroy(&self->section_);
        memset(self, 0, sizeof(*self));
    }
}

Bool TSfCriticalSectionCreate(TSfCriticalSection *self)
{
    Bool result = False;

    if (self != NULL)
    {
        memset(self, 0, sizeof(*self));
        if (pthread_mutex_init(&self->section_, NULL) == 0)
        {
            self->parent_.Destroy = L_DestroyCriticalSection;
            result = True;
        }
    }
    return result;
}

Bool TSfCriticalSectionEnter(TSfCriticalSection *self)
{
    Bool result = False;

    if (self != NULL && pthread_mutex_lock(&self->section_) == 0)
    {
        result = True;
    }
    return result;
}

void TSfCriticalSectionLeave(TSfCriticalSection *self)
{
    if (self != NULL)
    {
        (void)pthread_mutex_unlock(&self->section_);
    }
}

void OfDestroyObject(TOfObject *self)
{
    if (self != NULL && self->Destroy != NULL)
    {
        self->Destroy(self);
    }
}

