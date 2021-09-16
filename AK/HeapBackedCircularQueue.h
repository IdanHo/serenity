/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/ByteBuffer.h>
#include <AK/Forward.h>
#include <AK/StdLibExtras.h>

namespace AK {

template<typename T>
class HeapBackedCircularQueue {
    friend class HeapBackedCircularDuplexStream;

public:
    HeapBackedCircularQueue(size_t capacity)
        : m_storage(ByteBuffer::create_uninitialized(sizeof(T) * capacity).release_value())
        , m_capacity(capacity)
    {
    }

    ~HeapBackedCircularQueue()
    {
        clear();
    }

    void clear()
    {
        for (size_t i = 0; i < m_size; ++i)
            elements()[(m_head + i) % m_capacity].~T();

        m_head = 0;
        m_size = 0;
    }

    bool is_empty() const { return !m_size; }
    size_t size() const { return m_size; }

    size_t capacity() const { return m_capacity; }

    template<typename U = T>
    void enqueue(U&& value)
    {
        auto& slot = elements()[(m_head + m_size) % m_capacity];
        if (m_size == m_capacity)
            slot.~T();

        new (&slot) T(forward<U>(value));
        if (m_size == m_capacity)
            m_head = (m_head + 1) % m_capacity;
        else
            ++m_size;
    }

    T dequeue()
    {
        VERIFY(!is_empty());
        auto& slot = elements()[m_head];
        T value = move(slot);
        slot.~T();
        m_head = (m_head + 1) % m_capacity;
        --m_size;
        return value;
    }

    const T& at(size_t index) const { return elements()[(m_head + index) % m_capacity]; }

    const T& first() const { return at(0); }
    const T& last() const { return at(size() - 1); }

    class ConstIterator {
    public:
        bool operator!=(const ConstIterator& other) { return m_index != other.m_index; }
        ConstIterator& operator++()
        {
            m_index = (m_index + 1) % m_queue.m_capacity;
            if (m_index == m_queue.m_head)
                m_index = m_queue.m_size;
            return *this;
        }

        const T& operator*() const { return m_queue.elements()[m_index]; }

    private:
        friend class HeapBackedCircularQueue;
        ConstIterator(const HeapBackedCircularQueue& queue, const size_t index)
            : m_queue(queue)
            , m_index(index)
        {
        }
        const HeapBackedCircularQueue& m_queue;
        size_t m_index { 0 };
    };

    ConstIterator begin() const { return ConstIterator(*this, m_head); }
    ConstIterator end() const { return ConstIterator(*this, size()); }

    size_t head_index() const { return m_head; }

protected:
    T* elements() { return reinterpret_cast<T*>(m_storage.data()); }
    const T* elements() const { return reinterpret_cast<const T*>(m_storage.data()); }

    friend class ConstIterator;
    ByteBuffer m_storage;
    size_t m_capacity { 0 };
    size_t m_size { 0 };
    size_t m_head { 0 };
};

}

using AK::HeapBackedCircularQueue;
