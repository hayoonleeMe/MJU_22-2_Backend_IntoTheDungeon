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
	static const char* SERVER_ADDRESS = "127.0.0.1";	// ������ ���� �ּ�
	static const unsigned short SERVER_PORT = 27015;	// ������ ���� ��Ʈ��ȣ
	SOCKET sock;										// TCP ����
	static const int PACKET_SIZE = 65536;				// packet ������ ũ��, 64KB
	char packet[PACKET_SIZE];							// �ִ� 64KB �� ��Ŷ ������ ����
	string ID;											// ���� ���̵�
}

// ���� ���� ���� ���ӽ����̽�
namespace Logic
{
	static const char* QUIT = "quit";	// ���� ��ɾ�
	static const char* BOT = "bot";		// Bot ��ɾ�
	static const char* HELP = "help";	// help ��ɾ�

	static const int MOVE_DIS = 3;		// ������ �� ��� �̵��� �� �ִ� ĭ ��

	// ������ �����͸� ������ �Լ�
	bool SendData(const string& text);

	// �����κ��� �����͸� �޴� �Լ�
	bool ReceiveData();

	// cin���� �Է¹��� �ؽ�Ʈ�� json ���ڿ��� ��ȯ�� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetInputTextJson(string& text);

	// �α����� �õ��ϴ� �Լ�
	void Login();

	// ���α׷��� �����ϴ� �Լ�
	void ExitProgram();

	// info ��ɾ ������ ������ �Լ�
	void Info();

	// help ��ɾ ���� ���� ������ ����ϴ� �Լ�
	void Help();
}

/// <summary> json ���� ���ӽ����̽�
/// json ���ڿ��� ù��° Ű�� TEXT, ���� Ű���� ������� PARAM1, PARAM2�̴�.
/// �޽��� Ÿ���� �ִ� ��� �ι�° Ű�� PARAM1�� value�� �����ȴ�.
/// </summary>
namespace Json
{
	// �޽��� Ÿ���� ��Ÿ���� enum class
	enum class M_Type
	{
		E_DIE,
		E_DUP_CONNECTION,
		E_LOGIN_SUCCESS,
	};

	typedef function<bool(string&, const string&)> Handler;
	map<string, Handler> handlers;								// json ���ڿ��� ��ȯ�ϴ� �Լ� ��

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

	// handlers�� ����Ǵ�, key�� �ϳ��̰� value�� input�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetJson(const string& input);

	// key�� �ΰ��� value�� input, param1�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetJson(const string& input, const string& param1);

	// key�� ������ value�� input, param1, param2�� json ���ڿ� ��ȯ�ϴ� �Լ�
	string GetJson(const string& input, const string& param1, const string& param2);

	// move ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetMoveReqJson(string& text, const string& input);

	// attack ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetAttackReqJson(string& text, const string& input);

	// monsters ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetMonstersReqJson(string& text, const string& input);

	// users ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetUsersReqJson(string& text, const string& input);

	// chat ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetChatReqJson(string& text, const string& input);

	// usepotion ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetUsePotionReqJson(string& text, const string& input);

	// info ��ɾ ���� ��û �޽����� json ���ڿ��� ��ȯ�ϴ� �Լ�
	// ����, ���� ���θ� bool�� ��ȯ�ϰ�, ��ȯ�� json ���ڿ��� �Ķ���� text�� ����ȴ�.
	bool GetInfoReqJson(string& text, const string& input);

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
			cerr << "[����] failed to send length: " << WSAGetLastError() << endl;
			return false;
		}
		offset += r;
	}
	//cout << "Sent length info: " << dataLen << endl;

	offset = 0;
	while (offset < dataLen) {
		r = send(Client::sock, text.c_str() + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			cerr << "[����] send failed with error " << WSAGetLastError() << endl;
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
			std::cerr << "[����] recv failed with error " << WSAGetLastError() << std::endl;
			return false;
		}
		else if (r == 0) {
			// �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
			// ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
			std::cerr << "[����] socket closed while reading length" << std::endl;
			return false;
		}
		offset += r;
	}
	int dataLen = ntohl(dataLenNetByteOrder);
	//std::cout << "Received length info: " << dataLen << std::endl;

	// Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
	if (dataLen > sizeof(Client::packet))
	{
		cerr << "[����] [" << Client::sock << "] Too big data: " << dataLen << endl;
		return false;
	}

	// socket ���κ��� �����͸� �޴´�.
	// TCP �� ���� ����̹Ƿ� ���� ���´����� accept �� �����ǰ� �� �ڷδ� send/recv �� ȣ���Ѵ�.
	//std::cout << "Receiving stream" << std::endl;
	offset = 0;
	while (offset < dataLen) {
		r = recv(Client::sock, Client::packet + offset, dataLen - offset, 0);
		if (r == SOCKET_ERROR) {
			std::cerr << "[����] recv failed with error " << WSAGetLastError() << std::endl;
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
	const string json = string(Client::packet).substr(0, dataLen);

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
		// �α��� ������ �˸��� �޽����� ��
		else if (Json::M_Type(stoi(document[Json::PARAM1].GetString())) == Json::M_Type::E_LOGIN_SUCCESS)
		{
			// ���� ������ ����Ѵ�.
			Help();
		}
	}

	return true;
}

bool Logic::GetInputTextJson(string& text)
{
	string input;
	cin >> input;

	// ������ ������� �ʴ� ��ɾ�, ������ ������ �ʵ��� false�� ��ȯ�Ѵ�.
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

	// input ��ɾ �����ϸ�
	if (Json::handlers.find(input) != Json::handlers.end())
	{
		// ��ɾ� �Է¹��� ����� ��ȯ
		return (Json::handlers[input])(text, input);
	}

	// input�� ��ɾ �ƴ϶�� ����
	cout << "[����] �������� �ʴ� ��ɾ��Դϴ�.\n";
	return false;
}

void Logic::Login()
{
	cout << "[�ý���] �α����� �ʿ��մϴ�.\n[�ý���] ���̵� �Է��ϼ���.\n";

	while (true)
	{
		string ID;
		cin >> ID;

		// ID�� ��ɾ�� ��ġ�� �ʾƾ� ��밡��
		if (ID != Json::LOGIN && ID != Json::MOVE && ID != Json::ATTACK && ID != Json::MONSTERS && ID != Json::USERS && ID != Json::CHAT && ID != Json::USE_POTION && ID != Json::HP_POTION && ID != Json::STR_POTION && ID != Json::INFO && ID != Logic::QUIT && ID != Logic::BOT && ID != Logic::HELP)
		{
			// ID ����
			Client::ID = ID;

			break;
		}
		// ID�� ��ɾ�� ��ġ�� ����
		else
		{
			cout << "[����] ��� �Ұ����� ���̵��Դϴ�.\n";
		}
	}

	string text = Json::GetJson(Json::LOGIN, Client::ID);

	SendData(text);
}

void Logic::ExitProgram()
{
	cout << "[�ý���] ���α׷��� �����մϴ�.\n";

	// Socket�� �ݴ´�.
	int r = closesocket(Client::sock);
	if (r == SOCKET_ERROR) {
		cerr << "[����] closesocket failed with error " << WSAGetLastError() << endl;
		exit(1);
	}

	// Winsock �� �����Ѵ�.
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
	cout << "\n[�ý���] ���� ����\n";
	cout << "move x y : ���� ��ġ�κ��� x������ x��ŭ, y������ y��ŭ �̵��մϴ�.\n";
	cout << "attack : ���� ���� ���� �ִ� �����ӵ��� �����մϴ�.\n";
	cout << "monsters : ���� �����ϴ� �����ӵ��� ��ġ ������ ����մϴ�.\n";
	cout << "users : ���� ������ �������� ��ġ ������ ����մϴ�.\n";
	cout << "chat user message : user���� message��� ������ �ӼӸ��� �����ϴ�.\n";
	cout << "bot : ������ ��ɾ 1�ʸ��� �����ϴ� �� ��带 �����մϴ�.\n";
	cout << "usepotion hp : HP ������ ����� ü���� ȸ���մϴ�.\n";
	cout << "usepotion str : STR ������ ����� ���ݷ��� ������ŵ�ϴ�.\n";
	cout << "info : ���� ������ ����մϴ�.\n";
	cout << "help : ���� ������ ����մϴ�.\n";
	cout << "quit : ���α׷��� �����մϴ�.\n";
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

	// �� ��� 3ĭ �ʰ� �̵��̸� ����
	if (param1 > Logic::MOVE_DIS || param1 < -1 * Logic::MOVE_DIS || param2 > Logic::MOVE_DIS || param2 < -1 * Logic::MOVE_DIS)
	{
		cout << "[����] ������ �� �������� 3ĭ�� �̵��� �� �ֽ��ϴ�.\n";
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

	// userID�� ���� �ڽ��̸� ����
	if (toUserID == Client::ID)
	{
		cout << "[����] �ڽſ��Դ� �ӼӸ��� ���� �� �����ϴ�.\n";
		return false;
	}

	text = GetJson(input, toUserID, message);
	return true;
}

bool Json::GetUsePotionReqJson(string& text, const string& input)
{
	string potionType;
	cin >> potionType;

	// ��ġ�ϴ� ������ ������ ����
	if (potionType != Json::HP_POTION && potionType != Json::STR_POTION)
	{
		cout << "[����] �������� �ʴ� ���� �����Դϴ�.\n";
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