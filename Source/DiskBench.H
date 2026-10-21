#ifndef _DISKBENCH_H
#define _DISKBENCH_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BENCH_BYTES_PER_MB 1048576.0

enum BenchPhase { BenchPhaseWrite, BenchPhaseRead };
enum BenchEvent { BenchEventPhaseStart, BenchEventRunDone };
enum BenchResult { BenchOk, BenchCancelled, BenchFailed };

typedef struct _tag_BenchConfig {
	char Drive[MAX_PATH];
	DWORD BlockSize;		// Bytes per I/O. Must be a multiple of the volume's sector size.
	int BlocksPerRun;		// I/Os issued (and timed) per run.
	int Runs;				// Timed runs per phase.
	bool AllowWriteCache;	// Writes use the OS cache. The flush is included in the timed region.
} BENCHCONFIG, * LPBENCHCONFIG;

typedef struct _tag_BenchStats {
	BenchPhase Phase;
	int RunsCompleted;
	double LastMBps;		// Throughput of the most recent run.
	double MinMBps;			// Slowest run.
	double MaxMBps;			// Fastest run.
	double AvgMBps;			// Total bytes / total time (not a mean of per-run rates).
	double Iops;			// Total I/Os / total time.
	double AvgLatencyMs;	// Total time / total I/Os.
	double TotalSeconds;	// Time actually spent in timed I/O.
	unsigned __int64 TotalBytes;
	unsigned __int64 TotalBlocks;
} BENCHSTATS, * LPBENCHSTATS;

// Called from the benchmark thread. iStep counts completed runs across both phases.
typedef void (*BenchProgressCallback)(void* pContext, BenchEvent eEvent, const BENCHSTATS* pStats, int iStep, int iTotalSteps);

DWORD BenchGetSectorSize(const char* sDrive);
DWORD BenchAlignBlockSize(const char* sDrive, DWORD dwBlockSize);

BenchResult RunDiskBench(const BENCHCONFIG* pConfig, volatile bool* pbCancel,
	BenchProgressCallback pCallback, void* pContext, char* sError, size_t iErrorSz);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
