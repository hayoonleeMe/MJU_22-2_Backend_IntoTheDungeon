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
	static const char* SERVER_ADDRESS = "127.0.0.1";	// 연결할 서버 주소
	static const unsigned short SERVER_PORT = 27015;	// 연결할 서버 포트번호
	SOCKET sock;										// TCP 소켓
	static const int PACKET_SIZE = 65536;				// packet 버퍼의 크기, 64KB
	char packet[PACKET_SIZE];							// 최대 64KB 로 패킷 사이즈 고정
	string ID;											// 유저 아이디
}

// 내부 로직 관련 네임스페이스
namespace Logic
{
	static const char* QUIT = "quit";	// 종료 명령어
	static const char* BOT = "bot";		// Bot 명령어
	static const char* HELP = "help";	// help 명령어

	static const int MOVE_DIS = 3;		// 유저가 한 축당 이동할 수 있는 칸 수

	// 서버로 데이터를 보내는 함수
	bool SendData(const string& text);

	// 서버로부터 데이터를 받는 함수
	bool ReceiveData();

	// cin으로 입력받은 텍스트를 json 문자열로 변환해 반환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetInputTextJson(string& text);

	// 로그인을 시도하는 함수
	void Login();

	// 프로그램을 종료하는 함수
	void ExitProgram();

	// info 명령어를 서버로 보내는 함수
	void Info();

	// help 명령어를 통해 조작 도움말을 출력하는 함수
	void Help();
}

/// <summary> json 관련 네임스페이스
/// json 문자열의 첫번째 키는 TEXT, 이후 키들은 순서대로 PARAM1, PARAM2이다.
/// 메시지 타입이 있는 경우 두번째 키인 PARAM1의 value로 제공된다.
/// </summary>
namespace Json
{
	// 메시지 타입을 나타내는 enum class
	enum class M_Type
	{
		E_DIE,
		E_DUP_CONNECTION,
		E_LOGIN_SUCCESS,
	};

	typedef function<bool(string&, const string&)> Handler;
	map<string, Handler> handlers;								// json 문자열로 변환하는 함수 맵

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
	static const char* INFO = "info";

	// handlers에 저장되는, key가 하나이고 value가 input인 json 문자열 반환하는 함수
	string GetJson(const string& input);

	// key가 두개고 value가 input, param1인 json 문자열 반환하는 함수
	string GetJson(const string& input, const string& param1);

	// key가 세개고 value가 input, param1, param2인 json 문자열 반환하는 함수
	string GetJson(const string& input, const string& param1, const string& param2);

	// move 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetMoveReqJson(string& text, const string& input);

	// attack 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetAttackReqJson(string& text, const string& input);

	// monsters 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetMonstersReqJson(string& text, const string& input);

	// users 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetUsersReqJson(string& text, const string& input);

	// chat 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetChatReqJson(string& text, const string& input);

	// usepotion 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetUsePotionReqJson(string& text, const string& input);

	// info 명령어에 대한 요청 메시지를 json 문자열로 변환하는 함수
	// 성공, 실패 여부를 bool로 반환하고, 변환된 json 문자열은 파라미터 text에 저장된다.
	bool GetInfoReqJson(string& text, const string& input);

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
			cerr << "[오류] failed to send length: " << WSAGetLastError() << endl;
			return false;
		}
		offset += r;
	}
	//cout << "Sent length info: " << dataLen << endl;

	offset = 0;
	while (offset < dataLen) {
		r = send(Client::sock, text.c_str() + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			cerr << "[오류] send failed with error " << WSAGetLastError() << endl;
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
			std::cerr << "[오류] recv failed with error " << WSAGetLastError() << std::endl;
			return false;
		}
		else if (r == 0) {
			// 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
			// 따라서 r == 0 인 경우도 loop 을 탈출하게 해야된다.
			std::cerr << "[오류] socket closed while reading length" << std::endl;
			return false;
		}
		offset += r;
	}
	int dataLen = ntohl(dataLenNetByteOrder);
	//std::cout << "Received length info: " << dataLen << std::endl;

	// 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
	if (dataLen > sizeof(Client::packet))
	{
		cerr << "[오류] [" << Client::sock << "] Too big data: " << dataLen << endl;
		return false;
	}

	// socket 으로부터 데이터를 받는다.
	// TCP 는 연결 기반이므로 누가 보냈는지는 accept 시 결정되고 그 뒤로는 send/recv 만 호출한다.
	//std::cout << "Receiving stream" << std::endl;
	offset = 0;
	while (offset < dataLen) {
		r = recv(Client::sock, Client::packet + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			std::cerr << "[오류] recv failed with error " << WSAGetLastError() << std::endl;
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
	const string json = string(Client::packet).substr(0, dataLen);

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
		// 로그인 성공을 알리는 메시지일 때
		else if (Json::M_Type(stoi(document[Json::PARAM1].GetString())) == Json::M_Type::E_LOGIN_SUCCESS)
		{
			// 조작 도움말을 출력한다.
			Help();
		}
	}

	return true;
}

bool Logic::GetInputTextJson(string& text)
{
	string input;
	cin >> input;

	// 서버와 통신하지 않는 명령어, 서버에 보내지 않도록 false를 반환한다.
	if (input == QUIT)
	{
		ExitProgram();
		return false;
	}
	else if (input == HELP)
	{
		Help();
		return false;
	}

	// input 명령어가 존재하면
	if (Json::handlers.find(input) != Json::handlers.end())
	{
		// 명령어 입력받은 결과를 반환
		return (Json::handlers[input])(text, input);
	}

	// input이 명령어가 아니라면 오류
	cout << "[오류] 존재하지 않는 명령어입니다.\n";
	return false;
}

void Logic::Login()
{
	cout << "[시스템] 로그인이 필요합니다.\n[시스템] 아이디를 입력하세요.\n";

	while (true)
	{
		string ID;
		cin >> ID;

		// ID가 명령어와 겹치지 않아야 사용가능
		if (ID != Json::LOGIN && ID != Json::MOVE && ID != Json::ATTACK && ID != Json::MONSTERS && ID != Json::USERS && ID != Json::CHAT && ID != Json::USE_POTION && ID != Json::HP_POTION && ID != Json::STR_POTION && ID != Json::INFO && ID != Logic::QUIT && ID != Logic::BOT && ID != Logic::HELP)
		{
			// ID 저장
			Client::ID = ID;

			break;
		}
		// ID가 명령어와 겹치면 오류
		else
		{
			cout << "[오류] 사용 불가능한 아이디입니다.\n";
		}
	}

	string text = Json::GetJson(Json::LOGIN, Client::ID);

	SendData(text);
}

void Logic::ExitProgram()
{
	cout << "[시스템] 프로그램을 종료합니다.\n";

	// Socket을 닫는다.
	int r = closesocket(Client::sock);
	if (r == SOCKET_ERROR) {
		cerr << "[오류] closesocket failed with error " << WSAGetLastError() << endl;
		exit(1);
	}

	// Winsock 을 정리한다.
	WSACleanup();

	exit(0);
}

void Logic::Info()
{
	string text;
	Json::handlers[Json::INFO](text, Json::INFO);

	if (!SendData(text))
		ExitProgram();
}

void Logic::Help()
{
	cout << "\n[시스템] 조작 도움말\n";
	cout << "move x y : 현재 위치로부터 x축으로 x만큼, y축으로 y만큼 이동합니다.\n";
	cout << "attack : 공격 범위 내에 있는 슬라임들을 공격합니다.\n";
	cout << "monsters : 현재 존재하는 슬라임들의 위치 정보를 출력합니다.\n";
	cout << "users : 현재 접속한 유저들의 위치 정보를 출력합니다.\n";
	cout << "chat user message : user에게 message라는 내용의 귓속말을 보냅니다.\n";
	cout << "bot : 랜덤한 명령어를 1초마다 실행하는 봇 모드를 실행합니다.\n";
	cout << "usepotion hp : HP 포션을 사용해 체력을 회복합니다.\n";
	cout << "usepotion str : STR 포션을 사용해 공격력을 증가시킵니다.\n";
	cout << "info : 유저 정보를 출력합니다.\n";
	cout << "help : 조작 도움말을 출력합니다.\n";
	cout << "quit : 프로그램을 종료합니다.\n";
	cout << '\n';
}


// namespace Json definition
string Json::GetJson(const string& input)
{
	return "{\"" + string(TEXT) + "\":\"" + input + "\"}";
}

string Json::GetJson(const string& input, const string& param1)
{
	return "{\"" + string(TEXT) + "\":\"" + input + "\",\"" + string(PARAM1) + "\":\"" + param1 + "\"}";
}

string Json::GetJson(const string& input, const string& param1, const string& param2)
{
	return "{\"" + string(TEXT) + "\":\"" + input + "\",\"" + string(PARAM1) + "\":\"" + param1 + "\",\"" + string(PARAM2) + "\":\"" + param2 + "\"}";
}

bool Json::GetMoveReqJson(string& text, const string& input)
{
	int param1, param2;
	cin >> param1 >> param2;

	// 한 축당 3칸 초과 이동이면 오류
	if (param1 > Logic::MOVE_DIS || param1 < -1 * Logic::MOVE_DIS || param2 > Logic::MOVE_DIS || param2 < -1 * Logic::MOVE_DIS)
	{
		cout << "[오류] 유저는 한 방향으로 3칸씩 이동할 수 있습니다.\n";
		return false;
	}

	text = GetJson(input, to_string(param1), to_string(param2));
	return true;
}

bool Json::GetAttackReqJson(string& text, const string& input)
{
	text = GetJson(input);
	return true;
}

bool Json::GetMonstersReqJson(string& text, const string& input)
{
	text = GetJson(input);
	return true;
}

bool Json::GetUsersReqJson(string& text, const string& input)
{
	text = GetJson(input);
	return true;
}

bool Json::GetChatReqJson(string& text, const string& input)
{
	string toUserID, message;
	cin >> toUserID;
	getline(cin, message);

	// userID가 유저 자신이면 오류
	if (toUserID == Client::ID)
	{
		cout << "[오류] 자신에게는 귓속말을 보낼 수 없습니다.\n";
		return false;
	}

	text = GetJson(input, toUserID, message);
	return true;
}

bool Json::GetUsePotionReqJson(string& text, const string& input)
{
	string potionType;
	cin >> potionType;

	// 일치하는 포션이 없으면 오류
	if (potionType != Json::HP_POTION && potionType != Json::STR_POTION)
	{
		cout << "[오류] 존재하지 않는 포션 종류입니다.\n";
		return false;
	}

	text = GetJson(input, potionType);
	return true;
}

bool Json::GetInfoReqJson(string& text, const string& input)
{
	text = GetJson(input);
	return true;
}

void Json::InitHandlers()
{
	handlers[MOVE] = GetMoveReqJson;
	handlers[ATTACK] = GetAttackReqJson;
	handlers[MONSTERS] = GetMonstersReqJson;
	handlers[USERS] = GetUsersReqJson;
	handlers[CHAT] = GetChatReqJson;
	handlers[USE_POTION] = GetUsePotionReqJson;
	handlers[INFO] = GetInfoReqJson;
}