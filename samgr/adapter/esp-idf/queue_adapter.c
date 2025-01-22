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

#include "queue_adapter.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_err.h>
#include <ohos_types.h>
#include <ohos_errno.h>


MQueueId QUEUE_Create(const char *name, int size, int count)
{
    (void)name;  // ESP-IDF 的 FreeRTOS 队列不支持命名
    return xQueueCreate(count, size);
}

int QUEUE_Put(MQueueId queueId, const void *element, uint8_t pri, int timeout)
{
    if (queueId == NULL || element == NULL) {
        return EC_FAILURE;
    }

    BaseType_t ret;
    if (timeout <= 0) {
        ret = xQueueSend(queueId, element, 0);  // 非阻塞
    } else {
        ret = xQueueSend(queueId, element, pdMS_TO_TICKS(timeout));  // 指定超时时间
    }

    if (ret != pdPASS) {
        return EC_BUSBUSY;
    }
    return EC_SUCCESS;
}

int QUEUE_Pop(MQueueId queueId, void *element, uint8_t *pri, int timeout)
{
    if (queueId == NULL || element == NULL) {
        return EC_FAILURE;
    }
    (void)pri;  // FreeRTOS 队列不支持优先级，但保留参数以兼容接口
    BaseType_t ret;
    if (timeout <= 0) {
        ret = xQueueReceive(queueId, element, portMAX_DELAY);  // 阻塞模式
    } else {
        ret = xQueueReceive(queueId, element, pdMS_TO_TICKS(timeout));  // 指定超时时间
    }

    if (ret != pdPASS) {
        return EC_BUSBUSY;
    }
    return EC_SUCCESS;
}

int QUEUE_Destroy(MQueueId queueId)
{
    if (queueId == NULL) {
        return EC_FAILURE;
    }

    vQueueDelete(queueId);  // 释放队列资源
    return EC_SUCCESS;
}
