#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
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

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

// 서버로 로그인된 유저 클라이언트
class Client
{
public:
	Client(SOCKET sock);

	~Client();

public:
	SOCKET sock;  // 이 클라이언트의 active socket
	string ID;	// 로그인된 유저의 ID

	atomic<bool> doingRecv;
	bool sendTurn;
	bool lenCompleted;
	int packetLen;
	char packet[65536];  // 최대 64KB 로 패킷 사이즈 고정
	string sendPacket;
	int offset;
};

struct Job
{
	string param1;
	string param2;

	Job()
	{}
	Job(string param1, string param2)
		: param1(param1), param2(param2)
	{}
};

typedef function<string(string, Job)> Handler;

// 서버 구동 관련
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

	// ID를 가진 기존의 접속을 모두 종료한다.
	void TerminateRemainUser(const string& ID)
	{
		list<SOCKET> toDelete;

		// 기존의 접속된 클라이언트를 찾는다.
		for (auto& entry : Server::activeClients)
		{
			if (entry.second->ID == ID)
				toDelete.push_back(entry.first);
		}

		// 지워야 하는 클라이언트들을 지운다.
		{
			lock_guard<mutex> lg(Server::activeClientsMutex);

			for (auto& sock : toDelete)
			{
				closesocket(sock);
				Server::activeClients.erase(sock);
			}
		}
	}
}

// 내부 로직 관련
namespace Logic
{
	static const int NUM_DUNGEON_X = 30;
	static const int NUM_DUNGEON_Y = 30;

	map<string, Handler> handlers;

	int Clamp(int value, int minValue, int maxValue)
	{
		int result = max(minValue, value);
		result = min(maxValue, value);
		return result;
	}

	string ProcessMove(const string& ID, const Job& job);
	string ProcessAttack(const string& ID, const Job& job);
	string ProcessMonsters(const string& ID, const Job& job);
	string ProcessUsers(const string& ID, const Job& job);
	string ProcessChat(const string& ID, const Job& job);
	void InitHandlers();
}

// 난수 관련
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

// json 관련
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
}

// redis 관련
namespace Redis
{
	// hiredis 
	redisContext* redis;
	mutex redisMutex;

	enum class Type
	{
		E_ABSOLUTE,
		E_RELATIVE
	};

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

	string GetLocationX(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + LOC_X;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	string GetLocationY(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + LOC_Y;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	string GetHp(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + HP;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(Redis::redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

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

	void SetLocation(const string& ID, int x, int y, Type t = Type::E_ABSOLUTE)
	{
		string setCmd1, setCmd2;
		if (t == Type::E_ABSOLUTE)
		{
			setCmd1 = "SET USER:" + ID + LOC_X + " " + to_string(x);
			setCmd2 = "SET USER:" + ID + LOC_Y + " " + to_string(y);
		}
		else
		{
			int newX = Logic::Clamp(stoi(GetLocationX(ID)) + x, 0, Logic::NUM_DUNGEON_X - 1);
			int newY = Logic::Clamp(stoi(GetLocationY(ID)) + y, 0, Logic::NUM_DUNGEON_Y - 1);

			setCmd1 = "SET USER:" + ID + LOC_X + " " + to_string(newX);
			setCmd2 = "SET USER:" + ID + LOC_Y + " " + to_string(newY);
		}

		cout << "setCmd1 : " << setCmd1 << " setCmd2 : " << setCmd2 << '\n';
		
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
	
	void SetHp(const string& ID, int hp, Type t = Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == Type::E_ABSOLUTE)
		{
			setCmd = "SET USER:" + ID + HP + " " + to_string(hp);
		}
		else
		{
			int newHp = Logic::Clamp(stoi(GetHp(ID)) + hp, 0, 30);
			setCmd = "SET USER:" + ID + HP + " " + to_string(newHp);
		}
		
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

	// expire 된 유저의 key들을 다시 되돌림
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

	// 유저를 redis에 등록한다.
	void RegisterUser(const string& ID)
	{
		// 해당 ID로 로그인한 유저가 있는지 체크
		string exitCmd = "Exists USER:" + ID;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, exitCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_INTEGER)
		{
			// USER:ID 가 존재할 때
			if (reply->integer == EXIST_ID)
			{
				// 이미 아이디가 로그인 중일 때 기존의 접속 강제 종료
				if (strcmp(GetUserConnection(ID).c_str(), LOGINED) == 0)
				{
					cout << ID + " 이미 로그인 되어 있음\n";

					// 기존 접속들을 강제 종료했으므로 redis의 key들은 expire 된다.
					// 이를 다시 영구적인 값으로 되돌려 새로 로그인한 클라만이 사용하도록 한다.
					Server::TerminateRemainUser(ID);
					PersistUser(ID);
				}
				// 종료 후 5분이 지나기 전에 재접속
				else if (strcmp(GetUserConnection(ID).c_str(), EXPIRED) == 0)
				{
					cout << ID + " 재접속함\n";
					PersistUser(ID);
				}
			}
			// USER:ID가 존재하지 않을 때
			else
			{
				cout << "New User : " << ID << '\n';
				// redis에 등록
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
	// 유저가 접속 종료할 때 Expire
	if (ID != "")
	{
		Redis::ExpireUser(ID);
	}

	cout << "Client destroyed. Socket: " << sock << endl;
}

string Logic::ProcessMove(const string& ID, const Job& job)
{
	cout << "ProcessMove is called\n";
	Redis::SetLocation(ID, stoi(job.param1), stoi(job.param2), Redis::Type::E_RELATIVE);
	return "";
}

string Logic::ProcessAttack(const string& ID, const Job& job)
{
	cout << "ProcessAttack is called\n";
	return "";
}

string Logic::ProcessMonsters(const string& ID, const Job& job)
{
	cout << "ProcessMonsters is called\n";
	return "";
}

string Logic::ProcessUsers(const string& ID, const Job& job)
{
	cout << "ProcessUsers is called\n";
	return "";
}

string Logic::ProcessChat(const string& ID, const Job& job)
{
	cout << "ProcessChat is called\n";
	return "";
}

void Logic::InitHandlers()
{
	handlers[Json::MOVE] = ProcessMove;
	handlers[Json::ATTACK] = ProcessAttack;
	handlers[Json::MONSTERS] = ProcessMonsters;
	handlers[Json::USERS] = ProcessUsers;
	handlers[Json::CHAT] = ProcessChat;
}

//bool isRepeat = false;
//void SendDataRepeat()
//{
//	std::this_thread::sleep_for(std::chrono::seconds(7));
//
//	while (1)
//	{
//		{
//			lock_guard<mutex> lg(Server::activeClientsMutex);
//
//			for (auto& entry : Server::activeClients)
//			{
//				if (entry.second->doingRecv.load() == false)
//				{
//					cout << "repeated send set : " << entry.first << '\n';
//					entry.second->sendTurn = true;
//					entry.second->sendPacket = string("repeated data");
//					entry.second->packetLen = htonl(entry.second->sendPacket.length());
//					isRepeat = true;
//				}
//			}
//		}
//
//		this_thread::sleep_for(chrono::seconds(4));
//	}
//}