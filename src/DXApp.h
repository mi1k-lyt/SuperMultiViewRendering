#ifndef __DXAPP_H__
#define __DXAPP_H__

#include "dx12win.h"
#include "GameTimer.h"
#include "FrameResource.h"
#include <thread>
#include <unordered_map>
#include <vector>


class DXApp {
public:
	DXApp(HINSTANCE hInstance);
	~DXApp();
	bool Init();
	bool InitWindows();
	bool InitDX();
	bool InitPipeLine();

	int Run();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // 窗口过程函数
	HWND GetHWND();
	static DXApp* GetApp();

	float Aspect()const;
private:
	void CreateQueue();
	void CreateSwapChain_RtvDsvHeap();
	void CreateAllocator_List();

	void OnResize();
	void FlushCmdQueue();

	Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);

	// 初始化
	void BuildRootSignature();
	void BuildShaderAndInputLayout();
	void BuildTexDescHeap();
	void BuildSampleHeapAndDesc();
	void BuildFrameResource();
	void BuildMat();
	void BuildQuad();
	void BuildPSO();

	void CalculateFrameStats();
	void CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer);

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void Update();
	void ComputeAndDraw();
	//void Draw();

	



protected:
	static DXApp* mApp;

	GameTimer mTimer;

	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled
	//***************************************************************
	// windows
	//***************************************************************
	std::wstring mWindowName = WindowName;

	int ClientWidth = total_pixel_x;
	int ClientHeight = total_pixel_y;

	HINSTANCE hInst = nullptr; // 实例句柄
	HWND hWnd = nullptr; // 窗口程序句柄
	//***************************************************************
	// Resource
	//***************************************************************
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mTexDescHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mCbMat = nullptr;
	BYTE* mCbMatMappedData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCamMat = nullptr;
	BYTE* mCamMatMappedData = nullptr;
	//***************************************************************
	// FrameResource
	//***************************************************************
	const int NumFrameResource = 1;
	const int NumSrvPerFrame = 4;
	const int NumUavPerFrame = 3;
	const int NumSamplePerFrame = 2;
	int mCurrentFrameResourceIndex = 0;
	FrameResource* mCurrentFrameResource;
	std::vector<std::unique_ptr<FrameResource>> mAllFrameResource;

	//***************************************************************
	// DirectX12
	//***************************************************************
	const float n_plane = down_left_z;
	const float f_plane = 500.f;
	DirectX::XMFLOAT4X4 mProjMat;

	Microsoft::WRL::ComPtr<IDXGIFactory5> mDXGIFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	Microsoft::WRL::ComPtr<ID3D12Fence> mGPUFence;
	UINT64 mCurrentCPUFence = 0;

	UINT mRtvDescSize = 0;
	UINT mDsvDescSize = 0;
	UINT mCbvSrvUavDescSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Device4> md3dDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCmdQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCmdList;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAllocPost;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCmdListPost;


	bool m4xMsaaState = false;    // 4X MSAA enabled
	UINT m4xMsaaQuality = 0;      // quality level of 4X MSAA

	static const int SwapChainBufferCount = 3;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	//根签名
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	//PSO
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSO;
	//ShaderCode和输入布局
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	//采样器
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSampleHeap = nullptr;

	friend class FrameResource;
};

#endif // !__DXAPP_H__