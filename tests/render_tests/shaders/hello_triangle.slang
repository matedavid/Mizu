struct VertexInput
{
    float3 pos : POSITION;
    float2 tex_coord : TEXCOORD0;
    float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 tex_coord : TEXCOORD0;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.pos, 1.0f);
    output.tex_coord = input.tex_coord;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0f);
}
