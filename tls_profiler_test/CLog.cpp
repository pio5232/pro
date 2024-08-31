#include "CLog.h"

#define LOG_EXPORT
void C_Utility::Log(WCHAR* szString, int iLogLevel)
{
	wprintf(L"%s \n", szString);
}
