#ifndef BLOCKINGPRIORITYQUEUE_H
#define BLOCKINGPRIORITYQUEUE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>
#include <functional>

template <typename T,
          typename Container = std::vector<T>,
          typename Compare = std::less<typename Container::value_type>>
class BlockingPriorityQueue {
  public:
    explicit BlockingPriorityQueue(const size_t capacity) : m_capacity(capacity) {
    }

    // Enqueue with timeout. Returns true on success, false on timeout.
    bool enqueue(T &&item, std::chrono::milliseconds max_wait) {
        std::unique_lock lock(m_mutex);
        if (!m_cond_not_full.wait_for(lock, max_wait,
                                      [this]() { return m_queue.size() < m_capacity; })) {
            return false;
        }

        m_queue.push(std::move(item));
        m_cond_not_empty.notify_one();
        return true;
    }

    // Dequeue with timeout. Returns optional<T> (empty on timeout).
    std::optional<T> dequeue(std::chrono::milliseconds max_wait) {
        std::unique_lock lock(m_mutex);
        if (!m_cond_not_empty.wait_for(lock, max_wait, [this]() { return !m_queue.empty(); })) {
            return std::nullopt;
        }

        T item = std::move(m_queue.top());
        m_queue.pop();
        m_cond_not_full.notify_one();
        return item;
    }

  private:
    std::priority_queue<T, Container, Compare> m_queue;
    size_t m_capacity;
    std::mutex m_mutex;
    std::condition_variable m_cond_not_empty;
    std::condition_variable m_cond_not_full;
};

#endif // BLOCKINGPRIORITYQUEUE_H
