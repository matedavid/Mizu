#pragma once

#include <array>
#include <concepts>
#include <initializer_list>
#include <span>
#include <type_traits>

#include "base/debug/assert.h"
#include "mizu_base_module.h"

namespace Mizu
{

template <typename T, size_t Capacity>
class inplace_vector
{
  public:
    template <typename... Args>
    constexpr inplace_vector(Args&&... values) : m_data({})
                                               , m_size(0)
    {
        static_assert(sizeof...(Args) <= Capacity, "Initializer list exceeds capacity");
        static_assert(
            (std::is_same_v<T, std::decay_t<Args>> && ...),
            "All arguments must be of the exact same type as the one declared in the container");

        ((m_data[m_size++] = std::forward<Args>(values)), ...);
    }

    constexpr inplace_vector(const inplace_vector<T, Capacity>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;
    }

    constexpr inplace_vector<T, Capacity>& operator=(const inplace_vector<T, Capacity>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;

        return *this;
    }

    constexpr inplace_vector(inplace_vector<T, Capacity>&& other)
    {
        m_data = std::move(other.m_data);
        m_size = other.size();

        other.m_size = 0;
    }

    constexpr size_t size() const { return m_size; }
    constexpr size_t capacity() const { return Capacity; }

    constexpr void push_back(T value)
    {
        MIZU_ASSERT(m_size + 1 <= Capacity, "Exceeding capacity ({} > {})", m_size + 1, Capacity);
        m_data[m_size++] = std::move(value);
    }

    constexpr const T& operator[](size_t index) const
    {
        MIZU_ASSERT(index < m_size, "Index out of bounds ({} >= {})", index, m_size);
        return m_data[index];
    }

    constexpr T& operator[](size_t index)
    {
        MIZU_ASSERT(index < m_size, "Index out of bounds ({} >= {})", index, m_size);
        return m_data[index];
    }

    constexpr T& emplace_back()
    {
        MIZU_ASSERT(m_size + 1 <= Capacity, "Exceeding capacity ({} > {})", m_size + 1, Capacity);
        m_data[m_size] = T{};
        return m_data[m_size++];
    }

    template <typename... Args>
    constexpr T& emplace_back(Args&&... args)
    {
        MIZU_ASSERT(m_size + 1 <= Capacity, "Exceeding capacity ({} > {})", m_size + 1, Capacity);
        m_data[m_size] = T(std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    T* data() { return m_data.data(); }
    const T* data() const { return m_data.data(); }

    bool is_empty() const { return m_size == 0; }

    template <typename IteratorType>
    struct ContainerIterator
    {
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = IteratorType;
        using pointer = IteratorType*;
        using reference = IteratorType&;

        ContainerIterator() : m_ptr(nullptr) {}
        ContainerIterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }

        ContainerIterator& operator++()
        {
            m_ptr++;
            return *this;
        }

        ContainerIterator operator++(int)
        {
            ContainerIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        ContainerIterator& operator--()
        {
            m_ptr--;
            return *this;
        }

        ContainerIterator operator--(int)
        {
            ContainerIterator tmp = *this;
            --(*this);
            return tmp;
        }

        ContainerIterator& operator+=(difference_type n)
        {
            m_ptr += n;
            return *this;
        }

        ContainerIterator& operator-=(difference_type n)
        {
            m_ptr -= n;
            return *this;
        }

        friend ContainerIterator operator+(ContainerIterator a, difference_type n) { return a += n; }
        friend ContainerIterator operator+(difference_type n, ContainerIterator a) { return a += n; }
        friend ContainerIterator operator-(ContainerIterator a, difference_type n) { return a -= n; }

        friend difference_type operator-(const ContainerIterator& a, const ContainerIterator& b)
        {
            return a.m_ptr - b.m_ptr;
        }

        // Comparison
        reference operator[](difference_type n) const { return m_ptr[n]; }

        friend bool operator==(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr == b.m_ptr; }
        friend bool operator!=(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr != b.m_ptr; }
        friend bool operator<(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr < b.m_ptr; }
        friend bool operator>(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr > b.m_ptr; }
        friend bool operator<=(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr <= b.m_ptr; }
        friend bool operator>=(const ContainerIterator& a, const ContainerIterator& b) { return a.m_ptr >= b.m_ptr; }

      private:
        pointer m_ptr{nullptr};
    };

    using Iterator = ContainerIterator<T>;
    using ConstIterator = ContainerIterator<const T>;

    Iterator begin() { return Iterator(m_data.data()); }
    Iterator end() { return Iterator(std::next(m_data.data(), m_size)); }

    ConstIterator begin() const { return ConstIterator(m_data.data()); }
    ConstIterator end() const { return ConstIterator(std::next(m_data.data(), m_size)); }

    ConstIterator cbegin() const { return ConstIterator(m_data.data()); }
    ConstIterator cend() const { return ConstIterator(std::next(m_data.data(), m_size)); }

    constexpr operator std::span<T>() { return std::span(m_data.data(), m_size); }
    constexpr operator std::span<const T>() const { return std::span(m_data.data(), m_size); }

    constexpr std::span<T> as_span() { return *this; }
    constexpr std::span<const T> as_span() const { return *this; }

  private:
    std::array<T, Capacity> m_data{};
    size_t m_size = 0;
};

} // namespace Mizu