#include "ray_tsdf_DX.h"


RAYTSDF::RAYTSDF(HINSTANCE hInstance) :D3DApp(hInstance) {}
RAYTSDF::~RAYTSDF() {
	if (md3dDevice != nullptr) {
		FlushCommandQueue();
	}
}



bool RAYTSDF::Initialize() {
	if (!D3DApp::Initialize()) {
		return false;
	}
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//初始化过程
	BuildRootSignature();
	BuildShaderAndInputLayout();
	BuildFrameResource();
	BuildSampleHeapAndDesc();
	BuildTexSrvHeap();
	BuildTexture();
	BuildMat();
	//BuildDescriptorHeaps();
	//BuildConstantBufferView();
	BuildPSO();

	//初始化结束
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	//CPU等待GPU初始化完成
	FlushCommandQueue();
}

void RAYTSDF::OnResize() {
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), n_plane, f_plane);
	DirectX::XMStoreFloat4x4(&mProjMat, P);
}


//__global__
//void testCUDA(Vertex* d_crood) {
//	int i = blockDim.x * blockIdx.x + threadIdx.x;
//	int j = blockDim.y * blockIdx.y + threadIdx.y;
//	int id = gridDim.x * blockDim.x * j + i;
//	float3 pos = make_float3(float(i)-320.0f, float(j)-240.0f, 580.0f);
//	float4 color = make_float4(0.3, 1, 1, 255);
//	*(d_crood + id) = { pos,color };
//}



void RAYTSDF::BuildRootSignature() {

	//TexDescTable
	CD3DX12_DESCRIPTOR_RANGE TexDescTable = {};
	TexDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	CD3DX12_DESCRIPTOR_RANGE SamplerDescTable = {};
	SamplerDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);
	
	//根参数
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	slotRootParameter[0].InitAsDescriptorTable(1, &TexDescTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsDescriptorTable(1, &SamplerDescTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsConstantBufferView(0);
	slotRootParameter[3].InitAsConstantBufferView(1);
	

	//根签名
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	//用单个寄存器槽创建根签名
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);
	if (errorBlob != nullptr) {
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);
	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf()))
	);
} 

void RAYTSDF::BuildShaderAndInputLayout() {
	HRESULT hr = S_OK;

	mShader["VertexShader"] = d3dUtil::CompileShader(
		L"E:\\CGT\\starline\\DX_CUDA\\Shader\\ray_tsdf.hlsl",
		nullptr,
		"VS",
		"vs_5_0"
	);

	mShader["PixelShader"] = d3dUtil::CompileShader(
		L"E:\\CGT\\starline\\DX_CUDA\\Shader\\ray_tsdf.hlsl",
		nullptr,
		"PS",
		"ps_5_0"
	);

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }/*,
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }*/
	};
}

void RAYTSDF::BuildSampleHeapAndDesc() {
	D3D12_DESCRIPTOR_HEAP_DESC sampleHeapDesc = {};
	sampleHeapDesc.NumDescriptors = 2;
	sampleHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	sampleHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&sampleHeapDesc, 
		IID_PPV_ARGS(&mSampleHeap)
	));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hSampleHeap(mSampleHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_SAMPLER_DESC sampleLinearDesc = {};
	sampleLinearDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampleLinearDesc.MinLOD = 0;
	sampleLinearDesc.MaxLOD = 0;
	sampleLinearDesc.MaxAnisotropy = 1;
	sampleLinearDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampleLinearDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampleLinearDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampleLinearDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	md3dDevice->CreateSampler(&sampleLinearDesc, hSampleHeap);

	hSampleHeap.Offset(
		1,
		md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	);

	D3D12_SAMPLER_DESC sampleShadowMapDesc = {};
	sampleShadowMapDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
	sampleShadowMapDesc.MinLOD = 0;
	sampleShadowMapDesc.MaxLOD = 0;
	sampleShadowMapDesc.MaxAnisotropy = 1;
	sampleShadowMapDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	sampleShadowMapDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampleShadowMapDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampleShadowMapDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	md3dDevice->CreateSampler(&sampleShadowMapDesc, hSampleHeap);

}

void RAYTSDF::BuildTexSrvHeap() {
	D3D12_DESCRIPTOR_HEAP_DESC TexSrvHeapDesc = {};
	TexSrvHeapDesc.NumDescriptors = NumFrameResource * 2;
	TexSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TexSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&TexSrvHeapDesc,
		IID_PPV_ARGS(&mTexSrvHeap)
	));

	/*D3D12_DESCRIPTOR_HEAP_DESC DepthTexSrvHeapDesc = {};
	DepthTexSrvHeapDesc.NumDescriptors = NumFrameResource;
	DepthTexSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	DepthTexSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&DepthTexSrvHeapDesc,
		IID_PPV_ARGS(&mDepthTexSrvHeap)
	));*/
}

void RAYTSDF::BuildFrameResource() {
	for (int i = 0; i < NumFrameResource; ++i) {
		mAllFrameResource.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), mVertexCount));
	}
}



void RAYTSDF::BuildTexture() {

	CD3DX12_CPU_DESCRIPTOR_HANDLE hTexSrvHeap(mTexSrvHeap->GetCPUDescriptorHandleForHeapStart());
	//CD3DX12_CPU_DESCRIPTOR_HANDLE hDepthTexSrvHeap(mDepthTexSrvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < NumFrameResource; ++i) {
		D3D12_SUBRESOURCE_DATA teximg_color[NumColor] = {
			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->p_h_color_img_left),
			src_pixel_color_Nx * sizeof(COLOR_TYPE),
			src_pixel_color_Nx * src_pixel_color_Ny * sizeof(COLOR_TYPE)},

			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->p_h_color_img_mid),
			src_pixel_color_Nx * sizeof(COLOR_TYPE),
			src_pixel_color_Nx * src_pixel_color_Ny * sizeof(COLOR_TYPE)},

			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->p_h_color_img_right),
			src_pixel_color_Nx * sizeof(COLOR_TYPE),
			src_pixel_color_Nx * src_pixel_color_Ny * sizeof(COLOR_TYPE)},
		};

		D3D12_SUBRESOURCE_DATA teximg_depth[NumDepth] = {
			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->tex_depth_img_left),
			src_pixel_depth_Nx * sizeof(float),
			src_pixel_depth_Nx * src_pixel_depth_Ny * sizeof(float)},

			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->tex_depth_img_mid),
			src_pixel_depth_Nx * sizeof(float),
			src_pixel_depth_Nx * src_pixel_depth_Ny * sizeof(float)},

			{reinterpret_cast<const void*>(mAllFrameResource[i]->mCuResource->tex_depth_img_right),
			src_pixel_depth_Nx * sizeof(float),
			src_pixel_depth_Nx * src_pixel_depth_Ny * sizeof(float)},
		};
		

		UpdateSubresources(
			mCommandList.Get(),
			mAllFrameResource[i]->mColorTexDefault.Get(),
			mAllFrameResource[i]->mColorTexUpload.Get(),
			0,//intermediate offset
			0,//first subr
			NumColor,//num subr
			teximg_color
		);

		UpdateSubresources(
			mCommandList.Get(),
			mAllFrameResource[i]->mDepthTexDefault.Get(),
			mAllFrameResource[i]->mDepthTexUpload.Get(),
			0,//intermediate offset
			0,//first subr
			NumDepth,//num subr
			teximg_depth
		);


		mAllFrameResource[i]->mCuResource->UpdateTexture();
		//资源屏障转换
		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->mColorTexDefault.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		);

		mCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->mDepthTexDefault.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		);

		//创建colorTexSrv
		D3D12_SHADER_RESOURCE_VIEW_DESC colorTexSrvDesc = {};
		colorTexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		colorTexSrvDesc.Format = mAllFrameResource[i]->mColorTexDefault->GetDesc().Format;
		colorTexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		colorTexSrvDesc.Texture2DArray.ArraySize = mAllFrameResource[i]->mColorTexDefault->GetDesc().DepthOrArraySize;
		colorTexSrvDesc.Texture2DArray.MipLevels = mAllFrameResource[i]->mColorTexDefault->GetDesc().MipLevels;
		colorTexSrvDesc.Texture2DArray.FirstArraySlice = 0;
		colorTexSrvDesc.Texture2DArray.MostDetailedMip = 0;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->mColorTexDefault.Get(),
			&colorTexSrvDesc,
			hTexSrvHeap
		);
		//描述符堆偏移到下一个描述符
		hTexSrvHeap.Offset(1, mCbvSrvUavDescriptorSize);


		//创建depthTexSrv
		D3D12_SHADER_RESOURCE_VIEW_DESC depthTexSrvDesc = {};
		depthTexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthTexSrvDesc.Format = mAllFrameResource[i]->mDepthTexDefault->GetDesc().Format;
		depthTexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		depthTexSrvDesc.Texture2DArray.ArraySize = mAllFrameResource[i]->mDepthTexDefault->GetDesc().DepthOrArraySize;
		depthTexSrvDesc.Texture2DArray.MipLevels = mAllFrameResource[i]->mDepthTexDefault->GetDesc().MipLevels;
		depthTexSrvDesc.Texture2DArray.FirstArraySlice = 0;
		depthTexSrvDesc.Texture2DArray.MostDetailedMip = 0;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->mDepthTexDefault.Get(),
			&depthTexSrvDesc,
			hTexSrvHeap
		);
		//描述符堆偏移到下一个描述符
		hTexSrvHeap.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void RAYTSDF::BuildMat() {

	DirectX::XMVECTOR pos = DirectX::XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	DirectX::XMStoreFloat4x4(&mViewMat, view);
	DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProjMat);
	DirectX::XMMATRIX view_porj = DirectX::XMMatrixMultiply(view, proj);


	cbMat mcbmat;
	mcbmat.WorldMat = mWorldMat;
	DirectX::XMStoreFloat4x4(&mcbmat.ViewMat, DirectX::XMMatrixTranspose(view));
	DirectX::XMStoreFloat4x4(&mcbmat.ProjMat, DirectX::XMMatrixTranspose(proj));
	DirectX::XMStoreFloat4x4(&mcbmat.ViewProjMat, DirectX::XMMatrixTranspose(view_porj));

	// DX12 GPU矩阵只支持float4x4内置类型，所以以4x4方式传入3x3矩阵
	// DX12 GPU向量也只支持float4，所以float3同理
	// 这里可能是因为GPU内存需要64字节的对齐，而对于整体的constantBuf需要256字节对齐
	// 传进去会自动进行一次转置(因为这里HLSL里是列主序)，DXmath是行主序，所以需要额外进行一次转置(无语 只能说6)
	CamMat mcammat;
	DirectX::XMMATRIX viewinternal = DirectX::XMMatrixSet(
		607.447937, 0.0f, 320, 0,
		0.0f, 607.522095, 240, 0,
		0.0f, 0.0f, 1, 0,
		0,0,0,0
	);
	DirectX::XMStoreFloat4x4(&mcammat.ViewInternalMat, DirectX::XMMatrixTranspose(viewinternal));
	DirectX::XMMATRIX viewexternal_r = DirectX::XMMatrixSet(
		0.97282915, 0.072511701, -0.21987611, 0,
		-0.035734491, 0.98533569, 0.16684311, 0,
		0.22874985, -0.15445268, 0.96115445, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.ViewExternalMat_R, DirectX::XMMatrixTranspose(viewexternal_r));
	mcammat.ViewExternalMat_T = DirectX::XMFLOAT4(
		-0.063299724e+02, -0.049407028e+02, 0.76696249e+02, 0
	);

	DirectX::XMMATRIX leftinternal = DirectX::XMMatrixSet(
		606.134, 0, 320, 0,
		0, 605.951, 240, 0,
		0, 0, 1, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.LeftInternalMat, DirectX::XMMatrixTranspose(leftinternal));
	DirectX::XMMATRIX leftexternal_r = DirectX::XMMatrixSet(
		0.87402824, 0.04817779, -0.48348065, 0,
		-0.032189193, 0.99862732, 0.041319944, 0,
		0.48480769, -0.020551946, 0.87437928, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.LeftExternalMat_R, DirectX::XMMatrixTranspose(leftexternal_r));
	mcammat.LeftExternalMat_T = DirectX::XMFLOAT4(
		0.025058259e+02, -0.035155708e+02, 0.79021269e+02, 0
	);


	DirectX::XMMATRIX midinternal = DirectX::XMMatrixSet(
		605.924133, 0, 320, 0,
		0, 605.991394, 240, 0,
		0, 0, 1, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.MidInternalMat, DirectX::XMMatrixTranspose(midinternal));
	DirectX::XMMATRIX midexternal_r = DirectX::XMMatrixSet(
		0.92238833, 0.016037037, 0.38593081, 0,
		-0.06859051, 0.99005924, 0.12279266, 0,
		-0.38012513, -0.1397337, 0.91431908, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.MidExternalMat_R, DirectX::XMMatrixTranspose(midexternal_r));
	mcammat.MidExternalMat_T = DirectX::XMFLOAT4(
		-0.044661758e+02, -0.080205671e+02, 0.85012851e+02, 0
	);


	DirectX::XMMATRIX rightinternal = DirectX::XMMatrixSet(
		603.888306, 0, 320, 0,
		0, 603.703613, 240, 0,
		0, 0, 1, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.RightInternalMat, DirectX::XMMatrixTranspose(rightinternal));
	DirectX::XMMATRIX rightexternal_r = DirectX::XMMatrixSet(
		0.79995099, 0.021275003, 0.59968808, 0,
		-0.021173286, 0.99974972, -0.0072239019, 0,
		-0.59969168, -0.0069185994, 0.80020124, 0,
		0, 0, 0, 0
	);
	DirectX::XMStoreFloat4x4(&mcammat.RightExternalMat_R, DirectX::XMMatrixTranspose(rightexternal_r));
	mcammat.RightExternalMat_T = DirectX::XMFLOAT4(
		-0.066983473e+02, -0.079096781e+02, 0.95016361e+02, 0
	);

	mCbMat = std::make_unique<UploadBuffer<cbMat>>(md3dDevice.Get(), 1, true);
	auto CurrentCbMat = mCbMat.get();
	CurrentCbMat->CopyData(0, mcbmat);

	mCamMat = std::make_unique<UploadBuffer<CamMat>>(md3dDevice.Get(), 1, true);
	auto CurrentCamMat = mCamMat.get();
	CurrentCamMat->CopyData(0, mcammat);

}
///////////////////////////////////////////////


void RAYTSDF::Update(const GameTimer& gt) {

	//帧资源计数循环
	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % NumFrameResource;
	mCurrentFrameResource = mAllFrameResource[mCurrentFrameResourceIndex].get();

	/*mCurrentFrameResource->mCuResource->LoadImg();
	mCurrentFrameResource->mCuResource->UpdateTexture();*/

	//GPU_Fence判断
	if (mCurrentFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateUploadVertexBuffer();
	
}

void RAYTSDF::UpdateUploadVertexBuffer() {
	

	//CUDA
	dim3 block(32, 15);
	dim3 grid(20, 32); //(40, 48)
	//testCUDA<<<grid, block>>>(mCurrentFrameResource->d_crood);
	/*mCurrentFrameResource->mCuResource->LoadImg();
	mCurrentFrameResource->mCuResource->UpdateTexture();*/

	depth_img_point_splatting <<<grid, block>>> (
		mCurrentFrameResource->mCuResource->d_depth_rasterize_min,
		mCurrentFrameResource->mCuResource->d_depth_rasterize_max,
		mCurrentFrameResource->mCuResource->depth_tex_left_obj,
		mCurrentFrameResource->mCuResource->depth_tex_mid_obj,
		mCurrentFrameResource->mCuResource->depth_tex_right_obj);
	//cudaDeviceSynchronize();

	ray_cast <<<grid, block>>> (
		mCurrentFrameResource->d_crood,
		mCurrentFrameResource->mCuResource->color_tex_left_obj,
		mCurrentFrameResource->mCuResource->color_tex_mid_obj,
		mCurrentFrameResource->mCuResource->color_tex_right_obj,
		mCurrentFrameResource->mCuResource->depth_tex_left_obj,
		mCurrentFrameResource->mCuResource->depth_tex_mid_obj,
		mCurrentFrameResource->mCuResource->depth_tex_right_obj,
		mCurrentFrameResource->mCuResource->depth_tex_left_border_obj,
		mCurrentFrameResource->mCuResource->depth_tex_mid_border_obj,
		mCurrentFrameResource->mCuResource->depth_tex_right_border_obj,
		mCurrentFrameResource->mCuResource->depth_tex_rasterize_min_obj,
		mCurrentFrameResource->mCuResource->depth_tex_rasterize_max_obj);


	//将顶点的上传堆设置到当前帧的顶点缓冲区
	mCurrentVertexBufferGPU = mCurrentFrameResource->cuVertex;
	//mCurrentVertexBufferGPU = CurrentUploadVB->Resource();
}



void RAYTSDF::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
	ZeroMemory(&PsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	PsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	PsoDesc.pRootSignature = mRootSignature.Get();
	PsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShader["VertexShader"]->GetBufferPointer()),
		mShader["VertexShader"]->GetBufferSize()
	};
	PsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShader["PixelShader"]->GetBufferPointer()),
		mShader["PixelShader"]->GetBufferSize()
	};
	PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	PsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	PsoDesc.SampleMask = UINT_MAX;
	PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	PsoDesc.NumRenderTargets = 1;
	PsoDesc.RTVFormats[0] = mBackBufferFormat;
	PsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	PsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	PsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&mPSO)));
}



void RAYTSDF::Draw(const GameTimer& gt) {

	auto cmdListAlloc = mCurrentFrameResource->FrameCmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());

	// 复用记录命令所用的内存
	// 只有当GPU中的命令列表执行完毕后，我们才可以对其进行重置
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	// 设置视口和裁剪矩形，它们需要随着命令列表的重置而重置
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// 按照资源的用途指示其状态的转变，此处将资源从呈现状态转换为渲染目标状态
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(), 
		D3D12_RESOURCE_STATE_PRESENT, 
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	// 清除后台缓冲区和深度缓冲区
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::White, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 指定将要渲染的目标缓冲区
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());


	//设置描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { 
		mTexSrvHeap.Get(), 
		mSampleHeap.Get() 
	};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	

	// 设置根签名
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	CD3DX12_GPU_DESCRIPTOR_HANDLE Tex(mTexSrvHeap->GetGPUDescriptorHandleForHeapStart());
	Tex.Offset(mCurrentFrameResourceIndex * 2, mCbvSrvUavDescriptorSize);

	CD3DX12_GPU_DESCRIPTOR_HANDLE sample(mSampleHeap->GetGPUDescriptorHandleForHeapStart());


	// 根常量参数更新
	auto cbmat = mCbMat->Resource();
	auto cammat = mCamMat->Resource();
	mCommandList->SetGraphicsRootDescriptorTable(0, Tex);
	mCommandList->SetGraphicsRootDescriptorTable(1, sample);
	mCommandList->SetGraphicsRootConstantBufferView(2, cbmat->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootConstantBufferView(3, cammat->GetGPUVirtualAddress());
	

	// 设置顶点视图
	mCommandList->IASetVertexBuffers(0, 1, &VertexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	
	//绘制实例
	mCommandList->DrawInstanced(mVertexCount * sizeof(Vertex), 1, 0, 0);

	// 资源转换
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	));

	// 完成命令设置
	ThrowIfFailed(mCommandList->Close());

	// 将CPU端的List命令加入GPU的queue命令
	ID3D12CommandList* cmdList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	// 交换帧
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Fence设置
	mCurrentFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);


}

D3D12_VERTEX_BUFFER_VIEW RAYTSDF::VertexBufferView()const {
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = mCurrentVertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = sizeof(Vertex);
	vbv.SizeInBytes = mVertexCount * sizeof(Vertex);

	return vbv;
}



