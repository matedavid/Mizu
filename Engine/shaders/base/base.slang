struct CameraInfo
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
    float4x4 inverse_view;
    float4x4 inverse_proj;
    float4x4 inverse_view_proj;
    float3 pos;
    float _pad0;
    float znear;
    float zfar;
    float2 _pad1;
};

struct InstanceTransformInfo
{
    float4x4 transform;
    float4x4 normal_matrix;
};
