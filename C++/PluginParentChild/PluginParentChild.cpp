/* Copyright (C) 2017 Rainmeter Project Developers
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>. */
//#define _AFXDLL
#include "GetWeather.hpp"
#include <time.h>
#include "../../API/RainmeterAPI.h"
#include <vector>
// Overview: This example demonstrates a basic implementation of a parent/child
// measure structure. In this particular example, we have a "parent" measure
// which contains the values for the options "ValueA", "ValueB", and "ValueC".
// The child measures are used to return a specific value from the parent.

// Use case: You could, for example, have a "main" parent measure that queries
// information from some data set. The child measures can then be used to return
// specific information from the data queried by the parent measure.

// Sample skin:
/*
	[Rainmeter]
	Update=1000
	DynamicWindowSize=1
	BackgroundMode=2
	SolidColor=255,255,255

	[mParent]
	Measure=Plugin
	Plugin=ParentChild
	ValueA=111
	ValueB=222
	ValueC=333
	Type=A

	[mChild1]
	Measure=Plugin
	Plugin=ParentChild
	ParentName=mParent
	Type=B

	[mChild2]
	Measure=Plugin
	Plugin=ParentChild
	ParentName=mParent
	Type=C

	[Text]
	Meter=String
	MeasureName=mParent
	MeasureName2=mChild1
	MeasureName3=mChild2
	Text="mParent: %1#CRLF#mChild1: %2#CRLF#mChild2: %3"
*/

//#ifndef _DEBUG
//
//// default lib setting.
//#pragma comment(linker, "/defaultlib:kernel32.lib") 
//#pragma comment(linker, "/defaultlib:LIBCTINY.LIB")
//#pragma comment(linker, "/nodefaultlib:libc.lib")
//#pragma comment(linker, "/nodefaultlib:libcmt.lib")
//
//// section size
//#pragma comment(linker, "/FILEALIGN:16")
//#pragma comment(linker, "/ALIGN:16") 
//#pragma comment(linker, "/OPT:NOWIN98")
//
//// 合并段
//#pragma comment(linker, "/MERGE:.rdata=.data")
//#pragma comment(linker, "/MERGE:.text=.data")
//#pragma comment(linker, "/MERGE:.reloc=.data")
//
//#endif

enum MeasureType
{
	UNKNOWN,
	REFRESH,
	MIN_TEMPERATURE,
	MAX_TEMPERATURE,
	CUR_TEMPERATURE
};

struct ChildMeasure;

struct ParentMeasure
{
	void* skin;
	LPCWSTR name;
	ChildMeasure* ownerChild;

	Weather *weather;

	void *rm;

	ParentMeasure() :
		skin(nullptr),
		name(nullptr),
		ownerChild(nullptr),
		weather(nullptr) {}
};

struct ChildMeasure
{
	MeasureType type;
	ParentMeasure* parent;

	ChildMeasure() : 
		type(UNKNOWN) {}
};

std::vector<ParentMeasure*> g_ParentMeasures;

//std::string wstring2string(std::wstring wstr)
//{
//	std::string result;
//	//获取缓冲区大小，并申请空间，缓冲区大小事按字节计算的  
//	int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
//	char* buffer = new char[len + 1];
//	//宽字节编码转换成多字节编码  
//	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
//	buffer[len] = '\0';
//	//删除缓冲区并返回值  
//	result.append(buffer);
//	delete[] buffer;
//	return result;
//}

//BOOL WCharToMByte(LPCWSTR lpcwszStr, std::string &str)
//{
//	DWORD dwMinSize = 0;
//	LPSTR lpszStr = NULL;
//	dwMinSize = WideCharToMultiByte(CP_UTF8, NULL, lpcwszStr, -1, NULL, 0, NULL, FALSE);
//	if (0 == dwMinSize)
//	{
//		return FALSE;
//	}
//	lpszStr = new char[dwMinSize];
//	WideCharToMultiByte(CP_UTF8, NULL, lpcwszStr, -1, lpszStr, dwMinSize, NULL, FALSE);
//	str = lpszStr;
//	delete[] lpszStr;
//	lpszStr = NULL;
//	return TRUE;
//	
//	//wctomb(lpszStr, (LPWSTR)lpcwszStr);
//}

//std::string WChar2Ansi(LPCWSTR pwszSrc)
//{
//	int nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);
//
//	if (nLen <= 0) return std::string("");
//
//	char* pszDst = new char[nLen];
//	if (NULL == pszDst) return std::string("");
//
//	WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
//	pszDst[nLen - 1] = 0;
//
//	std::string strTemp(pszDst);
//	delete[] pszDst;
//
//	return strTemp;
//}

char* ConvertLPWSTRToLPSTR(LPCWSTR lpwszStrIn)
{
	LPSTR pszOut = NULL;

	int nInputStrLen = wcslen(lpwszStrIn);

	// Double NULL Termination  
	int nOutputStrLen = WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, NULL, 0, 0, 0) + 2;
	pszOut = new char[nOutputStrLen];

	if (pszOut)
	{
		memset(pszOut, 0x00, nOutputStrLen);
		WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, pszOut, nOutputStrLen, 0, 0);
	}

	return pszOut;
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	ChildMeasure* child = new ChildMeasure;
	*data = child;

	void* skin = RmGetSkin(rm);

	LPCWSTR parentName = RmReadString(rm, L"ParentName", L"");
	if (!*parentName)
	{
		child->parent = new ParentMeasure;
		child->parent->name = RmGetMeasureName(rm);
		child->parent->skin = skin;
		child->parent->ownerChild = child;
		g_ParentMeasures.push_back(child->parent);
		child->parent->weather = new Weather();
		child->parent->rm = rm;
	}
	else
	{
		// Find parent using name AND the skin handle to be sure that it's the right one
		std::vector<ParentMeasure*>::const_iterator iter = g_ParentMeasures.begin();
		for ( ; iter != g_ParentMeasures.end(); ++iter)
		{
			if (_wcsicmp((*iter)->name, parentName) == 0 &&
				(*iter)->skin == skin)
			{
				child->parent = (*iter);
				return;
			}
		}

		RmLog(rm, LOG_ERROR, L"Invalid \"ParentName\"");
	}

	RmLogF(rm, LOG_NOTICE, L"Weather : Initialize");
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	ChildMeasure* child = (ChildMeasure*)data;
	ParentMeasure* parent = child->parent;

	if (!parent)
	{
		return;
	}

	// Read common options
	LPCWSTR type = RmReadString(rm, L"Type", L"");
	if (_wcsicmp(type, L"refresh") == 0) {
		child->type = REFRESH;
	}
	else if (_wcsicmp(type, L"min_temperature") == 0) {
		child->type = MIN_TEMPERATURE;
	}
	else if (_wcsicmp(type, L"max_temperature") == 0) {
		child->type = MAX_TEMPERATURE;
	}
	else if (_wcsicmp(type, L"cur_temperature") == 0) {
		child->type = CUR_TEMPERATURE;
	}
	else
	{
		RmLog(rm, LOG_ERROR, L"Invalid \"Type\"");
	}

	// Read parent specific options
	if (parent->ownerChild == child)
	{
		time_t timep;
		time(&timep);
		struct tm *pTime = NULL;
		pTime = localtime(&timep);
		parent->weather->setCurrentDay(pTime->tm_mday);

		//std::wstring code(RmReadString(rm, L"CityCode", L""));
		//CStringA code();
		//RmLogF(rm, LOG_NOTICE, L"Weather : Reload : CityCode(wstring) = %s", code.c_str());

		char *ret = ConvertLPWSTRToLPSTR(RmReadString(rm, L"CityCode", L""));
		//WCharToMByte(RmReadString(rm, L"CityCode", L""), cityCode);
		RmLogF(rm, LOG_NOTICE, L"Weather : Reload : CityCode(string) = %s", ret);

		std::string cityCode("101010100");
		parent->weather->setCityCode(cityCode);
	}

	RmLogF(rm, LOG_NOTICE, L"Weather : Reload");
}

PLUGIN_EXPORT double Update(void* data)
{
	ChildMeasure* child = (ChildMeasure*)data;
	ParentMeasure* parent = child->parent;

	if (!parent)
	{
		return 0.0;
	}

	RmLogF(parent->rm, LOG_NOTICE, L"Weather : Update");

	switch (child->type)
	{
	case REFRESH:
		{
			int ret = parent->weather->getCurDayWeather();
			RmLogF(parent->rm, LOG_NOTICE, L"Weather : refresh = %d", ret);
		}
		break;
	case MIN_TEMPERATURE:
		return (double)parent->weather->getMinTemperature();
		break;
	case MAX_TEMPERATURE:
		return (double)parent->weather->getMaxTemperature();
		break;
	case CUR_TEMPERATURE:
		//RmLogF(parent->rm, LOG_NOTICE, L"Weather : Current = %d", parent->weather->getCurrentTemperature());
		return (double)parent->weather->getCurrentTemperature();
		break;
	}

	return 0.0;
}

PLUGIN_EXPORT void Finalize(void* data)
{
	ChildMeasure* child = (ChildMeasure*)data;
	ParentMeasure* parent = child->parent;

	RmLogF(parent->rm, LOG_NOTICE, L"Weather : Finalize");

	if (parent->weather) {
		delete (parent->weather);
		parent->weather = nullptr;
	}

	if (parent && parent->ownerChild == child)
	{
		g_ParentMeasures.erase(
			std::remove(g_ParentMeasures.begin(),g_ParentMeasures.end(),parent),
			g_ParentMeasures.end());
		delete parent;
	}


	delete child;
}
