#include "TLS_Profiler.h"
#include <sstream>
#include <iomanip>
#include "CLog.h"
#include <utility>
#include <string>
using namespace C_Utility;
#define TLS_PROFILER_EXPORT

LARGE_INTEGER ProfileBoss::s_frequency = {};
volatile USHORT ProfileBoss::s_count = -1;
TlsProfileManager* ProfileBoss::s_managers[TLS_PROFILE_ARR_MAX]{};
ProfileBoss::ERROR_CODE ProfileBoss::s_errCode = ProfileBoss::ERROR_CODE::NONE;
FILE* ProfileWriter::file = nullptr;

//thread_local Wrapper wrapper;

/*--------------------------------------------------
		SAMPLE (MANAGER���� �����ϴ� ����ü)
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
	// Begin - [AllDataReset] - End �� ��Ȳ �Ǵ� UB
	if (!IStartTime.QuadPart)
	{
		wprintf(L"\nBegin - [ Tag : %s ] [ Thread - %d ] [ AllDataReset Called ] - End \n",szName,dwThreadId);
		return;
	}
	// ó�� ȣ�� �� N���� �Ϲ������� �����Ѵ�.
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

void C_Utility::TlsProfileSample::GetMaxLen(ItemType type, OUT std::wstring& prevMaxWstr)
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
		double calc = (double)iTotalTime / (ProfileBoss::GetFrequency().QuadPart * iCall) * 1000000;
		wss << std::fixed << std::setprecision(3) << calc << L"��";
		break;
	}
	case C_Utility::MIN:
	{
		if (iCall)
		{
			double calc = ((double)iMin[0]) / ProfileBoss::GetFrequency().QuadPart * 1000000;
			wss << std::fixed << std::setprecision(3) << calc << L"��";
		}
		break;
	}
	case C_Utility::MAX:
	{
		double calc = ((double)iMax[0]) / ProfileBoss::GetFrequency().QuadPart * 1000000;
		wss << std::fixed << std::setprecision(3) << calc << L"��";
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

void C_Utility::TlsProfileSample::WriteContent(size_t* len_arr)
{
	std::wstring attributes[6];

	GetMaxLen(THREAD_ID, attributes[THREAD_ID]);
	GetMaxLen(NAME, attributes[NAME]);
	GetMaxLen(AVERAGE, attributes[AVERAGE]);
	GetMaxLen(MIN, attributes[MIN]);
	GetMaxLen(MAX, attributes[MAX]);
	GetMaxLen(CALL, attributes[CALL]);

	// if CallCnt == 0, Skip
	if (attributes[CALL] == L"0")
		return;

	ProfileWriter::WriteContent(len_arr, attributes);
	return;
}

/*--------------------------------------------------
			MANAGER (�����帶�� 1����)
--------------------------------------------------*/

C_Utility::TlsProfileManager::TlsProfileManager()
{
	InitializeSRWLock(&_lock);
	ProfileBoss::RegistManager(this);
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
		pair.second->GetMaxLen((type), maxWstr);
	}
	return maxWstr.length();
}

void C_Utility::TlsProfileManager::WriteContent(size_t* len_arr)
{
	SRWLockGuard lockGuard(&_lock);
	for (auto& pair : _sampleMappingTable)
	{
		pair.second->WriteContent(len_arr);
	}
}

/*--------------------------------------------------
					BOSS (�Ѱ�)
--------------------------------------------------*/

bool C_Utility::ProfileBoss::RegistManager(TlsProfileManager* manager)
{
	USHORT cur = InterlockedIncrement16((SHORT*)&s_count);
	
	if (cur == 0) // �ʱ⿡ init ����
		Init();

	if (cur >= TLS_PROFILE_ARR_MAX)
	{
		s_errCode = ERROR_CODE::REGIST_FAILED;
		return false;
	}
	s_managers[cur] = manager;

	return true;
}

bool C_Utility::ProfileBoss::SaveFile(const WCHAR* fileName)
{
	if (false == ProfileWriter::Open(fileName))
	{
		s_errCode = ERROR_CODE::FILE_OPEN_FAILED;
		return false;
	}
	
	const wchar_t* str[] = { L"Thread ID", L"Func Name", L"Average",L"Min", L"Max", L"Call" };
	size_t len_arr[] = { wcslen(str[0]) ,wcslen(str[1]),wcslen(str[2]) ,wcslen(str[3]),wcslen(str[4]), wcslen(str[5]) };

	for (int i = 0; i <= s_count; i++)
	{
		TlsProfileManager* manager = s_managers[i];

		len_arr[0] = max(len_arr[0], manager->GetMaxLen(THREAD_ID));
		len_arr[1] = max(len_arr[1], manager->GetMaxLen(NAME));
		len_arr[2] = max(len_arr[2], manager->GetMaxLen(AVERAGE));
		len_arr[3] = max(len_arr[3], manager->GetMaxLen(MIN));
		len_arr[4] = max(len_arr[4], manager->GetMaxLen(MAX));
		len_arr[5] = max(len_arr[5], manager->GetMaxLen(CALL));
	}

	ProfileWriter::WriteTitle(len_arr, str, _countof(str));
	
	for (int i = 0; i <= s_count; i++)
	{
		std::wstring sample_attr[_countof(str)];

		TlsProfileManager* manager = s_managers[i];

		if (manager->GetElementsCount())
		{
			manager->WriteContent(len_arr);

			ProfileWriter::WriteEnter();
		}
	}

	ProfileWriter::WriteLine();

	ProfileWriter::Close();

	AllDataReset();

	return true;
}

bool C_Utility::ProfileBoss::AllDataReset()
{
	for (int i = 0; i <= s_count; i++)
	{
		s_managers[i]->ReleaseAll();
	}
	return true;
}

bool C_Utility::ProfileBoss::AllResourceRelease()
{
	USHORT sCnt = InterlockedExchange16((SHORT*)&s_count, -1);

	for (int i = 0; i <= sCnt; i++)
	{
		s_managers[i]->ReleaseAll();
		delete s_managers[i];
	}
	return true;
}

C_Utility::ProfileBoss::~ProfileBoss()
{
	AllResourceRelease();
}


bool C_Utility::ProfileWriter::Open(const WCHAR* fileName)
{
	std::wstring wstr(fileName);

	if (std::wstring::npos == wstr.find(L".txt"))
	{
		wstr += L".txt";
	}

	errno_t error = _wfopen_s(&file, wstr.c_str(), L"w,ccs=UTF-16LE");

	if (!file || error)
	{
		std::cout << "File is null [ GetLastError : " << GetLastError() << " ] " << std::endl;

		return false;
	}
	WriteLine();

	return true;
}

void C_Utility::ProfileWriter::WriteTitle(size_t* len_arr, const wchar_t** title_arr,int size)
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
	fwprintf_s(file, title.c_str());
	WriteLine();
}

void C_Utility::ProfileWriter::WriteContent(size_t* len_arr, std::wstring* attribute)
{
	std::wstring content;
	content += L"|";

	for (int k = 0; k < 6; k++)
	{
		int diff = len_arr[k] - wcslen(attribute[k].c_str());

		int r_padding_count = diff % 2 == 0 ? (diff >> 1) + 1 : (diff >> 1) + 2;

		int l_padding_count = (diff >> 1) + 1;

		for (int l = 0; l < l_padding_count; l++)
			content += L" ";

		content += attribute[k];


		for (int l = 0; l < r_padding_count; l++)
			content += L" ";

		content += L"|";
	}
	content += L"\n";

	fwprintf_s(file, content.c_str());
}

inline void C_Utility::ProfileWriter::WriteEnter()
{
	fwprintf_s(file, L"\n");
}

inline void C_Utility::ProfileWriter::WriteLine()
{
	fwprintf_s(file, L"--------------------------------------------------------------------------------------------\n");
}

void C_Utility::ProfileWriter::Close()
{
	if (file)
	{
		fclose(file);
		file = nullptr;
	}
}
