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

#define TLS_SAVE(x) (ProfileBoss::GetInstance().SaveFile(L##x))
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
		ITEM_TYPE_MAX,
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
		void GetMaxStr(ItemType type, OUT std::wstring& prevMaxWstr);
		void ContainAttributes(std::vector<std::wstring>& attributesArray);
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
					���� ����
	----------------------------------------*/
		class ProfileWriter
		{
		public:
			// ����, Profiler ����
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

- ���ҽ��� ������ ProfileBoss �Ҹ��ڿ��� �˾Ƽ� ��� ���ҽ��� �����Ѵ�.

- �����͸� �̾Ƴ��� �ش� �������� ��� sample�� �����Ѵ�. (PROFILE_BOSS:AllDataReset) 
( TLS_PROFILE_MANAGER�� �����ϴ� ���� )


--------------------------------------------------------------------------------------*/






/*--------------------------------------------------
		   ���߿� ��Ȳ ���� �߰��ؾ� �� ��

		1. PROFILE_BOSS�� ���� �ڵ�� ���� ó�� �����.
--------------------------------------------------*/

