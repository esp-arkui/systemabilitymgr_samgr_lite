/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "thread_adapter.h"
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


MutexId MUTEX_InitValue()
{
    return xSemaphoreCreateMutex();  // FreeRTOS 提供的互斥锁创建函数
}

void MUTEX_Lock(MutexId mutex)
{
    if (mutex == NULL) {
        return;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);  // 无限等待获取锁
}

void MUTEX_Unlock(MutexId mutex)
{
    if (mutex == NULL) {
        return;
    }
    xSemaphoreGive(mutex);  // 释放锁
}

void MUTEX_GlobalLock(void)
{
    vTaskSuspendAll();  // FreeRTOS 全局调度锁
}

void MUTEX_GlobalUnlock(void)
{
    xTaskResumeAll();  // 释放全局调度锁
}

ThreadId THREAD_Create(Runnable run, void *argv, const ThreadAttr *attr)
{
    TaskHandle_t handle = NULL;
    BaseType_t result = xTaskCreate(
        (TaskFunction_t)run,      // 任务函数
        attr->name,               // 任务名称
        attr->stackSize / 4,      // 栈大小（单位：字）
        argv,                     // 传递的参数
        attr->priority,           // 优先级
        &handle                   // 输出的任务句柄
    );
    return (result == pdPASS) ? handle : NULL;  // 返回任务句柄或 NULL
}

int THREAD_Total(void)
{
    return uxTaskGetNumberOfTasks();  // 返回当前运行任务总数
}

void *THREAD_GetThreadLocal(void)
{
    // FreeRTOS 没有直接提供线程局部存储，但可以通过 TaskLocalStoragePointer 实现
    return pvTaskGetThreadLocalStoragePointer(NULL, 0);
}

void THREAD_SetThreadLocal(const void *local)
{
    // 使用 TaskLocalStoragePointer 设置线程局部存储
    vTaskSetThreadLocalStoragePointer(NULL, 0, (void *)local);
}
