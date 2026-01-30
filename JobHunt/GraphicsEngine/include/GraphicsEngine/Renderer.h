#pragma once
#include "Export.h"
#include "Camera.h"
#include "Geometry.h"
#include "D3D12Helpers.h"
#include "SolMath.h"

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

        // Input hooks
        void OnKeyDown(WPARAM key);
        void OnKeyUp(WPARAM key);
        void OnMouseMove(int x, int y, bool lmb, bool rmb);
        void OnMouseWheel(int delta);

        // Toggles
        void ToggleFrustum() { m_showPlayerFrustum = !m_showPlayerFrustum; }
        void ToggleGrid() { m_showGrid = !m_showGrid; }

        // Player box
        void MovePlayer(float dx, float dy, float dz);

    private:
        // Core creation
        bool CreateDevice();
        bool CreateSwapchainAndRTVs(HWND hwnd, uint32_t width, uint32_t height);
        bool CreateDepth(uint32_t width, uint32_t height);
        bool CreateRootAndPSO();
        bool CreateCommandObjects();
        bool CreateGeometry();

        // Shadows
        bool CreateShadowMap(uint32_t size);
        void RenderShadowPass(ID3D12GraphicsCommandList* cmd);

        // Frame orchestration
        void WaitForGPU();
        void MoveToNextFrame();

        void RecordDrawCalls(ID3D12GraphicsCommandList* cmd);
        void RenderHUD(ID3D12GraphicsCommandList* cmd);

        void UpdateTitleFPS(HWND hwnd);
        void RecreateOnResize(uint32_t width, uint32_t height);

        // Light helpers
        void UpdateLight(float dt);
        float3 ComputeLightDir() const;

    private:
        // ==== D3D12 core ====
        static constexpr UINT kFrameCount = 3;
        ComPtr<ID3D12Device>                m_device;
        ComPtr<ID3D12CommandQueue>          m_cmdQueue;
        ComPtr<IDXGISwapChain3>             m_swapchain;
        ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
        UINT                                m_rtvDescriptorSize = 0;
        ComPtr<ID3D12Resource>              m_backBuffers[kFrameCount];
        UINT                                m_frameIndex = 0;

        ComPtr<ID3D12DescriptorHeap>        m_dsvHeap;     // main depth DSV heap
        ComPtr<ID3D12Resource>              m_depth;

        ComPtr<ID3D12CommandAllocator>      m_cmdAlloc[kFrameCount];
        ComPtr<ID3D12GraphicsCommandList>   m_cmdList;
        ComPtr<ID3D12Fence>                 m_fence;
        UINT64                              m_fenceValue = 0;
        HANDLE                              m_fenceEvent = nullptr;

        ComPtr<ID3D12RootSignature>         m_rootSig;
        ComPtr<ID3D12PipelineState>         m_pso;        // triangles (lit)
        ComPtr<ID3D12PipelineState>         m_psoLines;   // lines (unlit, GS thick)
        ComPtr<ID3D12PipelineState>         m_psoNoDepth; // HUD triangles (unlit, depth off)

        D3D12_VIEWPORT                      m_viewport{};
        D3D12_RECT                          m_scissor{};
        DXGI_FORMAT                         m_backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT                         m_depthFormat = DXGI_FORMAT_D32_FLOAT;

        // Constant buffer ring (upload)
        ComPtr<ID3D12Resource>              m_cbUpload;
        uint8_t* m_cbMapped = nullptr;
        UINT64                              m_cbSizeBytes = 256ULL * 1024ULL;
        UINT64                              m_cbHead = 0;

        // Geometry buffers
        ComPtr<ID3D12Resource>              m_vbLines;
        D3D12_VERTEX_BUFFER_VIEW            m_vbLinesView{};
        UINT                                m_vertexCountLines = 0;

        ComPtr<ID3D12Resource>              m_vbTris;
        D3D12_VERTEX_BUFFER_VIEW            m_vbTrisView{};
        UINT                                m_vertexCountTris = 0;

        // CPU geometry stores
        std::vector<VertexPC>               m_lines;
        std::vector<VertexPNC>              m_trisLit;

        // Line subranges packed into m_lines
        struct LineRanges {
            UINT gridStart = 0, gridCount = 0;
            UINT axesStart = 0, axesCount = 0;
            
        };
        LineRanges                           m_lineRanges;

        // Debug boxes for frustum culling
        struct Box { AABB_t aabb; float3 color; };
        std::vector<Box>                     m_debugBoxes;

        // Frustum offset (relative to the player), and step size for hotkeys
        float3 m_frustumOffset{ 0.0f, 0.0f, 0.0f };
        float  m_frustumStep = 0.25f;  // units per key press

        // Per-frame dynamic uploads (HUD/frustum lines)
        std::vector<ComPtr<ID3D12Resource>>  m_transientUploads;

        // Cameras and player
        Camera                               m_camera;
        Camera                               m_playerCam;
        struct Player { float3 pos{ 0,0.5f,0 }; float yaw = 0.0f; } m_player;

        // Input state
        bool                                 m_keys[256] = { false };
        bool                                 m_rmb = false;
        int                                  m_lastMouseX = 0;
        int                                  m_lastMouseY = 0;

        // Toggles
        bool                                 m_showPlayerFrustum = true;
        bool                                 m_showGrid = true;

        // Window & stats
        HWND                                 m_hwnd = nullptr;
        uint32_t                             m_width = 1280, m_height = 720;
        float                                m_timeSinceTitle = 0.0f;
        float                                m_fpsAccum = 0.0f;
        int                                  m_fpsFrames = 0;
        float                                m_lastFPS = 0.0f;

        // ==== Quality/feel extras ====
        // VSync toggle
        bool                                 m_vsync = true;

        // Mouse look smoothing/params
        float                                m_mouseSens = 0.0025f;   // base sensitivity
        float                                m_mouseAccel = 0.00015f; // per-pixel gain

        // Camera modes
        enum class CameraMode { Free = 0, ThirdPerson = 1, Orbit = 2 };
        CameraMode                           m_camMode = CameraMode::Free;
        float                                m_followDist = 4.0f;      // third-person distance
        float                                m_orbitDist = 6.0f;       // orbit distance
        float3                               m_orbitFocus{ 0,0,0 };    // orbit focus point

        // Frame-time graph
        float                                m_lastFrameMs = 0.0f;
        std::array<float, 160>               m_frameTimes{};
        int                                  m_ftHead = 0;

        // Movement constants
        static constexpr float kBaseMoveSpeed = 5.0f;
        static constexpr float kSprintMul = 2.0f;

        // Culling-only near/far override (independent of render camera)
        bool  m_useCullOverride = true;
        float m_cullNear = 0.1f;
        float m_cullFar = 5.0f;

        // ==== Directional light and day/night ====
        bool   m_lightEnabled = true;      // toggle on/off
        bool   m_lightAutoOrbit = false;     // day/night auto mode
        float  m_lightYaw = 0.3f;      // radians
        float  m_lightPitch = -0.7f;     // radians
        float  m_dayNightSpeed = 0.5f;      // radians per second when auto

        // ==== Content toggles ====
        bool   m_showRandomCubes = true;     // show/hide randomized cubes
        bool   m_showTestCube = true;     // dedicated lit test cube

        // Test cube transform
        float3 m_testCubePos{ 9.4f, 0.9f, 0.0f };
        float3 m_testCubeScale{ 1.0f, 1.0f, 1.0f };
        float  m_testCubeYaw = 0.0f;

        // ==== Shadow map (directional) ====
        bool   m_shadowsEnabled = true;
        static constexpr UINT kShadowMapSize = 2048;

        D3D12_RESOURCE_STATES m_shadowState = D3D12_RESOURCE_STATE_DEPTH_WRITE;


        // Shadow resources: depth texture with DSV + SRV
        ComPtr<ID3D12Resource>              m_shadowTex;        // typeless shadow map
        ComPtr<ID3D12DescriptorHeap>        m_dsvHeapShadow;    // DSV heap for shadow
        ComPtr<ID3D12DescriptorHeap>        m_srvHeap;          // shader-visible SRV heap (shadow SRV)
        D3D12_CPU_DESCRIPTOR_HANDLE         m_shadowDsv{};      // CPU handle to shadow DSV
        D3D12_GPU_DESCRIPTOR_HANDLE         m_shadowSrv{};      // GPU handle to shadow SRV

        D3D12_VIEWPORT                      m_shadowViewport{};
        D3D12_RECT                          m_shadowScissor{};

        ComPtr<ID3D12PipelineState>         m_psoShadow;        // depth-only shadow pass

        // Light matrices for shadowing
        float4x4                            m_lightView = m_identity();
        float4x4                            m_lightProj = m_identity();
    };
}
