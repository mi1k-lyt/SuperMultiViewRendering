#include "FrameResource.h"

#include "root_directory.h"
#include "tinytiffreader.h"
#include <fstream>
FrameResource::FrameResource(ID3D12Device* device) {
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(FrameCmdListAlloc.GetAddressOf()))
	);

	LoadImg();

	// 彩色图纹理
	// 创建纹理资源描述符
	D3D12_RESOURCE_DESC colorTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		color_pixel_x,
		color_pixel_y,
		NumColor,//array size
		1,//mip_levels
		1,//sam count
		0,//sam quality
		D3D12_RESOURCE_FLAG_NONE
	);
	D3D12_RESOURCE_ALLOCATION_INFO colortexinfo = device->GetResourceAllocationInfo(
		0,
		1,
		&colorTexResourceDesc
	);
	// 创建纹理默认堆
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&colorTexResourceDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&ColorTex)
	));
	// 获取内存布局(创建纹理的上传堆必要的一步)
	UINT64 ColorTexRequiredSize;
	device->GetCopyableFootprints(
		&ColorTex->GetDesc(),
		0,//first sub
		NumColor,//num sub
		0,//base offset
		nullptr,
		nullptr,
		nullptr,
		&ColorTexRequiredSize
	);
	// 创建上传堆
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(ColorTexRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&ColorTexUpload)
	));

	// 深度图纹理
	// 创建纹理资源描述符
	D3D12_RESOURCE_DESC depthTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_FLOAT,
		depth_pixel_x,
		depth_pixel_y,
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
	// 创建纹理默认堆
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthTexResourceDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&DepthTex)
	));
	// 获取内存布局(创建纹理的上传堆必要的一步)
	UINT64 DepthTexRequiredSize;
	device->GetCopyableFootprints(
		&DepthTex->GetDesc(),
		0,//first sub
		NumDepth,//num sub
		0,//base offset
		nullptr,
		nullptr,
		nullptr,
		&DepthTexRequiredSize
	);
	// 创建上传堆
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(DepthTexRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&DepthTexUpload)
	));

	// 光场编码模板
	D3D12_RESOURCE_DESC encodingTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_FLOAT,
		total_pixel_x,
		total_pixel_y,
		1,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // 这里为了让cs计算设置成无序访问
	);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&encodingTexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&EncodingTex)
	));

	// 点溅步进范围模板
	D3D12_RESOURCE_DESC splattingTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32G32_FLOAT,
		splatting_pixel_x,
		splatting_pixel_y,
		_ViewNum,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // 这里为了让cs计算设置成无序访问
	);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&splattingTexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&SplattingTex)
	));

	// 输出顶点模板
	D3D12_RESOURCE_DESC outVertexTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		total_pixel_x,
		total_pixel_y,
		1,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // 这里为了让cs计算设置成无序访问
	);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&outVertexTexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&OutVertexTex)
	));

}

void FrameResource::LoadImg() {
	std::string rootPath = std::string(logl_root);
	std::string filePath = rootPath + "/Resource/ue_data/";
	int width, height, channels;
	// 读取彩色图

	leftColorImg = stbi_load((filePath + "left_color.png").c_str(), &width, &height, &channels, STBI_rgb_alpha);
	midColorImg = stbi_load((filePath + "mid_color.png").c_str(), &width, &height, &channels, STBI_rgb_alpha);
	rightColorImg = stbi_load((filePath + "right_color.png").c_str(), &width, &height, &channels, STBI_rgb_alpha);

	//读取深度图
	/*TinyTIFFReaderFile* test = TinyTIFFReader_open((filePath + "left_depth.tiff").c_str());

	leftDepthImg = ReadTiffDepthImage((filePath + "left_depth.tiff").c_str());
	midDepthImg = ReadTiffDepthImage((filePath + "mid_depth.tiff").c_str());
	rightDepthImg = ReadTiffDepthImage((filePath + "right_depth.tiff").c_str());*/

	/*cv::Mat _leftDepthImg = cv::imread((filePath + "left_depth.tiff"), cv::IMREAD_UNCHANGED);
	cv::Mat _midDepthImg = cv::imread((filePath + "mid_depth.tiff"), cv::IMREAD_UNCHANGED);
	cv::Mat _rightDepthImg = cv::imread((filePath + "right_depth.tiff"), cv::IMREAD_UNCHANGED);

	leftDepthImg = reinterpret_cast<void*>(_leftDepthImg.data);
	midDepthImg = reinterpret_cast<void*>(_midDepthImg.data);
	rightDepthImg = reinterpret_cast<void*>(_rightDepthImg.data);*/

	std::ifstream left_depthFile((filePath + "left_depth.bin").c_str(), std::ios::in | std::ios::binary);
	std::ifstream mid_depthFile((filePath + "mid_depth.bin").c_str(), std::ios::in | std::ios::binary);
	std::ifstream right_depthFile((filePath + "right_depth.bin").c_str(), std::ios::in | std::ios::binary);

	leftDepthImg = (leftDepthImg == nullptr ? malloc(depth_pixel_x * depth_pixel_y * sizeof(float)) : leftDepthImg);
	midDepthImg = (midDepthImg == nullptr ? malloc(depth_pixel_x * depth_pixel_y * sizeof(float)) : midDepthImg);
	rightDepthImg = (rightDepthImg == nullptr ? malloc(depth_pixel_x * depth_pixel_y * sizeof(float)) : rightDepthImg);

	left_depthFile.read(reinterpret_cast<char*>(leftDepthImg), depth_pixel_x * depth_pixel_y * sizeof(float));
	mid_depthFile.read(reinterpret_cast<char*>(midDepthImg), depth_pixel_x * depth_pixel_y * sizeof(float));
	right_depthFile.read(reinterpret_cast<char*>(rightDepthImg), depth_pixel_x * depth_pixel_y * sizeof(float));
}

void* FrameResource::ReadTiffDepthImage(const char* filePath)
{
	void* DepthImgData = nullptr;
	TinyTIFFReaderFile* tiffFile = TinyTIFFReader_open(filePath);
	if(!tiffFile)
	{
		OutputDebugString(L"Can not open tiff file!\n");
		return nullptr;
	}

	if(TinyTIFFReader_getBitsPerSample(tiffFile, 0) != 32 || TinyTIFFReader_getSampleFormat(tiffFile) != TINYTIFF_SAMPLEFORMAT_FLOAT)
	{
		OutputDebugString(L"Wrong tiff format!\n");
		TinyTIFFReader_close(tiffFile);
		return nullptr;
	}

	uint32_t width = TinyTIFFReader_getWidth(tiffFile);
	uint32_t height = TinyTIFFReader_getHeight(tiffFile);

	std::vector<float> ImgData(width * height);
	auto testnums = TinyTIFFReader_countFrames(tiffFile);

	if(!TinyTIFFReader_getSampleData(tiffFile, ImgData.data(), TINYTIFF_SAMPLEFORMAT_FLOAT))
	{
		OutputDebugString(L"cant read tiff!\n");
		return nullptr;
	}

	TinyTIFFReader_close(tiffFile);

	return ImgData.data();
}
