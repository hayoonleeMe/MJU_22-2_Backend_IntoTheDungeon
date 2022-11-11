#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <hiredis/hiredis.h>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;
using namespace rapidjson;

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

class Client
{
public:
	Client(SOCKET sock) : sock(sock), doingRecv(false), lenCompleted(false), packetLen(0), offset(0)
	{}

	~Client()
	{
		cout << "Client destroyed. Socket: " << sock << endl;
	}

public:
	SOCKET sock;  // �� Ŭ���̾�Ʈ�� active socket

	atomic<bool> doingRecv;

	bool lenCompleted;
	int packetLen;
	char packet[65536];  // �ִ� 64KB �� ��Ŷ ������ ����
	int offset;
};

// socket
static const char* SERVER_ADDRESS = "127.0.0.1";
static const unsigned short SERVER_PORT = 27015;
static const int NUM_WORKER_THREADS = 10;
map<SOCKET, shared_ptr<Client> > activeClients;
mutex activeClientsMutex;
queue<shared_ptr<Client> > jobQueue;
mutex jobQueueMutex;
condition_variable jobQueueFilledCv;

// Dungeon
static const int NUM_DUNGEON_X = 30;
static const int NUM_DUNGEON_Y = 30;

namespace Json
{
	// json key
	static const char* TEXT = "text";
	static const char* PARAM1 = "first";
	static const char* PARAM2 = "second";

	// json value
	static const char* LOGIN = "login";
}

namespace Redis
{
	// hiredis 
	redisContext* redis;
}