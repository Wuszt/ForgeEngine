#include "CommonPP.fxh"

Texture2D Tex : register(t0);

static float2 offset = float2(1 / 1920.0f, 1 / 1080.0f);

float4 PS(VS_OUTPUT input) : SV_Target
{
    float leftUp = Tex.Sample(PointSampler, input.Tex + float2(-1, 1) * offset).r;
    float leftMid = Tex.Sample(PointSampler, input.Tex + float2(-1, 0) * offset).r;
    float leftDown = Tex.Sample(PointSampler, input.Tex + float2(-1, -1) * offset).r;

    float midUp = Tex.Sample(PointSampler, input.Tex + float2(0, 1) * offset).r;
    float midMid = Tex.Sample(PointSampler, input.Tex + float2(0, 0) * offset).r;
    float midDown = Tex.Sample(PointSampler, input.Tex + float2(0, -1) * offset).r;

    float rightUp = Tex.Sample(PointSampler, input.Tex + float2(1, 1) * offset).r;
    float rightMid = Tex.Sample(PointSampler, input.Tex + float2(1, 0) * offset).r;
    float rightDown = Tex.Sample(PointSampler, input.Tex + float2(1, -1) * offset).r;

    float x = leftUp + 2 * midUp + rightUp - leftDown - 2 * midDown - rightDown;
    float y = leftUp + 2 * leftMid + leftDown - rightUp - 2 * rightMid - rightDown;

    float magnitude = sqrt(x * x + y * y);
    
    x = (x + 1.0f) / 2.0f;
    y = (y + 1.0f) / 2.0f;

    return float4(x, y, magnitude, 1.0f);
}