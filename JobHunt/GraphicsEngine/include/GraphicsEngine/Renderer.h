#pragma once
#include "Export.h"
#include "Camera.h"
#include "Geometry.h"
#include "D3D12Helpers.h"
#include "SolMath.h"
#include "Memory/UploadAlloc.h"

#include <Windows.h>
#include <vector>
#include <array>
#include <cstdint>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct IDXGISwapChain3;
struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12Fence;

namespace GraphicsEngine
{
    class GRAPHICS_API Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
        void Shutdown();

        void Resize(uint32_t width, uint32_t height);

        void Update(float dt);
        void Render();

        void OnKeyDown(WPARAM key);
        void OnKeyUp(WPARAM key);
        void OnMouseMove(int x, int y, bool lmb, bool rmb);
        void OnMouseWheel(int delta);

        void ToggleFrustum() { m_showPlayerFrustum = !m_showPlayerFrustum; }
        void ToggleGrid() { m_showGrid = !m_showGrid; }

        void MovePlayer(float dx, float dy, float dz);

    private:
        bool CreateDevice();
        bool CreateSwapchainAndRTVs(HWND hwnd, uint32_t width, uint32_t height);
        bool CreateDepth(uint32_t width, uint32_t height);
        bool CreateRootAndPSO();
        bool CreateCommandObjects();
        bool CreateGeometry();

        bool CreateShadowMap(uint32_t size);
        void RenderShadowPass(ID3D12GraphicsCommandList* cmd);

        void WaitForGPU();
        void MoveToNextFrame();

        void RecordDrawCalls(ID3D12GraphicsCommandList* cmd);
        void RenderHUD(ID3D12GraphicsCommandList* cmd);

        void UpdateTitleFPS(HWND hwnd);
        void RecreateOnResize(uint32_t width, uint32_t height);

        void UpdateLight(float dt);
        float3 ComputeLightDir() const;

    private:
        static constexpr UINT kFrameCount = 3;
        ComPtr<ID3D12Device>                m_device;
        ComPtr<ID3D12CommandQueue>          m_cmdQueue;
        ComPtr<IDXGISwapChain3>             m_swapchain;
        ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
        UINT                                m_rtvDescriptorSize = 0;
        ComPtr<ID3D12Resource>              m_backBuffers[kFrameCount];
        UINT                                m_frameIndex = 0;

        ComPtr<ID3D12DescriptorHeap>        m_dsvHeap;
        ComPtr<ID3D12Resource>              m_depth;

        ComPtr<ID3D12CommandAllocator>      m_cmdAlloc[kFrameCount];
        ComPtr<ID3D12GraphicsCommandList>   m_cmdList;
        ComPtr<ID3D12Fence>                 m_fence;
        UINT64                              m_fenceValue = 0;
        UINT64                              m_fenceValues[kFrameCount] = { 0 };
        HANDLE                              m_fenceEvent = nullptr;

        ComPtr<ID3D12RootSignature>         m_rootSig;
        ComPtr<ID3D12PipelineState>         m_pso;
        ComPtr<ID3D12PipelineState>         m_psoLines;
        ComPtr<ID3D12PipelineState>         m_psoNoDepth;

        D3D12_VIEWPORT                      m_viewport{};
        D3D12_RECT                          m_scissor{};
        DXGI_FORMAT                         m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT                         m_depthFormat = DXGI_FORMAT_D32_FLOAT;

        ComPtr<ID3D12Resource>              m_cbUpload;
        uint8_t* m_cbMapped = nullptr;
        UINT64                              m_cbSizeBytes = 256ULL * 1024ULL;
        UINT64                              m_cbHead = 0;

        ComPtr<ID3D12Resource>              m_vbLines;
        D3D12_VERTEX_BUFFER_VIEW            m_vbLinesView{};
        UINT                                m_vertexCountLines = 0;

        ComPtr<ID3D12Resource>              m_vbTris;
        D3D12_VERTEX_BUFFER_VIEW            m_vbTrisView{};
        UINT                                m_vertexCountTris = 0;

        std::vector<VertexPC>               m_lines;
        std::vector<VertexPNC>              m_trisLit;

        struct LineRanges {
            UINT gridStart = 0, gridCount = 0;
            UINT axesStart = 0, axesCount = 0;            
        };
        LineRanges                           m_lineRanges;

        struct Box { AABB_t aabb; float3 color; };
        std::vector<Box>                     m_debugBoxes;

        float3 m_frustumOffset{ 0.0f, 0.0f, 0.0f };
        float  m_frustumStep = 0.25f;

        UploadAlloc                          m_dynamicUpload;

        std::vector<VertexPC>                m_hudVertices;
        std::vector<VertexPC>                m_boxLineVertices;
        std::vector<VertexPC>                m_frustumVertices;

        Camera                               m_camera;
        Camera                               m_playerCam;
        struct Player { float3 pos{ 0,0.5f,0 }; float yaw = 0.0f; } m_player;

        bool                                 m_keys[256] = { false };
        bool                                 m_rmb = false;
        int                                  m_lastMouseX = 0;
        int                                  m_lastMouseY = 0;

        bool                                 m_showPlayerFrustum = true;
        bool                                 m_showGrid = true;

        HWND                                 m_hwnd = nullptr;
        uint32_t                             m_width = 1280, m_height = 720;
        float                                m_timeSinceTitle = 0.0f;
        float                                m_fpsAccum = 0.0f;
        int                                  m_fpsFrames = 0;
        float                                m_lastFPS = 0.0f;

        bool                                 m_vsync = true;

        float                                m_mouseSens = 0.0025f;
        float                                m_mouseAccel = 0.00015f;

        enum class CameraMode { Free = 0, ThirdPerson = 1, Orbit = 2 };
        CameraMode                           m_camMode = CameraMode::Free;
        float                                m_followDist = 4.0f;
        float                                m_orbitDist = 6.0f;
        float3                               m_orbitFocus{ 0,0,0 };

        float                                m_lastFrameMs = 0.0f;
        std::array<float, 160>               m_frameTimes{};
        int                                  m_ftHead = 0;

        static constexpr float kBaseMoveSpeed = 5.0f;
        static constexpr float kSprintMul = 2.0f;

        bool  m_useCullOverride = true;
        float m_cullNear = 0.1f;
        float m_cullFar = 5.0f;

        bool   m_lightEnabled = true;
        bool   m_lightAutoOrbit = false;
        float  m_lightYaw = 0.3f;
        float  m_lightPitch = -0.7f;
        float  m_dayNightSpeed = 0.5f;

        bool   m_showRandomCubes = true;
        bool   m_showTestCube = true;

        float3 m_testCubePos{ 9.4f, 0.9f, 0.0f };
        float3 m_testCubeScale{ 1.0f, 1.0f, 1.0f };
        float  m_testCubeYaw = 0.0f;

        bool   m_shadowsEnabled = true;
        static constexpr UINT kShadowMapSize = 2048;

        D3D12_RESOURCE_STATES m_shadowState = D3D12_RESOURCE_STATE_DEPTH_WRITE;


        ComPtr<ID3D12Resource>              m_shadowTex;
        ComPtr<ID3D12DescriptorHeap>        m_dsvHeapShadow;
        ComPtr<ID3D12DescriptorHeap>        m_srvHeap;
        D3D12_CPU_DESCRIPTOR_HANDLE         m_shadowDsv{};
        D3D12_GPU_DESCRIPTOR_HANDLE         m_shadowSrv{};

        D3D12_VIEWPORT                      m_shadowViewport{};
        D3D12_RECT                          m_shadowScissor{};

        ComPtr<ID3D12PipelineState>         m_psoShadow;

        float4x4                            m_lightView = m_identity();
        float4x4                            m_lightProj = m_identity();
    };
}
