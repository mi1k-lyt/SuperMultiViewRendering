#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device) {
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(FrameCmdListAlloc.GetAddressOf()))
	);

	LoadImg();

	// ��ɫͼ����
	// ����������Դ������
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
	// ��������Ĭ�϶�
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&colorTexResourceDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&ColorTex)
	));
	// ��ȡ�ڴ沼��(����������ϴ��ѱ�Ҫ��һ��)
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
	// �����ϴ���
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(ColorTexRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&ColorTexUpload)
	));

	// ���ͼ����
	// ����������Դ������
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
	// ��������Ĭ�϶�
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthTexResourceDesc,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&DepthTex)
	));
	// ��ȡ�ڴ沼��(����������ϴ��ѱ�Ҫ��һ��)
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
	// �����ϴ���
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(DepthTexRequiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&DepthTexUpload)
	));

	// �ⳡ����ģ��
	D3D12_RESOURCE_DESC encodingTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_FLOAT,
		total_pixel_x,
		total_pixel_y,
		1,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // ����Ϊ����cs�������ó��������
	);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&encodingTexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&EncodingTex)
	));

	// �㽦������Χģ��
	D3D12_RESOURCE_DESC splattingTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32G32_FLOAT,
		splatting_pixel_x,
		splatting_pixel_y,
		_ViewNum,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // ����Ϊ����cs�������ó��������
	);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&splattingTexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&SplattingTex)
	));

	// �������ģ��
	D3D12_RESOURCE_DESC outVertexTexResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		total_pixel_x,
		total_pixel_y,
		1,// array size
		1,// mip levels
		1,// sam count
		0,// sam quality
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // ����Ϊ����cs�������ó��������
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
	std::string filePath = "ue_data_2\\";

	// ��ȡ��ɫͼ
	_leftColorImg = cv::imread(filePath + "left_color.png", cv::IMREAD_UNCHANGED);
	_midColorImg = cv::imread(filePath + "mid_color.png", cv::IMREAD_UNCHANGED);
	_rightColorImg = cv::imread(filePath + "right_color.png", cv::IMREAD_UNCHANGED);
	leftColorImg = reinterpret_cast<const void*>(_leftColorImg.data);
	midColorImg = reinterpret_cast<const void*>(_midColorImg.data);
	rightColorImg = reinterpret_cast<const void*>(_rightColorImg.data);

	//��ȡ���ͼ
	_leftDepthImg = cv::imread(filePath + "left_depth.tiff", cv::IMREAD_ANYDEPTH);
	_midDepthImg = cv::imread(filePath + "mid_depth.tiff", cv::IMREAD_ANYDEPTH);
	_rightDepthImg = cv::imread(filePath + "right_depth.tiff", cv::IMREAD_ANYDEPTH);
	leftDepthImg = reinterpret_cast<const void*>(_leftDepthImg.data);
	midDepthImg = reinterpret_cast<const void*>(_midDepthImg.data);
	rightDepthImg = reinterpret_cast<const void*>(_rightDepthImg.data);
}