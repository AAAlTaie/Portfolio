#pragma once
#include "Descriptors/DescriptorSystem.h"
#include "Memory/LinearArena.h"
#include "Memory/UploadAlloc.h"

namespace GraphicsEngine {

struct FrameResources {
    uint32_t index = 0;
    DescriptorSystem::FrameView descriptors;
    LinearArena frameArena{ 256 * 1024 };
    LinearArena passArena{ 128 * 1024 };
};

class FrameResourceManager {
public:
    bool Init(ID3D12Device* device, uint32_t frameCount, uint32_t srvCount = 4096, uint32_t rtvCount = 128, uint32_t dsvCount = 32, size_t uploadSize = 32 * 1024 * 1024) {
        m_frameCount = frameCount;
        return m_descriptors.Init(device, frameCount, srvCount, rtvCount, dsvCount)
            && m_uploadAlloc.Init(device, uploadSize);
    }

    FrameResources& BeginFrame(uint32_t frameIndex, UINT64 fenceValue) {
        m_currentFrameIndex = frameIndex;
        m_currentFenceValue = fenceValue;
        
        static FrameResources frameRes;
        frameRes.index = frameIndex;
        frameRes.frameArena.Reset();
        frameRes.passArena.Reset();
        frameRes.descriptors = m_descriptors.BeginFrame(frameIndex);
        
        //m_uploadAlloc.Reset();
        
        return frameRes;
    }

    void EndFrame(uint32_t frameIndex) {
        m_descriptors.EndFrame(frameIndex);
    }

    DescriptorSystem& GetDescriptors() { return m_descriptors; }
    UploadAlloc& GetUploadAlloc() { return m_uploadAlloc; }

private:
    DescriptorSystem m_descriptors;
    UploadAlloc m_uploadAlloc;
    uint32_t m_frameCount = 0;
    uint32_t m_currentFrameIndex = 0;
    UINT64 m_currentFenceValue = 0;
};

}
