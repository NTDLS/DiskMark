#ifndef _DISKBENCH_CPP
#define _DISKBENCH_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

#include "DiskBench.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void SetLastErrorText(char* sError, size_t iErrorSz, const char* sAction, const char* sFileName)
{
	DWORD dwError = GetLastError();
	char sSystem[256] = "";

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwError, 0, sSystem, sizeof(sSystem), NULL);

	size_t iLen = strlen(sSystem);
	while (iLen > 0 && (sSystem[iLen - 1] == '\r' || sSystem[iLen - 1] == '\n' || sSystem[iLen - 1] == ' '))
	{
		sSystem[--iLen] = '\0';
	}

	sprintf_s(sError, iErrorSz, "Failed to %s \"%s\".\r\n%s (error %lu)", sAction, sFileName, sSystem, dwError);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD BenchGetSectorSize(const char* sDrive)
{
	DWORD dwSectorsPerCluster = 0;
	DWORD dwBytesPerSector = 0;
	DWORD dwFreeClusters = 0;
	DWORD dwTotalClusters = 0;

	if (GetDiskFreeSpace(sDrive, &dwSectorsPerCluster, &dwBytesPerSector, &dwFreeClusters, &dwTotalClusters)
		&& dwBytesPerSector > 0)
	{
		return dwBytesPerSector;
	}

	return 4096; // Safe for both 512 and 4K sector devices.
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Rounds the block size up to a multiple of the volume's sector size (required for unbuffered I/O).
DWORD BenchAlignBlockSize(const char* sDrive, DWORD dwBlockSize)
{
	DWORD dwSector = BenchGetSectorSize(sDrive);

	if (dwBlockSize < dwSector)
	{
		return dwSector;
	}

	return ((dwBlockSize + dwSector - 1) / dwSector) * dwSector;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// xorshift64 fill. Incompressible data so controllers that compress/dedupe cannot inflate the results.
static void FillRandom(unsigned char* pBuffer, DWORD dwSize, unsigned __int64 u64Seed)
{
	unsigned __int64 s = u64Seed | 1;

	DWORD i = 0;
	for (; i + 8 <= dwSize; i += 8)
	{
		s ^= s << 13;
		s ^= s >> 7;
		s ^= s << 17;
		*(unsigned __int64*)(pBuffer + i) = s;
	}
	for (; i < dwSize; i++)
	{
		pBuffer[i] = (unsigned char)(s >> ((i & 7) * 8));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void AccumulateRun(BENCHSTATS* pStats, double dSeconds, unsigned __int64 u64Bytes, unsigned __int64 u64Blocks)
{
	double dMBps = ((double)u64Bytes / dSeconds) / BENCH_BYTES_PER_MB;

	pStats->RunsCompleted++;
	pStats->LastMBps = dMBps;

	if (pStats->RunsCompleted == 1 || dMBps < pStats->MinMBps)
	{
		pStats->MinMBps = dMBps;
	}
	if (pStats->RunsCompleted == 1 || dMBps > pStats->MaxMBps)
	{
		pStats->MaxMBps = dMBps;
	}

	pStats->TotalSeconds += dSeconds;
	pStats->TotalBytes += u64Bytes;
	pStats->TotalBlocks += u64Blocks;

	pStats->AvgMBps = ((double)pStats->TotalBytes / pStats->TotalSeconds) / BENCH_BYTES_PER_MB;
	pStats->Iops = (double)pStats->TotalBlocks / pStats->TotalSeconds;
	pStats->AvgLatencyMs = (pStats->TotalSeconds * 1000.0) / (double)pStats->TotalBlocks;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BenchResult RunPhase(BenchPhase ePhase, HANDLE hFile, unsigned char* pBuffer, const BENCHCONFIG* pConfig,
	volatile bool* pbCancel, BenchProgressCallback pCallback, void* pContext, int iStepBase, BENCHSTATS* pStats,
	char* sError, size_t iErrorSz)
{
	LARGE_INTEGER liFrequency;
	QueryPerformanceFrequency(&liFrequency);

	memset(pStats, 0, sizeof(BENCHSTATS));
	pStats->Phase = ePhase;

	if (pCallback)
	{
		pCallback(pContext, BenchEventPhaseStart, pStats, iStepBase, pConfig->Runs * 2);
	}

	LARGE_INTEGER liZero;
	liZero.QuadPart = 0;

	for (int iRun = 0; iRun < pConfig->Runs; iRun++)
	{
		if (!SetFilePointerEx(hFile, liZero, NULL, FILE_BEGIN))
		{
			SetLastErrorText(sError, iErrorSz, "seek in", pConfig->Drive);
			return BenchFailed;
		}

		LARGE_INTEGER liStart, liStop;
		QueryPerformanceCounter(&liStart);

		for (int iBlock = 0; iBlock < pConfig->BlocksPerRun; iBlock++)
		{
			if (*pbCancel)
			{
				return BenchCancelled;
			}

			DWORD dwTransferred = 0;
			BOOL bOk = (ePhase == BenchPhaseWrite)
				? WriteFile(hFile, pBuffer, pConfig->BlockSize, &dwTransferred, NULL)
				: ReadFile(hFile, pBuffer, pConfig->BlockSize, &dwTransferred, NULL);

			if (!bOk || dwTransferred != pConfig->BlockSize)
			{
				if (bOk)
				{
					SetLastError(ERROR_HANDLE_EOF);
				}
				SetLastErrorText(sError, iErrorSz, (ePhase == BenchPhaseWrite) ? "write to" : "read from", "the test file");
				return BenchFailed;
			}
		}

		if (ePhase == BenchPhaseWrite && pConfig->AllowWriteCache)
		{
			// Data sitting in the OS cache has not been written yet. Make the disk do the work inside the timed region.
			if (!FlushFileBuffers(hFile))
			{
				SetLastErrorText(sError, iErrorSz, "flush", "the test file");
				return BenchFailed;
			}
		}

		QueryPerformanceCounter(&liStop);

		double dSeconds = (double)(liStop.QuadPart - liStart.QuadPart) / (double)liFrequency.QuadPart;
		if (dSeconds <= 0)
		{
			continue; // Unmeasurably short; skip rather than report an infinite rate.
		}

		AccumulateRun(pStats, dSeconds,
			(unsigned __int64)pConfig->BlockSize * pConfig->BlocksPerRun, (unsigned __int64)pConfig->BlocksPerRun);

		if (pCallback)
		{
			pCallback(pContext, BenchEventRunDone, pStats, iStepBase + iRun + 1, pConfig->Runs * 2);
		}
	}

	return BenchOk;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BenchResult RunDiskBench(const BENCHCONFIG* pConfig, volatile bool* pbCancel,
	BenchProgressCallback pCallback, void* pContext, char* sError, size_t iErrorSz)
{
	sError[0] = '\0';

	if (pConfig->BlockSize == 0 || pConfig->BlocksPerRun < 1 || pConfig->Runs < 1)
	{
		strcpy_s(sError, iErrorSz, "Invalid benchmark parameters.");
		return BenchFailed;
	}

	DWORD dwSector = BenchGetSectorSize(pConfig->Drive);
	if (pConfig->BlockSize % dwSector)
	{
		sprintf_s(sError, iErrorSz, "The block size must be a multiple of the volume sector size (%lu bytes).", dwSector);
		return BenchFailed;
	}

	// VirtualAlloc is page aligned, which satisfies the buffer alignment rules of unbuffered I/O.
	unsigned char* pBuffer = (unsigned char*)VirtualAlloc(NULL, pConfig->BlockSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pBuffer)
	{
		strcpy_s(sError, iErrorSz, "Failed to allocate the I/O buffer.");
		return BenchFailed;
	}

	LARGE_INTEGER liSeed;
	QueryPerformanceCounter(&liSeed);
	FillRandom(pBuffer, pConfig->BlockSize, (unsigned __int64)liSeed.QuadPart ^ ((unsigned __int64)GetCurrentProcessId() << 32));

	char sRoot[MAX_PATH];
	strcpy_s(sRoot, sizeof(sRoot), pConfig->Drive);
	size_t iRootLen = strlen(sRoot);
	while (iRootLen > 0 && (sRoot[iRootLen - 1] == '\\' || sRoot[iRootLen - 1] == '/'))
	{
		sRoot[--iRootLen] = '\0';
	}

	char sFileName[MAX_PATH];
	sprintf_s(sFileName, sizeof(sFileName), "%s\\DiskMark-%lu-%lu.tmp", sRoot, GetCurrentProcessId(), GetTickCount());

	BenchResult eResult = BenchFailed;
	BENCHSTATS writeStats, readStats;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	//-------------------------------------------------------------------------------------------------------------
	// Write phase.

	DWORD dwWriteFlags = FILE_ATTRIBUTE_NORMAL;
	if (!pConfig->AllowWriteCache)
	{
		dwWriteFlags |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
	}

	char sFolder[MAX_PATH] = "";

	hFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, dwWriteFlags, NULL);
	if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED)
	{
		// Non-admin users cannot create files in a volume root, but can create folders there.
		sprintf_s(sFolder, sizeof(sFolder), "%s\\DiskMark-Temp", sRoot);
		if (CreateDirectory(sFolder, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			sprintf_s(sFileName, sizeof(sFileName), "%s\\DiskMark-%lu-%lu.tmp", sFolder, GetCurrentProcessId(), GetTickCount());
			hFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, dwWriteFlags, NULL);
		}
		else {
			SetLastError(ERROR_ACCESS_DENIED);
		}
	}
	if (hFile == INVALID_HANDLE_VALUE)
	{
		SetLastErrorText(sError, iErrorSz, "create the temporary file", sFileName);
		if (sFolder[0])
		{
			RemoveDirectory(sFolder);
		}
		VirtualFree(pBuffer, 0, MEM_RELEASE);
		return BenchFailed;
	}

	// Reserve the space up front so the timed runs measure writes, not file-system growth.
	LARGE_INTEGER liSize;
	liSize.QuadPart = (__int64)pConfig->BlockSize * pConfig->BlocksPerRun;
	if (!SetFilePointerEx(hFile, liSize, NULL, FILE_BEGIN) || !SetEndOfFile(hFile))
	{
		SetLastErrorText(sError, iErrorSz, "allocate the temporary file", sFileName);
		CloseHandle(hFile);
		DeleteFile(sFileName);
		if (sFolder[0])
		{
			RemoveDirectory(sFolder);
		}
		VirtualFree(pBuffer, 0, MEM_RELEASE);
		return BenchFailed;
	}

	eResult = RunPhase(BenchPhaseWrite, hFile, pBuffer, pConfig, pbCancel, pCallback, pContext, 0, &writeStats, sError, iErrorSz);
	CloseHandle(hFile);
	hFile = INVALID_HANDLE_VALUE;

	//-------------------------------------------------------------------------------------------------------------
	// Read phase. Always unbuffered so the numbers come from the device rather than the file cache.

	if (eResult == BenchOk)
	{
		hFile = CreateFile(sFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			SetLastErrorText(sError, iErrorSz, "open the temporary file", sFileName);
			eResult = BenchFailed;
		}
		else {
			eResult = RunPhase(BenchPhaseRead, hFile, pBuffer, pConfig, pbCancel, pCallback, pContext, pConfig->Runs, &readStats, sError, iErrorSz);
			CloseHandle(hFile);
		}
	}

	DeleteFile(sFileName);
	if (sFolder[0])
	{
		RemoveDirectory(sFolder);
	}
	VirtualFree(pBuffer, 0, MEM_RELEASE);

	return eResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
