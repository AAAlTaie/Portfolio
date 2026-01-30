#include "GraphicsEngine/Renderer.h"
#include "GraphicsEngine/GraphicsEngine.h"
#include "GraphicsEngine/Camera.h"
#include "GraphicsEngine/Geometry.h"
#include "GraphicsEngine/D3D12Helpers.h"
#include "SolMath.h"

#include <vector>
#include <array>
#include <chrono>
#include <string>
#include <cwchar>
#include <cmath>

using namespace GraphicsEngine; // SolMath types are global (no namespace)

// ============================================================================
// Helpers / CB layout
// ============================================================================
struct SceneCB
{
    float mvp[16];      // 64
    float lightDir[3];  // 12
    float _pad0;        // 4  => 80
    float viewport[2];  // 8
    float thicknessPx;  // 4
    float _pad1;        // 4  => 96
    float lightVP[16];  // 64 => 160
};
static_assert(sizeof(SceneCB) == 160, "SceneCB must be exactly 160 bytes");

static inline void WriteCB(const float4x4& MrowMajor,
    SceneCB& cb,
    float3 light = { -0.4f, -0.7f, -0.2f },
    float viewportW = 0.0f,
    float viewportH = 0.0f,
    float thicknessPx = 0.0f,
    const float4x4* lightVP = nullptr)
{
    store_column_major(MrowMajor, cb.mvp);
    cb.lightDir[0] = light.x; cb.lightDir[1] = light.y; cb.lightDir[2] = light.z;
    cb._pad0 = 0.0f;
    cb.viewport[0] = viewportW; cb.viewport[1] = viewportH;
    cb.thicknessPx = thicknessPx;
    cb._pad1 = 0.0f;
    if (lightVP) store_column_major(*lightVP, cb.lightVP);
    else         store_column_major(m_identity(), cb.lightVP);
}

static inline D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 bytes)
{
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = bytes;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}

// Pretty shader compiler (prints errors to Output window)
template<typename T>
static inline void ThrowIfFailedHR(HRESULT hr, const T& msg) {
    if (FAILED(hr)) throw std::runtime_error(msg);
}
static inline void CompileShader(LPCWSTR file, LPCSTR entry, LPCSTR target, ComPtr<ID3DBlob>& out, UINT flags = 0)
{
    ComPtr<ID3DBlob> errs;
    HRESULT hr = D3DCompileFromFile(file, nullptr, nullptr, entry, target, flags, 0, &out, &errs);
    if (errs) OutputDebugStringA((const char*)errs->GetBufferPointer());
    if (FAILED(hr)) {
        std::string s = "Shader compile failed: ";
        s += entry; s += "/"; s += target; s += " in ";
        int len = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        std::string path(len > 0 ? len - 1 : 0, '\0');
        if (len > 1) WideCharToMultiByte(CP_UTF8, 0, file, -1, path.data(), len, nullptr, nullptr);
        s += path;
        if (errs) { s += "\n"; s += (const char*)errs->GetBufferPointer(); }
        throw std::runtime_error(s);
    }
}

// ============================================================================
// Init / Shutdown
// ============================================================================
Renderer::Renderer() {}
Renderer::~Renderer() { Shutdown(); }

bool Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    m_hwnd = hwnd;
    m_width = width; m_height = height;

    if (!CreateDevice())                      return false;
    if (!CreateCommandObjects())              return false;
    if (!CreateSwapchainAndRTVs(hwnd, width, height)) return false;
    if (!CreateDepth(width, height))          return false;
    if (!CreateRootAndPSO())                  return false;
    if (!CreateGeometry())                    return false;
    if (!CreateShadowMap(2048))               return false;

    // Cameras
    m_camera.SetLens(to_radians(60.0f), float(width) / float(height), 0.1f, 500.0f);
    m_camera.SetPosition({ -5.0f, 3.0f, -5.0f });
    m_camera.YawPitch(+0.7f, -0.2f);

    m_playerCam.SetLens(to_radians(60.0f), float(width) / float(height), 0.1f, 5.0f);
    m_playerCam.SetPosition({ 0.0f, 0.5f, 0.0f });

    // Seed culling override from player cam
    m_cullNear = m_playerCam.GetNearZ();
    m_cullFar = m_playerCam.GetFarZ();

    // Random debug boxes
    uint32_t seed = 1337u;
    auto r01 = [&]() { seed = seed * 1664525u + 1013904223u; return float((seed >> 8) & 0xFFFF) / 65535.0f; };
    for (int i = 0; i < 200; i++)
    {
        float3 c{ (r01() - 0.5f) * 60.0f, r01() * 5.0f, (r01() - 0.5f) * 60.0f };
        float3 e{ 0.5f + r01() * 1.5f, 0.5f + r01() * 1.5f, 0.5f + r01() * 1.5f };
        float3 col{ 0.4f + 0.6f * r01(), 0.4f + 0.6f * r01(), 0.4f + 0.6f * r01() };
        m_debugBoxes.push_back({ AABB_t{ c, e }, col });
    }

    // Frame-time graph priming
    for (float& v : m_frameTimes) v = 16.6f; // ~60 FPS baseline
    m_ftHead = 0;

    // Defaults / toggles
    m_vsync = true;
    m_camMode = CameraMode::Free;
    m_followDist = 4.0f;
    m_orbitDist = 6.0f;
    m_orbitFocus = { 0,0,0 };

    // Mouse params
    m_mouseSens = 0.0025f;
    m_mouseAccel = 0.00015f;

    return true;
}

void Renderer::Shutdown()
{
    if (m_cmdQueue) WaitForGPU();
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }

    m_cbMapped = nullptr;
    m_cbUpload.Reset();
    m_depth.Reset();
    for (auto& bb : m_backBuffers) bb.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_cmdList.Reset();
    for (auto& a : m_cmdAlloc) a.Reset();
    m_swapchain.Reset();
    m_cmdQueue.Reset();

    m_psoShadow.Reset();
    m_psoNoDepth.Reset();
    m_psoLines.Reset();
    m_pso.Reset();
    m_rootSig.Reset();

    m_vbLines.Reset();
    m_vbTris.Reset();

    m_dsvHeapShadow.Reset();
    m_srvHeap.Reset();
    m_shadowTex.Reset();

    m_transientUploads.clear();

    m_device.Reset();
}

// ============================================================================
// Device / Swapchain / Cmd objects
// ============================================================================
bool Renderer::CreateDevice()
{
#if defined(_DEBUG)
    { ComPtr<ID3D12Debug> dbg; if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer(); }
#endif
ComPtr<IDXGIFactory6> fac; ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&fac)));

ComPtr<IDXGIAdapter1> ad;
for (UINT i = 0; DXGI_ERROR_NOT_FOUND != fac->EnumAdapters1(i, &ad); ++i)
{
    DXGI_ADAPTER_DESC1 d; ad->GetDesc1(&d);
    if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
    if (SUCCEEDED(D3D12CreateDevice(ad.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device))))
        break;
}
if (!m_device)
{
    ComPtr<IDXGIAdapter> warp;
    ThrowIfFailed(fac->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
    ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
}
return m_device != nullptr;
}

bool Renderer::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_cmdQueue)));

    for (UINT i = 0; i < kFrameCount; i++)
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc[i])));

    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc[0].Get(), nullptr, IID_PPV_ARGS(&m_cmdList)));
    m_cmdList->Close();

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return m_fenceEvent != nullptr;
}

bool Renderer::CreateSwapchainAndRTVs(HWND hwnd, uint32_t w, uint32_t h)
{
    ComPtr<IDXGIFactory4> f; ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&f)));

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = kFrameCount;
    sc.Width = w;
    sc.Height = h;
    sc.Format = m_backbufferFormat;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> tmp;
    ThrowIfFailed(f->CreateSwapChainForHwnd(m_cmdQueue.Get(), hwnd, &sc, nullptr, nullptr, &tmp));
    ThrowIfFailed(tmp.As(&m_swapchain));
    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rd{}; rd.NumDescriptors = kFrameCount; rd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto hRTV = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; i++)
    {
        ThrowIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, hRTV);
        hRTV.ptr += SIZE_T(m_rtvDescriptorSize);
    }

    m_viewport = D3D12_VIEWPORT{ 0,0,(float)w,(float)h, 0.0f, 1.0f };
    m_scissor = D3D12_RECT{ 0,0,(LONG)w,(LONG)h };
    return true;
}

bool Renderer::CreateDepth(uint32_t w, uint32_t h)
{
    D3D12_DESCRIPTOR_HEAP_DESC dd{}; dd.NumDescriptors = 1; dd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dd, IID_PPV_ARGS(&m_dsvHeap)));

    // STANDARD Z: clear depth to 1.0 (not reversed-Z)
    D3D12_CLEAR_VALUE cv{}; cv.Format = m_depthFormat; cv.DepthStencil.Depth = 1.0f; cv.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = w; rd.Height = h; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.SampleDesc.Count = 1;
    rd.Format = m_depthFormat; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_depth)));

    m_device->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

// ============================================================================
// RootSig + PSOs + CB
// ============================================================================
bool Renderer::CreateRootAndPSO()
{
    // Root: b0 (SceneCB), t0 (Shadow SRV), s0 (static sampler), s1 (comparison sampler)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0; // t0
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[2]{};

    // b0 : SceneCB
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].Descriptor.RegisterSpace = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t0 : ShadowMap SRV (descriptor table)
    D3D12_ROOT_DESCRIPTOR_TABLE tbl{};
    tbl.NumDescriptorRanges = 1;
    tbl.pDescriptorRanges = &range;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = tbl;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s0 : static sampler (linear clamp)
    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samp.RegisterSpace = 0;
    samp.ShaderRegister = 0; // s0

    // s1 : comparison sampler for hardware PCF
    D3D12_STATIC_SAMPLER_DESC compSamp{};
    compSamp.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    compSamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    compSamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    compSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    compSamp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    compSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    compSamp.RegisterSpace = 0;
    compSamp.ShaderRegister = 1; // s1

    // Both samplers
    D3D12_STATIC_SAMPLER_DESC samplers[2] = { samp, compSamp };

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 2;
    rs.pParameters = params;
    rs.NumStaticSamplers = 2; 
    rs.pStaticSamplers = samplers; 
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob));
    ThrowIfFailed(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));

    // Compile shaders
    UINT cf = 0;
#if defined(_DEBUG)
    cf = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> vsL, psL, gsL, vsT, psT;
    CompileShader(L"Shaders\\Basic.hlsl", "VSMain", "vs_5_0", vsL, cf);
    CompileShader(L"Shaders\\Basic.hlsl", "PSMain", "ps_5_0", psL, cf);
    CompileShader(L"Shaders\\BasicLit.hlsl", "VSMainLit", "vs_5_0", vsT, cf);
    CompileShader(L"Shaders\\BasicLit.hlsl", "PSMainLit", "ps_5_0", psT, cf);

    // Input layouts
    D3D12_INPUT_ELEMENT_DESC layoutL[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    D3D12_INPUT_ELEMENT_DESC layoutT[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Blending
    D3D12_BLEND_DESC blend{};
    D3D12_RENDER_TARGET_BLEND_DESC rtb{};
    rtb.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rtb.BlendEnable = FALSE;
    blend.RenderTarget[0] = rtb;

    // STANDARD Z (fix): write ALL, compare LESS_EQUAL
    D3D12_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    ds.StencilEnable = FALSE;

    // Lines: no depth writes
    D3D12_DEPTH_STENCIL_DESC dsLines = ds;
    dsLines.DepthEnable = FALSE;           
    dsLines.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // Triangles two-sided (avoid winding surprises)
    D3D12_RASTERIZER_DESC rastTri{};
    rastTri.FillMode = D3D12_FILL_MODE_SOLID;
    rastTri.CullMode = D3D12_CULL_MODE_NONE;
    rastTri.FrontCounterClockwise = FALSE;
    rastTri.DepthClipEnable = TRUE;

    // Lines: never cull (GS quads)
    D3D12_RASTERIZER_DESC rastLines = rastTri;
    rastLines.CullMode = D3D12_CULL_MODE_NONE;


    // Shadow pass: standard Z, two-sided
    D3D12_DEPTH_STENCIL_DESC dsShadow{};
    dsShadow.DepthEnable = TRUE;
    dsShadow.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsShadow.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    dsShadow.StencilEnable = FALSE;

    D3D12_RASTERIZER_DESC rastShadow = rastTri;

    // PSO: triangles (lit)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dT{};
    dT.pRootSignature = m_rootSig.Get();
    dT.VS = { vsT->GetBufferPointer(), vsT->GetBufferSize() };
    dT.PS = { psT->GetBufferPointer(), psT->GetBufferSize() };
    dT.BlendState = blend;
    dT.RasterizerState = rastTri;
    dT.DepthStencilState = ds;
    dT.SampleMask = UINT_MAX;
    dT.InputLayout = { layoutT, _countof(layoutT) };
    dT.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    dT.NumRenderTargets = 1;
    dT.RTVFormats[0] = m_backbufferFormat;
    dT.DSVFormat = m_depthFormat;
    dT.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dT, IID_PPV_ARGS(&m_pso)));

    // PSO: lines (unlit, GS thick)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dL{};
    dL.pRootSignature = m_rootSig.Get();
    dL.VS = { vsL->GetBufferPointer(), vsL->GetBufferSize() };
    dL.PS = { psL->GetBufferPointer(), psL->GetBufferSize() };
    //dL.GS = { gsL->GetBufferPointer(), gsL->GetBufferSize() };
    dL.BlendState = blend;
    dL.RasterizerState = rastLines;
    dL.DepthStencilState = dsLines;
    dL.SampleMask = UINT_MAX;
    dL.InputLayout = { layoutL, _countof(layoutL) };
    dL.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    dL.NumRenderTargets = 1;
    dL.RTVFormats[0] = m_backbufferFormat;
    dL.DSVFormat = m_depthFormat;
    dL.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dL, IID_PPV_ARGS(&m_psoLines)));

    // PSO: HUD (unlit triangles, depth OFF)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dHUD = dL;
    dHUD.GS = { nullptr, 0 };
    dHUD.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    dHUD.DepthStencilState.DepthEnable = FALSE;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dHUD, IID_PPV_ARGS(&m_psoNoDepth)));

    // PSO: shadow (depth-only; standard Z)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dS{};
    dS.pRootSignature = m_rootSig.Get();
    dS.VS = { vsT->GetBufferPointer(), vsT->GetBufferSize() };
    dS.PS = { nullptr, 0 }; // depth-only
    dS.BlendState = blend;
    dS.RasterizerState = rastShadow;
    dS.DepthStencilState = dsShadow;
    dS.SampleMask = UINT_MAX;
    dS.InputLayout = { layoutT, _countof(layoutT) };
    dS.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    dS.NumRenderTargets = 0;
    dS.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    dS.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dS, IID_PPV_ARGS(&m_psoShadow)));

    // Constant buffer (upload)
    D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC    cb = MakeBufferDesc(m_cbSizeBytes);
    ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &cb,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cbUpload)));
    ThrowIfFailed(m_cbUpload->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMapped)));
    m_cbHead = 0;
    m_transientUploads.clear();

    return true;
}

// ============================================================================
// Geometry (build & upload VBs)
// ============================================================================
bool Renderer::CreateGeometry()
{
    std::vector<VertexPC>  grid, axes; //cubeWire;
    Geom::BuildGridXZ(100.0f, 1.0f, { 0.25f,0.25f,0.25f }, grid);
    Geom::BuildAxes(1.5f, axes);
    

    std::vector<VertexPNC> cubeSolid;
    Geom::BuildSolidCubePNC(0.5f, cubeSolid);

    // Pack line VB ranges
    m_lines.clear();

    m_lineRanges.gridStart = (UINT)m_lines.size();
    m_lines.insert(m_lines.end(), grid.begin(), grid.end());
    m_lineRanges.gridCount = (UINT)grid.size();

    m_lineRanges.axesStart = (UINT)m_lines.size();
    m_lines.insert(m_lines.end(), axes.begin(), axes.end());
    m_lineRanges.axesCount = (UINT)axes.size();

    m_vertexCountLines = (UINT)m_lines.size();

    // Triangles (lit)
    m_trisLit = cubeSolid;
    m_vertexCountTris = (UINT)m_trisLit.size();

    // Create & upload line VB
    const UINT vbL = (UINT)(m_lines.size() * sizeof(VertexPC));
    D3D12_HEAP_PROPERTIES hd{}; hd.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC   rdL = MakeBufferDesc(vbL);

    ThrowIfFailed(m_device->CreateCommittedResource(&hd, D3D12_HEAP_FLAG_NONE, &rdL,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vbLines)));  

    ComPtr<ID3D12Resource> uplL;
    D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
    ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rdL,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uplL)));
    void* mp = nullptr; ThrowIfFailed(uplL->Map(0, nullptr, &mp));
    memcpy(mp, m_lines.data(), vbL);
    uplL->Unmap(0, nullptr);

    // Create & upload tri VB
    const UINT vbT = (UINT)(m_trisLit.size() * sizeof(VertexPNC));
    D3D12_RESOURCE_DESC rdT = MakeBufferDesc(vbT);

   
    ThrowIfFailed(m_device->CreateCommittedResource(&hd, D3D12_HEAP_FLAG_NONE, &rdT,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vbTris)));  


    ComPtr<ID3D12Resource> uplT;
    ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rdT,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uplT)));
    void* mp2 = nullptr; ThrowIfFailed(uplT->Map(0, nullptr, &mp2));
    memcpy(mp2, m_trisLit.data(), vbT);
    uplT->Unmap(0, nullptr);

    // Copy & transition both VBs
    ThrowIfFailed(m_cmdAlloc[m_frameIndex]->Reset());
    ThrowIfFailed(m_cmdList->Reset(m_cmdAlloc[m_frameIndex].Get(), nullptr));
    m_cmdList->CopyResource(m_vbLines.Get(), uplL.Get());
    m_cmdList->CopyResource(m_vbTris.Get(), uplT.Get());

    D3D12_RESOURCE_BARRIER b[2]{};
    for (int i = 0; i < 2; i++)
    {
        b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b[i].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    b[0].Transition.pResource = m_vbLines.Get();
    b[1].Transition.pResource = m_vbTris.Get();

    m_cmdList->ResourceBarrier(2, b);
    ThrowIfFailed(m_cmdList->Close());
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_cmdQueue->ExecuteCommandLists(1, lists);
    WaitForGPU();

    // VB views
    m_vbLinesView.BufferLocation = m_vbLines->GetGPUVirtualAddress();
    m_vbLinesView.StrideInBytes = sizeof(VertexPC);
    m_vbLinesView.SizeInBytes = vbL;

    m_vbTrisView.BufferLocation = m_vbTris->GetGPUVirtualAddress();
    m_vbTrisView.StrideInBytes = sizeof(VertexPNC);
    m_vbTrisView.SizeInBytes = vbT;

    return true;
}

// ============================================================================
// Shadow map resources (depth-only target + DSV + SRV)
// ============================================================================
bool Renderer::CreateShadowMap(uint32_t size)
{
    // Typeless so we can have DSV as D32 and SRV as R32
    D3D12_RESOURCE_DESC tex{};
    tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex.Width = size; 
    tex.Height = size;
    tex.DepthOrArraySize = 1; 
    tex.MipLevels = 1;
    tex.Format = DXGI_FORMAT_R32_TYPELESS;
    tex.SampleDesc.Count = 1;
    tex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES hp{}; 
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE cv{};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &tex,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_shadowTex)));

    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{}; dsvDesc.NumDescriptors = 1; dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeapShadow)));
    m_shadowDsv = m_dsvHeapShadow->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_shadowTex.Get(), &dsv, m_shadowDsv);

    // SRV heap (shader-visible)
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.NumDescriptors = 1;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_shadowTex.Get(), &srv, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    m_shadowSrv = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    m_shadowViewport = { 0,0,(float)size,(float)size, 0.0f, 1.0f };
    m_shadowScissor = { 0,0,(LONG)size,(LONG)size };
    return true;
}

// ============================================================================
// Resize / Sync
// ============================================================================
void Renderer::Resize(uint32_t w, uint32_t h)
{
    if (w == 0 || h == 0) return;
    RecreateOnResize(w, h);
}

void Renderer::RecreateOnResize(uint32_t w, uint32_t h)
{
    WaitForGPU();
    for (UINT i = 0; i < kFrameCount; i++) m_backBuffers[i].Reset();
    m_depth.Reset();

    DXGI_SWAP_CHAIN_DESC sc{}; m_swapchain->GetDesc(&sc);
    ThrowIfFailed(m_swapchain->ResizeBuffers(kFrameCount, w, h, sc.BufferDesc.Format, sc.Flags));
    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    auto hRTV = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; i++)
    {
        ThrowIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, hRTV);
        hRTV.ptr += SIZE_T(m_rtvDescriptorSize);
    }

    CreateDepth(w, h);

    m_width = w; m_height = h;
    m_viewport = D3D12_VIEWPORT{ 0,0,(float)w,(float)h,0.0f,1.0f };
    m_scissor = D3D12_RECT{ 0,0,(LONG)w,(LONG)h };

    m_camera.SetLens(m_camera.GetFovY(), float(w) / float(h), m_camera.GetNearZ(), m_camera.GetFarZ());
    m_playerCam.SetLens(m_playerCam.GetFovY(), float(w) / float(h), m_playerCam.GetNearZ(), m_playerCam.GetFarZ());
}

void Renderer::WaitForGPU() {
    const UINT64 v = m_fenceValue;
    ThrowIfFailed(m_cmdQueue->Signal(m_fence.Get(), v));
    m_fenceValue++;
    if (m_fence->GetCompletedValue() < v) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(v, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
void Renderer::MoveToNextFrame() {
    UINT64 v = m_fenceValue;
    ThrowIfFailed(m_cmdQueue->Signal(m_fence.Get(), v));
    m_fenceValue++;
    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
    if (m_fence->GetCompletedValue() < v) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(v, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_cbHead = 0;
    m_transientUploads.clear();
}

// ============================================================================
// Input / Update / Title
// ============================================================================
void Renderer::OnKeyDown(WPARAM k)
{
    if (k < 256) m_keys[k] = true;

    switch (k)
    {
    case 'F': ToggleFrustum(); break;
    case 'G': ToggleGrid();    break;
    case 'V': m_vsync = !m_vsync; break;

    case '[': m_mouseSens = (m_mouseSens * 0.9f < 0.0005f) ? 0.0005f : (m_mouseSens * 0.9f);  break;
    case ']': m_mouseSens = (m_mouseSens * 1.1f > 0.02f) ? 0.02f : (m_mouseSens * 1.1f);      break;
    case ';': m_mouseAccel = (m_mouseAccel - 0.00005f < 0.0f) ? 0.0f : (m_mouseAccel - 0.00005f); break;
    case '\'':m_mouseAccel = (m_mouseAccel + 0.00005f > 0.001f) ? 0.001f : (m_mouseAccel + 0.00005f); break;

    case 'C':
    {
        CameraMode oldMode = m_camMode;
        m_camMode = (m_camMode == CameraMode::Free) ? CameraMode::ThirdPerson :
            (m_camMode == CameraMode::ThirdPerson) ? CameraMode::Orbit : CameraMode::Free;

        // FIX: When switching TO ThirdPerson/Orbit, position camera properly behind player
        if ((oldMode == CameraMode::Free) && (m_camMode == CameraMode::ThirdPerson || m_camMode == CameraMode::Orbit))
        {
            float3 target = (m_camMode == CameraMode::Orbit) ? m_orbitFocus : m_player.pos;
            float dist = (m_camMode == CameraMode::Orbit) ? m_orbitDist : m_followDist;

            // Get camera's current forward direction
            float4x4 camToWorld = m_camera.GetCameraToWorld();
            float3 camForward = { camToWorld[2].x, camToWorld[2].y, camToWorld[2].z };
            camForward.y = 0; // Keep it horizontal

            // FIX: Use m_normalize instead of normalize old..
            float len = sqrtf(camForward.x * camForward.x + camForward.y * camForward.y + camForward.z * camForward.z);
            if (len < 0.001f) camForward = float3{ 0, 0, 1 };
            else camForward = float3{ camForward.x / len, camForward.y / len, camForward.z / len };

            // Position camera behind player along camera's forward direction
            float3 camP = target - camForward * dist + float3{ 0, 1.2f, 0 };
            m_camera.SetPosition(camP);
        }
    }
    break;

    case 'O': m_useCullOverride = !m_useCullOverride; break;
    case VK_OEM_PLUS: { float nf = m_cullFar * 1.25f; m_cullFar = (nf > 500.0f) ? 500.0f : nf; if (m_cullFar <= m_cullNear + 0.01f) m_cullFar = m_cullNear + 0.01f; } break;
    case VK_OEM_MINUS: { float nf = m_cullFar * 0.8f; if (nf < m_cullNear + 0.02f) nf = m_cullNear + 0.02f; m_cullFar = nf; } break;
    case '0': { float nn = m_cullNear * 1.25f; if (nn > m_cullFar - 0.02f) nn = m_cullFar - 0.02f; if (nn < 0.01f) nn = 0.01f; m_cullNear = nn; } break;
    case '9': { float nn = m_cullNear * 0.8f; if (nn < 0.01f) nn = 0.01f; m_cullNear = nn; } break;

    case 'H': m_lightEnabled = !m_lightEnabled; break;
    case 'B': m_shadowsEnabled = !m_shadowsEnabled; break;
    case 'T': m_showTestCube = !m_showTestCube; break;
    case 'R': m_showRandomCubes = !m_showRandomCubes; break;
    case 'N': m_lightAutoOrbit = !m_lightAutoOrbit; break;

    case 'J': m_lightYaw -= 0.08f; break;
    case 'L': m_lightYaw += 0.08f; break;
    case 'I': m_lightPitch += 0.08f; if (m_lightPitch > 1.35f) m_lightPitch = 1.35f; break;
    case 'K': m_lightPitch -= 0.08f; if (m_lightPitch < -1.35f) m_lightPitch = -1.35f; break;

    // --- Frustum offset controls ---
    case VK_HOME: { // up +Y
        float step = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? m_frustumStep * 5.0f : m_frustumStep;
        m_frustumOffset.y += step;
    } break;

    case VK_END: {  // down -Y
        float step = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? m_frustumStep * 5.0f : m_frustumStep;
        m_frustumOffset.y -= step;
    } break;

    case VK_INSERT: { // +X (right)
        float step = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? m_frustumStep * 5.0f : m_frustumStep;
        m_frustumOffset.x += step;
    } break;

    case VK_DELETE: { // -X (left)
        float step = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? m_frustumStep * 5.0f : m_frustumStep;
        m_frustumOffset.x -= step;
    } break;

    case 'M': { // -Z backward
        float step = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? m_frustumStep * 5.0f : m_frustumStep;
        m_frustumOffset.z -= step;
    } break;

    case VK_BACK: // reset offset
        m_frustumOffset = float3{ 0,0,0 };
        break;

    default: break;
    }
}

void Renderer::OnKeyUp(WPARAM k) { if (k < 256) m_keys[k] = false; }

void Renderer::OnMouseMove(int x, int y, bool /*lmb*/, bool rmb)
{
    if (rmb && m_rmb)
    {
        float dx = float(x - m_lastMouseX);
        float dy = float(y - m_lastMouseY);
        float mag = std::sqrt(dx * dx + dy * dy);
        float gain = 1.0f + m_mouseAccel * mag;
        m_camera.YawPitch(dx * m_mouseSens * gain, dy * m_mouseSens * gain);
    }
    m_rmb = rmb;
    m_lastMouseX = x; m_lastMouseY = y;
}

void Renderer::OnMouseWheel(int delta)
{
    float fov = m_camera.GetFovY();
    float step = (delta > 0) ? -to_radians(2.0f) : to_radians(2.0f);
    if (fov + step < to_radians(20.0f)) fov = to_radians(20.0f);
    else if (fov + step > to_radians(110.0f)) fov = to_radians(110.0f);
    else fov += step;
    m_camera.SetLens(fov, m_camera.GetAspect(), m_camera.GetNearZ(), m_camera.GetFarZ());
}

void Renderer::MovePlayer(float dx, float dy, float dz)
{
    m_player.pos.x += dx; m_player.pos.y += dy; m_player.pos.z += dz;
}

void Renderer::Update(float dt)
{
    float s = kBaseMoveSpeed * ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) ? kSprintMul : 1.0f);

    if (m_keys['W']) m_camera.TranslateRelative(0, 0, +s * dt);
    if (m_keys['S']) m_camera.TranslateRelative(0, 0, -s * dt);
    if (m_keys['A']) m_camera.TranslateRelative(-s * dt, 0, 0);
    if (m_keys['D']) m_camera.TranslateRelative(+s * dt, 0, 0);
    if (m_keys['Q']) m_camera.TranslateRelative(0, -s * dt, 0);
    if (m_keys['E']) m_camera.TranslateRelative(0, +s * dt, 0);

    float p = 3.0f;
    if (m_keys[VK_LEFT])  MovePlayer(-p * dt, 0, 0);
    if (m_keys[VK_RIGHT]) MovePlayer(+p * dt, 0, 0);
    if (m_keys[VK_UP])    MovePlayer(0, 0, +p * dt);
    if (m_keys[VK_DOWN])  MovePlayer(0, 0, -p * dt);
    if (m_keys[VK_PRIOR]) MovePlayer(0, +p * dt, 0);
    if (m_keys[VK_NEXT])  MovePlayer(0, -p * dt, 0);

    if (m_camMode != CameraMode::Free)
    {
        float3 target = (m_camMode == CameraMode::Orbit) ? m_orbitFocus : m_player.pos;
        float  dist = (m_camMode == CameraMode::Orbit) ? m_orbitDist : m_followDist;
        float3 camP = target + float3{ 0, 1.2f, -dist };
        m_camera.SetPosition(camP);

        // Update player camera position
        m_playerCam.SetPosition(m_player.pos + m_frustumOffset);
    }

    UpdateLight(dt);

    m_lastFrameMs = dt * 1000.0f;
    m_frameTimes[m_ftHead] = m_lastFrameMs;
    m_ftHead = (m_ftHead + 1) % (int)m_frameTimes.size();

    m_timeSinceTitle += dt;
    m_fpsAccum += dt; m_fpsFrames++;
    if (m_timeSinceTitle > 0.5f) { UpdateTitleFPS(m_hwnd); m_timeSinceTitle = 0.0f; }
}

void Renderer::UpdateTitleFPS(HWND hwnd)
{
    float fps = (m_fpsFrames / (m_fpsAccum > 0 ? m_fpsAccum : 1));
    m_lastFPS = fps;
    m_fpsAccum = 0.0f; m_fpsFrames = 0;

    wchar_t t[256];
    swprintf_s(t, _countof(t),
        L"DX12 Engine Prototype | FPS: %.1f | VSync: %s | Light %s Auto:%s | Random:%s Test:%s | FrustumOff (%.2f, %.2f, %.2f)",
        fps,
        m_vsync ? L"On" : L"Off",
        m_lightEnabled ? L"On" : L"Off",
        m_lightAutoOrbit ? L"On" : L"Off",
        m_showRandomCubes ? L"On" : L"Off",
        m_showTestCube ? L"On" : L"Off",
        m_frustumOffset.x, m_frustumOffset.y, m_frustumOffset.z);

    SetWindowTextW(hwnd, t);
}

// ============================================================================
// Light helpers
// ============================================================================
float3 Renderer::ComputeLightDir() const
{
    float cy = cosf(m_lightYaw), sy = sinf(m_lightYaw);
    float cp = cosf(m_lightPitch), sp = sinf(m_lightPitch);
    float3 dir{ cy * cp, sp, sy * cp }; 
    float L = length(dir);
    return (L > 1e-6f) ? (dir * (1.0f / L)) : float3{ 0.0f, 1.0f, 0.0f }; 
}
void Renderer::UpdateLight(float dt)
{
    if (m_lightAutoOrbit) {
        m_lightYaw += m_dayNightSpeed * dt; // orbit around Y
    }

    float3 dir = ComputeLightDir();
    float3 target = m_testCubePos;
    float3 up{ 0,1,0 };
    float  dist = 30.0f;
    float3 pos = target - dir * dist;

    m_lightView = look_at(pos, target, up);

    // INCREASE ORTHO BOUNDS FOR BETTER COVERAGE
    float orthoHalf = 5.0f; // WAS 25.0f
    m_lightProj = ortho_off_center(-orthoHalf, +orthoHalf, -orthoHalf, +orthoHalf, 1.0f, 200.0f);
}

// ============================================================================
// Record world draws
// ============================================================================
void Renderer::RecordDrawCalls(ID3D12GraphicsCommandList* cmd)
{
    auto rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += SIZE_T(m_rtvDescriptorSize) * SIZE_T(m_frameIndex);
    auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    const float clr[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    cmd->ClearRenderTargetView(rtv, clr, 0, nullptr);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr); // STANDARD Z clear

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    if (m_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetGraphicsRootDescriptorTable(1, m_shadowSrv); // reg t0
    }

    float4x4 V = m_camera.GetView();
    float4x4 P = m_camera.GetProj();
    float4x4 lightVP = m_mul(m_lightView, m_lightProj);  // lightView × lightProj

    auto bindMVP = [&](const float4x4& M, float3 lightDir)
        {
            SceneCB cb{};
            float4x4 MVP = m_mul(M, m_mul(V, P));
            float4x4 lightMVP = m_mul(M, lightVP);

            WriteCB(MVP, cb, lightDir, 0.0f, 0.0f, 0.0f, &lightMVP);  

            UINT off = (UINT)((m_cbHead + 255) & ~255u);
            m_cbHead = off + sizeof(SceneCB);
            memcpy(m_cbMapped + off, &cb, sizeof(SceneCB));
            cmd->SetGraphicsRootConstantBufferView(0, m_cbUpload->GetGPUVirtualAddress() + off);
        };
    auto bindMVP_Lines = [&](const float4x4& M, float thicknessPx)
        {
            SceneCB cb{};
            float4x4 MVP = m_mul(M, m_mul(V, P));
            float3 L = ComputeLightDir();

            // Light MVP for shadows: M × lightView × lightProj
            float4x4 lightMVP = m_mul(M, m_mul(m_lightView, m_lightProj));

            WriteCB(MVP, cb, m_lightEnabled ? L : float3{ 0,0,0 }, (float)m_width, (float)m_height, thicknessPx, &lightMVP);

            UINT off = (UINT)((m_cbHead + 255) & ~255u);
            m_cbHead = off + sizeof(SceneCB);
            memcpy(m_cbMapped + off, &cb, sizeof(SceneCB));
            cmd->SetGraphicsRootConstantBufferView(0, m_cbUpload->GetGPUVirtualAddress() + off);
        };

    // Frustum from player
    // Frustum camera = player position + user-controlled offset
    m_playerCam.SetPosition(m_player.pos + m_frustumOffset);
    // Build frustum using render camera's orientation and player position + offset
    float4x4 RCW = m_camera.GetCameraToWorld();
    float3 fwd = { RCW[2].x, RCW[2].y, RCW[2].z };
    float3 up = { RCW[1].x, RCW[1].y, RCW[1].z };
    float4x4 FrCW = camera_to_world(m_player.pos + m_frustumOffset, fwd, up);


    struct Plane { float3 n{}; float d{}; };
    struct Fr6 { Plane p[6]; float3 c[8]; };

    auto cross3 = [](const float3& a, const float3& b)->float3 {
        return float3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        };
    auto normalize3 = [](const float3& v)->float3 {
        float L = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (L <= 0.0f) return float3{ 0,0,0 };
        float inv = 1.0f / L; return float3{ v.x * inv, v.y * inv, v.z * inv };
        };
    auto makePlane = [&](const float3& a, const float3& b, const float3& c)->Plane {
        float3 n = normalize3(cross3(b - a, c - a));
        float d = -(n.x * a.x + n.y * a.y + n.z * a.z);
        return { n,d };
        };

    auto buildWorldFrustum = [&](Fr6& F, const float4x4& CW, float fovY, float aspect, float zn, float zf)
        {
            const float3 right{ CW[0].x, CW[0].y, CW[0].z };
            const float3 up{ CW[1].x, CW[1].y, CW[1].z };
            const float3 fwd{ CW[2].x, CW[2].y, CW[2].z };
            const float3 pos{ CW[3].x, CW[3].y, CW[3].z };

            const float t = std::tan(fovY * 0.5f);
            const float nh = t * zn, nw = nh * aspect;
            const float fh = t * zf, fw = fh * aspect;

            const float3 nc = pos + fwd * zn;
            const float3 fc = pos + fwd * zf;

            const float3 ntl = nc + up * nh - right * nw;
            const float3 ntr = nc + up * nh + right * nw;
            const float3 nbl = nc - up * nh - right * nw;
            const float3 nbr = nc - up * nh + right * nw;
            const float3 ftl = fc + up * fh - right * fw;
            const float3 ftr = fc + up * fh + right * fw;
            const float3 fbl = fc - up * fh - right * fw;
            const float3 fbr = fc - up * fh + right * fw;

            F.c[0] = ntl; F.c[1] = ntr; F.c[2] = nbl; F.c[3] = nbr;
            F.c[4] = ftl; F.c[5] = ftr; F.c[6] = fbl; F.c[7] = fbr;

            F.p[0] = makePlane(nbl, ntl, ftl); // Left
            F.p[1] = makePlane(nbr, fbr, ftr); // Right
            F.p[2] = makePlane(nbl, fbl, fbr); // Bottom
            F.p[3] = makePlane(ntl, ftl, ftr); // Top
            F.p[4] = makePlane(ntl, ntr, nbr); // Near
            F.p[5] = makePlane(ftr, ftl, fbl); // Far

            const float3 ctr{
                (ntl.x + ntr.x + nbl.x + nbr.x + ftl.x + ftr.x + fbl.x + fbr.x) * (1.0f / 8.0f),
                (ntl.y + ntr.y + nbl.y + nbr.y + ftl.y + ftr.y + fbl.y + fbr.y) * (1.0f / 8.0f),
                (ntl.z + ntr.z + nbl.z + nbr.z + ftl.z + ftr.z + fbl.z + fbr.z) * (1.0f / 8.0f)
            };
            for (int i = 0; i < 6; i++) {
                float s = F.p[i].n.x * ctr.x + F.p[i].n.y * ctr.y + F.p[i].n.z * ctr.z + F.p[i].d;
                if (s > 0.0f) { F.p[i].n = float3{ -F.p[i].n.x,-F.p[i].n.y,-F.p[i].n.z }; F.p[i].d = -F.p[i].d; }
            }
    };

    auto aabbIntersectsFrustum = [&](const AABB_t& b, const Fr6& F)->bool
        {
            const float3 c = b.center, e = b.extents;
            for (int i = 0; i < 6; i++) {
                const float3 n = F.p[i].n;
                const float  s = n.x * c.x + n.y * c.y + n.z * c.z + F.p[i].d;
                const float  r = std::fabs(n.x) * e.x + std::fabs(n.y) * e.y + std::fabs(n.z) * e.z;
                if (s > r) return false;
            }
            return true;
    };

    float nearZ = m_useCullOverride ? m_cullNear : m_playerCam.GetNearZ();
    float farZ = m_useCullOverride ? m_cullFar : m_playerCam.GetFarZ();
    if (farZ <= nearZ + 0.001f) farZ = nearZ + 0.001f;

    Fr6 F{};
    buildWorldFrustum(F, m_playerCam.GetCameraToWorld(), m_playerCam.GetFovY(), m_playerCam.GetAspect(), nearZ, farZ);

    // SOLID GROUND (white) for shadows
    {
        struct VPNC { float3 p; float3 n; float3 c; };
        const float3 groundCol{ 0.5f, 0.5f, 0.5f }; // WHITE ground
        VPNC ground[6] = {
            {{-50,0,-50},{0,1,0},groundCol},
            {{ 50,0,-50},{0,1,0},groundCol},
            {{ 50,0, 50},{0,1,0},groundCol},
            {{-50,0,-50},{0,1,0},groundCol},
            {{ 50,0, 50},{0,1,0},groundCol},
            {{-50,0, 50},{0,1,0},groundCol},
        };

        const UINT bytes = sizeof(ground);
        ComPtr<ID3D12Resource> dyn; D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = MakeBufferDesc(bytes);
        ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dyn)));
        void* mp = nullptr; ThrowIfFailed(dyn->Map(0, nullptr, &mp)); memcpy(mp, ground, bytes); dyn->Unmap(0, nullptr);
        D3D12_VERTEX_BUFFER_VIEW vb{}; vb.BufferLocation = dyn->GetGPUVirtualAddress(); vb.StrideInBytes = sizeof(VPNC); vb.SizeInBytes = bytes;

        cmd->SetPipelineState(m_pso.Get());  // <--- LIT SHADER (NOT unlit!)
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &vb);

        // USE PROPER LIGHTING
        float3 L = ComputeLightDir();
        bindMVP(m_identity(), m_lightEnabled ? L : float3{ 0,0,0 });

        cmd->DrawInstanced(6, 1, 0, 0);
        m_transientUploads.push_back(dyn);
    }

    // GRID
    if (m_showGrid) {
        cmd->SetPipelineState(m_psoLines.Get());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        cmd->IASetVertexBuffers(0, 1, &m_vbLinesView);
        bindMVP_Lines(m_identity(),1.00f);
        cmd->DrawInstanced(m_lineRanges.gridCount, 1, m_lineRanges.gridStart, 0);
    }

    // RANDOMIZED BOXES (culled)
    if (m_showRandomCubes && !m_debugBoxes.empty()) {
        std::vector<VertexPC> lines; lines.reserve(m_debugBoxes.size() * 24);
        auto addBox = [&](const AABB_t& b, const float3& col) {
            const float3 c = b.center, e = b.extents;
            const float3 p[8] = {
                {c.x - e.x,c.y - e.y,c.z - e.z},{c.x + e.x,c.y - e.y,c.z - e.z},
                {c.x - e.x,c.y + e.y,c.z - e.z},{c.x + e.x,c.y + e.y,c.z - e.z},
                {c.x - e.x,c.y - e.y,c.z + e.z},{c.x + e.x,c.y - e.y,c.z + e.z},
                {c.x - e.x,c.y + e.y,c.z + e.z},{c.x + e.x,c.y + e.y,c.z + e.z}
            };
            static const int E[12][2] = { {0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{3,7},{2,6} };
            for (int i = 0; i < 12; i++) { lines.push_back({ p[E[i][0]], col }); lines.push_back({ p[E[i][1]], col }); }
            };
        for (const auto& bx : m_debugBoxes)
            if (aabbIntersectsFrustum(bx.aabb, F)) addBox(bx.aabb, bx.color);

        if (!lines.empty()) {
            const UINT bytes = (UINT)lines.size() * (UINT)sizeof(VertexPC);
            ComPtr<ID3D12Resource> dyn; D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = MakeBufferDesc(bytes);
            ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dyn)));
            void* mp = nullptr; ThrowIfFailed(dyn->Map(0, nullptr, &mp)); memcpy(mp, lines.data(), bytes); dyn->Unmap(0, nullptr);
            D3D12_VERTEX_BUFFER_VIEW v{}; v.BufferLocation = dyn->GetGPUVirtualAddress(); v.StrideInBytes = sizeof(VertexPC); v.SizeInBytes = bytes;

            cmd->SetPipelineState(m_psoLines.Get());
            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            cmd->IASetVertexBuffers(0, 1, &v);
            bindMVP_Lines(m_identity(), 2.5f);
            cmd->DrawInstanced((UINT)lines.size(), 1, 0, 0);
            m_transientUploads.push_back(dyn);
        }
    }

    // PLAYER AXES
    {
        cmd->SetPipelineState(m_psoLines.Get());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        cmd->IASetVertexBuffers(0, 1, &m_vbLinesView);
        float4x4 Mplayer = m_translation(m_player.pos);
        bindMVP_Lines(Mplayer, 2.5f);
        cmd->DrawInstanced(m_lineRanges.axesCount, 1, m_lineRanges.axesStart, 0);
    }

    // TEST CUBE (lit)
    if (m_showTestCube) {
        cmd->SetPipelineState(m_pso.Get());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &m_vbTrisView);

        float4x4 M = m_trs(m_testCubePos, q_from_axis_angle(float3{ 0,1,0 }, m_testCubeYaw), m_testCubeScale);
        float3 L = ComputeLightDir();
        bindMVP(M, m_lightEnabled ? L : float3{ 0,0,0 });
        cmd->DrawInstanced(m_vertexCountTris, 1, 0, 0);
    }

    // FRUSTUM VIZ
    if (m_showPlayerFrustum) {
        std::vector<VertexPC> fr; fr.reserve(24 + 12);
        auto add = [&](const float3& a, const float3& b, const float3& col) {
            fr.push_back({ a,col }); fr.push_back({ b,col });
            };
        const float3 edgeCol{ 1,1,0 };
        // near
        add(F.c[0], F.c[1], edgeCol); add(F.c[1], F.c[3], edgeCol); add(F.c[3], F.c[2], edgeCol); add(F.c[2], F.c[0], edgeCol);
        // far
        add(F.c[4], F.c[5], edgeCol); add(F.c[5], F.c[7], edgeCol); add(F.c[7], F.c[6], edgeCol); add(F.c[6], F.c[4], edgeCol);
        // connectors
        add(F.c[0], F.c[4], edgeCol); add(F.c[1], F.c[5], edgeCol); add(F.c[2], F.c[6], edgeCol); add(F.c[3], F.c[7], edgeCol);

        // detector normals
        auto centroid4 = [](const float3& a, const float3& b, const float3& c, const float3& d)->float3 {
            return float3{ (a.x + b.x + c.x + d.x) * 0.25f, (a.y + b.y + c.y + d.y) * 0.25f, (a.z + b.z + c.z + d.z) * 0.25f };
            };
        const float3 nearCtr = centroid4(F.c[0], F.c[1], F.c[2], F.c[3]);
        const float3 farCtr = centroid4(F.c[4], F.c[5], F.c[6], F.c[7]);
        const float3 leftCtr = centroid4(F.c[0], F.c[2], F.c[4], F.c[6]);
        const float3 rightCtr = centroid4(F.c[1], F.c[3], F.c[5], F.c[7]);
        const float3 topCtr = centroid4(F.c[0], F.c[1], F.c[4], F.c[5]);
        const float3 botCtr = centroid4(F.c[2], F.c[3], F.c[6], F.c[7]);
        const float len = length(farCtr - nearCtr) * 0.15f;

        const float3 colLeft{ 1.0f,0.25f,0.25f }, colRight{ 0.25f,1.0f,0.25f };
        const float3 colTop{ 0.25f,0.25f,1.0f }, colBottom{ 1.0f,0.0f,1.0f };
        const float3 colNear{ 0.0f,1.0f,1.0f }, colFar{ 1.0f,1.0f,0.0f };

        add(leftCtr, leftCtr + F.p[0].n * len, colLeft);
        add(rightCtr, rightCtr + F.p[1].n * len, colRight);
        add(botCtr, botCtr + F.p[2].n * len, colBottom);
        add(topCtr, topCtr + F.p[3].n * len, colTop);
        add(nearCtr, nearCtr + F.p[4].n * len, colNear);
        add(farCtr, farCtr + F.p[5].n * len, colFar);

        const UINT bytes = (UINT)fr.size() * (UINT)sizeof(VertexPC);
        if (bytes) {
            ComPtr<ID3D12Resource> dyn; D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = MakeBufferDesc(bytes);
            ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dyn)));
            void* mp = nullptr; ThrowIfFailed(dyn->Map(0, nullptr, &mp)); memcpy(mp, fr.data(), bytes); dyn->Unmap(0, nullptr);
            D3D12_VERTEX_BUFFER_VIEW vb{}; vb.BufferLocation = dyn->GetGPUVirtualAddress(); vb.StrideInBytes = sizeof(VertexPC); vb.SizeInBytes = bytes;

            cmd->SetPipelineState(m_psoLines.Get());
            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            cmd->IASetVertexBuffers(0, 1, &vb);
            bindMVP_Lines(m_identity(), 2.5f);
            cmd->DrawInstanced((UINT)fr.size(), 1, 0, 0);

            m_transientUploads.push_back(dyn);
        }
    }
}

// HUD (crosshair; can add more later)
void Renderer::RenderHUD(ID3D12GraphicsCommandList* cmd)
{
    struct Vtx { float3 pos; float3 color; };
    std::vector<Vtx> hud; hud.reserve(4096);

    auto addRect = [&](float x0, float y0, float x1, float y1, const float3& c)
        {
            hud.push_back({ {x0,y0,0}, c }); hud.push_back({ {x1,y0,0}, c }); hud.push_back({ {x1,y1,0}, c });
            hud.push_back({ {x0,y0,0}, c }); hud.push_back({ {x1,y1,0}, c }); hud.push_back({ {x0,y1,0}, c });
        };

    // 7-seg digits ------------------------------------------------------------
    auto addH = [&](float x, float y, float w, float t, const float3& c) { addRect(x, y, x + w, y + t, c); };
    auto addV = [&](float x, float y, float t, float h, const float3& c) { addRect(x, y, x + t, y + h, c); };

    auto addDigit = [&](int d, float x, float y, float s, float th, const float3& c)
        {
            bool seg[7] = {};
            switch (d) {
            case 0: seg[0] = seg[1] = seg[2] = seg[3] = seg[4] = seg[5] = true; break;
            case 1: seg[1] = seg[2] = true; break;
            case 2: seg[0] = seg[1] = seg[6] = seg[4] = seg[3] = true; break;
            case 3: seg[0] = seg[1] = seg[6] = seg[2] = seg[3] = true; break;
            case 4: seg[5] = seg[6] = seg[1] = seg[2] = true; break;
            case 5: seg[0] = seg[5] = seg[6] = seg[2] = seg[3] = true; break;
            case 6: seg[0] = seg[5] = seg[6] = seg[2] = seg[3] = seg[4] = true; break;
            case 7: seg[0] = seg[1] = seg[2] = true; break;
            case 8: for (int i = 0; i < 7; i++) seg[i] = true; break;
            case 9: seg[0] = seg[1] = seg[2] = seg[3] = seg[5] = seg[6] = true; break;
            }
            float x0 = x, x1 = x + s, y0 = y, ym = y + s, y2 = y + 2 * s;
            if (seg[0]) addH(x0 + th, y0, s - 2 * th, th, c);
            if (seg[1]) addV(x1 - th, y0 + th, th, s - 2 * th, c);
            if (seg[2]) addV(x1 - th, ym + th, th, s - 2 * th, c);
            if (seg[3]) addH(x0 + th, y2 - th, s - 2 * th, th, c);
            if (seg[4]) addV(x0, ym + th, th, s - 2 * th, c);
            if (seg[5]) addV(x0, y0 + th, th, s - 2 * th, c);
            if (seg[6]) addH(x0 + th, ym - th * 0.5f, s - 2 * th, th, c);
        };

    auto addFloat = [&](float value, float x, float y, float s, float th, const float3& c)
        {
            if (value < 0) value = -value;
            int v10 = (int)(value * 10.0f + 0.5f);
            int d1 = (v10 / 1000) % 10, d2 = (v10 / 100) % 10, d3 = (v10 / 10) % 10, d4 = v10 % 10;
            float dx = 0;
            if (d1) { addDigit(d1, x + dx, y, s, th, c); dx += s * 1.6f; }
            addDigit(d2, x + dx, y, s, th, c); dx += s * 1.6f;
            addDigit(d3, x + dx, y, s, th, c); // decimal point
            addRect(x + dx + s * 0.9f, y + 2 * s - th, x + dx + s * 1.1f, y + 2 * s, c); dx += s * 1.4f;
            addDigit(d4, x + dx, y, s, th, c);
        };

    // Colors used for HUD
    const float3 dark{ 1.00f,1.00f,1.00f };
    const float3 dim{ 1.00f,1.00f,1.00f };
    const float3 on{ 0.10f,0.70f,0.10f };
    const float3 off{ 0.70f,0.10f,0.10f };
    const float3 yel{ 0.95f,0.85f,0.10f };

    const float W = (float)m_width, H = (float)m_height;

    // Crosshair 
    {
        const float cx = W * 0.5f, cy = H * 0.5f, len = 30.0f, th = 4.0f;
        addRect(cx - len, cy - th * 0.5f, cx + len, cy + th * 0.5f, dark);
        addRect(cx - th * 0.5f, cy - len, cx + th * 0.5f, cy + len, dark);
    }

    // FPS label + number
    {
        float x = 16.0f, y = 16.0f;
        // simple "FPS" label
        // F
        addV(x, y, 2.0f, 2 * 10.0f, yel); addH(x, y, 10.0f, 2.0f, yel); addH(x, y + 10.0f, 8.0f, 2.0f, yel);
        x += 10.0f * 1.8f;
        // P
        addV(x, y, 2.0f, 2 * 10.0f, yel); addH(x, y, 10.0f, 2.0f, yel); addV(x + 10.0f - 2.0f, y, 2.0f, 10.0f, yel); addH(x, y + 10.0f, 10.0f, 2.0f, yel);
        x += 10.0f * 1.8f;
        // S
        addH(x, y, 10.0f, 2.0f, yel); addV(x, y, 2.0f, 10.0f, yel); addH(x, y + 10.0f, 10.0f, 2.0f, yel); addV(x + 10.0f - 2.0f, y + 10.0f, 2.0f, 10.0f, yel); addH(x, y + 20.0f - 2.0f, 10.0f, 2.0f, yel);

        // number
        float fps = (m_lastFPS > 0 ? m_lastFPS : 0.0f);
        addFloat(fps, 16.0f + 10.0f * 1.8f * 3.0f + 12.0f, 16.0f, 9.0f, 2.0f, dark);
    }

    // POS / YAW / PITCH / SPEED
    {
        float x = 16.0f, y = 16.0f + 32.0f;

        // camera world position
        float3 cp = m_camera.GetPosition();
        addFloat(cp.x, x, y, 8.0f, 1.6f, dim); x += 8.0f * 5.0f;
        addFloat(cp.y, x, y, 8.0f, 1.6f, dim); x += 8.0f * 5.0f;
        addFloat(cp.z, x, y, 8.0f, 1.6f, dim);

        // yaw / pitch
        float4x4 CW = m_camera.GetCameraToWorld();
        float3 fwd{ CW[2].x, CW[2].y, CW[2].z };
        float yaw = std::atan2f(fwd.x, fwd.z) * 57.2957795f;
        float pitch = std::asin(SOL_MAX(-1.0f, SOL_MIN(1.0f, fwd.y))) * 57.2957795f;

        x = 16.0f; y += 24.0f;
        addFloat(yaw, x, y, 8.0f, 1.6f, dim); x += 8.0f * 5.0f;
        addFloat(pitch, x, y, 8.0f, 1.6f, dim);

        // speed (pixels/frame estimate)
        static float3 lastPos = { 0,0,0 }; static bool initPos = false;
        float speed = 0.0f;
        if (!initPos) { lastPos = cp; initPos = true; }
        else { float3 d = cp - lastPos; speed = length(d) * 60.0f; lastPos = cp; }
        x = 16.0f; y += 24.0f;
        addFloat(speed, x, y, 8.0f, 1.6f, dim);
    }

    // Toggle boxes (Light, Shadows, Grid, Frustum, Test, Random)
    {
        float x = 16.0f, y = H - 16.0f - 12.0f;
        auto box = [&](bool onOff) { addRect(x, y, x + 12.0f, y + 12.0f, onOff ? on : off); x += 16.0f; };
        box(m_lightEnabled);
        box(m_shadowsEnabled);
        box(m_showGrid);
        box(m_showPlayerFrustum);
        box(m_showTestCube);
        box(m_showRandomCubes);
    }

    if (hud.empty()) return;

    // Upload vertices
    const UINT bytes = (UINT)(hud.size() * sizeof(Vtx));
    ComPtr<ID3D12Resource> dyn;
    D3D12_HEAP_PROPERTIES hu{}; hu.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = bytes;
    rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dyn)));
    void* mp = nullptr; ThrowIfFailed(dyn->Map(0, nullptr, &mp)); memcpy(mp, hud.data(), bytes); dyn->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vb{};
    vb.BufferLocation = dyn->GetGPUVirtualAddress();
    vb.StrideInBytes = sizeof(Vtx);
    vb.SizeInBytes = bytes;

    // Pixel-space ortho
    const float l = 0, r = W, t = 0, b = H, zn = 0, zf = 1;
    float4x4 P = m_identity();
    P[0].x = 2.0f / (r - l);
    P[1].y = 2.0f / (t - b);
    P[2].z = 1.0f / (zf - zn);
    P[3].x = -(r + l) / (r - l);
    P[3].y = -(t + b) / (t - b);
    P[3].z = -zn / (zf - zn);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    if (m_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetGraphicsRootDescriptorTable(1, m_shadowSrv); // harmless for HUD
    }

    auto bindHUD = [&](const float4x4& M)
        {
            SceneCB cb{}; float4x4 MVP = m_mul(M, P);
            WriteCB(MVP, cb, /*light*/float3{ 0,0,0 }, 0.0f, 0.0f, 0.0f, nullptr);
            UINT off = (UINT)((m_cbHead + 255) & ~255u);
            m_cbHead = off + sizeof(SceneCB);
            memcpy(m_cbMapped + off, &cb, sizeof(SceneCB));
            cmd->SetGraphicsRootConstantBufferView(0, m_cbUpload->GetGPUVirtualAddress() + off);
        };

    cmd->SetPipelineState(m_psoNoDepth.Get());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vb);
    bindHUD(m_identity());
    cmd->DrawInstanced((UINT)hud.size(), 1, 0, 0);

    m_transientUploads.push_back(dyn);
}


// Depth-only shadow pass
void Renderer::RenderShadowPass(ID3D12GraphicsCommandList* cmd)
{
    if (!m_shadowsEnabled || !m_lightEnabled) return;

    if (m_shadowState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = m_shadowTex.Get();
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = m_shadowState;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        cmd->ResourceBarrier(1, &b);
        m_shadowState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    cmd->RSSetViewports(1, &m_shadowViewport);
    cmd->RSSetScissorRects(1, &m_shadowScissor);
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowDsv);
    cmd->ClearDepthStencilView(m_shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_psoShadow.Get());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbTrisView);

    float4x4 VP = m_mul(m_lightView, m_lightProj);
    auto bindShadow = [&](const float4x4& M)
    {
            SceneCB cb{};
            float4x4 MVP = m_mul(M, VP);
            float4x4 lightMVP = m_mul(M, VP);  // SAME as main pass!
            WriteCB(MVP, cb, ComputeLightDir(), 0, 0, 0, &lightMVP);  // Pass lightMVP
            UINT off = (UINT)((m_cbHead + 255) & ~255u);
            m_cbHead = off + sizeof(SceneCB);
            memcpy(m_cbMapped + off, &cb, sizeof(SceneCB));
            cmd->SetGraphicsRootConstantBufferView(0, m_cbUpload->GetGPUVirtualAddress() + off);
    };

    float4x4 M = m_trs(m_testCubePos, q_from_axis_angle(float3{ 0,1,0 }, m_testCubeYaw), m_testCubeScale);
    bindShadow(M);
    cmd->DrawInstanced(m_vertexCountTris, 1, 0, 0);

    if (m_shadowState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = m_shadowTex.Get();
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = m_shadowState;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &b);
        m_shadowState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

// ============================================================================
// Render
// ============================================================================
void Renderer::Render()
{
    auto t0 = std::chrono::high_resolution_clock::now();

    ThrowIfFailed(m_cmdAlloc[m_frameIndex]->Reset());
    ThrowIfFailed(m_cmdList->Reset(m_cmdAlloc[m_frameIndex].Get(), nullptr));

    RenderShadowPass(m_cmdList.Get());

    D3D12_RESOURCE_BARRIER toRT{};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = m_backBuffers[m_frameIndex].Get();
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toRT);

    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissor);

    RecordDrawCalls(m_cmdList.Get());
    RenderHUD(m_cmdList.Get());

    D3D12_RESOURCE_BARRIER toPresent{};
    toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toPresent.Transition.pResource = m_backBuffers[m_frameIndex].Get();
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &toPresent);

    ThrowIfFailed(m_cmdList->Close());
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_cmdQueue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(m_swapchain->Present(m_vsync ? 1 : 0, 0));
    MoveToNextFrame();

    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = t1 - t0;
    m_lastFrameMs = (float)ms.count();
    m_frameTimes[m_ftHead] = m_lastFrameMs;
    m_ftHead = (m_ftHead + 1) % (int)m_frameTimes.size();
}
