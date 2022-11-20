#pragma once

#define _CRT_SECURE_NO_WARNINGS

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

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

struct Job
{
	string param1;
	string param2;
};

class Slime;

// ������ �α��ε� ���� Ŭ���̾�Ʈ
class Client
{
public:
	Client(SOCKET sock);

	~Client();

	void OnAttack(const shared_ptr<Slime>& slime);
	bool IsDead();

public:
	SOCKET sock;  // �� Ŭ���̾�Ʈ�� active socket
	string ID;	// �α��ε� ������ ID

	atomic<bool> doingProc;
	char packet[65536];  // �ִ� 64KB �� ��Ŷ ������ ����
	string sendPacket;
	bool sendTurn;
	bool lenCompleted;
	int packetLen;
	int offset;

	static const int MAX_X_ATTACK_RANGE = 1;
	static const int MIN_X_ATTACK_RANGE = -1;
	static const int MAX_Y_ATTACK_RANGE = 1;
	static const int MIN_Y_ATTACK_RANGE = -1;
};

class Slime
{
	enum class PotionType
	{
		E_POTION_HP,
		E_POTION_STR,
	};

public:
	Slime();

	int OnAttack(const shared_ptr<Client>& client); 

public:
	int index;
	int locX;
	int locY;
	int hp;
	int str;
	PotionType potionType;

	static int slimeIndex;
	static const int MAX_X_ATTACK_RANGE = 1;
	static const int MIN_X_ATTACK_RANGE = -1;
	static const int MAX_Y_ATTACK_RANGE = 1;
	static const int MIN_Y_ATTACK_RANGE = -1;
};
int Slime::slimeIndex = 0;

typedef function<string(shared_ptr<Client>, Job)> Handler;

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
	void TerminateRemainUser(const string& ID);
}

// ���� ���� ����
namespace Logic
{
	static const int NUM_DUNGEON_X = 30;
	static const int NUM_DUNGEON_Y = 30;

	static const int SLIME_GEN_PERIOD = 60;
	static const int SLIME_ATTACK_PERIOD = 5;
	static const int MAX_NUM_OF_SLIME = 10;
	static const int SLIME_MIN_HP = 5;
	static const int SLIME_MAX_HP = 10;
	static const int SLIME_MIN_STR = 3;	
	static const int SLIME_MAX_STR = 5;	

	static const int NUM_POTION = 2;

	map<SOCKET, list<string>> shouldSendPackets;
	mutex shouldSendPacketsMutex;

	map<string, Handler> handlers;

	list<shared_ptr<Slime>> slimes;
	mutex slimesMutex;

	int Clamp(int value, int minValue, int maxValue)
	{
		int result = min(max(minValue, value), maxValue);
		return result;
	}

	void BroadcastToClients(const string& message, const string& exceptID = "");

	string ProcessMove(const shared_ptr<Client>& client, const Job& job);
	string ProcessAttack(const shared_ptr<Client>& client, const Job& job);
	string ProcessMonsters(const shared_ptr<Client>& client, const Job& job);
	string ProcessUsers(const shared_ptr<Client>& client, const Job& job);
	string ProcessChat(const shared_ptr<Client>& client, const Job& job);
	void InitHandlers();

	void SpawnSlime(int num);
}

// ���� ����
namespace Rand
{
	// random
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> locDis(0, Logic::NUM_DUNGEON_X - 1);
	uniform_int_distribution<int> slimeHpDis(Logic::SLIME_MIN_HP, Logic::SLIME_MAX_HP);
	uniform_int_distribution<int> slimeStrDis(Logic::SLIME_MIN_STR, Logic::SLIME_MAX_STR);
	uniform_int_distribution<int> potionTypeDis(0, Logic::NUM_POTION - 1);

	int GetRandomLoc() 
	{
		return locDis(gen);
	}

	int GetRandomSlimeHp()
	{
		return slimeHpDis(gen);
	}

	int GetRandomSlimeStr()
	{
		return slimeStrDis(gen);
	}

	int GetRandomPotionType()
	{
		return potionTypeDis(gen);
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
}

// redis ����
namespace Redis
{
	// hiredis 
	redisContext* redis;
	mutex redisMutex;

	enum class F_Type
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

	string GetStr(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + STR;
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

	void SetLocation(const string& ID, int x, int y, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd1, setCmd2;
		if (t == F_Type::E_ABSOLUTE)
		{
			int newX = Logic::Clamp(x, 0, Logic::NUM_DUNGEON_X - 1);
			int newY = Logic::Clamp(y, 0, Logic::NUM_DUNGEON_Y - 1);

			setCmd1 = "SET USER:" + ID + LOC_X + " " + to_string(newX);
			setCmd2 = "SET USER:" + ID + LOC_Y + " " + to_string(newY);
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
	
	void SetHp(const string& ID, int hp, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == F_Type::E_ABSOLUTE)
		{
			// TODO : HP�� ����, ������ ���ؾ� ��
			int newHp = Logic::Clamp(hp, 0, 30);
			setCmd = "SET USER:" + ID + HP + " " + to_string(newHp);
		}
		else
		{
			// TODO : HP�� ����, ������ ���ؾ� ��
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
	
	// �� ������ ��� Ű�� �����Ѵ�.
	void DeleteAllUserKeys(const string& ID)
	{
		const int numOfCmd = 7;
		string properties[] = { "", LOC_X, LOC_Y, HP, STR, POTION_HP, POTION_STR };

		string delCmd = "DEL";
		for (int i = 0; i < numOfCmd; ++i)
		{
			delCmd += " USER:" + ID + properties[i];
		}

		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, delCmd.c_str());
		}
		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << delCmd << '\n';

		freeReplyObject(reply);
	}
}

// namespace Server Definition
void Server::TerminateRemainUser(const string& ID)
{
	list<SOCKET> toDelete;

	// ������ ���ӵ� Ŭ���̾�Ʈ�� ã�´�.
	for (auto& entry : Server::activeClients)
	{
		if (entry.second->ID == ID)
			toDelete.push_back(entry.first);
	}

	// ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ش�.
	{
		lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

		for (auto& closedSock : toDelete)
		{
			while (!Logic::shouldSendPackets[closedSock].empty())
				Logic::shouldSendPackets[closedSock].pop_front();
		}
	}

	// ������ �ϴ� Ŭ���̾�Ʈ���� �����.
	{
		lock_guard<mutex> lg(Server::activeClientsMutex);

		for (auto& closedSock : toDelete)
		{
			closesocket(closedSock);
			Server::activeClients.erase(closedSock);
		}
	}
}

// class Client Definition
Client::Client(SOCKET sock) : sock(sock), sendTurn(false), doingProc(false), lenCompleted(false), packetLen(0), offset(0), ID(""), sendPacket("")
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

void Client::OnAttack(const shared_ptr<Slime>& slime)
{
	cout << ID << " is On Attack by slime" << slime->index << '\n';

	Redis::SetHp(ID, -1 * slime->str, Redis::F_Type::E_RELATIVE);

	cout << "hayoon's remain hp : " << stoi(Redis::GetHp(ID)) << '\n';
}

bool Client::IsDead()
{
	if (ID == "")
		return false;

	return (stoi(Redis::GetHp(ID)) <= 0);
}

// class Slime Definition
Slime::Slime()
{
	index = slimeIndex++;
	locX = Rand::GetRandomLoc();
	locY = Rand::GetRandomLoc();
	hp = Rand::GetRandomSlimeHp();
	str = Rand::GetRandomSlimeStr();
	potionType = PotionType(Rand::GetRandomPotionType());
}

int Slime::OnAttack(const shared_ptr<Client>& client)
{
	cout << "slime" << index << " is On Attack by " << client->ID << '\n';

	int userStr = stoi(Redis::GetStr(client->ID));
	hp = Logic::Clamp(hp - userStr, 0, 30);

	return hp;
}

// namespace Logic Definition
void Logic::BroadcastToClients(const string& message, const string& exceptUser)
{
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		for (auto& entry : Server::activeClients)
		{
			// ��ε�ĳ��Ʈ���� �����ϰ� ���� ������ �ִٸ� ��ŵ
			if (entry.second->ID == exceptUser)
				continue;

			shouldSendPackets[entry.first].push_back(message);
		}
	}
}

string Logic::ProcessMove(const shared_ptr<Client>& client, const Job& job)
{
	cout << "ProcessMove is called\n";
	Redis::SetLocation(client->ID, stoi(job.param1), stoi(job.param2), Redis::F_Type::E_RELATIVE);

	return "{\"text\":\"(" + Redis::GetLocationX(client->ID) + "," + Redis::GetLocationY(client->ID) + ")�� �̵��ߴ�.\"}";
}

string Logic::ProcessAttack(const shared_ptr<Client>& client, const Job& job)
{
	cout << "ProcessAttack is called\n";

	int userLocX = stoi(Redis::GetLocationX(client->ID));
	int userLocY = stoi(Redis::GetLocationY(client->ID));
	int userStr = stoi(Redis::GetStr(client->ID));

	for (auto& slime : slimes)
	{
		int slimeLocX = slime->locX;
		int slimeLocY = slime->locY;

		// ������ ���� ���� �ȿ� �������� ������
		if ((userLocX - slimeLocX <= Client::MAX_X_ATTACK_RANGE && userLocX - slimeLocX >= Client::MIN_X_ATTACK_RANGE) &&
			(userLocY - slimeLocY <= Client::MAX_Y_ATTACK_RANGE && userLocY - slimeLocY >= Client::MIN_Y_ATTACK_RANGE))
		{
			slime->OnAttack(client);
			string message = "{\"text\":\"" + client->ID + " ��/�� ������" + to_string(slime->index) + " ��/�� �����ؼ� ������ " + to_string(userStr) + " ��/�� ���߽��ϴ�.\"}";
			BroadcastToClients(message);
		}
	}

	return "";
}

string Logic::ProcessMonsters(const shared_ptr<Client>& client, const Job& job)
{
	cout << "ProcessMonsters is called\n";
	return "";
}

string Logic::ProcessUsers(const shared_ptr<Client>& client, const Job& job)
{
	cout << "ProcessUsers is called\n";

	// Ŭ���̾�Ʈ�� ��ġ
	string msg = "{\"text\":\"" + client->ID + ":(" + Redis::GetLocationX(client->ID) + "," + Redis::GetLocationY(client->ID) + ") ";

	// �ٸ� Ŭ���̾�Ʈ���� ��ġ
	for (auto& entry : Server::activeClients)
	{
		if (entry.second->ID == client->ID)
			continue;

		msg += entry.second->ID + ":(" + Redis::GetLocationX(entry.second->ID) + "," + Redis::GetLocationY(entry.second->ID) + ") ";
	}
	msg += "\"}";

	return msg;
}

string Logic::ProcessChat(const shared_ptr<Client>& client, const Job& job)
{
	cout << "ProcessChat is called\n";

	SOCKET toSock = -1;

	// ������ ã��
	for (auto& entry : Server::activeClients)
		if (entry.second->ID == job.param1)
		{
			toSock = entry.first;
			break;
		}

	// ������ �������� ���� ��
	if (toSock == -1)
		return "{\"text\":\"�������� �ʴ� �����Դϴ�.\"}";

	// ������ ������ ��
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		shouldSendPackets[toSock].push_back("{\"text\":\"" + job.param1 + "(��)�� ���� �� �޽��� : " + job.param2 + "\"}");
	}

	return "{\"text\":\"�޽����� �����߽��ϴ�.\"}";
}

void Logic::InitHandlers()
{
	handlers[Json::MOVE] = ProcessMove;
	handlers[Json::ATTACK] = ProcessAttack;
	handlers[Json::MONSTERS] = ProcessMonsters;
	handlers[Json::USERS] = ProcessUsers;
	handlers[Json::CHAT] = ProcessChat;
}

void Logic::SpawnSlime(int num)
{
	if (num <= 0)
		return;

	{
		lock_guard<mutex> lg(slimesMutex);

		for (int i = 0; i < num; ++i)
		{
			slimes.push_back(shared_ptr<Slime>(new Slime()));
		}
	}
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
//				if (entry.second->doingProc.load() == false)
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
