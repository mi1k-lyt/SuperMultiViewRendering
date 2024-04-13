//***************************************************************
// DirectX_12�Լ�Windowsͷ�ļ�
// directx/d3dx12.h����CD3DX12�ĸ����ṹ
// ���Ӹ����ṹ�����ܻᵼ����Windows SDK��ͻ
// ������Ҫ��ǰ�ڰ���Ŀ¼�а���
// Created By YunTao Li
//***************************************************************

#ifndef __DX12WIN_H__
#define __DX12WIN_H__

// ����Win
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <string>
#include <comdef.h>

// ����wrl,֧��ComPtr
#include <wrl.h>

// ����DX12
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <directx/d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "directx/d3dx12.h"

// link lib
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(DEBUG) || defined(_DEBUG) 
#include <dxgidebug.h>
#endif
//***************************************************************
// �������ú궨��
//***************************************************************
#define WindowName L"Metahuman-MultiView-LightFiled-Renderer"
#define WndClassName L"MainWin"
#define BackBufferFormat DXGI_FORMAT_R8G8B8A8_UNORM
#define DepthStencilFormat DXGI_FORMAT_D24_UNORM_S8_UINT

#define MY_PI 3.1415926535f
//**************************************************************
// �ֱ�������
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
// ��ƽ�����
//***************************************************************
#define down_left_x -2.0f
#define down_left_y -1.125f
#define down_left_z 7.4641f
#define scales_x 4.0f
#define scales_y 2.25f
//***************************************************************
// �ⳡ�������
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
// DX12���쳣�����׳�
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

#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}


//***************************************************************
// �����ֽڶ���
//***************************************************************
#define UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B-1)))

#endif // !__DX12WIN_H__