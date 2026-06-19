
Texture2D BaseTexture : register(t0);
SamplerState Sampler  : register(s0);

struct PixelShaderInput
{
    float2 texCoords : TEXCOORD;
    float4 normal : NORMAL;
};

float4 main(PixelShaderInput IN) : SV_Target
{
    float4 baseColor = BaseTexture.Sample(Sampler, IN.texCoords);
    return baseColor;
}