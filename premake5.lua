workspace "Hitori"
	architecture "x64"
	startproject "HitoriEditor"

	configurations
	{
		"Debug",
		"Release",
		"ObjViewer",
		"Benchmark",
		"Benchmark_Debug",
	}

	multiprocessorcompile "On"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"] = "Source/ThirdParty/ImGui"
IncludeDir["stb"]   = "Source/ThirdParty/stb"
IncludeDir["json"]  = "Source/ThirdParty/json"
IncludeDir["DDSLoader"] = "Source/ThirdParty/DDSLoader"

-- premake의 filter는 project()를 만나면 초기화된다.
-- 두 프로젝트가 같은 런타임(/MDd vs /MD)으로 컴파일되지 않으면 링크가 실패하므로
-- 공통 설정을 함수로 묶어 각 프로젝트에서 호출한다.
function CommonSettings()
	language   "C++"
	cppdialect "C++20"
	staticruntime "off"
	characterset  "Unicode"

	targetdir ("Build/Bin/" .. outputdir)
	objdir    ("Build/Intermediate/" .. outputdir)
	-- 에셋·쉐이더를 상대 경로로 읽으므로 작업 디렉터리는 저장소 루트다.
	debugdir  "%{wks.location}"

	defines
	{
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"UNICODE",
		"_UNICODE",
	}

	filter "files:**.hlsl"
		excludefrombuild "On"

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines { "ENGINE_PLATFORM_WINDOWS" }

	filter "configurations:Debug"
		defines { "ENGINE_DEBUG", "_DEBUG" }
		runtime  "Debug"
		symbols  "on"

	filter "configurations:Release"
		defines  { "ENGINE_RELEASE", "NDEBUG" }
		runtime  "Release"
		optimize "on"
		symbols  "on"

	-- 에디터 없이 OBJ 파일만 열어보는 Viewer 빌드
	filter "configurations:ObjViewer"
		defines  { "ENGINE_RELEASE", "NDEBUG", "OBJ_VIEWER" }
		runtime  "Release"
		optimize "on"
		symbols  "on"

	filter "configurations:Benchmark_Debug"
		defines  { "ENGINE_DEBUG", "_DEBUG", "BENCHMARK" }
		runtime  "Debug"
		symbols  "on"

	filter "configurations:Benchmark"
		defines  { "ENGINE_RELEASE", "NDEBUG", "BENCHMARK" }
		runtime  "Release"
		optimize "speed"
		-- /GL + /LTCG는 끈다. 켠 exe가 Defender ML 탐지(Trojan:Win32/Wacatac)에 오탐으로 걸렸다.
		vectorextensions "AVX2"
		-- /GS-(버퍼 보안 검사 끄기)는 성능 이득이 거의 없고 백신 휴리스틱 오탐을 부르기 쉬워 켜 둔다 (기본값 /GS)
		buildoptions { "/Gw" }
		symbols  "on"



	filter {}
end


-- 외부 라이브러리. 별도 프로젝트로 두면 에디터의 소스 루트가 Source/Editor 하나로 좁혀져
-- Solution Explorer의 "모든 파일 표시"에서 폴더 구조가 그대로 보인다.
project "ImGui"
	location "Source/ThirdParty/ImGui"
	kind     "StaticLib"
	CommonSettings()

	files
	{
		"%{IncludeDir.ImGui}/*.h",
		"%{IncludeDir.ImGui}/*.cpp",
		"%{IncludeDir.ImGui}/backends/imgui_impl_win32.*",
		"%{IncludeDir.ImGui}/backends/imgui_impl_dx11.*",
	}

	includedirs
	{
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}


-- 런타임. 에디터를 모른다. include 경로에 Source/Editor가 없는 것이 그 방벽이다.
-- location이 소스 루트와 같아야 "모든 파일 표시"에서 폴더가 보인다.
project "HitoriEngine"
	location "Source/Runtime"
	kind     "StaticLib"
	CommonSettings()

	pchheader "EnginePCH.h"
	pchsource "Source/Runtime/EnginePCH.cpp"

	files
	{
		"Source/Runtime/**.h",
		"Source/Runtime/**.hpp",
		"Source/Runtime/**.cpp",
		"%{IncludeDir.DDSLoader}/*.h",
		"%{IncludeDir.DDSLoader}/*.cpp",
	}

	includedirs
	{
		"Source/Runtime",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.DDSLoader}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	filter "files:Source/ThirdParty/DDSLoader/**.cpp"
		enablepch "Off"
	filter {}

-- 에디터 애플리케이션. ObjViewer 구성에서는 같은 exe가 뷰어로 빌드된다.
project "HitoriEditor"
	location "Source/Editor"
	kind     "WindowedApp"
	CommonSettings()

	pchheader "EnginePCH.h"
	pchsource "Source/Editor/EditorPCH.cpp"

	links { "HitoriEngine", "ImGui" }

	files
	{
		"Source/Editor/**.h",
		"Source/Editor/**.cpp",
		"Source/Programs/**.h",
		"Source/Programs/**.cpp",
		"Source/Editor/**.rc",   -- 창·exe 아이콘
	}

	includedirs
	{
		"Source",             -- "Editor/OutputLog/ConsolePanel.h"처럼 Source 기준 include를 쓴다
		"Source/Runtime",
		"Source/Programs",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	filter "configurations:ObjViewer"
		targetname "ObjViewer"

	filter "configurations:Benchmark"
		targetname "Benchmark"

	filter "configurations:Benchmark_Debug"
		targetname "Benchmark_Debug"

	filter {}


-- 셰이더를 VS에서 편집하기 위한 보기 전용 프로젝트 (언리얼의 Shaders와 같은 역할).
-- 에셋(Assets)은 에디터 Content Drawer에서 다루므로 솔루션에 넣지 않는다.
-- 새 셰이더 파일은 SlnGenerator.bat을 다시 돌려야 목록에 반영된다.
project "Shaders"
	location "Intermediate/ProjectFiles"
	kind     "Utility"

	files { "Resources/Shader/**.hlsl" }
	files { "Resources/Shader/**.hlsli" }
	vpaths { ["*"] = "Resources/Shader/**" }

	filter "files:**.hlsl"
		excludefrombuild "On"

	filter "files:**.hlsli"
		excludefrombuild "On"

	filter {}
