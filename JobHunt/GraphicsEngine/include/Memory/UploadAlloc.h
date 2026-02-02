// UploadAlloc.h - GOD LEVEL VERSION (2GB fixed, zero runtime allocations)
#pragma once
#include "../D3D12Helpers.h"
#include <cassert>
#include <vector>

namespace GraphicsEngine {

    class UploadAlloc {
    public:
        struct Allocation {
            uint8_t* cpuPtr = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
            size_t size = 0;

            bool IsValid() const { return cpuPtr != nullptr; }
        };

        UploadAlloc() = default;
        ~UploadAlloc() { Shutdown(); }

        // Initialize with fixed size - ZERO runtime allocations
        bool Init(ID3D12Device* device, size_t totalSize = 2147483648ULL, // 2GB
            uint32_t frameCount = 3) {

            m_frameCount = frameCount;
            m_totalSize = totalSize;

            // Calculate guaranteed space per frame (aligned down to 64KB)
            m_perFrameSize = totalSize / frameCount;
            m_perFrameSize = (m_perFrameSize / 65536) * 65536; // 64KB aligned

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = totalSize;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            if (FAILED(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_buffer))))
                return false;

            if (FAILED(m_buffer->Map(0, nullptr, (void**)&m_cpuBase)))
                return false;

            m_gpuBase = m_buffer->GetGPUVirtualAddress();
            m_currentFrame = 0;
            m_head = 0;

            // Initialize frame tracking for debugging
            m_frameHeads.resize(frameCount, 0);

            return true;
        }

        void Shutdown() {
            if (m_buffer) {
                m_buffer->Unmap(0, nullptr);
                m_buffer.Reset();
            }
            m_cpuBase = nullptr;
            m_gpuBase = 0;
            m_head = 0;
            m_frameHeads.clear();
        }

        void BeginFrame(uint32_t frameIndex) {
            m_currentFrame = frameIndex;
            m_head = frameIndex * m_perFrameSize;
            m_frameHeads[frameIndex] = m_head; // Track for debugging
        }

        Allocation Allocate(size_t size, size_t alignment = 256) {
            if (size == 0) return Allocation{};

            size_t frameStart = m_currentFrame * m_perFrameSize;
            size_t frameEnd = frameStart + m_perFrameSize;

            size_t aligned = (m_head + (alignment - 1)) & ~(alignment - 1);
            size_t newHead = aligned + size;

            // DEBUG CHECK: If this fails, increase totalSize in Init()
            if (newHead > frameEnd) {
                // OUT OF UPLOAD MEMORY FOR THIS FRAME
                // Increase totalSize parameter in Init() call
                assert(false && "UploadAlloc: Frame out of memory. Increase totalSize.");
                return Allocation{};
            }

            Allocation alloc;
            alloc.cpuPtr = m_cpuBase + aligned;
            alloc.gpuAddress = m_gpuBase + aligned;
            alloc.size = size;

            m_head = newHead;
            return alloc;
        }

        // Helper for vertex data
        template<typename T>
        Allocation AllocateVertices(const T* data, size_t count) {
            size_t size = sizeof(T) * count;
            auto alloc = Allocate(size, alignof(T));
            if (alloc.IsValid() && data) {
                memcpy(alloc.cpuPtr, data, size);
            }
            return alloc;
        }

        // Statistics
        size_t GetUsedThisFrame() const {
            size_t frameStart = m_currentFrame * m_perFrameSize;
            return m_head - frameStart;
        }

        size_t GetAvailableThisFrame() const {
            size_t frameStart = m_currentFrame * m_perFrameSize;
            size_t frameEnd = frameStart + m_perFrameSize;
            return frameEnd - m_head;
        }

        size_t GetPerFrameSize() const { return m_perFrameSize; }
        size_t GetTotalSize() const { return m_totalSize; }

        ID3D12Resource* GetBuffer() const { return m_buffer.Get(); }

    private:
        ComPtr<ID3D12Resource> m_buffer;
        uint8_t* m_cpuBase = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase = 0;

        size_t m_totalSize = 0;
        size_t m_perFrameSize = 0;
        uint32_t m_frameCount = 0;
        uint32_t m_currentFrame = 0;
        size_t m_head = 0;

        std::vector<size_t> m_frameHeads; // For debugging
    };

} // namespace GraphicsEngine