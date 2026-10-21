#ifndef _DISKBENCH_H
#define _DISKBENCH_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BENCH_BYTES_PER_MB 1048576.0
#define BENCH_SAMPLE_SECONDS 1.0	// Throughput is sampled (and graphed) once per interval.

enum BenchPhase { BenchPhaseWrite, BenchPhaseRead };
enum BenchEvent { BenchEventPhaseStart, BenchEventSample };
enum BenchResult { BenchOk, BenchCancelled, BenchFailed };

typedef struct _tag_BenchConfig {
	char Drive[MAX_PATH];
	DWORD BlockSize;		// Bytes per I/O. Must be a multiple of the volume's sector size.
	__int64 FileSize;		// Size of the test file. I/O wraps around it for the length of the test.
	int DurationSeconds;	// Length of the write phase, and of the read phase.
	bool AllowWriteCache;	// Writes use the OS cache. The flush is included in the timed region.
} BENCHCONFIG, * LPBENCHCONFIG;

typedef struct _tag_BenchStats {
	BenchPhase Phase;
	int Samples;			// One-second samples taken so far.
	double LastMBps;		// Throughput of the most recent sample.
	double MinMBps;			// Slowest sample.
	double MaxMBps;			// Fastest sample.
	double AvgMBps;			// Total bytes / total time (not a mean of per-sample rates).
	double Iops;			// Total I/Os / total time.
	double AvgLatencyMs;	// Total time / total I/Os.
	double TotalSeconds;	// Time actually spent in timed I/O.
	unsigned __int64 TotalBytes;
	unsigned __int64 TotalBlocks;
} BENCHSTATS, * LPBENCHSTATS;

// Called from the benchmark thread. iElapsedMs/iTotalMs describe progress across both phases.
typedef void (*BenchProgressCallback)(void* pContext, BenchEvent eEvent, const BENCHSTATS* pStats, int iElapsedMs, int iTotalMs);

DWORD BenchGetSectorSize(const char* sDrive);
DWORD BenchAlignBlockSize(const char* sDrive, DWORD dwBlockSize);

BenchResult RunDiskBench(const BENCHCONFIG* pConfig, volatile bool* pbCancel,
	BenchProgressCallback pCallback, void* pContext, char* sError, size_t iErrorSz);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
