#pragma once
#include "UploadBuffer.h"
#include "starline_CUDA.h"


#define UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B-1)))


struct cbMat {
	DirectX::XMFLOAT4X4 WorldMat = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewMat = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ProjMat = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProjMat = MathHelper::Identity4x4();

	/*DirectX::XMFLOAT3X3 ViewInternalMat;
	DirectX::XMFLOAT3X3 ViewExternalMat_R;
	DirectX::XMFLOAT3 ViewExternalMat_T;

	DirectX::XMFLOAT3X3 LeftInternalMat;
	DirectX::XMFLOAT3X3 LeftExternalMat_R;
	DirectX::XMFLOAT3 LeftExternalMat_T;

	DirectX::XMFLOAT3X3 MidInternalMat;
	DirectX::XMFLOAT3X3 MidExternalMat_R;
	DirectX::XMFLOAT3 MidExternalMat_T;

	DirectX::XMFLOAT3X3 RightInternalMat;
	DirectX::XMFLOAT3X3 RightExternalMat_R;
	DirectX::XMFLOAT3 RightExternalMat_T;*/
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

struct FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT VertexCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	void UpdateTexture();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> FrameCmdListAlloc;

	//std::unique_ptr<UploadBuffer<Vertex>> mUploadVB = nullptr;

	//CUDA共享资源
	Microsoft::WRL::ComPtr<ID3D12Resource> cuVertex;
	cudaExternalMemory_t cuExMem;
	
	//顶点
	Vertex* d_crood;
	
	//CUDA资源
	std::unique_ptr<CudaResource> mCuResource = nullptr;

	//纹理资源
	Microsoft::WRL::ComPtr<ID3D12Heap> mColorTexDefaultHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mColorTexDefault;
	Microsoft::WRL::ComPtr<ID3D12Heap> mColorTexUploadHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mColorTexUpload;

	Microsoft::WRL::ComPtr<ID3D12Heap> mDepthTexDefaultHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthTexDefault;
	Microsoft::WRL::ComPtr<ID3D12Heap> mDepthTexUploadHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthTexUpload;
	//BYTE* mTexImg = nullptr;


	//纹理内存资源布局
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT mColorTexLayouts[NumColor] = {};
	UINT mColorTextureRowNum[NumColor] = {};
	UINT64 mColorTextureRowSizes[NumColor] = {};
	UINT64 mColorRequiredSize = 0u;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT mDepthTexLayouts[NumDepth] = {};
	UINT mDepthTextureRowNum[NumDepth] = {};
	UINT64 mDepthTextureRowSizes[NumDepth] = {};
	UINT64 mDepthRequiredSize = 0u;
	
	
	//Fence
	UINT64 Fence = 0;
};