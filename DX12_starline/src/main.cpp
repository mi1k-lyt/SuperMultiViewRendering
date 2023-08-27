#include <iostream>
#include "DXApp.h"

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE preInstance,
	PSTR cmdLine,
	int showCmd) {

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	DXApp MyApp(hInstance);

	try {
		if (!MyApp.Init()) {
			return 0;
		}

		return MyApp.Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}


	return 0;

}