#pragma once
#include "DescriptorRing.h"

namespace GraphicsEngine {

class DescriptorSystem {
public:
    struct FrameView {
        DescriptorRing::Slice srvCbvUav;
        DescriptorRing::Slice rtv;
        DescriptorRing::Slice dsv;
    };

    bool Init(ID3D12Device* device, uint32_t frames, uint32_t srvTotal, uint32_t rtvTotal, uint32_t dsvTotal) {
        m_frameCount = frames;
        return m_ringSRV.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvTotal, frames, true)
            && m_ringRTV.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvTotal, frames, false)
            && m_ringDSV.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvTotal, frames, false);
    }

    FrameView BeginFrame(uint32_t frameIndex) {
        FrameView view{};
        view.srvCbvUav = m_ringSRV.BeginFrame(frameIndex);
        view.rtv = m_ringRTV.BeginFrame(frameIndex);
        view.dsv = m_ringDSV.BeginFrame(frameIndex);
        m_currentView = view;
        return view;
    }

    void EndFrame(uint32_t frameIndex) {
        m_ringSRV.EndFrame(frameIndex);
        m_ringRTV.EndFrame(frameIndex);
        m_ringDSV.EndFrame(frameIndex);
    }

    ID3D12DescriptorHeap* GetSRVHeap() const { return m_ringSRV.GetHeap(); }
    ID3D12DescriptorHeap* GetRTVHeap() const { return m_ringRTV.GetHeap(); }
    ID3D12DescriptorHeap* GetDSVHeap() const { return m_ringDSV.GetHeap(); }

private:
    uint32_t m_frameCount = 0;
    DescriptorRing m_ringSRV;
    DescriptorRing m_ringRTV;
    DescriptorRing m_ringDSV;
    FrameView m_currentView{};
};

}
