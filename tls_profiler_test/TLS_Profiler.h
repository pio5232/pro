#ifndef C_TLS_PROFILER
#define C_TLS_PROFILER


#include <Windows.h>
//#include <unordered_map>
#include <map>
#include <iostream>
#include "Functor.h"

namespace C_Utility
{

	// 현재. BOSS에서 관리하는 MANAGER 최대 개수.
#define TLS_PROFILE_ARR_MAX 100

#define TLS_WIDE2(x) L##x
#define TLS_WIDE1(x) TLS_WIDE2(x)
#define TLS_WDATE TLS_WIDE1(__DATE__)
#define TLS_WTIME TLS_WIDE1(__TIME__)
#define TLS_WFILE TLS_WIDE1(__FILE__)
#define TLS_WFUNC TLS_WIDE1(__FUNCTION__)
#define TLS_WTIMESTAMP TLS_WIDE1(__TIMESTAMP__)
#define TLS_CHK_START Profiler pro(Wrapper::GetInstance().Get(), TLS_WFUNC)

#define TLS_SAVE(x) (ProfileBoss::SaveFile(L##x))
#define IGNORE_CALL_COUNT 1

	/* 공용 사용 타입 */
	enum ItemType : UINT16
	{
		THREAD_ID = 0,
		NAME,
		AVERAGE,
		MIN,
		MAX,
		CALL,
	};

	/*--------------------------------------------------
			SAMPLE (MANAGER에서 관리하는 구조체)
	--------------------------------------------------*/
	struct TlsProfileSample
	{
		void InitializeSample(const WCHAR* tag);
	public:
		TlsProfileSample(const WCHAR* tag);
		~TlsProfileSample();
		inline void Begin();
		void End(LARGE_INTEGER endTime);

		// 
		void GetMaxLen(ItemType type, OUT std::wstring& prevMaxWstr);
		void WriteContent(size_t* len_arr);
	private:
		WCHAR szName[64]; // 태그 이름
		//__int64 iFlag; // 프로파일의 사용 여부

		LARGE_INTEGER IStartTime; // 시작 시간

		unsigned long long iTotalTime; // 전체 사용 시간
		unsigned long long iMin[2]; // 최소 사용 시간
		unsigned long long iMax[2]; // 최대 사용 시간.
		unsigned long long iCall; // 호출 횟수 
		DWORD dwThreadId;
		BYTE ucIgnoreCnt;
	};

	/*--------------------------------------------------
				MANAGER (스레드마다 1개씩)
	--------------------------------------------------*/
	// 스레드 하나에 있는 TLS_PROFILE_SAMPLE[N]에 대한 정보를 가지고 있는 클래스.
	class TlsProfileManager final
	{
	private:
	public:
		using MappingTable = std::map < const WCHAR*, TlsProfileSample*, Functor>;
		void Begin(const WCHAR* tag);
		void End(const WCHAR* tag);
		friend class Wrapper;

		int GetElementsCount() const { return _sampleMappingTable.size(); }
		int GetMaxLen(ItemType type);
		void WriteContent(size_t* len_arr);
		bool ReleaseAll();

		~TlsProfileManager();
	private:
		TlsProfileManager();
		TlsProfileSample* GetSample(const WCHAR* tag);
		MappingTable _sampleMappingTable;
		SRWLOCK _lock;
	};

	/*-----------------------------
	*		Manager Wrapper
	-----------------------------*/
	class Wrapper
	{
	public:
		static Wrapper& GetInstance()
		{
			static thread_local Wrapper wrapper{};
			return wrapper;
		}
		inline TlsProfileManager* Get() const { return _manager; }

		Wrapper(const Wrapper&) = delete;
		Wrapper& operator =(const Wrapper&) = delete;
	private:
		explicit Wrapper() { _manager = new TlsProfileManager(); }

		TlsProfileManager* _manager;
	};
	
	/*--------------------------------------------------
						BOSS (총괄)
	--------------------------------------------------*/
	class ProfileBoss final
	{
	public:
		enum class ERROR_CODE
		{
			NONE = 0,
			REGIST_FAILED,
			FILE_OPEN_FAILED,
			FILE_WRITE_FAILED,
			RESOURCES_RELEASE_FAILED,
		};

		static void Init() {QueryPerformanceFrequency(&s_frequency);}
		inline static LARGE_INTEGER GetFrequency() { return s_frequency;  }
		inline static ERROR_CODE GetErrorCode() { return s_errCode; }
		static bool RegistManager(TlsProfileManager* manager);
		static bool SaveFile(const WCHAR* fileName);
		static bool AllResourceRelease(); // delete all sample + manager
	private:
		static bool AllDataReset(); // delete all sample
		static LARGE_INTEGER s_frequency;
		static volatile USHORT s_count;
		static TlsProfileManager* s_managers[TLS_PROFILE_ARR_MAX];
		static ERROR_CODE s_errCode;
	};

	/*----------------------------------------
					파일 저장
	----------------------------------------*/
	class ProfileWriter
	{
	public:
		// 열기, Profiler 저장
		static bool Open(const WCHAR* fileName);
		static void WriteTitle(size_t* len_arr, const wchar_t** title_arr,int size);
		static void WriteContent(size_t* len_arr, std::wstring* attribute);
		inline static void WriteEnter();
		inline static void WriteLine();
		static void Close();
	private:
		ProfileWriter(){}
		static FILE* file;

	};

	/*-------RAII--------
	*     Profiler 실행
	-------------------*/
	class Profiler
	{
	public:
		Profiler(TlsProfileManager* manager, const WCHAR* tag) : _tag(tag), _manager(manager) { _manager->Begin(tag); }
		~Profiler() { _manager->End(_tag); }
	private:
		TlsProfileManager* _manager;
		const WCHAR* _tag;
	};

	/*-------RAII--------
		  LockGuard
	-------------------*/

	class SRWLockGuard
	{
	public:
		SRWLockGuard(SRWLOCK* pLock) : _pLock(pLock) { AcquireSRWLockExclusive(_pLock); }
		~SRWLockGuard() { ReleaseSRWLockExclusive(_pLock); }
		
		SRWLOCK* _pLock;
	};

}

	//extern thread_local Wrapper wrapper;

#endif


/*--------------------------------------------------------------------------------------
									  사용 규칙

- 저장을 하게되면 현재 기록한 측정 데이터는 삭제된다. 

- 리소스의 해제는 PROFILE_BOSS::AllResourceRelease()를 통해 모든 리소스를 해제하도록 한다.

- 데이터를 뽑아내면 해당 스레드의 모든 sample을 삭제한다. (PROFILE_BOSS:AllDataReset) 
( TLS_PROFILE_MANAGER만 존재하는 형태 )


--------------------------------------------------------------------------------------*/






/*--------------------------------------------------
		   나중에 상황 봐서 추가해야 할 것

		1. PROFILE_BOSS의 에러 코드와 에러 처리 만들기.
		2. PROFILE_BOSS::AllResourceRelease() 로그를 눈으로 볼 수 있는 타이밍 정하기 (현재 소멸자)
		3. 멀티스레드 환경에서 파일 저장을 위한 lock 추가
--------------------------------------------------*/

