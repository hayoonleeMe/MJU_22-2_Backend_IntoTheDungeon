#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <functional>
#include <string>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace rapidjson;
using namespace std;

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

// 클라이언트 구동 관련 네임스페이스
namespace Client
{
	static const char* SERVER_ADDRESS = "127.0.0.1";
	static const unsigned short SERVER_PORT = 27015;
	SOCKET sock;	// TCP 소켓
	mutex sockMutex;
	char recvBuf[65536];
	string ID;
}

// 내부 로직 관련 네임스페이스
namespace Logic
{
	// 서버로 데이터를 보낸다.
	bool SendData(const string& text);

	// 서버로부터 데이터를 받는다.
	bool ReceiveData();

	// cin으로 입력받은 텍스트를 json 문자열로 변환해 반환하는 함수
	string GetInputTextJson();

	// 로그인
	void Login();

	// 프로그램 종료 준비
	void ExitProgram();
}

/// <summary> json 관련 네임스페이스
/// json 문자열의 첫번째 키는 TEXT, 이후 키들은 순서대로 PARAM1, PARAM2이다.
/// 메시지 타입이 있는 경우 두번째 키인 PARAM1의 value로 제공된다.
/// </summary>
namespace Json
{
	enum class M_Type
	{
		E_DIE,
		E_DUP_CONNECTION,
	};

	typedef function<string(string)> Handler;
	map<string, Handler> handlers;

	// json key
	static const char* TEXT = "text";
	static const char* PARAM1 = "param1";
	static const char* PARAM2 = "param2";

	static const char* LOGIN = "login";
	static const char* MOVE = "move";
	static const char* ATTACK = "attack";
	static const char* MONSTERS = "monsters";
	static const char* USERS = "users";
	static const char* CHAT = "chat";
	static const char* USE_POTION = "usepotion";
	static const char* HP_POTION = "hp";
	static const char* STR_POTION = "str";

	// handlers에 저장되는, key가 하나이고 value가 input인 json 문자열 반환하는 함수
	string GetJson(const string& input);

	// key가 두개고 value가 input, param1인 json 문자열 반환하는 함수
	string GetParamsJson(const string& input, const string& param1);

	// key가 세개고 value가 input, param1, param2인 json 문자열 반환하는 함수
	string GetParamsJson(const string& input, const string& param1, const string& param2);

	// handlers에 저장되는, 문자열 하나를 입력받아 key가 2개인 json 문자열 반환하는 함수
	string GetOneParamJson(const string& input);

	// handlers에 저장되는, 문자열 두개를 입력받아 key가 3개인 json 문자열 반환하는 함수
	string GetTwoParamsJson(const string& input);

	// handlers 초기화하는 함수
	void InitHandlers();
}


// namespace Logic definition
bool Logic::SendData(const string& text)
{
	int r = 0;
	int dataLen = text.length();

	// 길이를 먼저 보낸다.
	// binary 로 4bytes 를 길이로 encoding 한다.
	// 이 때 network byte order 로 변환하기 위해서 htonl 을 호출해야된다.
	int dataLenNetByteOrder = htonl(dataLen);
	int offset = 0;
	while (offset < 4) {
		r = send(Client::sock, ((char*)&dataLenNetByteOrder) + offset, 4 - offset, 0);
		if (r == SOCKET_ERROR) {
			cerr << "failed to send length: " << WSAGetLastError() << endl;
			return false;
		}
		offset += r;
	}
	//cout << "Sent length info: " << dataLen << endl;

	offset = 0;
	while (offset < dataLen) {
		r = send(Client::sock, text.c_str() + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			cerr << "send failed with error " << WSAGetLastError() << endl;
			return false;
		}
		//cout << "Sent " << r << " bytes" << endl;
		offset += r;
	}

	return true;
}

bool Logic::ReceiveData()
{
	int r = 0;
	// 길이 정보를 받기 위해서 4바이트를 읽는다.
	// network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
	//std::cout << "Receiving length info" << std::endl;
	int dataLenNetByteOrder;
	int offset = 0;
	while (offset < 4) {
		r = recv(Client::sock, ((char*)&dataLenNetByteOrder) + offset, 4 - offset, 0);
		if (r == SOCKET_ERROR) {
			std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
			return false;
		}
		else if (r == 0) {
			// 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
			// 따라서 r == 0 인 경우도 loop 을 탈출하게 해야된다.
			std::cerr << "socket closed while reading length" << std::endl;
			return false;
		}
		offset += r;
	}
	int dataLen = ntohl(dataLenNetByteOrder);
	//std::cout << "Received length info: " << dataLen << std::endl;

	// 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
	if (dataLen > sizeof(Client::recvBuf))
	{
		cerr << "[" << Client::sock << "] Too big data: " << dataLen << endl;
		return false;
	}

	// socket 으로부터 데이터를 받는다.
	// TCP 는 연결 기반이므로 누가 보냈는지는 accept 시 결정되고 그 뒤로는 send/recv 만 호출한다.
	//std::cout << "Receiving stream" << std::endl;
	offset = 0;
	while (offset < dataLen) {
		r = recv(Client::sock, Client::recvBuf + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
			return false;
		}
		else if (r == 0) {
			// 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
			// 따라서 r == 0 인 경우도 loop 을 탈출하게 해야된다.
			return false;
		}
		//std::cout << "Received " << r << " bytes" << std::endl;
		offset += r;
	}

	// 받은 json 메시지 처리
	const string json = string(Client::recvBuf).substr(0, dataLen);

	Document document;
	document.Parse(json);

	//cout << "Received json : " << json << '\n';
	//cout << "Received text : " << document["text"].GetString() << '\n';
	cout << document[Json::TEXT].GetString() << '\n';

	// 받은 메세지가 메시지 타입이 존재하면
	if (document.HasMember(Json::PARAM1))
	{
		// 유저가 죽은 것을 알리는 메시지일 때
		if (Json::M_Type(stoi(document[Json::PARAM1].GetString())) == Json::M_Type::E_DIE)
		{
			//cout << "Receive Die message\n";
			// 죽은 클라이언트가 본 클라이언트라면 프로그램 종료
			if (document[Json::PARAM2].GetString() == Client::ID)
			{
				ExitProgram();
			}
		}
		// 다른 클라이언트에서 동일한 아이디로 로그인했을 때 프로그램 종료
		else if (Json::M_Type(stoi(document[Json::PARAM1].GetString())) == Json::M_Type::E_DUP_CONNECTION)
		{
			//cout << "Receive Dup Connection message\n";
			ExitProgram();
		}
	}

	return true;
}

string Logic::GetInputTextJson()
{
	string input;
	cin >> input;

	// input 명령어가 존재하면
	if (Json::handlers.find(input) != Json::handlers.end())
	{
		return (Json::handlers[input])(input);
	}

	// input이 명령어가 아니라면
	return "";
}

void Logic::Login()
{
	cout << "로그인 시작\n" << "아이디를 입력하세요.\n";

	string ID;
	cin >> ID;

	// ID 저장
	Client::ID = ID;

	string text = Json::GetParamsJson(Json::LOGIN, ID);

	SendData(text);
}

void Logic::ExitProgram()
{
	cout << "프로그램을 종료합니다.\n";

	// Socket을 닫는다.
	int r = closesocket(Client::sock);
	if (r == SOCKET_ERROR) {
		cerr << "closesocket failed with error " << WSAGetLastError() << endl;
		exit(1);
	}

	// Winsock 을 정리한다.
	WSACleanup();

	exit(0);
}


// namespace Json definition
string Json::GetJson(const string& input)
{
	return "{\"" + string(TEXT) + "\":         \"" + input + "\"}";
}

string Json::GetParamsJson(const string& input, const string& param1)
{
	return "{\"" + string(TEXT) + "\":\"" + input + "\",\"" + string(PARAM1) + "\":\"" + param1 + "\"}";
}

string Json::GetParamsJson(const string& input, const string& param1, const string& param2)
{
	return "{\"" + string(TEXT) + "\":\"" + input + "\",\"" + string(PARAM1) + "\":\"" + param1 + "\",\"" + string(PARAM2) + "\":\"" + param2 + "\"}";
}

string Json::GetOneParamJson(const string& input)
{
	string param1;
	cin >> param1;

	return GetParamsJson(input, param1);
}

string Json::GetTwoParamsJson(const string& input)
{
	string param1, param2;
	cin >> param1 >> param2;

	return GetParamsJson(input, param1, param2);
}

void Json::InitHandlers()
{
	handlers[MOVE] = GetTwoParamsJson;
	handlers[ATTACK] = GetJson;
	handlers[MONSTERS] = GetJson;
	handlers[USERS] = GetJson;
	handlers[CHAT] = GetTwoParamsJson;
	handlers[USE_POTION] = GetOneParamJson;
}