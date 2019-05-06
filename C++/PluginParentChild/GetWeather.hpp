#pragma once

//#include <afx.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <winsock2.h>
#include <sys/stat.h>
#include <algorithm>
//#include <zlib.h>

#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "zlibstaticd.lib")


//#define RECVDATALENGTH 1024*10

class Weather {
public:
	Weather() :
		weatherSock_(),
		cityCode_(),
		minTemperature_(0),
		maxTemperature_(0),
		currentTemperature_(0),
		json_(nullptr) {
		json_ = new std::string();
		json_->clear();
	}

	~Weather() {
		if (json_) {
			delete json_;
			json_ = nullptr;
		}


		closesocket(weatherSock_);
	}

public:


	//std::string cityName2Code(std::wstring& cityName) {

	//	const char *filePath = "../_city.json";

	//	std::string cityName_ansi = WChar2Ansi(cityName.c_str());

	//	FILE *fp = fopen(filePath, "r");
	//	if (!fp) {
	//		fprintf(stderr, "fopen false.\n");
	//		return std::string();
	//	}
	//	int fileSize = file_size2(filePath);
	//	std::vector<char> *_cityCodeFile = new std::vector<char>(fileSize);
	//	do {

	//		//char *_cityCodeFile = new char[300 * 1024];
	//		if (!_cityCodeFile) {
	//			fprintf(stderr, "new _cityCodeFile false.");

	//			break;
	//		}

	//		int ret = fread(_cityCodeFile->data(), sizeof(char), fileSize, fp);
	//		if (ret != fileSize) {
	//			fprintf(stderr, "fread false.");

	//			break;
	//		}

	//		rapidjson::Document doc;
	//		doc.Parse<0>(_cityCodeFile->data());
	//		if (doc.HasParseError()) {
	//			rapidjson::ParseErrorCode code = doc.GetParseError();
	//			fprintf(stderr, "code = %d", code);

	//			break;
	//		}

	//		rapidjson::Value &value = doc["city2code"];
	//		if (!value.IsArray()) {
	//			break;
	//		}
	//		for (size_t i = 0; i < value.Size(); ++i) {
	//			rapidjson::Value & v = value[i];
	//			//assert(v.IsObject());
	//			if (!v.IsObject()) {
	//				break;
	//			}

	//			if (v.HasMember("city_name") && v["city_name"].IsString() && \
	//				strcmp(v["city_name"].GetString(), cityName_ansi.c_str()) == 0) {

	//				if (v.HasMember("city_code") && v["city_code"].IsString()) {
	//					cityCode_.append(v["city_code"].GetString());
	//				}

	//			}
	//		}
	//	} while (0);

	//	if (_cityCodeFile) {
	//		delete _cityCodeFile;
	//		_cityCodeFile = NULL;
	//	}
	//	if (fp) {
	//		fclose(fp);
	//		fp = NULL;
	//	}

	//	return std::string();
	//}

	void setCityCode(std::string& code) {
		this->cityCode_ = code;
	}

	void setCurrentDay(int currentDay) {
		this->currentDay_ = currentDay;
	}

	int getCurDayWeather() {
		int ret = 0;
		WSADATA data;
		WORD sockVersion = MAKEWORD(2, 2);
		if (WSAStartup(sockVersion, &data) != 0) {
			return -1;
		}
		//������Ҫ������Ӧ��ͷ���ַ���
		char *recvResponse = NULL;
		recvResponse = (char*)calloc(1024, sizeof(char) );

		do{
			weatherSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (weatherSock_ == INVALID_SOCKET) {
				//printf("invalid socket!\n");
				ret = weatherSock_;
				break;
			}

			hostent *phst = gethostbyname("t.weather.sojson.com");
			in_addr * iddr = (in_addr*)phst->h_addr;
			//unsigned long IPUL = iddr->s_addr;
			char *IP = inet_ntoa(*iddr);

			sockaddr_in serAddr;
			serAddr.sin_family = AF_INET;
			serAddr.sin_port = htons(80);
			serAddr.sin_addr.S_un.S_addr = inet_addr(IP);
			if (connect(weatherSock_, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) {
				//����ʧ�� 
				//printf("connect error !\n");
				ret = SOCKET_ERROR;
				break;
			}

			std::string data;
			buildRequestHeader(data);

			ret = send(weatherSock_, data.c_str(), data.length(), 0);
			if (ret <= 0) {
				//С�ڵ���0���������
				break;
			}

			//������Ӧ��ͷ
			ret = recv(weatherSock_, recvResponse, 1024, 0);
			if (ret <= 0) {
				if (ret == EAGAIN) {
					break;
				}
				//����0�������ж�

				//С��0���������
				ret = WSAGetLastError();

				break;
			}

			//printf(json_->data());
			//printf(recvResponse);
			//�ɹ����� ״̬�룬ʧ�ܷ��� ����
			ret = parseResponseHeader(recvResponse, ret);
			if (ret < 0) {
				break;
			}
			
			//�õ�Json�ַ���
			json_->clear();
			//ʧ��С�ڵ���0���ɹ����ؽ����ַ���С
			ret = gainJsonData();
			if (ret <= 0) {
				break;
			}
			
			//���ﲻ֪��Ϊʲôjson_���е�size���ret��
			json_->resize(ret);

			//printf(json_->c_str());
			//�ɹ�����1�� ʧ�ܷ��ظ���
			ret = parseJsonData();
			if (ret < 0) {
				break;
			}

			ret = 0;
		}while (0);

		if (recvResponse) {
			free(recvResponse);
			recvResponse = NULL;
		}

		WSACleanup();

		return ret;
	}

	std::string& getCityCode() {
		return cityCode_;
	}

	int getMinTemperature() {
		return minTemperature_;
	}

	int getMaxTemperature() {
		return maxTemperature_;
	}

	int getCurrentTemperature() {
		return currentTemperature_;
	}
	
private:

	int parseJsonData() {
		int ret = 0;
		if (json_->empty()) {
			//printf("gain html false.\n");
			return -1;
		}

		do {
			rapidjson::Document doc;
			doc.Parse<0>(json_->data());
			if (doc.HasParseError()) {
				rapidjson::ParseErrorCode code = doc.GetParseError();
				//fprintf(stderr, "code = %d", code);
				ret = -2;
				break;
			}

			//ֱ�ӿ�ʼ����data�ֶ�
			rapidjson::Value &value = doc["data"];
			if (!value.IsObject()) {
				ret = -30;
				break;
			}
			if (!value.HasMember("wendu") || !value["wendu"].IsString()) {
				ret = -4;
				break;
			}

			//��ȡ��ǰ�¶�
			currentTemperature_ = atoi(value["wendu"].GetString());

			//��ȡ����¶�����¶�
			if (!value.HasMember("forecast") || !value["forecast"].IsArray()) {
				ret = -5;
				break;
			}

			rapidjson::Value &forecast = value["forecast"];
			for (size_t i = 0; i < forecast.Size(); ++i) {
				rapidjson::Value & v = forecast[i];

				if (!v.IsObject()) {
					continue;
				}

				if (!v.HasMember("date")) {
					continue;
				}

				if (!v["date"].IsString()) {
					continue;
				}

				char s[4];
				std::string str(itoa(currentDay_, s, 10));
				std::string str1(v["date"].GetString());
				//�ҵ�����������Ϣ����ʼ����
				if (strcmp(str.c_str(), str1.c_str()) == 0) {
					//����¶�
					if (!v.HasMember("high") || !v["high"].IsString()) {
						ret = -6;
						break;
					}
					std::string high(v["high"].GetString());
					auto highit = std::find(high.begin(), high.end(), -30);
					if (highit == high.end()) {
						ret = -7;
						break;
					}

					high.insert(highit, '\0');
					maxTemperature_ = atoi(&high[7]);

					//����¶�
					if (!v.HasMember("low") || !v["low"].IsString()) {
						ret = -8;
						break;
					}
					std::string low(v["low"].GetString());
					auto lowit = std::find(low.begin(), low.end(), -30);
					if (lowit == low.end()) {
						ret = -9;
						break;
					}

					low.insert(lowit, '\0');
					minTemperature_ = atoi(&low[7]);

					if (!v.HasMember("type") || !v["type"].IsString()) {
						ret = -10;
						break;
					}
					weatherType_ = v["type"].GetString();
					//
					ret = 1;
					break;
				}
			}

			ret = (ret == 0 ? -12 : ret);
		} while (0);

		return ret;
	}


	int file_size2(const char* filename)
	{
		struct stat statbuf;
		stat(filename, &statbuf);
		int size = statbuf.st_size;

		return size;
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

	std::string& buildRequestHeader(std::string& header) {

		header = "GET /api/weather/city/" + cityCode_ + " HTTP/1.1\r\n";
		header += "Host: t.weather.sojson.com\r\n";
		header += "Connection: keep-alive\r\n";
		header += "Cache-Control: no-cache\r\n";
		header += "Upgrade-Insecure-Requests: 1\r\n";
		header += "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3724.8 Safari/537.36\r\n";
		header += "User-Agent: text/html,application/json,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3\r\n";
		header += "Accept-Encoding: deflate\r\n";
		header += "Accept-Language: zh-CN,zh;q=0.9\r\n";
		header += "\r\n";
		return header;
	}

	int parseResponseHeader(char* recvResponse, int ret) {
		int i = 0, j = 0;
		//int size = json_->size();
		for (i = 0; i < ret && recvResponse[i] != ' '; ++i);

		for (j = i + 1; j < ret && recvResponse[j] != ' '; ++j);

		recvResponse[j] = '\0';

		int status = atoi(&(recvResponse[i + 1]));

		return status == 200 ? status : -1;
	}

	int gainJsonData() {
		int count = 0;
		//FILE *fp = fopen("C:/Users/Li/Desktop/test1.txt", "w");
		//if (fp) {
		//	return -5;
		//}
		while(1){
			char ret[64] = { 0 };
			int i = 0, size = 0;

			//��ȡ����Json����С��ʮ�������ֽ�
			for (char c = 0; i < 64 && c != '\n'; ++i) {
				if (recv(weatherSock_, &c, sizeof(char), 0) != sizeof(char)) {
					//recv����
					return -1;
				}
				ret[i] = c;
			}

			//ret�ռ䲻��
			if (i >= 64) {
				return -2;
			}
			//������������Json�����ֽ���
			ret[i - 2] = '\0';
			sscanf(ret, "%x", &size);
			if (size == 0) {
				//��ȷ��������Json��
				//������\r\n���������������ϰ��
				recv(weatherSock_, ret, sizeof(char) * 2, 0);
				break;
			}

			count += size;

			{
				char *revData = new char[size];
				if (recv(weatherSock_, revData, size * sizeof(char), 0) != size * sizeof(char)) {
					delete revData;
					revData = NULL;
					//��ȡrecvʧ��
					return -3;
				}

				//if(fwrite(revData, sizeof(char), size, fp) != sizeof(char)*size){
				//	delete revData;
				//	revData = NULL;
				//	return -6;
				//}

				json_->append(revData);
				delete revData;
				revData = NULL;
			}

			//��ȡ"\r\n"������
			if (recv(weatherSock_, ret, sizeof(char) * 2, 0) != sizeof(char) * 2) {
				//��ȡrecvʧ��
				return -4;
			}
		}

		//��ȡ��ȷ���ض�ȡ�������ֽ���
		return count;
	}
private:
	SOCKET weatherSock_;
	std::string cityCode_;
	int minTemperature_;
	int maxTemperature_;
	int currentTemperature_;
	int currentDay_;
	std::string weatherType_;
	std::string *json_;

	//int gzDecompress(Byte *zdata, uLong nzdata,
	//	Byte *data, uLong *ndata)
	//{
	//	int err = 0;
	//	z_stream d_stream = { 0 }; /* decompression stream */
	//	static char dummy_head[2] = {
	//		0x8 + 0x7 * 0x10,
	//		(((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
	//	};
	//	d_stream.zalloc = NULL;
	//	d_stream.zfree = NULL;
	//	d_stream.opaque = NULL;
	//	d_stream.next_in = zdata;
	//	d_stream.avail_in = 0;
	//	d_stream.next_out = data;
	//	//ֻ������ΪMAX_WBITS + 16�����ڽ�ѹ��header��trailer���ı�
	//	if (inflateInit2(&d_stream, MAX_WBITS + 16) != Z_OK) return -1;
	//	//if(inflateInit2(&d_stream, 47) != Z_OK) return -1;
	//	while (d_stream.total_out < *ndata && d_stream.total_in < nzdata) {
	//		d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
	//		if ((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END) break;
	//		if (err != Z_OK) {
	//			if (err == Z_DATA_ERROR) {
	//				d_stream.next_in = (Bytef*)dummy_head;
	//				d_stream.avail_in = sizeof(dummy_head);
	//				if ((err = inflate(&d_stream, Z_NO_FLUSH)) != Z_OK) {
	//					return -1;
	//				}
	//			}
	//			else return -1;
	//		}
	//	}
	//	if (inflateEnd(&d_stream) != Z_OK) return -1;
	//	*ndata = d_stream.total_out;
	//	return 0;
	//}
};