//***************************************************************
// DirectX_12以及Windows头文件
// directx/d3dx12.h包含CD3DX12的辅助结构
// 添加辅助结构，可能会导致与Windows SDK冲突
// 所以需要提前在包含目录中包含
// Created By YunTao Li
//***************************************************************

#ifndef __DX12WIN_H__
#define __DX12WIN_H__

// 添加Win
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <string>
#include <comdef.h>

// 添加wrl,支持ComPtr
#include <wrl.h>

// 添加DX12
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "directx/d3dx12.h"

// link lib
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
//***************************************************************
// 基础设置宏定义
//***************************************************************
#define WindowName L"Metahuman-MultiView-LightFiled-Renderer"
#define WndClassName L"MainWin"
#define BackBufferFormat DXGI_FORMAT_R8G8B8A8_UNORM
#define DepthStencilFormat DXGI_FORMAT_D24_UNORM_S8_UINT

#define MY_PI 3.1415926535f
//**************************************************************
// 分辨率设置
//**************************************************************
#define total_pixel_x 7680
#define total_pixel_y 4320

#define view_pixel_x 960
#define view_pixel_y 540

#define splatting_pixel_x 480
#define splatting_pixel_y 270

#define depth_pixel_x 960
#define depth_pixel_y 540
#define NumDepth 3

#define crop_depth_pixel_x 0
#define crop_depth_pixel_y 0

#define color_pixel_x 960
#define color_pixel_y 540
#define NumColor 3

#define crop_color_pixel_x 0
#define crop_color_pixel_y 0
//***************************************************************
// 近平面参数
//***************************************************************
#define down_left_x -2.0f
#define down_left_y -1.125f
#define down_left_z 7.4641f
#define scales_x 4.0f
#define scales_y 2.25f
//***************************************************************
// 光场编码参数
//***************************************************************
#define _ViewNum 70
#define _LineNum 30.90091
#define _IncAngle 0.1832
#define _shift 23
#define _k 2
#define _ViewDist 300.0f
#define _PerCamDeltaX 5.7971f

#define testViewNum 35
//***************************************************************
// DX12的异常处理抛出
//***************************************************************
class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) {};
    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;

    std::wstring ToString()const {
        // Get the string description of the error code.
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }
};
inline std::wstring AnsiToWString(const std::string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

//***************************************************************
// 向上字节对齐
//***************************************************************
#define UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B-1)))

#endif // !__DX12WIN_H__