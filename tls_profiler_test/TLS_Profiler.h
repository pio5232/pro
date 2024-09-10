#ifndef C_TLS_PROFILER
#define C_TLS_PROFILER


#include <Windows.h>
#include <vector>

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

#define TLS_SAVE(x) (ProfileBoss::GetInstance().SaveFile(L##x))
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
		ITEM_TYPE_MAX,
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
		void GetMaxStr(ItemType type, OUT std::wstring& prevMaxWstr);
		void ContainAttributes(std::vector<std::wstring>& attributesArray);
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

		int GetElementsCount() const { return _sampleMappingTable.size(); }
		int GetMaxLen(ItemType type);
		void ContainAttributes(std::vector<std::wstring>& attributesArray);

		~TlsProfileManager();
	private:
		friend class Wrapper;
		friend class ProfileBoss;

		bool ReleaseAll();
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

		static ProfileBoss& GetInstance()
		{
			static ProfileBoss instance;
			return instance;
		}
		inline LARGE_INTEGER GetFrequency() const { return _frequency;  }
		inline ERROR_CODE GetErrorCode() const { return _errCode; }
		bool RegistManager(TlsProfileManager* manager);
		bool SaveFile(const WCHAR* fileName);
	private:
		ProfileBoss() :_count(-1), _managers{},_errCode(ERROR_CODE::NONE),_writer() { QueryPerformanceFrequency(&_frequency); }
		~ProfileBoss() { AllResourceRelease(); }
		bool AllResourceRelease(); // delete all sample + manager
		bool AllDataReset(); // delete all sample
		LARGE_INTEGER _frequency;
		volatile USHORT _count;
		TlsProfileManager* _managers[TLS_PROFILE_ARR_MAX];
		ERROR_CODE _errCode;
		
	/*----------------------------------------
					파일 저장
	----------------------------------------*/
		class ProfileWriter
		{
		public:
			// 열기, Profiler 저장
			bool Open(const WCHAR* fileName);
			void WriteTitle(size_t* len_arr, const wchar_t** title_arr, int size);
			void WriteContent(size_t* len_arr, std::vector<std::wstring>& attribute);
			inline void WriteEnter();
			inline void WriteLine();
			inline SRWLOCK& GetLock() { return _fileLock; }
			void Close();
			ProfileWriter() :_file(nullptr) { InitializeSRWLock(&_fileLock); }
		private:
			FILE* _file;
			SRWLOCK _fileLock;
		};

		ProfileWriter _writer;

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

- 리소스의 해제는 ProfileBoss 소멸자에서 알아서 모든 리소스를 해제한다.

- 데이터를 뽑아내면 해당 스레드의 모든 sample을 삭제한다. (PROFILE_BOSS:AllDataReset) 
( TLS_PROFILE_MANAGER만 존재하는 형태 )


--------------------------------------------------------------------------------------*/






/*--------------------------------------------------
		   나중에 상황 봐서 추가해야 할 것

		1. PROFILE_BOSS의 에러 코드와 에러 처리 만들기.
--------------------------------------------------*/

