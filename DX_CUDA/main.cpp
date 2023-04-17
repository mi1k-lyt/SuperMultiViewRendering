#include "ray_tsdf_DX.h"
#include <iostream>
using namespace std;
int WINAPI WinMain(
	HINSTANCE hInstance, 
	HINSTANCE preInstance, 
	PSTR cmdLine, 
	int showCmd) {

    // WIN32应用程序调出控制台
    //AllocConsole();  //create console
    //SetConsoleTitle(L"SHMRenderDebugConsole"); //set console title   
    //FILE* tempFile = nullptr;
    //freopen_s(&tempFile, "conin$", "r+t", stdin); //reopen the stdin, we can use std::cout.
    //freopen_s(&tempFile, "conout$", "w+t", stdout);

    //std::cout << "hi" << std::endl;  //print ddd to console window
    //std::cin >> showCmd;
    //return showCmd;
    
//#if defined(DEBUG) | defined(_DEBUG)
//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//#endif

	try {
		RAYTSDF theApp(hInstance);
		if (!theApp.Initialize()) {
			return 0;
		}

		return theApp.Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}

}