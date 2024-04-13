#pragma once
#ifndef __FRAMERESOURCE_H__
#define __FRAMERESOURCE_H__
#include "dx12win.h"
//#include "cuda.h"
#include "stb_image/stb_image.h"

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/cvstd.hpp>
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

	void* leftColorImg = nullptr;
	void* midColorImg = nullptr;
	void* rightColorImg = nullptr;

	void* leftDepthImg = nullptr;
	void* midDepthImg = nullptr;
	void* rightDepthImg = nullptr;

	
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

private:
	void* ReadTiffDepthImage(const char* filePath);
	
};

#endif // __FRAMERESOURCE_H__
