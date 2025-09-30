//
// Created by Johnathon Slightham on 2025-07-12.
//

#ifndef PTRQUEUE_H
#define PTRQUEUE_H

#include <memory>
#include <iostream>

// Wrapped FreeRTOS queue to support unique_ptr

template <typename T>
class PtrQueue {
public:
    explicit PtrQueue(const UBaseType_t queueLength)
        : queue(xQueueCreate(queueLength, sizeof(T*))) {}

    ~PtrQueue() {
        if (queue) {
            vQueueDelete(queue);
        }
    }

    bool enqueue(std::unique_ptr<T>&& item, const TickType_t timeout = portMAX_DELAY) {
        T* rawPtr = item.release();  // Release ownership and get raw pointer
        return xQueueSendToBack(queue, &rawPtr, timeout) == pdPASS;
    }

    std::unique_ptr<T> dequeue(const TickType_t timeout = portMAX_DELAY) {
        T* rawPtr = nullptr;
        if (xQueueReceive(queue, &rawPtr, timeout) == pdPASS) {
            std::unique_ptr<T> ptr(rawPtr);
            return ptr;
        }
        return nullptr;
    }

private:
    QueueHandle_t queue;
};

#endif //PTRQUEUE_H
