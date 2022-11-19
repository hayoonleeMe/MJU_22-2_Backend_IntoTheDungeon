#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <string>
#include <iostream>
#include <list>
#include <memory>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;
using namespace rapidjson;

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

// �������� ���� ����
class Response
{
public:
	Response(SOCKET sock) : sock(sock), sendTurn(false), doingRecv(false), lenCompleted(false), packetLen(0), offset(0), ID(""), sendPacket("")
	{}

	~Response()
	{
		cout << "Response destroyed. Socket: " << sock << endl;
	}

public:
	atomic<bool> doingRecv;

	SOCKET sock;  // �� Ŭ���̾�Ʈ�� socket

	bool sendTurn;
	bool lenCompleted;
	int packetLen;
    char packet[65536];  // �ִ� 64KB �� ��Ŷ ������ ����
	string sendPacket;
	int offset;

	string ID;	// �α��ε� ������ ID
};

// Ŭ���̾�Ʈ ���� ����
namespace Client
{
	static const char* SERVER_ADDRESS = "127.0.0.1";
	static const unsigned short SERVER_PORT = 27015;
	//static const int NUM_ADDITIONAL_THREAD = 2;
	SOCKET sock;	// TCP ����
	bool bExit;
}

// ���� ���� ����
namespace Logic
{
	// ������ �����͸� ������.
	bool SendData(const string& text)
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

	// �����͸� �޴´�.
	bool ReceiveData()
	{
		int r = 0;
		char recvBuf[65536];
		// ���� ������ �ޱ� ���ؼ� 4����Ʈ�� �д´�.
		// network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
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
				// �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
				// ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
				std::cerr << "socket closed while reading length" << std::endl;
				return false;
			}
			offset += r;
		}
		int dataLen = ntohl(dataLenNetByteOrder);
		std::cout << "Received length info: " << dataLen << std::endl;

		// Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
		if (dataLen > sizeof(recvBuf))
		{
			cerr << "[" << Client::sock << "] Too big data: " << dataLen << endl;
			return false;
		}

		// socket ���κ��� �����͸� �޴´�.
		// TCP �� ���� ����̹Ƿ� ���� ���´����� accept �� �����ǰ� �� �ڷδ� send/recv �� ȣ���Ѵ�.
		std::cout << "Receiving stream" << std::endl;
		offset = 0;
		while (offset < dataLen) {
			r = recv(Client::sock, recvBuf + offset, dataLen - offset, 0);
			if (r == SOCKET_ERROR) {
				std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
				return false;
			}
			else if (r == 0) {
				// �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
				// ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
				return false;
			}
			std::cout << "Received " << r << " bytes" << std::endl;
			offset += r;
		}

		const string json = string(recvBuf).substr(0, dataLen);

		Document document;
		document.Parse(json);

		cout << "Received json : " << json << '\n';
		cout << "Received text : " << document["text"].GetString() << '\n';

		return true;
	}

	// cin���� �Է¹��� �ؽ�Ʈ�� json ���ڿ��� ��ȯ�� ��ȯ�ϴ� �Լ�
	string GetInputTextJson()
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

	// �α���
	void Login()
	{
		cout << "�α��� ����\n" << "���̵� �Է��ϼ���.\n";
		string ID;
		cin >> ID;

		string text = "{\"text\":\"login\",\"first\":\"" + ID + "\"}";

		SendData(text);
	}

	// ���α׷� ���� �غ�
	void ExitProgram()
	{
		Client::bExit = true;
		cout << "Error : ���α׷��� �����մϴ�.\n";
	}

	// recvThread�� ȣ���ϴ� ����ؼ� receive�ϴ� �Լ�
	void RecvThreadProc()
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
}


