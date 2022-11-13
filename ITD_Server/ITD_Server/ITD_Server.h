#pragma once

#define _CRT_SECURE_NO_WARNINGS

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <chrono>
#include <condition_variable>
#include <string>
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

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

// ������ �α��ε� ���� Ŭ���̾�Ʈ
class Client
{
public:
	Client(SOCKET sock);

	~Client();

public:
	atomic<bool> doingRecv;

	SOCKET sock;  // �� Ŭ���̾�Ʈ�� active socket

	bool sendTurn;
	bool lenCompleted;
	int packetLen;
	char packet[65536];  // �ִ� 64KB �� ��Ŷ ������ ����
	string sendPacket;
	int offset;

	string ID;	// �α��ε� ������ ID
};

// ���� ���� ����
namespace Server
{
	static const char* SERVER_ADDRESS = "127.0.0.1";
	static const unsigned short SERVER_PORT = 27015;
	static const int NUM_WORKER_THREADS = 10;
	map<SOCKET, shared_ptr<Client>> activeClients;
	mutex activeClientsMutex;
	queue<shared_ptr<Client>> jobQueue;
	mutex jobQueueMutex;
	condition_variable jobQueueFilledCv;

	// ID�� ���� ������ ������ ��� �����Ѵ�.
	void TerminateRemainUser(const string& ID)
	{
		list<SOCKET> toDelete;

		// ������ ���ӵ� Ŭ���̾�Ʈ�� ã�´�.
		for (auto& entry : Server::activeClients)
		{
			if (entry.second->ID == ID)
			{
				closesocket(entry.first);
				toDelete.push_back(entry.first);
			}
		}

		// ������ �ϴ� Ŭ���̾�Ʈ���� �����.
		{
			lock_guard<mutex> lg(Server::activeClientsMutex);

			for (auto& sock : toDelete)
				Server::activeClients.erase(sock);
		}
	}
}

// ���� ���� ����
namespace Logic
{
	static const int NUM_DUNGEON_X = 30;
	static const int NUM_DUNGEON_Y = 30;

	string ProcessMove(const string& ID);
	string ProcessAttack(const string& ID);
	string ProcessMonsters(const string& ID);
	string ProcessUsers(const string& ID);
	string ProcessChat(const string& ID);
	string ProcessBot(const string& ID);
}

// ���� ����
namespace Rand
{
	// random
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> dis(0, Logic::NUM_DUNGEON_X - 1);

	int GetRandomLoc() 
	{
		return dis(gen);
	}
}

// json ����
namespace Json
{
	// json key
	static const char* TEXT = "text";
	static const char* PARAM1 = "first";
	static const char* PARAM2 = "second";

	// json value
	static const char* LOGIN = "login";
	static const char* MOVE = "move";
	static const char* ATTACK = "attack";
	static const char* MONSTERS = "monsters";
	static const char* USERS = "users";
	static const char* CHAT = "chat";
	static const char* BOT = "bot";
}

// redis ����
namespace Redis
{
	// hiredis 
	redisContext* redis;
	mutex redisMutex;

	static const int EXIST_ID = 1;

	static const char* EXPIRE_TIME = "30";
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
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetLocation(const string& ID, int x, int y)
	{
		string setCmd1 = "SET USER:" + ID + LOC_X + " " + to_string(x);
		string setCmd2 = "SET USER:" + ID + LOC_Y + " " + to_string(y);
		redisReply* reply; 
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, setCmd1.c_str());
			if (reply->type == REDIS_REPLY_ERROR)
				cout << "Redis Command Error : " << setCmd1 << '\n';

			reply = (redisReply*)redisCommand(Redis::redis, setCmd2.c_str());
			if (reply->type == REDIS_REPLY_ERROR)
				cout << "Redis Command Error : " << setCmd2 << '\n';
		}

		freeReplyObject(reply);
	}

	void SetHp(const string& ID, int hp)
	{
		string setCmd = "SET USER:" + ID + HP + " " + to_string(hp);
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetStr(const string& ID, int str)
	{
		string setCmd = "SET USER:" + ID + STR + " " + to_string(str);
		redisReply* reply; 
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetHpPotion(const string& ID, int numOfPotion)
	{
		string setCmd = "SET USER:" + ID + POTION_HP + " " + to_string(numOfPotion);
		redisReply* reply; 
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	void SetStrPotion(const string& ID, int numOfPotion)
	{
		string setCmd = "SET USER:" + ID + POTION_STR + " " + to_string(numOfPotion);
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	string GetUserConnection(const string& ID)
	{
		string getCmd = "GET USER:" + ID;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_STRING)
			return reply->str;

		return "";
	}

	void ExpireUser(const string& ID)
	{
		SetUserConnection(ID, EXPIRED);

		const int numOfCmd = 7;
		string properties[] = { "", LOC_X, LOC_Y, HP, STR, POTION_HP, POTION_STR};

		string expireCmd;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			for (int i = 0; i < numOfCmd; ++i)
			{
				expireCmd = "EXPIRE USER:" + ID + properties[i] + " " + EXPIRE_TIME;

				reply = (redisReply*)redisCommand(redis, expireCmd.c_str());

				if (reply->type == REDIS_REPLY_ERROR)
					cout << "Redis Command Error : " << expireCmd << '\n';

			}
		}
		freeReplyObject(reply);
	}

	// expire �� ������ key���� �ٽ� �ǵ���
	void PersistUser(const string& ID)
	{
		const int numOfCmd = 7;
		string properties[] = { "", LOC_X, LOC_Y, HP, STR, POTION_HP, POTION_STR };

		string persistCmd;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			for (int i = 0; i < numOfCmd; ++i)
			{
				persistCmd = "PERSIST USER:" + ID + properties[i];

				reply = (redisReply*)redisCommand(redis, persistCmd.c_str());

				if (reply->type == REDIS_REPLY_ERROR)
					cout << "Redis Command Error : " << persistCmd << '\n';
			}
		}
		freeReplyObject(reply);

		SetUserConnection(ID, LOGINED);
	}

	// ������ redis�� ����Ѵ�.
	void RegisterUser(const string& ID)
	{
		// �ش� ID�� �α����� ������ �ִ��� üũ
		string exitCmd = "Exists USER:" + ID;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, exitCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_INTEGER)
		{
			// USER:ID �� ������ ��
			if (reply->integer == EXIST_ID)
			{
				// �̹� ���̵� �α��� ���� �� ������ ���� ���� ����
				if (strcmp(GetUserConnection(ID).c_str(), LOGINED) == 0)
				{
					cout << ID + " �̹� �α��� �Ǿ� ����\n";

					// ���� ���ӵ��� ���� ���������Ƿ� redis�� key���� expire �ȴ�.
					// �̸� �ٽ� �������� ������ �ǵ��� ���� �α����� Ŭ���� ����ϵ��� �Ѵ�.
					Server::TerminateRemainUser(ID);
					PersistUser(ID);
				}
				// ���� �� 5���� ������ ���� ������
				else if (strcmp(GetUserConnection(ID).c_str(), EXPIRED) == 0)
				{
					cout << ID + " ��������\n";
					PersistUser(ID);
				}
			}
			// USER:ID�� �������� ���� ��
			else
			{
				cout << "New User : " << ID << '\n';
				// redis�� ���
				SetUserConnection(ID, LOGINED);
				SetLocation(ID, Rand::GetRandomLoc(), Rand::GetRandomLoc());
				SetHp(ID, DEFAULT_HP);
				SetStr(ID, DEFAULT_STR);
				SetHpPotion(ID, DEFAULT_POTION_HP);
				SetStrPotion(ID, DEFAULT_POTION_STR);
			}
		}

		freeReplyObject(reply);
	}
}

Client::Client(SOCKET sock) : sock(sock), sendTurn(false), doingRecv(false), lenCompleted(false), packetLen(0), offset(0), ID(""), sendPacket("")
{}

Client::~Client()
{
	// ������ ���� ������ �� Expire
	if (ID != "")
	{
		Redis::ExpireUser(ID);
	}

	cout << "Client destroyed. Socket: " << sock << endl;
}

string Logic::ProcessMove(const string& ID)
{
	return "";
}

string Logic::ProcessAttack(const string& ID)
{
	return "";
}

string Logic::ProcessMonsters(const string& ID)
{
	return "";
}

string Logic::ProcessUsers(const string& ID)
{
	return "";
}

string Logic::ProcessChat(const string& ID)
{
	return "";
}

string Logic::ProcessBot(const string& ID)
{
	return "";
}