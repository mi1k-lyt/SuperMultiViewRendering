#include "FrameResource.h"


FrameResource::FrameResource(ID3D12Device* device, UINT VertexCount) {
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(FrameCmdListAlloc.GetAddressOf()))
	);
	
	//mUploadVB = std::make_unique<UploadBuffer<Vertex>>(device, VertexCount, false);

	// cuca shared init
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_SHARED,
		&CD3DX12_RESOURCE_DESC::Buffer(VertexCount * sizeof(Vertex)),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&cuVertex)
	));

	//共享cuda/dx资源
	HANDLE cuSharedHandle{};
	device->CreateSharedHandle(cuVertex.Get(), 0, GENERIC_ALL, 0, &cuSharedHandle);

	UINT64 requiredSize = GetRequiredIntermediateSize(cuVertex.Get(), 0, 1);
	auto tempBuffer = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
	D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo;
	d3d12ResourceAllocationInfo = device->GetResourceAllocationInfo(
		0, 1, &tempBuffer);
	size_t actualSize = d3d12ResourceAllocationInfo.SizeInBytes;
	size_t alignment = d3d12ResourceAllocationInfo.Alignment;

	cudaExternalMemoryHandleDesc cuExMemHandleDesc{};
	cuExMemHandleDesc.type = cudaExternalMemoryHandleTypeD3D12Resource;
	cuExMemHandleDesc.handle.win32.handle = cuSharedHandle;
	cuExMemHandleDesc.size = actualSize;
	cuExMemHandleDesc.flags = cudaExternalMemoryDedicated;
	cudaImportExternalMemory(&cuExMem, &cuExMemHandleDesc);

	cudaExternalMemoryBufferDesc cuExMemBufferDesc{};
	cuExMemBufferDesc.offset = 0;
	cuExMemBufferDesc.size = requiredSize;
	cuExMemBufferDesc.flags = 0;
	cudaExternalMemoryGetMappedBuffer((void**)&d_crood, cuExMem, &cuExMemBufferDesc);
	assert(d_crood);



	//创建纹理资源描述符
	D3D12_RESOURCE_DESC colorTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		src_pixel_color_Nx,
		src_pixel_color_Ny,
		NumColor,//array size
		1,//mip_levels
		1,//sam count
		0,//sam quality
		D3D12_RESOURCE_FLAG_NONE);

	D3D12_RESOURCE_ALLOCATION_INFO colortexinfo = device->GetResourceAllocationInfo(
		0,
		1,
		&colorTexResourceDesc
	);

	D3D12_RESOURCE_DESC depthTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_FLOAT,
		src_pixel_depth_Nx,
		src_pixel_depth_Ny,
		NumDepth,//array size
		1,//mip_levels
		1,//sam count
		0,//sam quality
		D3D12_RESOURCE_FLAG_NONE);

	D3D12_RESOURCE_ALLOCATION_INFO depthtexinfo = device->GetResourceAllocationInfo(
		0,
		1,
		&depthTexResourceDesc
	);

	// tex init
	//创建纹理的默认堆(定位方式)
	D3D12_HEAP_DESC colorTexDefaultHeapDesc = {};
	// 使用64k内存对齐 msaa需要4m对齐
	colorTexDefaultHeapDesc.SizeInBytes = UPPER(
		colortexinfo.SizeInBytes,
		colortexinfo.Alignment);
	colorTexDefaultHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	colorTexDefaultHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	colorTexDefaultHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	colorTexDefaultHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	colorTexDefaultHeapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS;
	ThrowIfFailed(device->CreateHeap(&colorTexDefaultHeapDesc, IID_PPV_ARGS(&mColorTexDefaultHeap)));

	D3D12_HEAP_DESC depthTexDefaultHeapDesc = {};
	// 使用64k内存对齐 msaa需要4m对齐
	depthTexDefaultHeapDesc.SizeInBytes = UPPER(
		depthtexinfo.SizeInBytes,
		depthtexinfo.Alignment);
	depthTexDefaultHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	depthTexDefaultHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthTexDefaultHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthTexDefaultHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depthTexDefaultHeapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS;
	ThrowIfFailed(device->CreateHeap(&depthTexDefaultHeapDesc, IID_PPV_ARGS(&mDepthTexDefaultHeap)));

	//创建纹理的默认堆
	ThrowIfFailed(device->CreatePlacedResource(
		mColorTexDefaultHeap.Get(),
		D3D12_HEAP_FLAG_NONE,
		&colorTexResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mColorTexDefault)
	));

	ThrowIfFailed(device->CreatePlacedResource(
		mDepthTexDefaultHeap.Get(),
		D3D12_HEAP_FLAG_NONE,
		&depthTexResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mDepthTexDefault)
	));

	//获取内存布局
	device->GetCopyableFootprints(
		&mColorTexDefault->GetDesc(),
		0,//first subr
		NumColor,//num subr
		0,//base offset
		mColorTexLayouts,
		mColorTextureRowNum,
		mColorTextureRowSizes,
		&mColorRequiredSize
	);

	device->GetCopyableFootprints(
		&mDepthTexDefault->GetDesc(),
		0,//first subr
		NumDepth,//num subr
		0,//base offset
		mDepthTexLayouts,
		mDepthTextureRowNum,
		mDepthTextureRowSizes,
		&mDepthRequiredSize
	);

	D3D12_HEAP_DESC colorTexUploadHeapDesc = {};
	//使用64k内存对齐 msaa需要4m对齐
	colorTexUploadHeapDesc.SizeInBytes = mColorRequiredSize;
	colorTexUploadHeapDesc.Alignment = 0;
	colorTexUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	colorTexUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	colorTexUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	colorTexUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	ThrowIfFailed(device->CreateHeap(&colorTexUploadHeapDesc, IID_PPV_ARGS(&mColorTexUploadHeap)));

	D3D12_HEAP_DESC depthTexUploadHeapDesc = {};
	//使用64k内存对齐 msaa需要4m对齐
	depthTexUploadHeapDesc.SizeInBytes = mDepthRequiredSize;
	depthTexUploadHeapDesc.Alignment = 0;
	depthTexUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	depthTexUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthTexUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depthTexUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	ThrowIfFailed(device->CreateHeap(&depthTexUploadHeapDesc, IID_PPV_ARGS(&mDepthTexUploadHeap)));

	
	//创建纹理的上传堆
	ThrowIfFailed(device->CreatePlacedResource(
		mColorTexUploadHeap.Get(),
		0,
		&CD3DX12_RESOURCE_DESC::Buffer(mColorRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mColorTexUpload)
	));

	ThrowIfFailed(device->CreatePlacedResource(
		mDepthTexUploadHeap.Get(),
		0,
		&CD3DX12_RESOURCE_DESC::Buffer(mDepthRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mDepthTexUpload)
	));

	
	
	//cuda resource init
	mCuResource = std::make_unique<CudaResource>();
	mCuResource->LoadImg();
	UpdateTexture();

	


}

void FrameResource::UpdateTexture() {
	
	mCuResource->UpdateTexture();
}

FrameResource::~FrameResource() {
	cudaDestroyExternalMemory(cuExMem);
	mColorTexUpload->Unmap(0, nullptr);
}



