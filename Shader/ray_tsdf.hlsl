cbuffer cbMat : register(b0)
{
    float4x4 WorldMat;
    float4x4 ViewMat;
    float4x4 ProjMat;
    float4x4 ViewProjMat;
};
cbuffer camMat : register(b1)
{
    float4x4 ViewInternalMat;
    float4x4 ViewExternalMat_R;
    float4 ViewExternalMat_T;
    
    float4x4 LeftInternalMat;
    float4x4 LeftExternalMat_R;
    float4 LeftExternalMat_T;
    
    float4x4 MidInternalMat;
    float4x4 MidExternalMat_R;
    float4 MidExternalMat_T;
    
    float4x4 RightInternalMat;
    float4x4 RightExternalMat_R;
    float4 RightExternalMat_T;
};

// 0:left  1:mid  2:right
Texture2DArray texColor : register(t0);
Texture2DArray texDepth : register(t1);

SamplerState samLinearWarp : register(s0);
SamplerComparisonState samShadowMap : register(s1);

#define src_pixel_Nx 1280.0f
#define src_pixel_Ny 720.0f
#define crop_src_pixel_Nx 320.0f
#define crop_src_pixel_Ny 120.0f

float4 ShadowMapTrans(
    const float4 posH,
    const float4x4 dstExternalR,
    const float4 dstExternalT,
    const float4x4 dstInternal)
{
    
    float4 src_cam_pos;
    src_cam_pos.x = ((posH.x - ViewInternalMat[0][2]) / ViewInternalMat[0][0]) * abs(posH.w);
    src_cam_pos.y = ((posH.y - ViewInternalMat[1][2]) / ViewInternalMat[1][1]) * abs(posH.w);
    src_cam_pos.z = posH.w;
    src_cam_pos.w = 1.0f;
    float3 world_pos = mul((src_cam_pos - ViewExternalMat_T), ViewExternalMat_R).xyz;
    
    // 旋转矩阵的逆等于旋转矩阵的转置
    float4 dst_cam_pos;
    dst_cam_pos = (mul(float4(world_pos, 1.0f), transpose(dstExternalR)) + (dstExternalT));
    float4 dst_pix_depth;
    float temp_depth = dst_cam_pos.z;
    
    // 透视除法
    dst_cam_pos /= abs(dst_cam_pos.z);
    // 此刻图像二维坐标原点在图像中心
    dst_pix_depth.xy = mul(dst_cam_pos, dstInternal).xy;
    dst_pix_depth.x += dstInternal[0][2];
    dst_pix_depth.y += dstInternal[1][2];
    
    dst_pix_depth.x = (dst_pix_depth.x + crop_src_pixel_Nx) / src_pixel_Nx;
    dst_pix_depth.y = (dst_pix_depth.y + crop_src_pixel_Ny) / src_pixel_Ny;
    dst_pix_depth.z = temp_depth;
    
    return dst_pix_depth;
}

struct VertexIn
{
    float3 PosL : POSITION;
    //float4 Color : COLOR;
    
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    float4 posW = mul(float4(vin.PosL, 1.0f), WorldMat);
    vout.PosH = mul(posW, ViewProjMat);
    //vout.PosH /= vout.PosH.w;
    vout.Color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return vout;
}


float4 PS(VertexOut pin) : SV_Target
{
    /*
        shadow map顶点位置转换   
    */
    float4 left_pix_depth = ShadowMapTrans(pin.PosH, LeftExternalMat_R, LeftExternalMat_T, LeftInternalMat);
    float4 mid_pix_depth = ShadowMapTrans(pin.PosH, MidExternalMat_R, MidExternalMat_T, MidInternalMat);
    float4 right_pix_depth = ShadowMapTrans(pin.PosH, RightExternalMat_R, RightExternalMat_T, RightInternalMat);
    /*
        shadow map算法模拟
    */
    //float left_shadowmap = texDepth.SampleCmpLevelZero(samShadowMap, float3(left_pix_depth.xy,0.0f), left_pix_depth.z).r;
    //float mid_shadowmap = texDepth.SampleCmpLevelZero(samShadowMap, float3(mid_pix_depth.xy, 1.0f), mid_pix_depth.z).r;
    //float right_shadowmap = texDepth.SampleCmpLevelZero(samShadowMap, float3(right_pix_depth.xy, 2.0f), right_pix_depth.z).r;
    
    float left_shadowmap = texDepth.Sample(samLinearWarp, float3(left_pix_depth.xy, 0.0f))/10.0f;
    float mid_shadowmap = texDepth.Sample(samLinearWarp, float3(mid_pix_depth.xy, 1.0f))/10.0f;
    float right_shadowmap = texDepth.Sample(samLinearWarp, float3(right_pix_depth.xy, 2.0f))/10.0f;
    
    //float parm = pin.Color == 0 ? 1 : 0.9245129346808465;
    
    float4 c_l = 0.9245129346808465 * ((left_shadowmap < (left_pix_depth.z + 2.0f)) && (left_shadowmap > (left_pix_depth.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(left_pix_depth.xy, 0.0f)) : pin.Color);
    float4 c_m = 0.07406844288584068 * ((mid_shadowmap < (mid_pix_depth.z + 2.0f)) && (mid_shadowmap > (mid_pix_depth.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(mid_pix_depth.xy, 1.0f)) : pin.Color);
    float4 c_r = 0.0014186224333128585 * ((right_shadowmap < (right_pix_depth.z + 2.0f)) && (right_shadowmap > (right_pix_depth.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(right_pix_depth.xy, 2.0f)) : pin.Color);
    
    //if (!(c_m == 0 && c_r == 0))
    //    c_l = 0.0004534305949426343 * c_l;
    
    //if (!(c_l == 0 && c_r == 0))
    //    c_m *= 0.27551889009352776 * c_m;
    
    //if (!(c_l == 0 && c_m == 0))
    //    c_r *= 0.7240276793115297 * c_r;
    
    pin.Color = c_l + c_m + c_r;
    
    //pin.Color *= 0.5f;
    pin.Color.w = 1.0f;
    
    //pin.Color = texColor.Sample(samLinearWarp, float3(0.01f, 0.54f, 2.0f));
    //float t = texDepth.Sample(samLinearWarp, float3(0.5f, 0.6f, 0.0f));
    //pin.Color.w *= t;
    
    //伽马矫正
    pin.Color = pow(pin.Color, (1 / 2.2f));
    return pin.Color;
}