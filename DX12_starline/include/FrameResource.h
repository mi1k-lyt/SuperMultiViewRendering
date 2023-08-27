#pragma once
#ifndef __FRAMERESOURCE_H__
#define __FRAMERESOURCE_H__
#include "dx12win.h"
//#include "cuda.h"
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <string>

struct Vertex {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT2 TexCoord;
};

struct CbMat {
	DirectX::XMFLOAT4X4 WorldMat;
	DirectX::XMFLOAT4X4 ViewMat;
	DirectX::XMFLOAT4X4 ProjMat;
	DirectX::XMFLOAT4X4 ViewProjMat;
};

struct CamMat {
	DirectX::XMFLOAT4X4 ViewInternalMat;
	DirectX::XMFLOAT4X4 ViewExternalMat_R;
	DirectX::XMFLOAT4 ViewExternalMat_T;

	DirectX::XMFLOAT4X4 LeftInternalMat;
	DirectX::XMFLOAT4X4 LeftExternalMat_R;
	DirectX::XMFLOAT4 LeftExternalMat_T;

	DirectX::XMFLOAT4X4 MidInternalMat;
	DirectX::XMFLOAT4X4 MidExternalMat_R;
	DirectX::XMFLOAT4 MidExternalMat_T;

	DirectX::XMFLOAT4X4 RightInternalMat;
	DirectX::XMFLOAT4X4 RightExternalMat_R;
	DirectX::XMFLOAT4 RightExternalMat_T;
};

class FrameResource {
public:
	FrameResource(ID3D12Device* device);

	// Test
	void LoadImg();

	cv::Mat _leftColorImg;
	cv::Mat _midColorImg;
	cv::Mat _rightColorImg;

	cv::Mat _leftDepthImg;
	cv::Mat _midDepthImg;
	cv::Mat _rightDepthImg;

	const void* leftColorImg;
	const void* midColorImg;
	const void* rightColorImg;

	const void* leftDepthImg;
	const void* midDepthImg;
	const void* rightDepthImg;

	
	//每帧资源命令分配器
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> FrameCmdListAlloc;

	//纹理资源
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorTex;//SRV
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorTexUpload;

	Microsoft::WRL::ComPtr<ID3D12Resource> DepthTex;//SRV
	Microsoft::WRL::ComPtr<ID3D12Resource> DepthTexUpload;

	Microsoft::WRL::ComPtr<ID3D12Resource> EncodingTex;//SRV & UAV
	Microsoft::WRL::ComPtr<ID3D12Resource> SplattingTex;//UAV
	Microsoft::WRL::ComPtr<ID3D12Resource> OutVertexTex;//SRV & UAV

	//Fence
	UINT64 Fence = 0;
	
};

#endif // __FRAMERESOURCE_H__
