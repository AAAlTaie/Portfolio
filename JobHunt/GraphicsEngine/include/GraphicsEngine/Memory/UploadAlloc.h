#pragma once
#include "../D3D12Helpers.h"

namespace GraphicsEngine {

    class UploadAlloc {
    public:
        struct Allocation {
            uint8_t* cpuPtr = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
            size_t size = 0;
        };

        UploadAlloc() = default;
        ~UploadAlloc() {
            Shutdown();
        }

        bool Init(ID3D12Device* device, size_t totalSize, uint32_t frameCount = 3) {
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
            m_totalSize = totalSize;
            m_frameCount = frameCount;
            m_perFrameSize = totalSize / frameCount;
            m_currentFrame = 0;
            m_head = 0;

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
        }

        void BeginFrame(uint32_t frameIndex) {
            m_currentFrame = frameIndex;
            m_head = frameIndex * m_perFrameSize;
        }

        Allocation Allocate(size_t size, size_t alignment = 256) {
            size_t frameStart = m_currentFrame * m_perFrameSize;
            size_t frameEnd = frameStart + m_perFrameSize;

            size_t aligned = (m_head + (alignment - 1)) & ~(alignment - 1);

            // Check if we have space in this frame's region
            if (aligned + size > frameEnd) {
                // Out of space - return null allocation
                return Allocation{};
            }

            Allocation alloc;
            alloc.cpuPtr = m_cpuBase + aligned;
            alloc.gpuAddress = m_gpuBase + aligned;
            alloc.size = size;

            m_head = aligned + size;

            return alloc;
        }

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
    };

}