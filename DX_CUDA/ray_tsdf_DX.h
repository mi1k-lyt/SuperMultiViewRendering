#pragma once
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include <iostream>
#include <stdio.h>
#include <fstream>



class RAYTSDF :public D3DApp {
public:
	RAYTSDF(HINSTANCE hInstance);
	RAYTSDF(const RAYTSDF& rhs) = delete;
	RAYTSDF& operator=(const RAYTSDF& rhs) = delete;
	~RAYTSDF();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;


	void UpdateUploadVertexBuffer();



	void BuildRootSignature();
	void BuildShaderAndInputLayout();
	void BuildTexSrvHeap();
	void BuildSampleHeapAndDesc();
	void BuildFrameResource();
	void BuildTexture();
	void BuildMat();
	/*void BuildDescriptorHeaps();
	void BuildConstantBufferView();*/
	void BuildPSO();

	


	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const;

private:
	////CUDA共享资源
	//Microsoft::WRL::ComPtr<ID3D12Resource> cuVertex;
	//cudaExternalMemory_t cuExMem;
	//Vertex* d_crood;
	
	//temp
	//std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	

	//根签名
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	//ShaderCode和输入布局
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	//帧资源 存顶点
	const int mVertexCount = 307200;
	const int NumFrameResource = NumFrameRrc;
	int mCurrentFrameResourceIndex = 0;
	FrameResource* mCurrentFrameResource;
	std::vector<std::unique_ptr<FrameResource>> mAllFrameResource;
	//当前帧顶点/索引缓冲
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrentVertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrentIndexBufferGPU = nullptr;
	//描述符堆
	/*Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;*/
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mTexSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSampleHeap = nullptr;
	//管线状态对象
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	//变换矩阵参数
	std::unique_ptr<UploadBuffer<cbMat>> mCbMat = nullptr;
	std::unique_ptr<UploadBuffer<CamMat>> mCamMat = nullptr;
	const float n_plane = up_left_corner_z;
	const float f_plane = 110.0f;
	DirectX::XMFLOAT4X4 mWorldMat = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 mViewMat = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProjMat = MathHelper::Identity4x4();
};



