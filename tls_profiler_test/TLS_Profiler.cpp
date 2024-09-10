#include "TLS_Profiler.h"
#include <sstream>
#include <iomanip>
#include "CLog.h"
#include <utility>
#include <string>
using namespace C_Utility;
#define TLS_PROFILER_EXPORT

#include <iostream>
#include <chrono>
//thread_local Wrapper wrapper;

/*--------------------------------------------------
		SAMPLE (MANAGER에서 관리하는 구조체)
--------------------------------------------------*/

C_Utility::TlsProfileSample::TlsProfileSample(const WCHAR* tag)
{
	InitializeSample(tag);
}

C_Utility::TlsProfileSample::~TlsProfileSample()
{
	std::wcout << L"[MANAGED THREAD ID - "<<dwThreadId<< "] TLS_PROFILE_SAMPLE Destructor "<< szName << std::endl;
}

void C_Utility::TlsProfileSample::InitializeSample(const WCHAR* tag)
{
	memset(this, 0, sizeof(TlsProfileSample));
	
	// if success 0
	errno_t err = wcscpy_s(szName, tag);

	iMin[0] = MAXINT64;
	iMin[1] = MAXINT64;

	dwThreadId = GetCurrentThreadId();
}

inline void C_Utility::TlsProfileSample::Begin()
{
	QueryPerformanceCounter(&IStartTime);
}

void C_Utility::TlsProfileSample::End(LARGE_INTEGER endTime)
{
	// Begin - [AllDataReset] - End 의 상황 또는 UB
	if (!IStartTime.QuadPart)
	{
		wprintf(L"\nBegin - [ Tag : %s ] [ Thread - %d ] [ AllDataReset Called ] - End \n",szName,dwThreadId);
		return;
	}
	// 처음 호출 후 N번은 일방적으로 무시한다.
	if (ucIgnoreCnt < IGNORE_CALL_COUNT)
	{
		++ucIgnoreCnt;
	
		return;
	}
	unsigned long long elapsed = endTime.QuadPart - IStartTime.QuadPart;
	iTotalTime += elapsed;

	if (elapsed < iMin[0])
		iMin[0] = elapsed;
	else if (elapsed < iMin[1])
		iMin[1] = elapsed;

	if (elapsed > iMax[0])
		iMax[0] = elapsed;
	else if (elapsed > iMax[1])
		iMax[1] = elapsed;

	iCall++;
}

void C_Utility::TlsProfileSample::GetMaxStr(ItemType type, OUT std::wstring& prevMaxWstr)
{
	int prevMaxLen = prevMaxWstr.length();

	std::wstringstream wss;
	switch (type)
	{
	case C_Utility::THREAD_ID:
	{
		wss << dwThreadId;
		break;
	}
	case C_Utility::NAME:
	{
		wss << szName;
		break;
	}
	case C_Utility::AVERAGE:
	{
		double calc = (double)iTotalTime / (ProfileBoss::GetInstance().GetFrequency().QuadPart * iCall) * 1000000;
		wss << std::fixed << std::setprecision(3) << calc << L"㎲";
		break;
	}
	case C_Utility::MIN:
	{
		if (iCall)
		{
			double calc = ((double)iMin[0]) / ProfileBoss::GetInstance().GetFrequency().QuadPart * 1000000;
			wss << std::fixed << std::setprecision(3) << calc << L"㎲";
		}
		break;
	}
	case C_Utility::MAX:
	{
		double calc = ((double)iMax[0]) / ProfileBoss::GetInstance().GetFrequency().QuadPart * 1000000;
		wss << std::fixed << std::setprecision(3) << calc << L"㎲";
		break;
	}
	case C_Utility::CALL:
	{
		wss << iCall;
		break;
	}
	default:
		return;
	}

	const std::wstring& newWstr = wss.str();

	if (prevMaxLen < newWstr.length())
	{
		prevMaxWstr = newWstr;
	}
}

void C_Utility::TlsProfileSample::ContainAttributes(std::vector<std::wstring>& attributesArray)
{
	std::wstring attributes[6];

	GetMaxStr(THREAD_ID, attributes[THREAD_ID]);
	GetMaxStr(NAME, attributes[NAME]);
	GetMaxStr(AVERAGE, attributes[AVERAGE]);
	GetMaxStr(MIN, attributes[MIN]);
	GetMaxStr(MAX, attributes[MAX]);
	GetMaxStr(CALL, attributes[CALL]);

	// if CallCnt == 0, Skip
	if (attributes[CALL] == L"0")
		return;

	for (int i = THREAD_ID; i < ITEM_TYPE_MAX; i++)
	{
		attributesArray.push_back(std::move(attributes[i]));
	}
	return;
}

/*--------------------------------------------------
			MANAGER (스레드마다 1개씩)
--------------------------------------------------*/

C_Utility::TlsProfileManager::TlsProfileManager()
{
	InitializeSRWLock(&_lock);
	ProfileBoss::GetInstance().RegistManager(this);
}

C_Utility::TlsProfileManager::~TlsProfileManager()
{
	std::wcout << L"PROFILE_MANAGER Destructor, Element Count : " << _sampleMappingTable.size() << std::endl;
}

TlsProfileSample* C_Utility::TlsProfileManager::GetSample(const WCHAR* tag)
{
	if (_sampleMappingTable.find(tag) == _sampleMappingTable.end())
	{
		TlsProfileSample* newSample = new TlsProfileSample(tag);
		_sampleMappingTable.insert({ tag, newSample });
		return newSample;
	}
	return _sampleMappingTable[tag];
}

bool C_Utility::TlsProfileManager::ReleaseAll()
{
	SRWLockGuard lockGuard(&_lock);

	for (auto iter = _sampleMappingTable.begin(); iter != _sampleMappingTable.end(); )
	{
		delete (iter->second);
	
		iter = _sampleMappingTable.erase(iter);
	}
	return true;
}

void C_Utility::TlsProfileManager::Begin(const WCHAR* tag)
{
	SRWLockGuard lockGuard(&_lock);
	TlsProfileSample* sample = GetSample(tag);
	
	sample->Begin();
}

void C_Utility::TlsProfileManager::End(const WCHAR* tag)
{
	SRWLockGuard lockGuard(&_lock);
	TlsProfileSample* sample = GetSample(tag);
	
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);
	sample->End(endTime);
}

int C_Utility::TlsProfileManager::GetMaxLen(ItemType type)
{
	if (type == THREAD_ID)
		return std::to_wstring(GetCurrentThreadId()).length();

	int maxLen = 0;
	std::wstring maxWstr(L"");
	for (auto& pair : _sampleMappingTable)
	{
		pair.second->GetMaxStr((type), maxWstr);
	}
	return maxWstr.length();
}

void C_Utility::TlsProfileManager::ContainAttributes(std::vector<std::wstring>& attributesArray)
{
	SRWLockGuard lockGuard(&_lock);
	for (auto& pair : _sampleMappingTable)
	{
		pair.second->ContainAttributes(attributesArray);
	}
}

/*--------------------------------------------------
					BOSS (총괄)
--------------------------------------------------*/

bool C_Utility::ProfileBoss::RegistManager(TlsProfileManager* manager)
{
	USHORT cur = InterlockedIncrement16((SHORT*)&_count);
	
	if (cur >= TLS_PROFILE_ARR_MAX)
	{
		_errCode = ERROR_CODE::REGIST_FAILED;
		return false;
	}
	_managers[cur] = manager;

	return true;
}

bool C_Utility::ProfileBoss::SaveFile(const WCHAR* fileName)
{
	SRWLockGuard lockGuard(&_writer.GetLock());

	if (false == _writer.Open(fileName))
	{
		_errCode = ERROR_CODE::FILE_OPEN_FAILED;
		return false;
	}
	
	const wchar_t* str[] = { L"Thread ID", L"Func Name", L"Average",L"Min", L"Max", L"Call" };
	size_t len_arr[] = { wcslen(str[0]) ,wcslen(str[1]),wcslen(str[2]) ,wcslen(str[3]),wcslen(str[4]), wcslen(str[5]) };

	// 카운트가 줄어들 일은 없다!
	int cntSnapshot = _count;
	for (int i = 0; i <= cntSnapshot; i++)
	{
		TlsProfileManager* manager = _managers[i];

		len_arr[0] = max(len_arr[0], manager->GetMaxLen(THREAD_ID));
		len_arr[1] = max(len_arr[1], manager->GetMaxLen(NAME));
		len_arr[2] = max(len_arr[2], manager->GetMaxLen(AVERAGE));
		len_arr[3] = max(len_arr[3], manager->GetMaxLen(MIN));
		len_arr[4] = max(len_arr[4], manager->GetMaxLen(MAX));
		len_arr[5] = max(len_arr[5], manager->GetMaxLen(CALL));
	}

	_writer.WriteTitle(len_arr, str, _countof(str));
	{
		std::vector<std::wstring> attributesArray;

		for (int i = 0; i <= cntSnapshot; i++)
		{
			TlsProfileManager* manager = _managers[i];

			int sampleCntSnapshot = manager->GetElementsCount();
			if (sampleCntSnapshot)
			{
				if (attributesArray.capacity() < sampleCntSnapshot)
					attributesArray.reserve(sampleCntSnapshot);
				
				//auto start = std::chrono::high_resolution_clock::now();
				manager->ContainAttributes(attributesArray);
				//auto end = std::chrono::high_resolution_clock::now();
				//std::chrono::duration<double> elapsed = end - start;
				//printf("%lf 초 경과\n", elapsed);

				_writer.WriteContent(len_arr, attributesArray);
				_writer.WriteEnter();
			}

			attributesArray.clear();
		}
	}
	_writer.WriteLine();

	_writer.Close();

	AllDataReset();

	return true;
}

bool C_Utility::ProfileBoss::AllDataReset()
{
	for (int i = 0; i <= _count; i++)
	{
		_managers[i]->ReleaseAll();
	}
	return true;
}

bool C_Utility::ProfileBoss::AllResourceRelease()
{
	USHORT sCnt = InterlockedExchange16((SHORT*)&_count, -1);

	for (int i = 0; i <= sCnt; i++)
	{
		_managers[i]->ReleaseAll();
		delete _managers[i];
	}
	return true;
}

bool C_Utility::ProfileBoss::ProfileWriter::Open(const WCHAR* fileName)
{
	std::wstring wstr(fileName);

	if (std::wstring::npos == wstr.find(L".txt"))
	{
		wstr += L".txt";
	}

	errno_t error = _wfopen_s(&_file, wstr.c_str(), L"w,ccs=UTF-16LE");

	if (!_file || error)
	{
		std::cout << "File is null [ GetLastError : " << GetLastError() << " ] " << std::endl;

		return false;
	}
	WriteLine();

	return true;
}

void C_Utility::ProfileBoss::ProfileWriter::WriteTitle(size_t* len_arr, const wchar_t** title_arr,int size)
{
	std::wstring title;

	title += L"|";
	for (int i = 0; i < size; i++) // thread_id, name . average. min, max, call
	{
		int diff = len_arr[i] - wcslen(title_arr[i]);

		int r_padding_count = diff % 2 == 0 ? (diff >> 1) + 1 : (diff >> 1) + 2;

		int l_padding_count = (diff >> 1) + 1;

		if (i > 1 && i < 5)
			++l_padding_count;

		for (int j = 0; j < l_padding_count; j++)
			title += L" ";

		title.append(title_arr[i]);

		for (int j = 0; j < r_padding_count; j++)
			title += L" ";

		title += L"|";
	}
	
	title += L"\n";
	fwprintf_s(_file, title.c_str());
	WriteLine();
}

void C_Utility::ProfileBoss::ProfileWriter::WriteContent(size_t* len_arr, std::vector<std::wstring>& attribute)
{
	std::wstring content;
	for (int i = 0; i < (attribute.size() / ITEM_TYPE_MAX); i++)
	{
		content.clear();
		content += L"|";

		for (int j = THREAD_ID + i * ITEM_TYPE_MAX; j < ITEM_TYPE_MAX * (i+1); j++)
		{
			int diff = len_arr[j%ITEM_TYPE_MAX] - wcslen(attribute[j].c_str());

			int r_padding_count = diff % 2 == 0 ? (diff >> 1) + 1 : (diff >> 1) + 2;

			int l_padding_count = (diff >> 1) + 1;

			for (int k = 0; k < l_padding_count; k++)
				content += L" ";

			content += attribute[j];


			for (int k = 0; k < r_padding_count; k++)
				content += L" ";

			content += L"|";
		}
		content += L"\n";

		fwprintf_s(_file, content.c_str());
	}
}

inline void C_Utility::ProfileBoss::ProfileWriter::WriteEnter()
{
	fwprintf_s(_file, L"\n");
}

inline void C_Utility::ProfileBoss::ProfileWriter::WriteLine()
{
	fwprintf_s(_file, L"--------------------------------------------------------------------------------------------\n");
}

void C_Utility::ProfileBoss::ProfileWriter::Close()
{
	if (_file)
	{
		fclose(_file);
		_file = nullptr;
	}
}
