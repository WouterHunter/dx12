struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexStaticMesh
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VertexShaderOutput
{
    float2 texCoords : TEXCOORD;
    float4 normal : NORMAL;
    float4 position : SV_Position;
};

VertexShaderOutput main(VertexStaticMesh IN)
{
    VertexShaderOutput OUT;

    OUT.position = mul(ModelViewProjectionCB.MVP, float4(IN.position, 1.0f));
    OUT.normal = mul(ModelViewProjectionCB.MVP, float4(IN.normal, 0.0f));
    OUT.texCoords = IN.texCoord;

    return OUT;
}