#pragma once

#include "json.hpp"
using json = nlohmann::json;

#ifndef NOMINMAX
#define NOMINMAX
#endif // !NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <type_traits>

#include <filesystem>

namespace fs = std::filesystem;

#include <wrl.h> 
#include <d3d11.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#undef RegisterClass

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler") 
#pragma comment(lib, "DXGI") 

using namespace Microsoft::WRL;

#include <iostream>
#include <fstream>
#include "Containers/Containers.h"
#include "Core/EngineString.h"
#include "UObject/Casts.h"
#include "Math/EngineMath.h"
#include "Core/EngineLog.h"


