#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexCoord = vin.TexCoord;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // 纹理查询得到光线位置(观察空间)
    float4 rayPos = _texOutVertex.Sample(samLinearWarp, pin.TexCoord);
    float4 Color = float4(0.f, 0.f, 0.f, 1.f);
    if (rayPos.z < 300) {
        // 得到当前像素视点编号
        // uint viewNum = rayPos.w;
        // rayPos.w = 1.0f;
        
        uint viewNum = _texEncoding.Sample(samLinearWarp, pin.TexCoord);
        //uint viewNum = testViewNum;
        float camDeltaX = (viewNum - (_ViewNum/2.0f - 0.5f)) * _PerCamDeltaX;
        float4 ShearViewExternalT = ViewExternalT;
        ShearViewExternalT.x += camDeltaX;
        //*************************
        float4 view_leftCampos = SrcCampos_DstCampos(
        rayPos,
        ViewExternalR,
        ShearViewExternalT,
        LeftExternalR,
        LeftExternalT
        );
        float2 view_leftPix = Campos_Pix(view_leftCampos, LeftInternal);
        float4 view_midCampos = SrcCampos_DstCampos(
        rayPos,
        ViewExternalR,
        ShearViewExternalT,
        MidExternalR,
        MidExternalT
        );
        float2 view_midPix = Campos_Pix(view_midCampos, MidInternal);
        float4 view_rightCampos = SrcCampos_DstCampos(
        rayPos,
        ViewExternalR,
        ShearViewExternalT,
        RightExternalR,
        RightExternalT
        );
        float2 view_rightPix = Campos_Pix(view_rightCampos, RightInternal);
        
        // pixel缩放至uv
        float2 leftTexPix = view_leftPix / float2(color_pixel_x, color_pixel_y);
        
        float2 midTexPix = view_midPix / float2(color_pixel_x, color_pixel_y);

        float2 rightTexPix = view_rightPix / float2(color_pixel_x, color_pixel_y);

        float left_shadowmap = texDepth.Sample(samLinearWarp, float3(leftTexPix, 0.0f));
        float mid_shadowmap = texDepth.Sample(samLinearWarp, float3(midTexPix, 1.0f));
        float right_shadowmap = texDepth.Sample(samLinearWarp, float3(rightTexPix, 2.0f));
        
        
        
        /*float4 c_l*/ Color= ((left_shadowmap < (view_leftCampos.z + 2.0f)) && (left_shadowmap > (view_leftCampos.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(leftTexPix, 0.0f)) : Color);
        /*float4 c_r*/ Color= ((right_shadowmap < (view_rightCampos.z + 2.0f)) && (right_shadowmap > (view_rightCampos.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(rightTexPix, 2.0f)) : Color);
        /*float4 c_m*/ Color= ((mid_shadowmap < (view_midCampos.z + 2.0f)) && (mid_shadowmap > (view_midCampos.z - 2.0f)) ? texColor.Sample(samLinearWarp, float3(midTexPix, 1.0f)) : Color);
        //float4 c = float4(1, 1, 1, 1);


        
        //pin.Color *= 0.5f;
        //Color = c_l + c_m + c_r;
        Color.w = 1.0f;
        
        
        // 伽马矫正
        Color = pow(Color, (1 / 2.2f));
    }
    
    return Color;
}