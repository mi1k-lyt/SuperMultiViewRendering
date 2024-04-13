#include "Common.hlsl"

[numthreads(32, 32, 1)]// 8k
void LightFieldEncoding(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float step_value = 1.0f / _ViewNum * _LineNum;
    float value_pixel = dispatchThreadID.x * 3 + 3 * dispatchThreadID.y * _IncAngle + _k;
    
    float judge_value = value_pixel - int(value_pixel / _LineNum) * _LineNum;
    if (judge_value < 0)
    {
        judge_value = judge_value + _LineNum;
    }
    

    texEncoding[dispatchThreadID.xy] = _ViewNum - uint(fmod(floor(judge_value / step_value) + _shift, _ViewNum)) - 1;
}

[numthreads(20, 20, 1)]// 深度分辨率
void Splatting(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 裁剪坐标
    float2 pix;
    pix = dispatchThreadID.xy;
    pix.x += crop_depth_pixel_x;
    pix.y += crop_depth_pixel_y;
    // 缩放至uv坐标
    float2 texpix;// = pix / float2(depth_pixel_x, depth_pixel_y);
    texpix.x = pix.x / float(depth_pixel_x);
    texpix.y = pix.y / float(depth_pixel_y);
    pix.x += 0.5f;
    pix.y += 0.5f;
    // 获得深度值，单位:cm
    float depth_left = texDepth.SampleLevel(samLinearWarp, float3(texpix, 0.0f), 0);
    float depth_mid = texDepth.SampleLevel(samLinearWarp, float3(texpix, 1.0f), 0);
    float depth_right = texDepth.SampleLevel(samLinearWarp, float3(texpix, 2.0f), 0);
    
    for (uint i = 0; i < _ViewNum; ++i)
    {
        //*************************************************************
        // 光场编码求出错切
        //*************************************************************
        float camDeltaX = (i - (_ViewNum/2.0f - 0.5f)) * _PerCamDeltaX;
        float pixDeltaX = (camDeltaX / _ViewDist) * ViewInternal[0][0];
        float4 ShearViewExternalT = ViewExternalT;
        ShearViewExternalT.x += camDeltaX;
        // 各个深度图相机转换到错切视点
        // 左
        float4 leftCampos = Pix_Campos(depth_left, uint2(pix), LeftInternal);
        float4 left_viewCampos = SrcCampos_DstCampos(
        leftCampos,
        LeftExternalR,
        LeftExternalT,
        ViewExternalR,
        ShearViewExternalT);
        float2 left_viewPix = Campos_Pix(left_viewCampos, ViewInternal);
        // shear
        left_viewPix.x += pixDeltaX;

        // 中
        float4 midCampos = Pix_Campos(depth_mid, uint2(pix), MidInternal);
        float4 mid_viewCampos = SrcCampos_DstCampos(
        midCampos,
        MidExternalR,
        MidExternalT,
        ViewExternalR,
        ShearViewExternalT);
        float2 mid_viewPix = Campos_Pix(mid_viewCampos, ViewInternal);
        //shear
        mid_viewPix.x += pixDeltaX;

        // 右
        float4 rightCampos = Pix_Campos(depth_right, uint2(pix), RightInternal);
        float4 right_viewCampos = SrcCampos_DstCampos(
        rightCampos,
        RightExternalR,
        RightExternalT,
        ViewExternalR,
        ShearViewExternalT);
        float2 right_viewPix = Campos_Pix(right_viewCampos, ViewInternal);
        //shear
        right_viewPix.x += pixDeltaX;

        // 缩小两倍
        left_viewPix /= 2;
        mid_viewPix /= 2;
        right_viewPix /= 2;
        // 比较
        // depth R:min G:max
        // 左
        if (left_viewPix.x >= 0
            && left_viewPix.x < splatting_pixel_x
            && left_viewPix.y >= 0
            && left_viewPix.y < splatting_pixel_y)
        {
            float minValue = min(texSplatting[uint3(left_viewPix.xy, i)].x, left_viewCampos.z);
            float maxValue = max(texSplatting[uint3(left_viewPix.xy, i)].y, left_viewCampos.z);
            if(minValue==0) {
                minValue = left_viewCampos.z;
            }
            texSplatting[uint3(left_viewPix.xy, i)] = float2(
            minValue,
            maxValue);
        }
        // 中
        if (mid_viewPix.x >= 0
            && mid_viewPix.x < splatting_pixel_x
            && mid_viewPix.y >= 0
            && mid_viewPix.y < splatting_pixel_y)
        {
            float minValue = min(texSplatting[uint3(mid_viewPix.xy, i)].x, mid_viewCampos.z);
            float maxValue = max(texSplatting[uint3(mid_viewPix.xy, i)].y, mid_viewCampos.z);
            if(minValue==0) {
                minValue = mid_viewCampos.z;
            }
            texSplatting[uint3(mid_viewPix.xy, i)] = float2(
            minValue,
            maxValue);
        }
        // 右
        if (right_viewPix.x >= 0
            && right_viewPix.x < splatting_pixel_x
            && right_viewPix.y >= 0
            && right_viewPix.y < splatting_pixel_y)
        {
            float minValue = min(texSplatting[uint3(right_viewPix.xy, i)].x, right_viewCampos.z);
            float maxValue = max(texSplatting[uint3(right_viewPix.xy, i)].y, right_viewCampos.z);
            if(minValue==0) {
                minValue = right_viewCampos.z;
            }
            texSplatting[uint3(right_viewPix.xy, i)] = float2(
            minValue,
            maxValue);
        }
    }
}

[numthreads(32, 32, 1)]// 8k
void Raycast(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    
    float2 splattingPix = dispatchThreadID.xy / 16 + 0.5;//(total_pixel_xy / splatting_pixel_xy);
    float2 viewPix = dispatchThreadID.xy / 8 + 0.5;
    
    //******************************************************************
    // 光场编码求出错切
    //******************************************************************
    uint viewNum = texEncoding[dispatchThreadID.xy];
    //uint viewNum = testViewNum;
    float camDeltaX = (viewNum - (_ViewNum/2.0f - 0.5f)) * _PerCamDeltaX;
    float rayDeltaX = (camDeltaX / _ViewDist) * down_left_z;
    float4 ShearViewExternalT = ViewExternalT;
    ShearViewExternalT.x += camDeltaX;
    //******************************************************************
    
    // 生成光线
    float2 uv = float2(
    viewPix.x / float(view_pixel_x), 
    viewPix.y / float(view_pixel_y));
    Ray r = RayCreate(uv, rayDeltaX);
    
    // 得到光线范围
    float2 rayRange = texSplatting[uint3(splattingPix, viewNum)];
    
    // 求出光线范围步长
    float step = rayRange.x / r.dir.z;
    float step_max = rayRange.y / r.dir.z;
    
    // 设定TSDF初值
    float s = 5.1e-02;
    
    //***************************************************************
    // 光线投射过程
    //***************************************************************
    // while (s > 5.0e-2 && step < step_max)
    // {
    //     // 初值化TSDF
    //     s = 0.0f;
    //     // 生成光线当前位置
    //     float4 rayPos = RayPos(r, step);
    //     // 光线从错切视点转换到深度相机
    //     //左
    //     float4 leftCampos = SrcCampos_DstCampos(
    //     rayPos,
    //     ViewExternalR,
    //     ShearViewExternalT,
    //     LeftExternalR,
    //     LeftExternalT);
    //     float2 leftPix = Campos_Pix(leftCampos, LeftInternal);
    //     // 中
    //     float4 midCampos = SrcCampos_DstCampos(
    //     rayPos,
    //     ViewExternalR,
    //     ShearViewExternalT,
    //     MidExternalR,
    //     MidExternalT);
    //     float2 midPix = Campos_Pix(midCampos, MidInternal);
    //     // 右
    //     float4 rightCampos = SrcCampos_DstCampos(
    //     rayPos,
    //     ViewExternalR,
    //     ShearViewExternalT,
    //     RightExternalR,
    //     RightExternalT);
    //     float2 rightPix = Campos_Pix(rightCampos, RightInternal);
    //     //********************************************************
    //     // 动态融合TSDF
    //     // 单位:cm
    //     //********************************************************
    //     // 左
    //     float inLeftRange = float(
    //     leftPix.x >= 0 &&
    //     leftPix.x < view_pixel_x &&
    //     leftPix.y >=0 &&
    //     leftPix.y < view_pixel_y);
    //     // 获得depth
    //     float2 leftTexPix = float2(
    //     (leftPix.x + crop_depth_pixel_x) / float(depth_pixel_x),
    //     (leftPix.y + crop_depth_pixel_y) / float(depth_pixel_y));
    //     float depth_left = texDepth.SampleLevel(samLinearWarp, float3(leftTexPix, 0.f), 0);
    //     // 计算sdf
    //     float sj_left = depth_left - leftCampos.z;
    //     // 高方差区域降噪
    //     float wj_left = 0.0f;
    //     float w_left = 0.0f;
    //     //********************************************************
    //     // 方差解耦
    //     //********************************************************      
        
    //     //********************************************************
    //     float omega_left = 0.082f; //prec / sqrt(w_left);
    //     wj_left = min(min(omega_left, 1.0f), !(sj_left < -2.0f));
    //     wj_left = 0.0f * (1.0f - inLeftRange) + wj_left * inLeftRange;
    //     // 计算TSDF
    //     s += wj_left * max(-2.0f, min(2.0f, sj_left));

    //     // 中
    //     float inMidRange = float(
    //     midPix.x >= 0 &&
    //     midPix.x < view_pixel_x &&
    //     midPix.y >= 0 &&
    //     midPix.y < view_pixel_y);
    //     // 获得depth
    //     float2 midTexPix = float2(
    //     (midPix.x + crop_depth_pixel_x) / float(depth_pixel_x),
    //     (midPix.y + crop_depth_pixel_y) / float(depth_pixel_y));
    //     float depth_mid = texDepth.SampleLevel(samLinearWarp, float3(midTexPix, 1.f), 0);
    //     // 计算sdf
    //     float sj_mid = depth_mid - midCampos.z;
    //     // 高方差区域降噪
    //     float wj_mid = 0.0f;
    //     float w_mid = 0.0f;
    //     //********************************************************
    //     // 方差解耦
    //     //********************************************************
        
    //     //********************************************************
    //     float omega_mid = 0.082f; //prec / sqrt(w_mid);
    //     wj_mid = min(min(omega_mid, 1.0f), !(sj_mid < -2.0f));
    //     wj_mid = 0.0f * (1.0f - inMidRange) + wj_mid * inMidRange;
    //     // 计算TSDF
    //     s += wj_mid * max(-2.0f, min(2.0f, sj_mid));
        
    //     // 右
    //     float inRightRange = float(
    //     rightPix.x >= 0 &&
    //     rightPix.x < view_pixel_x &&
    //     rightPix.y >= 0 &&
    //     rightPix.y < view_pixel_y);
    //     // 获得depth
    //     float2 rightTexPix = float2(
    //     (rightPix.x + crop_depth_pixel_x) / float(depth_pixel_x),
    //     (rightPix.y + crop_depth_pixel_y) / float(depth_pixel_y));
    //     float depth_right = texDepth.SampleLevel(samLinearWarp, float3(rightTexPix, 2.f), 0);
    //     // 计算sdf
    //     float sj_right = depth_right - rightCampos.z;
    //     // 高方差区域降噪
    //     float wj_right = 0.0f;
    //     float w_right = 0.0f;
    //     //********************************************************
    //     // 方差解耦
    //     //********************************************************
        
    //     //********************************************************
    //     float omega_right = 0.082f; //prec / sqrt(w_right);
    //     wj_right = min(min(omega_right, 1.0f), !(sj_right < -2.0f));
    //     wj_right = 0.0f * (1.0f - inRightRange) + wj_right * inRightRange;
    //     // 计算TSDF
    //     s += wj_right * max(-2.0f, min(2.0f, sj_right));
          
    //     // 更新步长
    //     step += 0.8 * s;
    // }   
    
    // 获得最终光线位置
    float4 rayPos = RayPos(r, step);
    // rayPos.w = viewNum;
    
    texOutVertex[dispatchThreadID.xy] = rayPos;
}

