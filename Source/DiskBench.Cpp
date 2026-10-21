#ifndef _DISKBENCH_CPP
#define _DISKBENCH_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <WinIoCtl.H>

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

static void AccumulateSample(BENCHSTATS* pStats, double dSeconds, unsigned __int64 u64Bytes, unsigned __int64 u64Blocks)
{
	double dMBps = ((double)u64Bytes / dSeconds) / BENCH_BYTES_PER_MB;

	pStats->Samples++;
	pStats->LastMBps = dMBps;

	if (pStats->Samples == 1 || dMBps < pStats->MinMBps)
	{
		pStats->MinMBps = dMBps;
	}
	if (pStats->Samples == 1 || dMBps > pStats->MaxMBps)
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

static double SecondsBetween(const LARGE_INTEGER& liStart, const LARGE_INTEGER& liStop, const LARGE_INTEGER& liFrequency)
{
	return (double)(liStop.QuadPart - liStart.QuadPart) / (double)liFrequency.QuadPart;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Issues sequential I/O for DurationSeconds, wrapping at u64BlocksInFile, and records one sample per interval.
static BenchResult RunPhase(BenchPhase ePhase, HANDLE hFile, unsigned char* pBuffer, const BENCHCONFIG* pConfig,
	unsigned __int64 u64BlocksInFile, volatile bool* pbCancel, BenchProgressCallback pCallback, void* pContext,
	double dPhaseOffsetSeconds, BENCHSTATS* pStats, char* sError, size_t iErrorSz)
{
	LARGE_INTEGER liFrequency;
	QueryPerformanceFrequency(&liFrequency);

	memset(pStats, 0, sizeof(BENCHSTATS));
	pStats->Phase = ePhase;

	const int iTotalMs = pConfig->DurationSeconds * 2 * 1000;

	if (pCallback)
	{
		pCallback(pContext, BenchEventPhaseStart, pStats, (int)(dPhaseOffsetSeconds * 1000.0), iTotalMs);
	}

	LARGE_INTEGER liZero;
	liZero.QuadPart = 0;

	if (!SetFilePointerEx(hFile, liZero, NULL, FILE_BEGIN))
	{
		SetLastErrorText(sError, iErrorSz, "seek in", "the test file");
		return BenchFailed;
	}

	unsigned __int64 u64Position = 0; // Next block index.

	LARGE_INTEGER liPhaseStart;
	QueryPerformanceCounter(&liPhaseStart);

	for (;;)
	{
		LARGE_INTEGER liNow;
		QueryPerformanceCounter(&liNow);

		double dRemaining = (double)pConfig->DurationSeconds - SecondsBetween(liPhaseStart, liNow, liFrequency);
		if (dRemaining < 0.1)
		{
			break; // Not worth a sample.
		}

		double dTarget = (dRemaining < BENCH_SAMPLE_SECONDS) ? dRemaining : BENCH_SAMPLE_SECONDS;

		LARGE_INTEGER liStart = liNow;
		double dElapsed = 0;
		unsigned __int64 u64Blocks = 0;

		do {
			if (*pbCancel)
			{
				return BenchCancelled;
			}

			if (u64Position >= u64BlocksInFile)
			{
				if (!SetFilePointerEx(hFile, liZero, NULL, FILE_BEGIN))
				{
					SetLastErrorText(sError, iErrorSz, "seek in", "the test file");
					return BenchFailed;
				}
				u64Position = 0;
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

			u64Position++;
			u64Blocks++;

			QueryPerformanceCounter(&liNow);
			dElapsed = SecondsBetween(liStart, liNow, liFrequency);
		} while (dElapsed < dTarget);

		if (ePhase == BenchPhaseWrite && pConfig->AllowWriteCache)
		{
			// Data sitting in the OS cache has not been written yet. Make the disk do the work inside the timed region.
			if (!FlushFileBuffers(hFile))
			{
				SetLastErrorText(sError, iErrorSz, "flush", "the test file");
				return BenchFailed;
			}

			QueryPerformanceCounter(&liNow);
			dElapsed = SecondsBetween(liStart, liNow, liFrequency);
		}

		AccumulateSample(pStats, dElapsed, u64Blocks * pConfig->BlockSize, u64Blocks);

		if (pCallback)
		{
			double dPhaseElapsed = SecondsBetween(liPhaseStart, liNow, liFrequency);
			pCallback(pContext, BenchEventSample, pStats, (int)((dPhaseOffsetSeconds + dPhaseElapsed) * 1000.0), iTotalMs);
		}
	}

	return BenchOk;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BenchResult RunDiskBench(const BENCHCONFIG* pConfig, volatile bool* pbCancel,
	BenchProgressCallback pCallback, void* pContext, char* sError, size_t iErrorSz)
{
	sError[0] = '\0';

	if (pConfig->BlockSize == 0 || pConfig->FileSize < (__int64)pConfig->BlockSize || pConfig->DurationSeconds < 1)
	{
		strcpy_s(sError, iErrorSz, "Invalid benchmark parameters. The test file must hold at least one block.");
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

	// Whole blocks only. Reserve the space up front so the timed I/O measures writes, not file-system growth.
	unsigned __int64 u64BlocksInFile = (unsigned __int64)pConfig->FileSize / pConfig->BlockSize;

	LARGE_INTEGER liSize;
	liSize.QuadPart = (__int64)(u64BlocksInFile * pConfig->BlockSize);
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

	eResult = RunPhase(BenchPhaseWrite, hFile, pBuffer, pConfig, u64BlocksInFile, pbCancel, pCallback, pContext,
		0.0, &writeStats, sError, iErrorSz);
	CloseHandle(hFile);
	hFile = INVALID_HANDLE_VALUE;

	//-------------------------------------------------------------------------------------------------------------
	// Read phase. Always unbuffered so the numbers come from the device rather than the file cache.
	// Only the region the write phase actually filled is read: unwritten space reads back as instant zeros.

	if (eResult == BenchOk)
	{
		unsigned __int64 u64BlocksWritten = (writeStats.TotalBlocks < u64BlocksInFile) ? writeStats.TotalBlocks : u64BlocksInFile;

		hFile = CreateFile(sFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			SetLastErrorText(sError, iErrorSz, "open the temporary file", sFileName);
			eResult = BenchFailed;
		}
		else {
			eResult = RunPhase(BenchPhaseRead, hFile, pBuffer, pConfig, u64BlocksWritten, pbCancel, pCallback, pContext,
				(double)pConfig->DurationSeconds, &readStats, sError, iErrorSz);
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

static void CopyTrimmed(char* sOut, size_t iOutSz, const char* sIn)
{
	while (*sIn == ' ')
	{
		sIn++;
	}

	strcpy_s(sOut, iOutSz, sIn);

	size_t iLen = strlen(sOut);
	while (iLen > 0 && sOut[iLen - 1] == ' ')
	{
		sOut[--iLen] = '\0';
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* BusTypeName(STORAGE_BUS_TYPE eType)
{
	switch (eType)
	{
	case BusTypeScsi: return "SCSI";
	case BusTypeAtapi: return "ATAPI";
	case BusTypeAta: return "ATA";
	case BusType1394: return "IEEE 1394";
	case BusTypeSsa: return "SSA";
	case BusTypeFibre: return "Fibre Channel";
	case BusTypeUsb: return "USB";
	case BusTypeRAID: return "RAID";
	case BusTypeiScsi: return "iSCSI";
	case BusTypeSas: return "SAS";
	case BusTypeSata: return "SATA";
	case BusTypeSd: return "SD";
	case BusTypeMmc: return "MMC";
	case BusTypeVirtual: return "Virtual";
	case BusTypeFileBackedVirtual: return "File-backed virtual";
	case BusTypeSpaces: return "Storage Spaces";
	case BusTypeNvme: return "NVMe";
	default: return "Unknown";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Gathers what can be learned about the volume without elevation. Anything unavailable is left as "Unknown".
void BenchGetDriveInfo(const char* sDrive, BENCHDRIVEINFO* pInfo)
{
	memset(pInfo, 0, sizeof(BENCHDRIVEINFO));
	strcpy_s(pInfo->Root, sizeof(pInfo->Root), sDrive);
	strcpy_s(pInfo->VolumeLabel, sizeof(pInfo->VolumeLabel), "");
	strcpy_s(pInfo->FileSystem, sizeof(pInfo->FileSystem), "Unknown");
	strcpy_s(pInfo->DriveType, sizeof(pInfo->DriveType), "Unknown");
	strcpy_s(pInfo->Device, sizeof(pInfo->Device), "Unknown");
	strcpy_s(pInfo->Bus, sizeof(pInfo->Bus), "Unknown");
	strcpy_s(pInfo->Media, sizeof(pInfo->Media), "Unknown");

	GetVolumeInformation(sDrive, pInfo->VolumeLabel, sizeof(pInfo->VolumeLabel), NULL, NULL, NULL,
		pInfo->FileSystem, sizeof(pInfo->FileSystem));

	ULARGE_INTEGER uliAvailable, uliTotal, uliFree;
	if (GetDiskFreeSpaceEx(sDrive, &uliAvailable, &uliTotal, &uliFree))
	{
		pInfo->TotalBytes = uliTotal.QuadPart;
		pInfo->FreeBytes = uliFree.QuadPart;
	}

	switch (GetDriveType(sDrive))
	{
	case DRIVE_REMOVABLE: strcpy_s(pInfo->DriveType, sizeof(pInfo->DriveType), "Removable"); break;
	case DRIVE_FIXED: strcpy_s(pInfo->DriveType, sizeof(pInfo->DriveType), "Fixed"); break;
	case DRIVE_REMOTE: strcpy_s(pInfo->DriveType, sizeof(pInfo->DriveType), "Network"); break;
	case DRIVE_RAMDISK: strcpy_s(pInfo->DriveType, sizeof(pInfo->DriveType), "RAM disk"); break;
	}

	// Query the physical device behind the volume. Zero access rights are enough for property queries.
	char sDevice[8] = "\\\\.\\X:";
	sDevice[4] = sDrive[0];

	HANDLE hDevice = CreateFile(sDevice, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return;
	}

	STORAGE_PROPERTY_QUERY query;
	memset(&query, 0, sizeof(query));
	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;

	BYTE bBuffer[1024];
	DWORD dwReturned = 0;

	if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), bBuffer, sizeof(bBuffer), &dwReturned, NULL))
	{
		STORAGE_DEVICE_DESCRIPTOR* pDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)bBuffer;

		char sVendor[128] = "";
		char sProduct[128] = "";

		if (pDescriptor->VendorIdOffset && pDescriptor->VendorIdOffset < sizeof(bBuffer))
		{
			CopyTrimmed(sVendor, sizeof(sVendor), (const char*)bBuffer + pDescriptor->VendorIdOffset);
		}
		if (pDescriptor->ProductIdOffset && pDescriptor->ProductIdOffset < sizeof(bBuffer))
		{
			CopyTrimmed(sProduct, sizeof(sProduct), (const char*)bBuffer + pDescriptor->ProductIdOffset);
		}

		if (sVendor[0] || sProduct[0])
		{
			sprintf_s(pInfo->Device, sizeof(pInfo->Device), "%s%s%s", sVendor, (sVendor[0] && sProduct[0]) ? " " : "", sProduct);
		}

		strcpy_s(pInfo->Bus, sizeof(pInfo->Bus), BusTypeName(pDescriptor->BusType));
	}

	query.PropertyId = StorageDeviceSeekPenaltyProperty;
	DEVICE_SEEK_PENALTY_DESCRIPTOR penalty;
	memset(&penalty, 0, sizeof(penalty));

	if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &penalty, sizeof(penalty), &dwReturned, NULL))
	{
		strcpy_s(pInfo->Media, sizeof(pInfo->Media), penalty.IncursSeekPenalty ? "HDD" : "SSD");
	}

	CloseHandle(hDevice);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
