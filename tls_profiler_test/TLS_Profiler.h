#ifndef C_TLS_PROFILER
#define C_TLS_PROFILER


#include <Windows.h>
//#include <unordered_map>
#include <map>
#include <iostream>
#include "Functor.h"

namespace C_Utility
{

	// ����. BOSS���� �����ϴ� MANAGER �ִ� ����.
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

	/* ���� ��� Ÿ�� */
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
			SAMPLE (MANAGER���� �����ϴ� ����ü)
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
		WCHAR szName[64]; // �±� �̸�
		//__int64 iFlag; // ���������� ��� ����

		LARGE_INTEGER IStartTime; // ���� �ð�

		unsigned long long iTotalTime; // ��ü ��� �ð�
		unsigned long long iMin[2]; // �ּ� ��� �ð�
		unsigned long long iMax[2]; // �ִ� ��� �ð�.
		unsigned long long iCall; // ȣ�� Ƚ�� 
		DWORD dwThreadId;
		BYTE ucIgnoreCnt;
	};

	/*--------------------------------------------------
				MANAGER (�����帶�� 1����)
	--------------------------------------------------*/
	// ������ �ϳ��� �ִ� TLS_PROFILE_SAMPLE[N]�� ���� ������ ������ �ִ� Ŭ����.
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
						BOSS (�Ѱ�)
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
					���� ����
	----------------------------------------*/
	class ProfileWriter
	{
	public:
		// ����, Profiler ����
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
	*     Profiler ����
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
									  ��� ��Ģ

- ������ �ϰԵǸ� ���� ����� ���� �����ʹ� �����ȴ�. 

- ���ҽ��� ������ PROFILE_BOSS::AllResourceRelease()�� ���� ��� ���ҽ��� �����ϵ��� �Ѵ�.

- �����͸� �̾Ƴ��� �ش� �������� ��� sample�� �����Ѵ�. (PROFILE_BOSS:AllDataReset) 
( TLS_PROFILE_MANAGER�� �����ϴ� ���� )


--------------------------------------------------------------------------------------*/






/*--------------------------------------------------
		   ���߿� ��Ȳ ���� �߰��ؾ� �� ��

		1. PROFILE_BOSS�� ���� �ڵ�� ���� ó�� �����.
		2. PROFILE_BOSS::AllResourceRelease() �α׸� ������ �� �� �ִ� Ÿ�̹� ���ϱ� (���� �Ҹ���)
		3. ��Ƽ������ ȯ�濡�� ���� ������ ���� lock �߰�
--------------------------------------------------*/

