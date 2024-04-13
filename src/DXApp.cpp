#include "DXApp.h"
#include <codecvt>

// 窗口过程函数
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return DXApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

DXApp* DXApp::mApp = nullptr;
DXApp* DXApp::GetApp() {
	return mApp;
}

HWND DXApp::GetHWND() {
	return hWnd;
}

DXApp::DXApp(HINSTANCE hInstance):hInst(hInstance) {
	assert(mApp == nullptr);
	mApp = this;
}

DXApp::~DXApp() {
	if (mCbMat != nullptr) {
		mCbMat->Unmap(0, nullptr);
		delete mCbMatMappedData;
	}
	if (mCamMat != nullptr) {
		mCamMat->Unmap(0, nullptr);
		delete mCamMatMappedData;
	}
}

bool DXApp::Init() {
	if (!InitWindows())
		return false;
	if (!InitDX())
		return false;
	if (!InitPipeLine())
		return false;
	

	return true;
}

bool DXApp::InitWindows() {
	//***************************************************************
	// 创建描述窗口类WNDCLASS
	//***************************************************************
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc; // 窗口过程，消息处理
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = WndClassName; // 窗口类名字

	if (!RegisterClass(&wc)) { // 检查是否注册失败
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}
	//***************************************************************
	// 计算窗口矩形边框
	//***************************************************************
	RECT R = { 0, 0, ClientWidth, ClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;
	//***************************************************************
	// 创建窗口
	//***************************************************************
	hWnd = CreateWindowEx(
		0,
		WndClassName,
		WindowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width,
		height,
		NULL,
		NULL,
		hInst,
		NULL);
	if (!hWnd) { // 判断是否创建成功
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}
	

	// 显示并更新窗口
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	return true;
}

bool DXApp::InitDX() {
// 开启debug layer
#if defined(DEBUG) || defined(_DEBUG) 
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	// 创建DXGI Factory对象
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDXGIFactory)));
	
	// 枚举适配器 创建设备
	{
		D3D12_FEATURE_DATA_ARCHITECTURE mGPUFeature = {}; // 显卡特性及属性
		Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter; // 适配器
		for (UINT AdapterIndex = 0; mDXGIFactory->EnumAdapters1(AdapterIndex, &mAdapter) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex) {
			DXGI_ADAPTER_DESC1 AdapterDesc = {};
			mAdapter->GetDesc1(&AdapterDesc);

			if (AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { // 跳过软件虚拟适配器
				continue;
			}
			else { //这里是只为了建立单个显卡，所以一旦检测到独显就不在枚举适配器，后期多显卡改这里
				HRESULT hr = D3D12CreateDevice(
					mAdapter.Get(),
					D3D_FEATURE_LEVEL_12_1,
					IID_PPV_ARGS(&md3dDevice)
				);
				if(SUCCEEDED(hr))
				{
					printf("yes\n");
				}
				else
				{
					printf("no\n");
				}
				/*ThrowIfFailed(D3D12CreateDevice(
					mAdapter.Get(),
					D3D_FEATURE_LEVEL_12_1,
					IID_PPV_ARGS(&md3dDevice)
				));*/
				ThrowIfFailed(md3dDevice->CheckFeatureSupport(
					D3D12_FEATURE_ARCHITECTURE,
					&mGPUFeature,
					sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)
				));
				//if (!mGPUFeature.UMA) { // 判断是否支持统一内存访问
				//	break;
				//}
				//md3dDevice.Reset();

				// 创建该设备的Fence
				ThrowIfFailed(md3dDevice->CreateFence(
					0, 
					D3D12_FENCE_FLAG_NONE,
					IID_PPV_ARGS(&mGPUFence)
				));
				
				// 获得该设备对应描述符信息
				mRtvDescSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				mDsvDescSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
				mCbvSrvUavDescSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				break; // 只创建单一显卡
			}
		}
		if (md3dDevice.Get() == nullptr) {
			return false;
		}
	}


	// 检测设备对MSAA质量支持
	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
		msQualityLevels.Format = BackBufferFormat;
		msQualityLevels.SampleCount = 4;
		msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msQualityLevels.NumQualityLevels = 0;
		ThrowIfFailed(md3dDevice->CheckFeatureSupport(
			D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&msQualityLevels,
			sizeof(msQualityLevels)));

		m4xMsaaQuality = msQualityLevels.NumQualityLevels;
		assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
	}

	// 创建命令队列Queue
	CreateQueue();
	// 创建交换链SwapChain及其对应渲染目标描述符堆RtvHeap深度缓冲描述符堆DsvHeap
	CreateSwapChain_RtvDsvHeap();
	// 创建命令列表及分配器
	CreateAllocator_List();

	assert(md3dDevice);
	assert(mSwapChain);
	assert(mCmdAlloc);

	// 创建渲染目标缓冲区以及深度蜡版缓冲区
	// 因为在调整尺寸时，依然需要更改两者的缓冲区，所以这里初始化和更改合并
	OnResize();

	return true;

}

bool DXApp::InitPipeLine() {
	// 清空List
	ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));
	// 初始化过程
	BuildRootSignature();
	BuildShaderAndInputLayout();
	BuildTexDescHeap();
	BuildSampleHeapAndDesc();
	BuildFrameResource();
	BuildMat();
	BuildQuad();
	BuildPSO();

	// 完成命令设置
	ThrowIfFailed(mCmdList->Close());

	// 将CPU端的List命令加入GPU的queue命令
	ID3D12CommandList* cmdList[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	//等待初始化完成
	FlushCmdQueue();

	return true;
}

void DXApp::Update() {
	// RingBuffer
	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % NumFrameResource;
	mCurrentFrameResource = mAllFrameResource[mCurrentFrameResourceIndex].get();

	// GPU_Fence
	if (mCurrentFrameResource->Fence != 0 && mGPUFence->GetCompletedValue() < mCurrentFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mGPUFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// upload texture
}

void DXApp::ComputeAndDraw() {
	auto cmdListAlloc = mCurrentFrameResource->FrameCmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());

	// 只有当GPU中的命令列表执行完毕后，我们才可以对其进行重置
	ThrowIfFailed(mCmdList->Reset(cmdListAlloc.Get(), nullptr));

	// 绑定资源到管线
	// 设置描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = {
		mTexDescHeap.Get(),
		mSampleHeap.Get()
	};
	mCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	// 设置根签名以及对应固定资源
	// 绑定当前帧所用的纹理资源到管线(渲染管线和计算管线)
	CD3DX12_GPU_DESCRIPTOR_HANDLE TexSrv(mTexDescHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE TexUav(mTexDescHeap->GetGPUDescriptorHandleForHeapStart());
	TexSrv.Offset(mCurrentFrameResourceIndex * (NumSrvPerFrame + NumUavPerFrame), mCbvSrvUavDescSize);
	TexUav.Offset(mCurrentFrameResourceIndex * (NumSrvPerFrame + NumUavPerFrame) + NumSrvPerFrame, mCbvSrvUavDescSize);

	mCmdList->SetComputeRootSignature(mRootSignature.Get());
	mCmdList->SetComputeRootDescriptorTable(0, TexSrv);
	mCmdList->SetComputeRootDescriptorTable(1, TexUav);
	mCmdList->SetComputeRootDescriptorTable(2, mSampleHeap->GetGPUDescriptorHandleForHeapStart());
	mCmdList->SetComputeRootConstantBufferView(3, mCbMat->GetGPUVirtualAddress());
	mCmdList->SetComputeRootConstantBufferView(4, mCamMat->GetGPUVirtualAddress());

	mCmdList->SetGraphicsRootSignature(mRootSignature.Get());
	mCmdList->SetGraphicsRootDescriptorTable(0, TexSrv);
	mCmdList->SetGraphicsRootDescriptorTable(1, TexUav);
	mCmdList->SetGraphicsRootDescriptorTable(2, mSampleHeap->GetGPUDescriptorHandleForHeapStart());
	mCmdList->SetGraphicsRootConstantBufferView(3, mCbMat->GetGPUVirtualAddress());
	mCmdList->SetGraphicsRootConstantBufferView(4, mCamMat->GetGPUVirtualAddress());

	// 设置固定Quad
	mCmdList->IASetVertexBuffers(0, 1, &vbv);
	mCmdList->IASetIndexBuffer(&ibv);
	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Compute 
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(
		mAllFrameResource[mCurrentFrameResourceIndex]->EncodingTex.Get()
	));
	mCmdList->SetPipelineState(mPSO["LightFieldEncodingPSO"].Get());
	mCmdList->Dispatch(total_pixel_x / 32, total_pixel_y / 32, 1);// 光场显示器分辨率


	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(
		mAllFrameResource[mCurrentFrameResourceIndex]->EncodingTex.Get()
	));
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(
		mAllFrameResource[mCurrentFrameResourceIndex]->SplattingTex.Get()
	));
	mCmdList->SetPipelineState(mPSO["SplattingPSO"].Get());// 深度图分辨率
	mCmdList->Dispatch(depth_pixel_x / 20, depth_pixel_y / 20, 1);


	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(
		mAllFrameResource[mCurrentFrameResourceIndex]->SplattingTex.Get()
	));
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(
		mAllFrameResource[mCurrentFrameResourceIndex]->OutVertexTex.Get()
	));
	mCmdList->SetPipelineState(mPSO["RaycastPSO"].Get());
	mCmdList->Dispatch(total_pixel_x / 32, total_pixel_y / 32, 1);// 光场显示器分辨率


	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mAllFrameResource[mCurrentFrameResourceIndex]->EncodingTex.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	));
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mAllFrameResource[mCurrentFrameResourceIndex]->OutVertexTex.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	));


	// DRAW
	mCmdList->SetPipelineState(mPSO["RenderPSO"].Get());

	// 设置视口和裁剪矩形，它们需要随着命令列表的重置而重置
	mCmdList->RSSetViewports(1, &mScreenViewport);
	mCmdList->RSSetScissorRects(1, &mScissorRect);

	// 资源转换
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mCurrBackBuffer].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	// 清除后台缓冲区和深度缓冲区
	mCmdList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::White, 0, nullptr);
	mCmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 指定将要渲染的目标缓冲区
	mCmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	
	mCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mAllFrameResource[mCurrentFrameResourceIndex]->EncodingTex.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	));
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mAllFrameResource[mCurrentFrameResourceIndex]->OutVertexTex.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	));

	// 资源转换
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mCurrBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	));

	// 完成命令设置
	ThrowIfFailed(mCmdList->Close());

	// 将CPU端的List命令加入GPU的queue命令
	ID3D12CommandList* cmdList[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

	// 交换帧
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Fence设置
	mCurrentFrameResource->Fence = ++mCurrentCPUFence;
	mCmdQueue->Signal(mGPUFence.Get(), mCurrentCPUFence);
}

//void DXApp::Draw() {
//
//}

int DXApp::Run() {
	MSG msg = { 0 };
	mTimer.Reset();


	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			mTimer.Tick();
			CalculateFrameStats();
			Update();
			ComputeAndDraw();
		}
	}
	return (int)msg.wParam;
}

void DXApp::CreateQueue() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(
		&queueDesc,
		IID_PPV_ARGS(&mCmdQueue)
	));
}

void DXApp::CreateSwapChain_RtvDsvHeap() {
	// 重置交换链
	mSwapChain.Reset();
	//************************************************************************
	// 创建交换链
	//************************************************************************
	{
		// 创建交换链描述符
		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = ClientWidth;
		sd.BufferDesc.Height = ClientHeight;
		sd.BufferDesc.Format = BackBufferFormat;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = SwapChainBufferCount;
		sd.OutputWindow = GetHWND();
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// 用到DXGIFactory和CommandQueue创立
		ThrowIfFailed(mDXGIFactory->CreateSwapChain(
			mCmdQueue.Get(),
			&sd,
			mSwapChain.GetAddressOf()));
	}
	//************************************************************************
	// 创建渲染目标描述符堆
	//************************************************************************
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS(mRtvHeap.GetAddressOf())
		));
	}
	//************************************************************************
	// 创建深度缓冲描述符堆
	//************************************************************************
	{
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
			&dsvHeapDesc, 
			IID_PPV_ARGS(mDsvHeap.GetAddressOf())
		));
	}
	
}

void DXApp::CreateAllocator_List() {
	//****************************************************************
	// 处理命令列表
	//****************************************************************
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&mCmdAlloc)
	));
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mCmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(&mCmdList)
	));
	mCmdList->Close();
	//****************************************************************
	// 后处理命令列表
	//****************************************************************
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&mCmdAllocPost)
	));
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mCmdAllocPost.Get(),
		nullptr,
		IID_PPV_ARGS(&mCmdListPost)
	));
	mCmdListPost->Close();
	
}

void DXApp::OnResize() {
	// 在调整之前，先等待GPU中的工作完成
	FlushCmdQueue();
	//************************************************************************************************
	// 重置
	//************************************************************************************************
	ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));
	for (int i = 0; i < SwapChainBufferCount; ++i) {
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	// Resize交换链Buffer
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		ClientWidth,
		ClientHeight,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));
	//************************************************************************************************
	// 创建/更新渲染目标视图
	//************************************************************************************************
	CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i) {
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, RtvHeapHandle);
		RtvHeapHandle.Offset(1, mRtvDescSize);
	}
	//************************************************************************************************
	// 创建/更新深度缓冲描述符
	//************************************************************************************************
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = ClientWidth;
	depthStencilDesc.Height = ClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	md3dDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(), 
		nullptr, 
		mDsvHeap->GetCPUDescriptorHandleForHeapStart()
	);
	
	// 资源转换屏障
	mCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);

	// 执行命令
	ThrowIfFailed(mCmdList->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// 等待Resize操作完成
	FlushCmdQueue();

	// 更新视口信息
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(ClientWidth);
	mScreenViewport.Height = static_cast<float>(ClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, ClientWidth, ClientHeight };

	// 更新透视矩阵
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.1667f * MY_PI, Aspect(), n_plane, f_plane);
	DirectX::XMStoreFloat4x4(&mProjMat, P);
}

void DXApp::FlushCmdQueue() {
	mCurrentCPUFence++;
	ThrowIfFailed(mCmdQueue->Signal(mGPUFence.Get(), mCurrentCPUFence));
	if (mGPUFence->GetCompletedValue() < mCurrentCPUFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mGPUFence->SetEventOnCompletion(mCurrentCPUFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

LRESULT DXApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{ 
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// The WM_MENUCHAR message is sent when a menu is active and the user presses 
	// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

	// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		ClientWidth = total_pixel_x;//LOWORD(lParam);
		ClientHeight = total_pixel_y;//HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DXApp::CalculateFrameStats() {
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = mWindowName +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(hWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

float DXApp::Aspect()const {
	return static_cast<float>(ClientWidth) / ClientHeight;
}

Microsoft::WRL::ComPtr<ID3DBlob> DXApp::CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target) {

	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr)
		OutputDebugStringA((char*)errors->GetBufferPointer());

	ThrowIfFailed(hr);

	return byteCode;
}

void DXApp::BuildRootSignature() {
	// TexDescTable
	CD3DX12_DESCRIPTOR_RANGE TexSrvDescTable = {};
	TexSrvDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NumSrvPerFrame, 0);

	CD3DX12_DESCRIPTOR_RANGE TexUavDescTable = {};
	TexUavDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, NumUavPerFrame, 0);

	// SamplerDescTable
	CD3DX12_DESCRIPTOR_RANGE SamplerDescTable = {};
	SamplerDescTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, NumSamplePerFrame, 0);

	// 根参数 按变更频率由高到低排序
	CD3DX12_ROOT_PARAMETER slotRootParameter[5] = {};
	slotRootParameter[0].InitAsDescriptorTable(1, &TexSrvDescTable);
	slotRootParameter[1].InitAsDescriptorTable(1, &TexUavDescTable);
	slotRootParameter[2].InitAsDescriptorTable(1, &SamplerDescTable);
	slotRootParameter[3].InitAsConstantBufferView(0);
	slotRootParameter[4].InitAsConstantBufferView(1);

	// 根签名
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, 0, nullptr,
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
		IID_PPV_ARGS(&mRootSignature))
	);
}
extern const char* logl_root;
void DXApp::BuildShaderAndInputLayout() {
	// 创建std::wstring_convert对象
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	// 将const char*转换为std::wstring
	std::wstring rootPath = converter.from_bytes(logl_root);

	mShader["VS"] = CompileShader(
		rootPath + L"/Resource/Shader/renderer.hlsl",
		nullptr,
		"VS",
		"vs_5_1"
	);

	mShader["PS"] = CompileShader(
		rootPath + L"/Resource/Shader/renderer.hlsl",
		nullptr,
		"PS",
		"ps_5_1"
	);

	mShader["RaycastCS"] = CompileShader(
		rootPath + L"/Resource/Shader/raycast.hlsl",
		nullptr,
		"Raycast",
		"cs_5_1"
	);

	mShader["SplattingCS"] = CompileShader(
		rootPath + L"/Resource/Shader/raycast.hlsl",
		nullptr,
		"Splatting",
		"cs_5_1"
	);

	mShader["LightFieldEncodingCS"] = CompileShader(
		rootPath + L"/Resource/Shader/raycast.hlsl",
		nullptr,
		"LightFieldEncoding",
		"cs_5_1"
	);

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DXApp::BuildTexDescHeap() {
	// 建立纹理的DescHeap
	D3D12_DESCRIPTOR_HEAP_DESC TexDescHeapDesc = {};
	TexDescHeapDesc.NumDescriptors = NumFrameResource * (NumSrvPerFrame + NumUavPerFrame);
	TexDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	TexDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&TexDescHeapDesc,
		IID_PPV_ARGS(&mTexDescHeap)
	));
}

void DXApp::BuildSampleHeapAndDesc() {
	D3D12_DESCRIPTOR_HEAP_DESC sampleHeapDesc = {};
	sampleHeapDesc.NumDescriptors = NumSamplePerFrame;
	sampleHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	sampleHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&sampleHeapDesc,
		IID_PPV_ARGS(&mSampleHeap)
	));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hSampleHeap(mSampleHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_SAMPLER_DESC sampleLinearDesc = {};
	sampleLinearDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
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

void DXApp::BuildFrameResource() {
	for (int i = 0; i < NumFrameResource; ++i) {
		mAllFrameResource.push_back(std::make_unique<FrameResource>(md3dDevice.Get()));
		CD3DX12_CPU_DESCRIPTOR_HANDLE hTexDescHeap(mTexDescHeap->GetCPUDescriptorHandleForHeapStart());

		// 建立纹理数组，并从Uploader上传到defalut
		D3D12_SUBRESOURCE_DATA colorTexArray[NumColor] = {
			{mAllFrameResource[i]->leftColorImg,
			color_pixel_x * sizeof(UCHAR) * 4,
			color_pixel_x* color_pixel_y * sizeof(UCHAR) * 4},
			{mAllFrameResource[i]->midColorImg,
			color_pixel_x * sizeof(UCHAR) * 4,
			color_pixel_x * color_pixel_y * sizeof(UCHAR) * 4},
			{mAllFrameResource[i]->rightColorImg,
			color_pixel_x * sizeof(UCHAR) * 4,
			color_pixel_x * color_pixel_y * sizeof(UCHAR) * 4}
		};

		D3D12_SUBRESOURCE_DATA depthTexArray[NumDepth] = {
			{mAllFrameResource[i]->leftDepthImg,
			depth_pixel_x * sizeof(float),
			depth_pixel_x * depth_pixel_y * sizeof(float)},
			{mAllFrameResource[i]->midDepthImg,
			depth_pixel_x * sizeof(float),
			depth_pixel_x * depth_pixel_y * sizeof(float)},
			{mAllFrameResource[i]->rightDepthImg,
			depth_pixel_x * sizeof(float),
			depth_pixel_x * depth_pixel_y * sizeof(float)}
		};

		// 资源屏障转换
		mCmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->ColorTex.Get(),
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST)
		);
		mCmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->DepthTex.Get(),
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST)
		);

		UpdateSubresources(
			mCmdList.Get(),
			mAllFrameResource[i]->ColorTex.Get(),
			mAllFrameResource[i]->ColorTexUpload.Get(),
			0,//intermediate offset
			0,//first subr
			NumColor,//num subr
			colorTexArray
		);

		UpdateSubresources(
			mCmdList.Get(),
			mAllFrameResource[i]->DepthTex.Get(),
			mAllFrameResource[i]->DepthTexUpload.Get(),
			0,//intermediate offset
			0,//first subr
			NumDepth,//num subr
			depthTexArray
		);
		// 资源屏障转换
		mCmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->ColorTex.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		);
		mCmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mAllFrameResource[i]->DepthTex.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		);
		

		// 创建纹理的SRV
		// 彩色纹理数组
		D3D12_SHADER_RESOURCE_VIEW_DESC colorTexSrvDesc = {};
		colorTexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		colorTexSrvDesc.Format = mAllFrameResource[i]->ColorTex->GetDesc().Format;
		colorTexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		colorTexSrvDesc.Texture2DArray.ArraySize = mAllFrameResource[i]->ColorTex->GetDesc().DepthOrArraySize;
		colorTexSrvDesc.Texture2DArray.MipLevels = mAllFrameResource[i]->ColorTex->GetDesc().MipLevels;
		colorTexSrvDesc.Texture2DArray.FirstArraySlice = 0;
		colorTexSrvDesc.Texture2DArray.MostDetailedMip = 0;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->ColorTex.Get(),
			&colorTexSrvDesc,
			hTexDescHeap
		);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
		// 深度纹理数组
		D3D12_SHADER_RESOURCE_VIEW_DESC depthTexSrvDesc = {};
		depthTexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthTexSrvDesc.Format = mAllFrameResource[i]->DepthTex->GetDesc().Format;
		depthTexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		depthTexSrvDesc.Texture2DArray.ArraySize = mAllFrameResource[i]->DepthTex->GetDesc().DepthOrArraySize;
		depthTexSrvDesc.Texture2DArray.MipLevels = mAllFrameResource[i]->DepthTex->GetDesc().MipLevels;
		depthTexSrvDesc.Texture2DArray.FirstArraySlice = 0;
		depthTexSrvDesc.Texture2DArray.MostDetailedMip = 0;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->DepthTex.Get(),
			&depthTexSrvDesc,
			hTexDescHeap
		);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
		// 光场编码模板
		D3D12_SHADER_RESOURCE_VIEW_DESC encodingTexSrcDesc = {};
		encodingTexSrcDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		encodingTexSrcDesc.Format = mAllFrameResource[i]->EncodingTex->GetDesc().Format;
		encodingTexSrcDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		encodingTexSrcDesc.Texture2D.MostDetailedMip = 0;
		encodingTexSrcDesc.Texture2D.MipLevels = 1;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->EncodingTex.Get(),
			&encodingTexSrcDesc,
			hTexDescHeap
		);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
		// 输出顶点模板
		D3D12_SHADER_RESOURCE_VIEW_DESC outVertexTexSrvDesc = {};
		outVertexTexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		outVertexTexSrvDesc.Format = mAllFrameResource[i]->OutVertexTex->GetDesc().Format;
		outVertexTexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		outVertexTexSrvDesc.Texture2D.MostDetailedMip = 0;
		outVertexTexSrvDesc.Texture2D.MipLevels = 1;
		md3dDevice->CreateShaderResourceView(
			mAllFrameResource[i]->OutVertexTex.Get(),
			&outVertexTexSrvDesc,
			hTexDescHeap);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);

		// 创建纹理的UAV
		// 光场编码
		D3D12_UNORDERED_ACCESS_VIEW_DESC encodingTexUavDesc = {};
		encodingTexUavDesc.Format = mAllFrameResource[i]->EncodingTex->GetDesc().Format;
		encodingTexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		encodingTexUavDesc.Texture2D.MipSlice = 0;
		md3dDevice->CreateUnorderedAccessView(
			mAllFrameResource[i]->EncodingTex.Get(),
			nullptr,
			&encodingTexUavDesc,
			hTexDescHeap);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
		// 点溅步进范围
		D3D12_UNORDERED_ACCESS_VIEW_DESC splattingTexUavDesc = {};
		splattingTexUavDesc.Format = mAllFrameResource[i]->SplattingTex->GetDesc().Format;
		splattingTexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		splattingTexUavDesc.Texture2DArray.ArraySize = mAllFrameResource[i]->SplattingTex->GetDesc().DepthOrArraySize;
		splattingTexUavDesc.Texture2DArray.FirstArraySlice = 0;
		splattingTexUavDesc.Texture2DArray.MipSlice = 0;
		
		md3dDevice->CreateUnorderedAccessView(
			mAllFrameResource[i]->SplattingTex.Get(),
			nullptr,
			&splattingTexUavDesc,
			hTexDescHeap);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
		// 输出顶点
		D3D12_UNORDERED_ACCESS_VIEW_DESC outVertexTexUavDesc = {};
		outVertexTexUavDesc.Format = mAllFrameResource[i]->OutVertexTex->GetDesc().Format;
		outVertexTexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		outVertexTexUavDesc.Texture2D.MipSlice = 0;	
		md3dDevice->CreateUnorderedAccessView(
			mAllFrameResource[i]->OutVertexTex.Get(),
			nullptr,
			&outVertexTexUavDesc,
			hTexDescHeap);
		hTexDescHeap.Offset(1, mCbvSrvUavDescSize);
	}
}

void DXApp::BuildMat() {
	DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f);
	DirectX::XMVECTOR target = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProjMat);
	DirectX::XMMATRIX view_porj = DirectX::XMMatrixMultiply(view, proj);

	CbMat cbmat;
	cbmat.WorldMat = { 
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMStoreFloat4x4(&cbmat.ViewMat, DirectX::XMMatrixTranspose(view));
	DirectX::XMStoreFloat4x4(&cbmat.ProjMat, DirectX::XMMatrixTranspose(proj));
	DirectX::XMStoreFloat4x4(&cbmat.ViewProjMat, DirectX::XMMatrixTranspose(view_porj));

	// DX12 GPU矩阵只支持float4x4内置类型，所以以4x4方式传入3x3矩阵
	// DX12 GPU向量也只支持float4，所以float3同理
	// 这里是因为GPU内存需要64字节的对齐，而对于整体的constantBuf需要256字节对齐
	// 传进去会自动进行一次转置(因为这里HLSL里是列主序)，DXmath是行主序，我们的计算为行主序，所以需要额外进行一次转置(无语 只能说6)
	CamMat cammat;
	// view
	DirectX::XMMATRIX viewinternal = DirectX::XMMatrixSet(
		1.79138439e+03, 0, 0, 0,
		0, 1.79138439e+03, 0, 0,
		view_pixel_x / 2, view_pixel_y / 2, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.ViewInternalMat, DirectX::XMMatrixTranspose(viewinternal));
	DirectX::XMMATRIX viewexternal_r = DirectX::XMMatrixSet(
		0, 1, 0, 0,
		-1, 0, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.ViewExternalMat_R, DirectX::XMMatrixTranspose(viewexternal_r));
	cammat.ViewExternalMat_T = DirectX::XMFLOAT4(
		0, 300, 135, 0
	);
	// left
	DirectX::XMMATRIX leftinternal = DirectX::XMMatrixSet(
		1.79138439e+03, 0, 0, 0,
		0, 1.79138439e+03, 0, 0,
		view_pixel_x / 2, view_pixel_y / 2, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.LeftInternalMat, DirectX::XMMatrixTranspose(leftinternal));
	DirectX::XMMATRIX leftexternal_r = DirectX::XMMatrixSet(
		0.5, 0.8660254, 0, 0,
		-0.8660254, 0.5, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.LeftExternalMat_R, DirectX::XMMatrixTranspose(leftexternal_r));
	cammat.LeftExternalMat_T = DirectX::XMFLOAT4(
		-150, 259.8, 135, 0
	);
	// mid
	DirectX::XMMATRIX midinternal = DirectX::XMMatrixSet(
		1.79138439e+03, 0, 0, 0,
		0, 1.79138439e+03, 0, 0,
		view_pixel_x / 2, view_pixel_y / 2, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.MidInternalMat, DirectX::XMMatrixTranspose(midinternal));
	DirectX::XMMATRIX midexternal_r = DirectX::XMMatrixSet(
		0, 1, 0, 0,
		-1, 0, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.MidExternalMat_R, DirectX::XMMatrixTranspose(midexternal_r));
	cammat.MidExternalMat_T = DirectX::XMFLOAT4(
		0, 300, 135, 0
	);
	// right
	DirectX::XMMATRIX rightinternal = DirectX::XMMatrixSet(
		1.79138439e+03, 0, 0, 0,
		0, 1.79138439e+03, 0, 0,
		view_pixel_x / 2, view_pixel_y / 2, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.RightInternalMat, DirectX::XMMatrixTranspose(rightinternal));
	DirectX::XMMATRIX rightexternal_r = DirectX::XMMatrixSet(
		-0.5, 0.8660254, 0, 0,
		-0.8660254, -0.5, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	DirectX::XMStoreFloat4x4(&cammat.RightExternalMat_R, DirectX::XMMatrixTranspose(rightexternal_r));
	cammat.RightExternalMat_T = DirectX::XMFLOAT4(
		150, 259.8, 135, 0
	);

	// 常量buffer需要256字节对齐
	auto mCbMatSize = UPPER(sizeof(CbMat), 256);
	auto mCamMatSize = UPPER(sizeof(CamMat), 256);
	// 建立常量的Upload堆
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mCbMatSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mCbMat)
	));
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mCamMatSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mCamMat)
	));
	// 建立CPU对应映射内存
	ThrowIfFailed(mCbMat->Map(0, nullptr, reinterpret_cast<void**>(&mCbMatMappedData)));
	ThrowIfFailed(mCamMat->Map(0, nullptr, reinterpret_cast<void**>(&mCamMatMappedData)));
	// 复制数据到映射内存
	memcpy(&mCbMatMappedData[0], &cbmat, sizeof(CbMat));
	memcpy(&mCamMatMappedData[0], &cammat, sizeof(CamMat));

}

void DXApp::BuildQuad() {
	Vertex quadVertex[] = {
		{{-1.f, 1.f, 0.f}, {0.f, 0.f}},
		{{1.f, 1.f, 0.f}, {1.f, 0.f}},
		{{1.f, -1.f, 0.f}, {1.f, 1.f}},
		{{-1.f, -1.f, 0.f}, {0.f, 1.f}}
	};
	UINT32 quadIndices[] = {
		0,1,2,
		0,2,3
	};

	UINT quadVertexSize = sizeof(quadVertex);
	UINT quadIndicesSize = sizeof(quadIndices);

	CreateDefaultBuffer(
		md3dDevice.Get(),
		mCmdList.Get(),
		quadVertex,
		quadVertexSize,
		VertexBufferUploader,
		VertexBufferGPU);

	CreateDefaultBuffer(
		md3dDevice.Get(),
		mCmdList.Get(),
		quadIndices,
		quadIndicesSize,
		IndexBufferUploader,
		IndexBufferGPU);

	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = sizeof(Vertex);
	vbv.SizeInBytes = quadVertexSize;

	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R32_UINT;
	ibv.SizeInBytes = quadIndicesSize;
}

void DXApp::BuildPSO() {
	D3D12_COMPUTE_PIPELINE_STATE_DESC LightFieldEncodingPSO = {};
	LightFieldEncodingPSO.pRootSignature = mRootSignature.Get();
	LightFieldEncodingPSO.CS = {
		reinterpret_cast<BYTE*>(mShader["LightFieldEncodingCS"]->GetBufferPointer()),
		mShader["LightFieldEncodingCS"]->GetBufferSize()
	};
	LightFieldEncodingPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(
		&LightFieldEncodingPSO,
		IID_PPV_ARGS(&mPSO["LightFieldEncodingPSO"])
	));

	D3D12_COMPUTE_PIPELINE_STATE_DESC SplattingPSO = {};
	SplattingPSO.pRootSignature = mRootSignature.Get();
	SplattingPSO.CS = {
		reinterpret_cast<BYTE*>(mShader["SplattingCS"]->GetBufferPointer()),
		mShader["SplattingCS"]->GetBufferSize()
	};
	SplattingPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(
		&SplattingPSO,
		IID_PPV_ARGS(&mPSO["SplattingPSO"])
	));

	D3D12_COMPUTE_PIPELINE_STATE_DESC RaycastPSO = {};
	RaycastPSO.pRootSignature = mRootSignature.Get();
	RaycastPSO.CS = {
		reinterpret_cast<BYTE*>(mShader["RaycastCS"]->GetBufferPointer()),
		mShader["RaycastCS"]->GetBufferSize()
	};
	RaycastPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(
		&RaycastPSO,
		IID_PPV_ARGS(&mPSO["RaycastPSO"])
	));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC RenderPSO = {};
	ZeroMemory(&RenderPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	RenderPSO.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	RenderPSO.pRootSignature = mRootSignature.Get();
	RenderPSO.VS = {
		reinterpret_cast<BYTE*>(mShader["VS"]->GetBufferPointer()),
		mShader["VS"]->GetBufferSize()
	};
	RenderPSO.PS = {
		reinterpret_cast<BYTE*>(mShader["PS"]->GetBufferPointer()),
		mShader["PS"]->GetBufferSize()
	};
	RenderPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	RenderPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RenderPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RenderPSO.SampleMask = UINT_MAX;
	RenderPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	RenderPSO.NumRenderTargets = 1;
	RenderPSO.RTVFormats[0] = BackBufferFormat;
	RenderPSO.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	RenderPSO.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	RenderPSO.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&RenderPSO, 
		IID_PPV_ARGS(&mPSO["RenderPSO"])
	));

}

void DXApp::CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer) {

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}



D3D12_CPU_DESCRIPTOR_HANDLE DXApp::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DXApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
