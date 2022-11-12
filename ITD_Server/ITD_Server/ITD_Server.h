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
#include <random>
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
	Client(SOCKET sock) : sock(sock), doingRecv(false), lenCompleted(false), packetLen(0), offset(0)
	{}

	~Client()
	{
		cout << "Client destroyed. Socket: " << sock << endl;
	}

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

namespace Rand
{
	// random
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> dis(0, NUM_DUNGEON_X - 1);

	int GetRandomLoc() 
	{
		return dis(gen);
	}
}

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

	static const char* EXPIRE_TIME = "15";
	static const char* LOGINED = "1";
	static const char* EXPIRED = "0";

	static const char* LOC_X = ":location:x";
	static const char* LOC_Y = ":location:y";
	static const char* HP = ":hp";
	static const char* STR = ":str";
	static const char* POTION_HP = ":potions:hp";
	static const char* POTION_STR = ":potions:str";

	static const int DEFAULT_HP = 30;
	static const int DEFAULT_STR = 3;
	static const int DEFAULT_POTION_HP = 1;
	static const int DEFAULT_POTION_STR = 1;

	void SetUserConnection(const string& ID, const char* status)
	{
		string setCmd = "SET USER:" + ID + " " + status;

		redisReply* reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetLocation(const string& ID, int x, int y)
	{
		string setCmd = "SET USER:" + ID + LOC_X + " " + to_string(x);

		redisReply* reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		setCmd = "SET USER:" + ID + LOC_Y + " " + to_string(y);

		reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetHp(const string& ID, int hp)
	{
		string setCmd = "SET USER:" + ID + HP + " " + to_string(hp);

		redisReply* reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetStr(const string& ID, int str)
	{
		string setCmd = "SET USER:" + ID + STR + " " + to_string(str);

		redisReply* reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetHpPotion(const string& ID, int numOfPotion)
	{
		string setCmd = "SET USER:" + ID + POTION_HP + " " + to_string(numOfPotion);

		redisReply* reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetStrPotion(const string& ID, int numOfPotion)
	{
		string setCmd = "SET USER:" + ID + POTION_STR + " " + to_string(numOfPotion);

		redisReply* reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	string GetUserConnection(const string& ID)
	{
		string getCmd = "GET USER:" + ID;

		redisReply* reply = (redisReply*)redisCommand(redis, getCmd.c_str());
		if (reply->type == REDIS_REPLY_STRING)
			return reply->str;

		return 0;
	}

	void ExpireUser(const string& ID)
	{
		string properties[7] = { "", LOC_X, LOC_Y, HP, STR, POTION_HP, POTION_STR};

		string expireCmd;
		redisReply* reply;
		for (int i = 0; i < 6; ++i)
		{
			expireCmd = "EXPIRE USER:" + ID + properties[i] + " " + EXPIRE_TIME;

			reply = (redisReply*)redisCommand(redis, expireCmd.c_str());

			if (reply->type == REDIS_REPLY_ERROR)
				cout << "Redis Command Error : " << expireCmd << '\n';

		}
		freeReplyObject(reply);

		SetUserConnection(ID, EXPIRED);
	}
}