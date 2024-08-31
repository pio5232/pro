#ifndef C_TLS_PROFILER
#define C_TLS_PROFILER


#include <Windows.h>
#include <unordered_map>
#include <iostream>

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
	struct TlsProfileSample sealed
	{
		void InitializeSample(const WCHAR* tag);
	public:
		TlsProfileSample(const WCHAR* tag);
		~TlsProfileSample();
		inline void Begin();
		void End(LARGE_INTEGER endTime);

		// 
		void GetMaxLen(ItemType type, OUT std::wstring& prevMaxWstr);
		bool WriteContent(size_t* len_arr);
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
	class TlsProfileManager sealed
	{
	public:
		using MappingTable = std::unordered_map<const WCHAR*, TlsProfileSample*>;
		void Begin(const WCHAR* tag);
		void End(const WCHAR* tag);
		friend class Wrapper;

		int GetMaxLen(ItemType type);
		bool WriteContent(size_t* len_arr);
		bool ReleaseAll();

		~TlsProfileManager();
	private:
		TlsProfileManager();
		TlsProfileSample* GetSample(const WCHAR* tag);
		MappingTable _sampleMappingTable;
	};


	/*-----------------------------
	*		Manager Wrapper
	-----------------------------*/
	// 스레드 하나에 있 TLS_PROFILE_SAMPLE[N]에 대한 정보를 가지고 있는 클래스.
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
	class ProfileBoss sealed
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
		
		static bool RegistManager(TlsProfileManager* manager);
		static bool SaveFile(const WCHAR* fileName);
	private:
		static bool AllResourcesRelease();
		ProfileBoss();
		static LARGE_INTEGER s_frequency;
		static volatile USHORT s_count;
		static TlsProfileManager* s_managers[TLS_PROFILE_ARR_MAX];
		static ERROR_CODE s_errCode;
	};

	/*----------------------------------------
					파일 저장
	----------------------------------------*/
	class ProfileWriter sealed
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

}

	//extern thread_local Wrapper wrapper;

#endif


/*--------------------------------------------------------------------------------------
									  사용 규칙

- 저장은 무조건 모든 스레드들이 Begin() / End() 호출을 하지 않는 상황에서 일어나야 한다.
- SaveFile은 무조건 메인 스레드가 실행시킨다.
- 또한 모든 스레드들이 종료된 상황에서 SaveFile()이 실행되어야한다.
- 그렇지 않으면 실행되고 있는 스레드가 저장하는 정보는 제대로 저장이 되지 않을 수 있다.

- 리소스의 해제는 PROFILE_BOSS::AllResourcesRelease()를 통해 모든 리소스를 해제하도록 한다.
- 현재는 파일 저장을 하면 모든 리소스를 해제하도록 했다.
--------------------------------------------------------------------------------------*/






/*--------------------------------------------------
		   나중에 상황 봐서 추가해야 할 것
			
		1. TLS_PROFILE_MANAGER의 RESET 기능
		2. PROFILE_BOSS의 에러 코드 업데이트
--------------------------------------------------*/