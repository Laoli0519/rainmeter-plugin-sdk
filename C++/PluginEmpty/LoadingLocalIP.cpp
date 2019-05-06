/* Copyright (C) 2017 Rainmeter Project Developers
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>. */

//#include <Windows.h>
#include <string>
//#include <sstream>
//#define _AFXDLL
#include <afx.h>
#include "../../API/RainmeterAPI.h"
//#include <WinSock2.h>
#include <Iphlpapi.h>
#pragma comment(lib,"Iphlpapi.lib") //需要添加Iphlpapi.lib库

 // Overview: This example demonstrates using both the data argument to keep data across
 // functions calls and also storing data in the |Rainmeter.data| file for presist access across
 // skin reloads and even measures. In this example we will make a counter that counts the number
 // of updates that happen and saves it when the skin unloads to the |Rainmeter.data| file.
 // Note: Remember that the Update function will not be called when the measure is paused or disabled.

 //Sample skin:
 /*
将生成的dll文件放入Rainmeter/plugins文件夹内，然后编写.ini，内容如下：
LoadingLocalIP是需要加载dll文件的名字
mLAN是一个对象，NetAdaName和IPType是对象中的变量，程序每调用一次LoadingLocalIP.dll中的函数，
就会读取一个对象中的变量，然后执行dll中的函数，将函数处理结果返回给mLAN，
这时MeterCount对象中MeasureName就可以读取到相应的值了，而Text中的%1则是第一个MeasureName的值，
对应的%2就是第二个MeasureName的值。。。
同样的，mWLAN也是一个对象，也会调用dll。。。
总之，[]内的标识符会创建一个对象，对象可以传递参数

	 [Rainmeter]
	 Update=1000
	 DynamicWindowSize=1
	 BackgroundMode=2
	 SolidColor=255,255,255

	 [mLAN]
	 Measure=Plugin
	 Plugin=LoadingLocalIP
	 NetAdaName=Realtek USB GbE Family Controller
	 IPType=LAN

	 [mWLAN]
	 Measure=Plugin
	 Plugin=LoadingLocalIP
     NetAdaName="Intel(R) Dual Band Wireless-AC 8265 #2"
	 IPType=WLAN

	 [MeterCount]
	 Meter=String
	 MeasureName=mLAN
	 Text="LAN = %1"

	 [MeterCountAt0]
	 Meter=String
	 MeasureName=mWLAN
	 Y=5R
	 Text="WLAN = %1"
 */


//#ifndef _DEBUG

 // default lib setting.
//#pragma comment(linker, "/defaultlib:kernel32.lib") 
//#pragma comment(linker, "/defaultlib:LIBCTINY.LIB")
//#pragma comment(linker, "/nodefaultlib:libc.lib")
//#pragma comment(linker, "/nodefaultlib:libcmt.lib")

//// section size
//#pragma comment(linker, "/FILEALIGN:16")
//#pragma comment(linker, "/ALIGN:16") 
//#pragma comment(linker, "/OPT:NOWIN98")
//
//// 合并段
//#pragma comment(linker, "/MERGE:.rdata=.data")
//#pragma comment(linker, "/MERGE:.text=.data")
//#pragma comment(linker, "/MERGE:.reloc=.data")

//#endif

enum IPType
{
	IP_UNKNOWN,
	IP_ETHERNET,
	IP_IEEE80211
};


struct Measure
{
	//int counter;
	//bool saveData;
	LPCWSTR dataFile;

	std::wstring *IPFile;
	//std::string *IPAddress;
	CString IPAddress;
	//IPType ipType;

	void *prm;
	Measure() :
		IPFile(nullptr),
		dataFile(nullptr),
		IPAddress() {}
};


std::wstring StringToWString(const std::string& str)
{
	setlocale(LC_ALL, "chs");
	const char* point_to_source = str.c_str();
	size_t new_size = str.size() + 1;
	wchar_t *point_to_destination = new wchar_t[new_size];
	wmemset(point_to_destination, 0, new_size);
	mbstowcs(point_to_destination, point_to_source, new_size);
	std::wstring result = point_to_destination;
	delete[]point_to_destination;
	setlocale(LC_ALL, "C");
	return result;
}

std::string WStringToString(const std::wstring &wstr)
{
	std::string str;
	int nLen = (int)wstr.length();
	str.resize(nLen, ' ');
	int nResult = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wstr.c_str(), nLen, (LPSTR)str.c_str(), nLen, NULL, NULL);
	if (nResult == 0)
	{
		return "";
	}
	return str;
}

std::string ws2s(const std::wstring &ws)
{
	size_t i;
	std::string curLocale = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "chs");
	const wchar_t* _source = ws.c_str();
	size_t _dsize = 2 * ws.size() + 1;
	char* _dest = new char[_dsize];
	memset(_dest, 0x0, _dsize);
	wcstombs_s(&i, _dest, _dsize, _source, _dsize);
	std::string result = _dest;
	delete[] _dest;
	setlocale(LC_ALL, curLocale.c_str());
	return result;
}

std::string WChar2Ansi(LPCWSTR pwszSrc)
{
	int nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);

	if (nLen <= 0) return std::string("");

	char* pszDst = new char[nLen];
	if (NULL == pszDst) return std::string("");

	WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
	pszDst[nLen - 1] = 0;

	std::string strTemp(pszDst);
	delete[] pszDst;

	return strTemp;
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	Measure* measure = new Measure;
	*data = measure;

	// Get the path to the settings data file
	measure->dataFile = RmGetSettingsFile();
	measure->IPFile = new std::wstring();
	measure->IPFile->clear();
	//measure->IPAddress = new std::string();
	//measure->IPAddress->clear();
	RmLogF(rm, LOG_DEBUG, L"In Initialize.");
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	Measure* measure = (Measure*)data;

	// Note: If |DynamicVariables=1| is set on the measure, this function will get called
	//  on every update cycle - meaning the counter values will use the starting value and
	//  appear to *not* update at all.

	// Get the starting value if one is defined
	//measure->counter = RmReadInt(rm, L"StartingValue", -1);
	
	LPCWSTR NetworkAda = RmReadString(rm, L"NetAdaName", L"");
	//std::wstring NetworkAdapter = NetworkAda;
	//RmLogF(rm, LOG_DEBUG, L"In Reload NetworkAdapter = %s.", NetworkAdapter.c_str());

	measure->IPFile->append(NetworkAda);
	RmLogF(rm, LOG_DEBUG, L"In Reload IPFILE = %s.", measure->IPFile->c_str());
	//LPCWSTR value = RmReadString(rm, L"IPType", L"");

	measure->prm = rm;


	//RmLogF(measure->prm, LOG_DEBUG, L"In Updateeeeeeeeee");

	//PIP_ADAPTER_INFO结构体指针存储本机网卡信息
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	//得到结构体大小,用于GetAdaptersInfo参数
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	//调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量;其中stSize参数既是一个输入量也是一个输出量
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	//记录网卡数量
	//int netCardNum = 0;
	//记录每张网卡上的IP地址数量
	//int IPnumPerNetCard = 0;

	//RmLogF(measure->prm, LOG_DEBUG, L"In Update after new IP_ADAPTER_INFO, nRel");
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		//如果函数返回的是ERROR_BUFFER_OVERFLOW
		//则说明GetAdaptersInfo参数传递的内存空间不够,同时其传出stSize,表示需要的空间大小
		//这也是说明为什么stSize既是一个输入量也是一个输出量
		//释放原来的内存空间
		delete pIpAdapterInfo;
		//重新申请内存空间用来存储所有网卡信息
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		//再次调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	}
	if (ERROR_SUCCESS == nRel)
	{
		RmLogF(measure->prm, LOG_DEBUG, L"In Update GetAdaptersInfo success.");
		while (pIpAdapterInfo) {

			CString str = CString(pIpAdapterInfo->Description);
			USES_CONVERSION;
			LPCWSTR wszClassName = A2CW(W2A(str));

			RmLogF(rm, LOG_DEBUG, L"In while (pIpAdapterInfo) { IPFILE = %s.", measure->IPFile->c_str());
			RmLogF(measure->prm, LOG_DEBUG, L"In while (pIpAdapterInfo) { wszClassName = %s", wszClassName);
			if (strcmp((LPCSTR)measure->IPFile->c_str(), (LPCSTR)wszClassName) == 0) {
				RmLogF(measure->prm, LOG_DEBUG, L"In if(){} %s == %s", wszClassName, measure->IPFile->c_str());
			//if (_wcsicmp(wszClassName, measure->IPFile) == 0) {
				IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
	//			//do
	//			//{
	//				//cout << "该网卡上的IP数量：" << ++IPnumPerNetCard << endl;
	//				//cout << "IP 地址：" << pIpAddrString->IpAddress.String << endl;

	//				//cout << "子网地址：" << pIpAddrString->IpMask.String << endl;
	//				//cout << "网关地址：" << pIpAdapterInfo->GatewayList.IpAddress.String << endl;
	//			//	pIpAddrString = pIpAddrString->Next;
	//			//} while (pIpAddrString);

				CString str(pIpAddrString->IpAddress.String);
				measure->IPAddress = str;
				//RmLogF(measure->prm, LOG_DEBUG, L"In update IPAddress = %s", measure->IPAddress->c_str());
				RmLogF(measure->prm, LOG_DEBUG, L"In update IPAddress = %s", measure->IPAddress);
				//str.ReleaseBuffer();
				//break;
			}
			str.ReleaseBuffer();
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	else {
		RmLogF(measure->prm, LOG_DEBUG, L"In update, nRel == NO_ERROR");
	}
	delete pIpAdapterInfo;

}

PLUGIN_EXPORT double Update(void* data)
{
	Measure* measure = (Measure*)data;

	// Every measure will have its own counter separate from other measures.
	// Increase the counter and reset if needed.
	//if (++measure->counter < 0)	measure->counter = 0;

	//return measure->counter;
	
	return 0.0;
}


PLUGIN_EXPORT LPCWSTR GetString(void* data)
{
	Measure* measure = (Measure*)data;

	// Although GetVersionEx is a rather inexpensive operation, it is recommended to
	// do any processing in the Update function. See the comments above.
	//if (!measure->IPAddress.empty())
	//{
	//RmLogF(measure->prm, LOG_DEBUG, L"In GetString IPAddress = %s", measure->IPAddress->c_str());
	//CString str(measure->IPAddress->c_str());
	//RmLogF(measure->prm, LOG_DEBUG, L"In GetString IPAddress = %s", measure->IPAddress);
	USES_CONVERSION;
	return A2CW(W2A(measure->IPAddress));
	//}

	// MEASURE_MAJOR, MEASURE_MINOR, and MEASURE_NUMBER are numbers. Therefore,
	// |nullptr| is returned here for them. This is to inform Rainmeter that it can
	// treat those types as numbers.

	//return nullptr;
}


PLUGIN_EXPORT void Finalize(void* data)
{
	Measure* measure = (Measure*)data;

	//if (measure->saveData)
	//{
	//	// Note: In this example multiple |DataHandling| plugin measures (from any skin) will
	//	//  overwrite this value. The counter from the last measure unloaded will persist.
	//	WCHAR buffer[SHRT_MAX];
	//	_itow_s(measure->counter, buffer, 10);
	//	WritePrivateProfileString(L"Plugin_DataHandling", L"Count", buffer, measure->dataFile);
	//}
	if (measure->IPFile) {
		delete measure->IPFile;
		measure->IPFile = nullptr;
	}

	//if (measure->IPAddress) {
	//	delete measure->IPAddress;
	//	measure->IPAddress = nullptr;
	//}
	measure->IPAddress.ReleaseBuffer();
	delete measure;
	measure = NULL;

}