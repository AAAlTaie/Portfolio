// BasicLit.hlsl — Directional Lambert + shadows (standard Z)

#pragma pack_matrix(column_major)

cbuffer SceneCB : register(b0)
{
    float4x4 mvp; // M * (View * Proj)
    float3 lightDir; // world dir FROM light TO scene
    float _pad0;

    // Unused here (shared layout with unlit)
    float2 viewport;
    float thicknessPx;
    float _pad1;

    float4x4 lightVP; // M * (LightView * LightProj) for shadow lookup
};

Texture2D ShadowMap : register(t0);
SamplerState LinearClamp : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 n : NORMAL;
    float3 color : COLOR0;
    float4 lightPos : TEXCOORD0; // position in light clip space
};

PSIn VSMainLit(VSIn i)
{
    PSIn o;
    float4 wp = float4(i.pos, 1.0f);
    o.pos = mul(wp, mvp);
    o.lightPos = mul(wp, lightVP);

    // For rigid transforms with uniform scale this is fine
    o.n = normalize(i.normal);
    o.color = i.color;
    return o;
}

float ShadowFactor(float4 lightPos)
{
    // Project to NDC then to UV
    float3 p = lightPos.xyz / max(lightPos.w, 1e-6f);
    
    // FIX: Use correct D3D clip space Y direction
    float2 uv = float2(p.x * 0.5f + 0.5f, p.y * -0.5f + 0.5f);

    // Sample only if within map
    bool inRange = (uv.x >= 0.0f && uv.x <= 1.0f &&
                    uv.y >= 0.0f && uv.y <= 1.0f &&
                    p.z >= 0.0f && p.z <= 1.0f);
    
    if (!inRange)
    {
        return 1.0f; // Outside shadow map = fully lit
    }
    else
    {
        float mapDepth = ShadowMap.SampleLevel(LinearClamp, uv, 0).r;
        const float depthBias = 0.0005f;
        float bias = depthBias;
        return (p.z > mapDepth + bias) ? 0.0f : 1.0f;
    }
}

float4 PSMainLit(PSIn i) : SV_Target
{
    // 1. Lambert lighting
    float3 L = normalize(-lightDir); // Direction TO light
    float3 N = normalize(i.n);
    float NdL = max(dot(N, L), 0.0f);
    
    // 2. Shadow factor
    float shadow = ShadowFactor(i.lightPos);
    
    // 3. Combine color * light * shadow
    float3 litColor = i.color * NdL * shadow;
    
    // Optional: add small ambient so shadows aren't pure black
    float3 ambient = float3(0.05f, 0.05f, 0.05f);
    litColor = max(litColor, ambient * i.color);
    
    return float4(litColor, 1.0f);
}