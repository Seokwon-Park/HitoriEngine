#include "EnginePCH.h"

#include "Launch/EntryPoint.h"

#include "Editor/HitoriEd/EditorEngine.h"
#ifdef OBJ_VIEWER
#include "Programs/ObjViewer/ObjViewerApp.h"
#elif BENCHMARK
#include "Programs/Benchmark/BenchmarkApp.h"
#endif

UClass* GetEngineClass()
{
#ifdef OBJ_VIEWER
	return UObjViewerEngine::StaticClass();
#elif BENCHMARK
	return UBenchmarkEngine::StaticClass();
#else
	return UEditorEngine::StaticClass();
#endif
}
