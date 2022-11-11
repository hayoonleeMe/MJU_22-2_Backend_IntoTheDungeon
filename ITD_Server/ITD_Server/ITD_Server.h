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

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

class Client
{
public:
	Client(SOCKET sock);

	~Client();

public:
	SOCKET sock;  // 이 클라이언트의 active socket

	atomic<bool> doingRecv;

	bool lenCompleted;
	int packetLen;
	char packet[65536];  // 최대 64KB 로 패킷 사이즈 고정
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

// json key
static const char* CMD = "text";
static const char* PARAM1 = "first";
static const char* PARAM2 = "second";

// hiredis 
redisContext* redis;

