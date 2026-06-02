
Texture2D BaseTexture : register(t0);
SamplerState Sampler  : register(s0);

struct PixelShaderInput
{
    float2 texCoords : TEXCOORD;
    float4 normal : NORMAL;
};

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = float2(IN.texCoords.x, 1.0 - IN.texCoords.y);
    float4 baseColor = BaseTexture.Sample(Sampler, uv);
    return pow(abs(baseColor), 1.0f / 2.2f);
}