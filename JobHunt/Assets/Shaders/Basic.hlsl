// Basic.hlsl — Unlit debug (lines + HUD). Simple lines with no thickness.
#pragma pack_matrix(column_major)

cbuffer SceneCB : register(b0)
{
    float4x4 mvp;
    float3 lightDir; // unused here
    float _pad0; // 16B align

    float2 viewport; // width, height in pixels (unused now)
    float thicknessPx; // unused now
    float _pad1; // 16B align

    float4x4 lightVP; // unused here
};

struct VSIn
{
    float3 pos : POSITION;
    float3 color : COLOR0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 color : COLOR0;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), mvp);
    o.color = i.color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return float4(i.color, 1.0f);
}