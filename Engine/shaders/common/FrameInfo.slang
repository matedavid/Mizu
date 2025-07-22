struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float4x4 inverseView;
    float4x4 inverseProj;
    float4x4 inverseViewProj;
    float3 pos;
}
