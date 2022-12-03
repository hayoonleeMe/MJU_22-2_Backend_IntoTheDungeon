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

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

// Ŭ���̾�Ʈ ���� ���� ���ӽ����̽�
namespace Client
{
	static const char* SERVER_ADDRESS = "127.0.0.1";
	static const unsigned short SERVER_PORT = 27015;
	SOCKET sock;	// TCP ����
	mutex sockMutex;
	char recvBuf[65536];
	string ID;
}

// ���� ���� ���� ���ӽ����̽�
namespace Logic
{
	// ������ �����͸� ������.
	bool SendData(const string& text);

	// �����κ��� �����͸� �޴´�.
	bool ReceiveData();

	// cin���� �Է¹��� �ؽ�Ʈ�� json ���ڿ��� ��ȯ�� ��ȯ�ϴ� �Լ�
	string GetInputTextJson();

	// �α���
	void Login();

	// ���α׷� ���� �غ�
	void ExitProgram();
}

/// <summary> json ���� ���ӽ����̽�
/// json ���ڿ��� ù��° Ű�� TEXT, ���� Ű���� ������� PARAM1, PARAM2�̴�.
/// �޽��� Ÿ���� �ִ� ��� �ι�° Ű�� PARAM1�� value�� �����ȴ�.
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

	// handlers�� ����Ǵ�, key�� �ϳ��̰� value�� input�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetJson(const string& input);

	// key�� �ΰ��� value�� input, param1�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetParamsJson(const string& input, const string& param1);

	// key�� ������ value�� input, param1, param2�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetParamsJson(const string& input, const string& param1, const string& param2);

	// handlers�� ����Ǵ�, ���ڿ� �ϳ��� �Է¹޾� key�� 2���� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetOneParamJson(const string& input);

	// handlers�� ����Ǵ�, ���ڿ� �ΰ��� �Է¹޾� key�� 3���� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetTwoParamsJson(const string& input);

	// handlers �ʱ�ȭ�ϴ� �Լ�
	void InitHandlers();
}


// namespace Logic definition
bool Logic::SendData(const string& text)
{
	int r = 0;
	int dataLen = text.length();

	// ���̸� ���� ������.
	// binary �� 4bytes �� ���̷� encoding �Ѵ�.
	// �� �� network byte order �� ��ȯ�ϱ� ���ؼ� htonl �� ȣ���ؾߵȴ�.
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
	// ���� ������ �ޱ� ���ؼ� 4����Ʈ�� �д´�.
	// network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
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
			// �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
			// ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
			std::cerr << "socket closed while reading length" << std::endl;
			return false;
		}
		offset += r;
	}
	int dataLen = ntohl(dataLenNetByteOrder);
	//std::cout << "Received length info: " << dataLen << std::endl;

	// Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
	if (dataLen > sizeof(Client::recvBuf))
	{
		cerr << "[" << Client::sock << "] Too big data: " << dataLen << endl;
		return false;
	}

	// socket ���κ��� �����͸� �޴´�.
	// TCP �� ���� ����̹Ƿ� ���� ���´����� accept �� �����ǰ� �� �ڷδ� send/recv �� ȣ���Ѵ�.
	//std::cout << "Receiving stream" << std::endl;
	offset = 0;
	while (offset < dataLen) {
		r = recv(Client::sock, Client::recvBuf + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
			return false;
		}
		else if (r == 0) {
			// �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
			// ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
			return false;
		}
		//std::cout << "Received " << r << " bytes" << std::endl;
		offset += r;
	}

	// ���� json �޽��� ó��
	const string json = string(Client::recvBuf).substr(0, dataLen);

	Document document;
	document.Parse(json);

	//cout << "Received json : " << json << '\n';
	//cout << "Received text : " << document["text"].GetString() << '\n';
	cout << document[Json::TEXT].GetString() << '\n';

	// ���� �޼����� �޽��� Ÿ���� �����ϸ�
	if (document.HasMember(Json::PARAM1))
	{
		// ������ ���� ���� �˸��� �޽����� ��
		if (Json::M_Type(stoi(document[Json::PARAM1].GetString())) == Json::M_Type::E_DIE)
		{
			//cout << "Receive Die message\n";
			// ���� Ŭ���̾�Ʈ�� �� Ŭ���̾�Ʈ��� ���α׷� ����
			if (document[Json::PARAM2].GetString() == Client::ID)
			{
				ExitProgram();
			}
		}
		// �ٸ� Ŭ���̾�Ʈ���� ������ ���̵�� �α������� �� ���α׷� ����
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

	// input ��ɾ �����ϸ�
	if (Json::handlers.find(input) != Json::handlers.end())
	{
		return (Json::handlers[input])(input);
	}

	// input�� ��ɾ �ƴ϶��
	return "";
}

void Logic::Login()
{
	cout << "�α��� ����\n" << "���̵� �Է��ϼ���.\n";

	string ID;
	cin >> ID;

	// ID ����
	Client::ID = ID;

	string text = Json::GetParamsJson(Json::LOGIN, ID);

	SendData(text);
}

void Logic::ExitProgram()
{
	cout << "���α׷��� �����մϴ�.\n";

	// Socket�� �ݴ´�.
	int r = closesocket(Client::sock);
	if (r == SOCKET_ERROR) {
		cerr << "closesocket failed with error " << WSAGetLastError() << endl;
		exit(1);
	}

	// Winsock �� �����Ѵ�.
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