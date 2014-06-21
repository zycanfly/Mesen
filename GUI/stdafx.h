// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Commdlg.h>

#ifdef _DEBUG
	#pragma comment( lib, "DirectXTK.debug.lib" )
#else 
	#pragma comment( lib, "DirectXTK.lib" )
#endif

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <io.h>
#include <Fcntl.h>
#include <thread>

using std::thread;

// TODO: reference additional headers your program requires here