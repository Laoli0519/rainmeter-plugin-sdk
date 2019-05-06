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
#pragma comment(lib,"Iphlpapi.lib") //��Ҫ���Iphlpapi.lib��

 // Overview: This example demonstrates using both the data argument to keep data across
 // functions calls and also storing data in the |Rainmeter.data| file for presist access across
 // skin reloads and even measures. In this example we will make a counter that counts the number
 // of updates that happen and saves it when the skin unloads to the |Rainmeter.data| file.
 // Note: Remember that the Update function will not be called when the measure is paused or disabled.

 //Sample skin:
 /*
�����ɵ�dll�ļ�����Rainmeter/plugins�ļ����ڣ�Ȼ���д.ini���������£�
LoadingLocalIP����Ҫ����dll�ļ�������
mLAN��һ������NetAdaName��IPType�Ƕ����еı���������ÿ����һ��LoadingLocalIP.dll�еĺ�����
�ͻ��ȡһ�������еı�����Ȼ��ִ��dll�еĺ��������������������ظ�mLAN��
��ʱMeterCount������MeasureName�Ϳ��Զ�ȡ����Ӧ��ֵ�ˣ���Text�е�%1���ǵ�һ��MeasureName��ֵ��
��Ӧ��%2���ǵڶ���MeasureName��ֵ������
ͬ���ģ�mWLANҲ��һ������Ҳ�����dll������
��֮��[]�ڵı�ʶ���ᴴ��һ�����󣬶�����Դ��ݲ���

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
//// �ϲ���
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

	//PIP_ADAPTER_INFO�ṹ��ָ��洢����������Ϣ
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	//�õ��ṹ���С,����GetAdaptersInfo����
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	//����GetAdaptersInfo����,���pIpAdapterInfoָ�����;����stSize��������һ��������Ҳ��һ�������
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	//��¼��������
	//int netCardNum = 0;
	//��¼ÿ�������ϵ�IP��ַ����
	//int IPnumPerNetCard = 0;

	//RmLogF(measure->prm, LOG_DEBUG, L"In Update after new IP_ADAPTER_INFO, nRel");
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		//����������ص���ERROR_BUFFER_OVERFLOW
		//��˵��GetAdaptersInfo�������ݵ��ڴ�ռ䲻��,ͬʱ�䴫��stSize,��ʾ��Ҫ�Ŀռ��С
		//��Ҳ��˵��ΪʲôstSize����һ��������Ҳ��һ�������
		//�ͷ�ԭ�����ڴ�ռ�
		delete pIpAdapterInfo;
		//���������ڴ�ռ������洢����������Ϣ
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		//�ٴε���GetAdaptersInfo����,���pIpAdapterInfoָ�����
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
	//				//cout << "�������ϵ�IP������" << ++IPnumPerNetCard << endl;
	//				//cout << "IP ��ַ��" << pIpAddrString->IpAddress.String << endl;

	//				//cout << "������ַ��" << pIpAddrString->IpMask.String << endl;
	//				//cout << "���ص�ַ��" << pIpAdapterInfo->GatewayList.IpAddress.String << endl;
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