/*
Copyright (c) 2015 Maarten Boone

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "stdafx.h"
#include "Mrrng64.h"

using namespace Mrrng64;

typedef union _SEED {
	UCHAR	u8[8];
	WORD	u16[4];
	DWORD	u32[2];
	DWORD64	u64;
} SEED, *PSEED;

void Mrrng64::Init(DWORD64* value)
{
	SEED seed = { 0 };
	seed.u64 = *value;

	// Mixing in the memory counters
	AddMemoryCounters(&seed.u64);
	// Mixing in the IO counters
	AddIoCounters(&seed.u64);
	// Mixing in cursor position 
	AddCursorPosition(&seed.u64);

	for (int i = 0; i < 32; i++)
	{
		// Mixing in the current thread's cycle time
		AddCycleTime(&seed.u64, false);
		// Mixing in the current filetime and performance counter
		AddTimeStamp(&seed.u64);
	}

	*value = seed.u64;
}

// Generic function to mix in new entropy sources
void Mrrng64::Add(DWORD64* value, PVOID buffer, DWORD length)
{
	SEED seed = { 0 };
	seed.u64 = *value;
		
	for (DWORD i = 0; i < length; i++)
	{
		// Only mix in values bigger then 0
		if (((UCHAR*)buffer)[i])
		{
			// Add some magic
			seed.u32[i % 2] += 0x9e3779b9;
			// Mix byte into state
			seed.u32[i % 2] ^= ((UCHAR*)buffer)[i];
			// Make sure we have smooth value distribution
			seed.u32[i % 2] = (seed.u32[i % 2] * 0xFFFFFFED + 0xFFFFFFC3) & 0xFFFFFFFF;
		}
	}
	*value = seed.u64;
}

void Mrrng64::AddTimeStamp(DWORD64* value)
{
	FILETIME SystemTimeAsFileTime = { 0 };
	LARGE_INTEGER PerformanceCount = { 0 };
	
	SEED seed = { 0 };
	seed.u64 = *value;
	
	// Mixing in systemtime in filetime format
	GetSystemTimeAsFileTime(&SystemTimeAsFileTime);
	Add(&seed.u64, &SystemTimeAsFileTime, sizeof(FILETIME));

	// Mixing in performance counter
	QueryPerformanceCounter(&PerformanceCount);
	Add(&seed.u64, &PerformanceCount, sizeof(LARGE_INTEGER));
		
	*value = seed.u64;
}

void Mrrng64::AddIoCounters(DWORD64* value)
{
	IO_COUNTERS IoCounters = { 0 };
	SEED seed = { 0 };
	
	seed.u64 = *value;

	if (GetProcessIoCounters(GetCurrentProcess(), &IoCounters))
	{
		if (IoCounters.ReadOperationCount)
			Add(&seed.u64, &IoCounters.ReadOperationCount, sizeof(DWORD64));
		if (IoCounters.WriteOperationCount)
			Add(&seed.u64, &IoCounters.WriteOperationCount, sizeof(DWORD64));
		if (IoCounters.OtherOperationCount)
			Add(&seed.u64, &IoCounters.OtherOperationCount, sizeof(DWORD64));
		if (IoCounters.ReadTransferCount)
			Add(&seed.u64, &IoCounters.ReadTransferCount, sizeof(DWORD64));
		if (IoCounters.WriteTransferCount)
			Add(&seed.u64, &IoCounters.WriteTransferCount, sizeof(DWORD64));
		if (IoCounters.OtherTransferCount)
			Add(&seed.u64, &IoCounters.OtherTransferCount, sizeof(DWORD64));
	}

	*value = seed.u64;
}

void Mrrng64::AddMemoryCounters(DWORD64* value)
{
	PROCESS_MEMORY_COUNTERS MemoryCounters = { 0 };
	SEED seed = { 0 };
	seed.u64 = *value;
	
	if (GetProcessMemoryInfo(GetCurrentProcess(), &MemoryCounters, sizeof(PROCESS_MEMORY_COUNTERS)))
	{
		if (MemoryCounters.PageFaultCount)
			Add(&seed.u64, &MemoryCounters.PageFaultCount, sizeof(DWORD));
		else if (MemoryCounters.PeakWorkingSetSize)
			Add(&seed.u64, &MemoryCounters.PeakWorkingSetSize, sizeof(size_t));
		else if (MemoryCounters.WorkingSetSize)
			Add(&seed.u64, &MemoryCounters.WorkingSetSize, sizeof(size_t));
		else if (MemoryCounters.QuotaPeakPagedPoolUsage)
			Add(&seed.u64, &MemoryCounters.QuotaPeakPagedPoolUsage, sizeof(size_t));
		else if (MemoryCounters.QuotaPagedPoolUsage)
			Add(&seed.u64, &MemoryCounters.QuotaPagedPoolUsage, sizeof(size_t));
		else if (MemoryCounters.QuotaPeakNonPagedPoolUsage)
			Add(&seed.u64, &MemoryCounters.QuotaPeakNonPagedPoolUsage, sizeof(size_t));
		else if (MemoryCounters.QuotaNonPagedPoolUsage)
			Add(&seed.u64, &MemoryCounters.QuotaNonPagedPoolUsage, sizeof(size_t));
		else if (MemoryCounters.PagefileUsage)
			Add(&seed.u64, &MemoryCounters.PagefileUsage, sizeof(size_t));
		else if (MemoryCounters.PeakPagefileUsage)
			Add(&seed.u64, &MemoryCounters.PeakPagefileUsage, sizeof(size_t));
	}
	*value = seed.u64;
}

void Mrrng64::AddCycleTime(DWORD64* value, bool process)
{
	DWORD64 cycles = 0;
	SEED seed = { 0 };

	seed.u64 = *value;

	// Better not use the process cycle time since it's very slow
	if (process && QueryProcessCycleTime(GetCurrentProcess(), &cycles))
	{
		Add(&seed.u64, &cycles, sizeof(DWORD64));
	}
	else if (QueryThreadCycleTime(GetCurrentThread(), &cycles))
	{
		Add(&seed.u64, &cycles, sizeof(DWORD64));
	}

	*value = seed.u64;
}

// Mixing in the curson position
void Mrrng64::AddCursorPosition(DWORD64* value)
{
	POINT lpPoint = { 0 };
	SEED seed = { 0 };
	seed.u64 = *value;

	if (GetCursorPos(&lpPoint))
	{
		Add(&seed.u64, &lpPoint, sizeof(POINT));
	}
	*value = seed.u64;
}

DWORD64 Mrrng64::Next(DWORD64* value)
{
	SEED seed = { 0 };
	seed.u64 = *value;

	// If seed is 0 initialize the value first
	if (!seed.u64)
	{
		Init(&seed.u64);
	}

	// Mixing in cursor position (doesn't add much yet makes generation factor 2 slower)
	//AddCursorPosition(&seed.u64);
	// Mixing in the IO counters (could be removed for better performance but still generating 1Gig of random data per minut with it)
	AddIoCounters(&seed.u64);
	// Mixing in the current thread's cycle time
	AddCycleTime(&seed.u64, false);
	// Mixing in the current filetime and performance counter
	AddTimeStamp(&seed.u64);
	
	*value = seed.u64;
	return seed.u64;
}

void Mrrng64::CalculateEntropy(FILE* file)
{
	DWORD frequencyList[256] = { 0 };
	size_t fileSize = { 0 };
	double frequency = { 0 };
	double entropy = 0.0;
	
	fseek(file, 0L, SEEK_END);
	fileSize = _ftelli64(file);
	fseek(file, 0L, SEEK_SET);

	UCHAR current[0x4000] = { 0 };
	size_t bytesRead;

	while ((bytesRead = fread(&current, 0x01, 0x4000, file)) > 0)
	{
		for (size_t j = 0; j < bytesRead; j++)
		{
			frequencyList[current[j]]++;
		}
	}

	for each (DWORD f in frequencyList)
	{
		if (f > 0)
		{
			frequency = double(f) / fileSize;
			entropy += frequency * log2(frequency);
		}
	}
	entropy *= -1;

	printf("Entropy: %.*lf\n", 11, entropy);
}

int _tmain(int argc, _TCHAR* argv[])
{
	SEED seed = { 0 };

	FILE* rn;

	fopen_s(&rn, "RNG.dat", "wb+");
	
	printf("Starting random generation...\n");

	
	float startTime = (float)clock() / CLOCKS_PER_SEC;
	
	for (int y = 0; y < (1024 * 1024) * 128; y++)
	{
		Next(&seed.u64);
		fwrite(&seed.u64, 0x08, 0x01, rn);
	}

	float endTime = (float)clock() / CLOCKS_PER_SEC;
	
	printf("%f\n", endTime - startTime);
	
	CalculateEntropy(rn);

	fclose(rn);


	return 0;
}

