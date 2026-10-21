#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <vector>

class FTaskPool
{
public:
	using FChunkFunc = std::function<void(uint32 Begin, uint32 End, uint32 Chunk)>;

	static FTaskPool& Get() { static FTaskPool Instance; return Instance; }
	~FTaskPool() { Shutdown(); }

	void Init(uint32 NumWorkers);
	void Shutdown();
	void ParallelFor(uint32 Count, uint32 ChunkCount, const FChunkFunc& Func);
	uint32 GetNumThreads() const { return static_cast<uint32>(Workers.size()) + 1; }
private:
	void WorkerLoop();
	void RunChunks();                        // 조각을 하나씩 가져가 실행 (워커와 메인 공용)

	std::vector<std::thread> Workers;

	// --- 깨우기 (Mutex로 보호) ---
	std::mutex Mutex;
	std::condition_variable WakeCV;
	uint64 Generation = 0;                   // 작업을 낼 때마다 +1
	bool bStop = false;

	// --- 현재 작업 (ParallelFor가 반환할 때까지 유효) ---
	const FChunkFunc* Func = nullptr;
	uint32 Count = 0;
	uint32 ChunkCount = 0;
	std::atomic<uint32> NextChunk{ 0 };      // 다음에 가져갈 조각 번호
	std::atomic<uint32> PendingWorkers{ 0 }; // 아직 이번 작업을 끝내지 않은 워커 수

};