#pragma once
#include "../D3D12Helpers.h"
#include <vector>

namespace GraphicsEngine {

class DescriptorRing {
public:
    struct Slice {
        uint32_t first = 0;
        uint32_t count = 0;
        uint32_t cursor = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuBase{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuBase{};
        uint32_t descriptorSize = 0;

        inline D3D12_CPU_DESCRIPTOR_HANDLE CPU(uint32_t index) const {
            auto handle = cpuBase;
            handle.ptr += SIZE_T((cursor + index) * descriptorSize);
            return handle;
        }

        inline D3D12_GPU_DESCRIPTOR_HANDLE GPU(uint32_t index) const {
            auto handle = gpuBase;
            handle.ptr += UINT64((cursor + index) * descriptorSize);
            return handle;
        }

        inline uint32_t Alloc(uint32_t n = 1) {
            uint32_t at = cursor;
            cursor += n;
            return at;
        }

        inline void Reset() {
            cursor = 0;
        }
    };

    bool Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t total, uint32_t frames, bool shaderVisible) {
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
        m_type = type;
        m_frameCount = frames;
        m_totalCount = total;
        m_shaderVisible = shaderVisible;

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = total;
        desc.Type = type;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap))))
            return false;

        m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
            m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();

        m_perFrameCount = total / frames;
        m_slices.resize(frames);

        for (uint32_t i = 0; i < frames; ++i) {
            auto& slice = m_slices[i];
            slice.first = i * m_perFrameCount;
            slice.count = m_perFrameCount;
            slice.cpuBase = m_cpuStart;
            slice.cpuBase.ptr += SIZE_T(slice.first) * SIZE_T(m_descriptorSize);
            slice.gpuBase = m_gpuStart;
            slice.gpuBase.ptr += UINT64(slice.first) * UINT64(m_descriptorSize);
            slice.descriptorSize = m_descriptorSize;
        }

        return true;
    }

    inline Slice& BeginFrame(uint32_t frameIndex) {
        auto& slice = m_slices[frameIndex];
        slice.Reset();
        return slice;
    }

    inline void EndFrame(uint32_t) {}

    inline ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    inline uint32_t GetPerFrameCount() const { return m_perFrameCount; }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    uint32_t m_descriptorSize = 0;
    uint32_t m_totalCount = 0;
    uint32_t m_perFrameCount = 0;
    uint32_t m_frameCount = 0;
    bool m_shaderVisible = false;
    std::vector<Slice> m_slices;
};

}
