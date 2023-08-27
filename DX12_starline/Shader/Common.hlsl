cbuffer cbMat : register(b0)
{
    float4x4 WorldMat;
    float4x4 ViewMat;
    float4x4 ProjMat;
    float4x4 ViewProjMat;
};
cbuffer camMat : register(b1)
{
    float4x4 ViewInternal;
    float4x4 ViewExternalR;
    float4 ViewExternalT;
    
    float4x4 LeftInternal;
    float4x4 LeftExternalR;
    float4 LeftExternalT;
    
    float4x4 MidInternal;
    float4x4 MidExternalR;
    float4 MidExternalT;
    
    float4x4 RightInternal;
    float4x4 RightExternalR;
    float4 RightExternalT;
};

// 0:left  1:mid  2:right
Texture2DArray texColor : register(t0);
Texture2DArray texDepth : register(t1);
Texture2D _texEncoding : register(t2); //8k
Texture2D _texOutVertex : register(t3); //8k

RWTexture2D<float> texEncoding : register(u0); //8k
RWTexture2DArray<float2> texSplatting : register(u1); //640*360  x:min , y:max
RWTexture2D<float4> texOutVertex : register(u2); //8k

SamplerState samLinearWarp : register(s0);
SamplerComparisonState samShadowMap : register(s1);
//***************************************************************
// 分辨率参数
//***************************************************************
#define total_pixel_x 7680
#define total_pixel_y 4320

#define view_pixel_x 960
#define view_pixel_y 540

#define splatting_pixel_x 480
#define splatting_pixel_y 270

#define depth_pixel_x 960
#define depth_pixel_y 540
#define NumDepth 3

#define crop_depth_pixel_x 0
#define crop_depth_pixel_y 0

#define color_pixel_x 960
#define color_pixel_y 540
#define NumColor 3

#define crop_color_pixel_x 0
#define crop_color_pixel_y 0
//***************************************************************
// 近平面参数
//***************************************************************
#define down_left_x -2.0f
#define down_left_y -1.125f
#define down_left_z 7.4641f
#define scales_x 4.0f
#define scales_y 2.25f
//***************************************************************
// 光场编码参数
//***************************************************************
#define _ViewNum 70
#define _LineNum 30.90091
#define _IncAngle 0.1832
#define _shift 23
#define _k 2
#define _ViewDist 300.0f
#define _PerCamDeltaX 5.7971f

#define testViewNum 34
//***************************************************************
// vulkan坐标系
float4 Pix_Campos(
    const float depth,
    const uint2 pix,
    const float4x4 Internal)
{
    return float4(
    ((pix.x - Internal[2][0]) / Internal[0][0]) * depth,
    ((pix.y - Internal[2][1]) / Internal[1][1]) * depth,
    depth,
    1.0f);
}

float2 Campos_Pix(
    const float4 srcCampos,
    const float4x4 Internal)
{
    // 透视除法
    float4 dstCampos = srcCampos / srcCampos.z;
    dstCampos.w = 1.0f;
    float4 dstPix = mul(dstCampos, Internal);

    return float2(dstPix.x + 0.5f, dstPix.y + 0.5f);
}

float4 RH_Ue(const float4 LHCoord) {
    float4 UeCoord;
    UeCoord.x = LHCoord.z;
    UeCoord.y = LHCoord.x;
    UeCoord.z = -LHCoord.y;
    UeCoord.w = 1.0f;

    return UeCoord;
}

float4 Ue_RH(float4 UeCoord) {
    float4 LHCoord;
    LHCoord.x = UeCoord.y;
    LHCoord.y = -UeCoord.z;
    LHCoord.z = UeCoord.x;
    LHCoord.w = 1.0f;

    return LHCoord;
}

float4 SrcCampos_DstCampos(
    const float4 srcCampos,
    const float4x4 srcExternalR,
    const float4 srcExternalT,
    const float4x4 dstExternalR,
    const float4 dstExternalT)
{
    float4 _srcCampos = RH_Ue(srcCampos);
    // 转换到世界坐标
    float4 _worldPos = mul(_srcCampos, transpose(srcExternalR));
    float4 worldPos = _worldPos + srcExternalT;
    // 转换到目标相机坐标
    float4 _dstCampos = worldPos - dstExternalT;
    float4 dstCampos = mul(_dstCampos, dstExternalR);
    
    return Ue_RH(dstCampos);
}

struct Ray
{
    float4 origin;
    float4 dir;
};

Ray RayCreate(float2 uv, float rayDeltaX)
{
    Ray r;
    r.origin = float4(0,0,0,1);
    r.dir = float4(
    down_left_x + uv.x * scales_x - rayDeltaX,
    down_left_y + uv.y * scales_y,
    down_left_z,
    0.f
    );
    
    return r;
}

float4 RayPos(Ray r, float t)
{
    return t * r.dir + r.origin;
}