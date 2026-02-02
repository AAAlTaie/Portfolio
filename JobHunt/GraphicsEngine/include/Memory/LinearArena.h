#pragma once
#include <vector>
#include <cstdint>

namespace GraphicsEngine {

class LinearArena {
    std::vector<uint8_t> buffer;
    size_t cursor = 0;

public:
    explicit LinearArena(size_t capacity = 0) {
        buffer.resize(capacity);
    }

    void Reset() {
        cursor = 0;
    }

    void* Alloc(size_t size, size_t alignment = 16) {
        size_t aligned = (cursor + (alignment - 1)) & ~(alignment - 1);
        if (aligned + size > buffer.size()) {
            buffer.resize(aligned + size);
        }
        void* ptr = buffer.data() + aligned;
        cursor = aligned + size;
        return ptr;
    }

    template<typename T>
    T* Push(const T& value) {
        T* ptr = (T*)Alloc(sizeof(T), alignof(T));
        *ptr = value;
        return ptr;
    }

    size_t GetUsed() const { return cursor; }
    size_t GetCapacity() const { return buffer.size(); }
};

}
