#include "Functor.h"
#include <string>

using namespace C_Utility;

bool Functor::operator()(const WCHAR* first, const WCHAR* second) const
{
	std::wstring wstr(first);
	std::wstring wstr2(second);

	return wstr < wstr2;
}