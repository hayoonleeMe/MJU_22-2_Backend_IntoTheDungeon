#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <string>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;
using namespace rapidjson;

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

// 클라이언트 구동 관련
namespace Client
{
	static const char* SERVER_ADDRESS = "127.0.0.1";
	static const unsigned short SERVER_PORT = 27015;
	//static const int NUM_ADDITIONAL_THREAD = 2;
	SOCKET sock;	// TCP 소켓
	mutex sockMutex;
	bool bExit;
	char recvBuf[65536];
}

// 내부 로직 관련
namespace Logic
{
	enum class M_Type
	{
		E_DIE,
		E_DUP_CONNECTION,
	};

	// 서버로 데이터를 보낸다.
	bool SendData(const string& text);

	// 데이터를 받는다.
	bool ReceiveData();

	// cin으로 입력받은 텍스트를 json 문자열로 변환해 반환하는 함수
	string GetInputTextJson();

	// 로그인
	void Login();

	// 프로그램 종료 준비
	void ExitProgram();

	// recvThread가 호출하는, 계속해서 receive하는 함수
	void RecvThreadProc();
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
	cout << "Sent length info: " << dataLen << endl;

	offset = 0;
	while (offset < dataLen) {
		r = send(Client::sock, text.c_str() + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			cerr << "send failed with error " << WSAGetLastError() << endl;
			return false;
		}
		cout << "Sent " << r << " bytes" << endl;
		offset += r;
	}

	return true;
}

bool Logic::ReceiveData()
{
	int r = 0;
	// 길이 정보를 받기 위해서 4바이트를 읽는다.
	// network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
	std::cout << "Receiving length info" << std::endl;
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
	std::cout << "Received length info: " << dataLen << std::endl;

	// 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
	if (dataLen > sizeof(Client::recvBuf))
	{
		cerr << "[" << Client::sock << "] Too big data: " << dataLen << endl;
		return false;
	}

	// socket 으로부터 데이터를 받는다.
	// TCP 는 연결 기반이므로 누가 보냈는지는 accept 시 결정되고 그 뒤로는 send/recv 만 호출한다.
	std::cout << "Receiving stream" << std::endl;
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
		std::cout << "Received " << r << " bytes" << std::endl;
		offset += r;
	}

	const string json = string(Client::recvBuf).substr(0, dataLen);

	Document document;
	document.Parse(json);

	cout << "Received json : " << json << '\n';
	cout << "Received text : " << document["text"].GetString() << '\n';

	// 받은 메세지가 문제를 알리는 메시지인지 확인
	if (document.HasMember("error"))
	{
		// 유저가 죽은 것을 알리는 메시지일 때
		if (M_Type(stoi(document["error"].GetString())) == M_Type::E_DIE)
		{
			cout << "Receive Die message\n";
			ExitProgram();
		}
		// 다른 클라이언트에서 동일한 아이디로 로그인했을 때
		else if (M_Type(stoi(document["error"].GetString())) == M_Type::E_DUP_CONNECTION)
		{
			cout << "Receive Dup Connection message\n";
			ExitProgram();
		}
	}

	return true;
}

string Logic::GetInputTextJson()
{
	string input;
	cin >> input;

	string text = "{\"text\":\"" + input + "\"";

	if (input == "move" || input == "chat")
	{
		string x, y;

		cin >> x >> y;

		text += ",\"first\":\"" + x + "\",\"second\":\"" + y + "\"";
	}
	else if (input != "attack" && input != "monsters" && input != "users" && input != "bot")
	{
		return "";
	}

	text += "}";
	return text;
}

void Logic::Login()
{
	cout << "로그인 시작\n" << "아이디를 입력하세요.\n";
	string ID;
	cin >> ID;

	string text = "{\"text\":\"login\",\"first\":\"" + ID + "\"}";

	SendData(text);
}

void Logic::ExitProgram()
{
	Client::bExit = true;
	cout << "프로그램을 종료합니다.\n";
}

void Logic::RecvThreadProc()
{
	while (!Client::bExit)
	{
		if (!ReceiveData())
		{
			ExitProgram();
			return;
		}
	}
}