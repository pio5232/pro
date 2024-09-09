#ifndef PRINT_LOG
#define PRINT_LOG

#include <Windows.h>
#include <iostream>


namespace C_Utility
{
	#define dfLOG_LEVEL_DEBUG 0
	#define dfLOG_LEVEL_ERROR 1
	#define dfLOG_LEVEL_SYSTEM 2

	#define _LOG(LogLevel, fmt, ...)					\
	do{													\
		if( g_iLogLevel <= LogLevel)					\
		{												\
			wsprintf(g_szLogBuff, fmt, ##__VA_ARGS__);	\
			C_Utility::Log(g_szLogBuff,LogLevel);					\
		}												\
	}while (0)		

	static int g_iLogLevel = dfLOG_LEVEL_ERROR; // ��� ���� ����� �α� ����
	static WCHAR g_szLogBuff[1024]; // �α� ����� �Ƿ��� �ӽ� ����


	void Log(WCHAR* szString, int iLogLevel);

	}
#endif
