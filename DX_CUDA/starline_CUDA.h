#pragma once
#include <fstream>
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdio.h>
#include "d3dUtil.h"
#include "MathHelper.h"

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    //DirectX::XMFLOAT4 Color;
    /*float3 Pos;
    float4 Color;*/
};

#define UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B-1)))

#define COLOR_TYPE uchar4
#define MAKE_COLOR(b,g,r,a) make_uchar4(b,g,r,a)
#define PARAM_TYPE float
#define COORD_TYPE float3
#define MAKE_COORD(x,y,z) make_float3(x,y,z)
#define PIXEL_TYPE short2
#define MAKE_PIXEL(x,y) make_short2(x,y)

#define MAT_3_3(mat_param,i,j) (*(mat_param+(3*(i))+(j)))
#define COL_3_1(col_param,i) (*(col_param+(i)))

#define pixel_Nx 640
#define pixel_Ny 480
#define point_splatting_Nx 320
#define point_splatting_Ny 240

#define src_pixel_color_Nx 1280
#define src_pixel_color_Ny 720
#define src_pixel_depth_Nx 1280
#define src_pixel_depth_Ny 720
#define crop_src_pixel_color_Nx 320//
#define crop_src_pixel_color_Ny 120//
#define crop_src_pixel_depth_Nx 320//
#define crop_src_pixel_depth_Ny 120//


#define up_left_corner_x -2.0f
#define up_left_corner_y -1.5f
#define up_left_corner_z 3.788f
#define horizontal_x 4.0f
#define horizontal_y 0.0f
#define horizontal_z 0.0f
#define vertical_x 0.0f
#define vertical_y 3.0f
#define vertical_z 0.0f

#define NumFrameRrc 3

#define NumColor 3
#define NumDepth 3

#define prec 0.01f



__constant__
PARAM_TYPE view_internal_param[9]
= { 607.447937,   0,   320,
0,   607.522095,   240,
0,   0,   1 };
__constant__
PARAM_TYPE R_view_external_param[9]
= { 0.97282915,   0.072511701, -0.21987611,
-0.035734491,   0.98533569,   0.16684311,
0.22874985, -0.15445268,   0.96115445 };
__constant__
PARAM_TYPE R_view_external_param_inverse[9]
= { 0.97282915, -0.03573449,  0.22874985,
    0.0725117,   0.98533569, -0.15445268,
    -0.21987611,  0.16684311,  0.96115445 };
__constant__
PARAM_TYPE T_view_external_param[3]
= { -0.063299724e+02, -0.049407028e+02, 0.76696249e+02 };
__constant__
PARAM_TYPE T_view_external_param_inverse[3]
= { 0.063299724e+02, 0.049407028e+02, -0.76696249e+02 };




__constant__
PARAM_TYPE left_internal_param[9]
= { 606.134,   0,   320,
0,   605.951,   240,
0,   0,   1 };
__constant__
PARAM_TYPE R_left_external_param[9]
= { 0.87402824,   0.04817779, -0.48348065,
-0.032189193,   0.99862732,   0.041319944,
0.48480769, -0.020551946,   0.87437928 };
__constant__
PARAM_TYPE R_left_external_param_inverse[9]
= { 0.87402824, -0.03218919,  0.48480769,
   0.04817779,  0.99862732, -0.02055195,
    -0.48348065,  0.04131994,  0.87437928 };
__constant__
PARAM_TYPE T_left_external_param[3]
= { 0.025058259e+02, -0.035155708e+02,   0.79021269e+02 };
__constant__
PARAM_TYPE T_left_external_param_inverse[3]
= { -0.025058259e+02, 0.035155708e+02,   -0.79021269e+02 };



__constant__
PARAM_TYPE mid_internal_param[9]
= { 605.924133,   0,   320,
0,   605.991394,   240,
0,   0,   1 };
__constant__
PARAM_TYPE R_mid_external_param[9]
= { 0.92238833,   0.016037037,   0.38593081,
-0.06859051,   0.99005924,  0.12279266,
-0.38012513, -0.1397337, 0.91431908 };
__constant__
PARAM_TYPE R_mid_external_param_inverse[9]
= { 0.92238833, -0.06859051, -0.38012513,
    0.01603704,  0.99005924, -0.1397337,
    0.38593081,  0.12279266,  0.91431908 };
__constant__
PARAM_TYPE T_mid_external_param[3]
= { -0.044661758e+02, -0.080205671e+02,   0.85012851e+02 };
__constant__
PARAM_TYPE T_mid_external_param_inverse[3]
= { 0.044661758e+02, 0.080205671e+02,   -0.85012851e+02 };


__constant__
PARAM_TYPE right_internal_param[9]
= { 603.888306,   0,   320,
0,   603.703613,   240,
0,   0,   1 };
__constant__
PARAM_TYPE R_right_external_param[9]
= { 0.79995099,   0.021275003,   0.59968808,
-0.021173286,   0.99974972, -0.0072239019,
-0.59969168, -0.0069185994,   0.80020124 };
__constant__
PARAM_TYPE R_right_external_param_inverse[9]
= { 0.79995099, -0.02117329, -0.59969168,
    0.021275,    0.99974972, -0.0069186,
    0.59968808, -0.0072239,   0.80020124 };
__constant__
PARAM_TYPE T_right_external_param[3]
= { -0.066983473e+02, -0.079096781e+02,   0.95016361e+02 };
__constant__
PARAM_TYPE T_right_external_param_inverse[3]
= { 0.066983473e+02, 0.079096781e+02,   -0.95016361e+02 };

__device__
class ray {
public:
	COORD_TYPE origin, dir, ray_pos;
	__device__ ray(const float u, const float v);
	__device__ void point_at_param(const float t);
};

__device__
float my_clamp(float data, float _t, float t);

__device__
COORD_TYPE SrcCampos_DstCampos(
    const COORD_TYPE* SrcCampos,
    const PARAM_TYPE* R_src_external_param,
    const PARAM_TYPE* T_src_external_param,
    const PARAM_TYPE* R_dst_external_param_inverse,
    const PARAM_TYPE* T_dst_external_param_inverse);

__device__
PIXEL_TYPE Campos_Pix(
    const COORD_TYPE* SrcCampos,
    const PARAM_TYPE* internal_param);

__device__
COORD_TYPE Pix_Campos(
    const float depth,
    const int pixel_x,
    const int pixel_y,
    const PARAM_TYPE* internal_param);

class CudaResource {
public:
    const int src_color_N = src_pixel_color_Nx * src_pixel_color_Ny;
    const int src_depth_N = src_pixel_depth_Nx * src_pixel_depth_Ny;
    const int N = pixel_Nx * pixel_Ny;
    const int point_splatting_N = point_splatting_Nx * point_splatting_Ny;
    const int Border_W = src_pixel_depth_Nx + 2;
    const int Border_H = src_pixel_depth_Ny + 2;
    const int Border_N = Border_W * Border_H;

    cv::Mat h_color_img_left;
    cv::Mat h_color_img_mid;
    cv::Mat h_color_img_right;

    cv::Mat h_depth_img_left;
    cv::Mat h_depth_img_mid;
    cv::Mat h_depth_img_right;
    cv::Mat h_depth_img_left_border;
    cv::Mat h_depth_img_mid_border;
    cv::Mat h_depth_img_right_border;

    COLOR_TYPE* p_h_color_img_left;
    COLOR_TYPE* p_h_color_img_mid;
    COLOR_TYPE* p_h_color_img_right;

    cv::uint16_t* p_h_depth_img_left;
    cv::uint16_t* p_h_depth_img_mid;
    cv::uint16_t* p_h_depth_img_right;
    cv::uint16_t* p_h_depth_img_left_border;
    cv::uint16_t* p_h_depth_img_mid_border;
    cv::uint16_t* p_h_depth_img_right_border;

    float* tex_depth_img_left;
    float* tex_depth_img_mid;
    float* tex_depth_img_right;

    cudaChannelFormatDesc color_tex_channel;
    
    cudaArray* cuA_color_img_left;
    struct cudaResourceDesc color_tex_left_resDesc;
    struct cudaTextureDesc color_tex_left_texDesc;
    

    cudaArray* cuA_color_img_mid;
    struct cudaResourceDesc color_tex_mid_resDesc;
    struct cudaTextureDesc color_tex_mid_texDesc;
    

    cudaArray* cuA_color_img_right;                                              
    struct cudaResourceDesc color_tex_right_resDesc;
    struct cudaTextureDesc color_tex_right_texDesc;
    

    cudaChannelFormatDesc depth_tex_channel;

    cudaArray* cuA_depth_img_left;
    struct cudaResourceDesc depth_tex_left_resDesc;
    struct cudaTextureDesc depth_tex_left_texDesc;
    

    cudaArray* cuA_depth_img_mid;
    struct cudaResourceDesc depth_tex_mid_resDesc;
    struct cudaTextureDesc depth_tex_mid_texDesc;
    

    cudaArray* cuA_depth_img_right;
    struct cudaResourceDesc depth_tex_right_resDesc;
    struct cudaTextureDesc depth_tex_right_texDesc;
    

    cudaArray* cuA_depth_img_left_border;
    struct cudaResourceDesc depth_tex_left_border_resDesc;
    struct cudaTextureDesc depth_tex_left_border_texDesc;
    

    cudaArray* cuA_depth_img_mid_border;
    struct cudaResourceDesc depth_tex_mid_border_resDesc;
    struct cudaTextureDesc depth_tex_mid_border_texDesc;
    

    cudaArray* cuA_depth_img_right_border;
    struct cudaResourceDesc depth_tex_right_border_resDesc;
    struct cudaTextureDesc depth_tex_right_border_texDesc;
    


    cudaChannelFormatDesc depth_tex_rasterize_channel;
    
    struct cudaResourceDesc depth_tex_rasterize_min_resDesc;
    struct cudaTextureDesc depth_tex_rasterize_min_texDesc;
    

    struct cudaResourceDesc depth_tex_rasterize_max_resDesc;
    struct cudaTextureDesc depth_tex_rasterize_max_texDesc;
    

    
public:
    CudaResource();
    ~CudaResource();

    void LoadImg();
    void UpdateTexture();
    void DestroyResource();

    float* d_depth_rasterize_min;
    float* d_depth_rasterize_max;

    cudaTextureObject_t color_tex_left_obj;
    cudaTextureObject_t color_tex_mid_obj;
    cudaTextureObject_t color_tex_right_obj;

    cudaTextureObject_t depth_tex_left_obj;
    cudaTextureObject_t depth_tex_mid_obj;
    cudaTextureObject_t depth_tex_right_obj;

    cudaTextureObject_t depth_tex_left_border_obj;
    cudaTextureObject_t depth_tex_mid_border_obj;
    cudaTextureObject_t depth_tex_right_border_obj;

    cudaTextureObject_t depth_tex_rasterize_min_obj;
    cudaTextureObject_t depth_tex_rasterize_max_obj;
};

__global__
void depth_img_point_splatting(
    float* depth_rasterize_min,
    float* depth_rasterize_max,
    cudaTextureObject_t depth_tex_left,
    cudaTextureObject_t depth_tex_mid,
    cudaTextureObject_t depth_tex_right);

__global__
void ray_cast(
    /*COORD_TYPE* crood,
    short4* color,*/
    Vertex* d_crood,
    cudaTextureObject_t color_tex_left,
    cudaTextureObject_t color_tex_mid,
    cudaTextureObject_t color_tex_right,
    cudaTextureObject_t depth_tex_left,
    cudaTextureObject_t depth_tex_mid,
    cudaTextureObject_t depth_tex_right,
    cudaTextureObject_t depth_tex_left_border,
    cudaTextureObject_t depth_tex_mid_border,
    cudaTextureObject_t depth_tex_right_border,
    cudaTextureObject_t depth_tex_rasterize_min,
    cudaTextureObject_t depth_tex_rasterize_max);





