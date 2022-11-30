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

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

class Slime;

// 서버로 로그인된 유저 클라이언트
class Client
{
public:
	Client(SOCKET sock);

	~Client();

	int OnAttack(const shared_ptr<Slime>& slime);
	//bool IsDead();

public:
	SOCKET sock;  // 이 클라이언트의 active socket
	string ID;	// 로그인된 유저의 ID

	atomic<bool> doingProc;
	char packet[65536];  // 최대 64KB 로 패킷 사이즈 고정
	string sendPacket;
	bool sendTurn;
	bool lenCompleted;
	int packetLen;
	int offset;
	bool shouldTerminate;

	static const int MAX_X_ATTACK_RANGE = 1;
	static const int MIN_X_ATTACK_RANGE = -1;
	static const int MAX_Y_ATTACK_RANGE = 1;
	static const int MIN_Y_ATTACK_RANGE = -1;
};

// 서버 구동 관련 네임스페이스
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
	void TerminateRemainUser(const string& ID);
}

// 내부 로직 관련 네임스페이스
namespace Logic
{
	// Process 함수들이 사용할 데이터를 나타내는 구조체
	struct ParamsForProc
	{
		string param1;
		string param2;
	};

	typedef function<string(shared_ptr<Client>, Logic::ParamsForProc)> Handler;

	static const int NUM_DUNGEON_X = 30;
	static const int NUM_DUNGEON_Y = 30;

	static const int SLIME_GEN_PERIOD = 60;
	static const int MAX_NUM_OF_SLIME = 10;

	static const int NUM_POTION_TYPE = 2;

	enum class PotionType
	{
		E_POTION_HP,
		E_POTION_STR,
	};

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

	string ProcessMove(const shared_ptr<Client>& client, const ParamsForProc& job);
	string ProcessAttack(const shared_ptr<Client>& client, const ParamsForProc& job);
	string ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& job);
	string ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& job);
	string ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& job);
	void InitHandlers();

	void SpawnSlime(int num);
}


// 슬라임 클래스
class Slime
{
public:
	Slime();

	~Slime()
	{
		cout << "Slime" << index << " is dead\n";
	}

	int OnAttack(const shared_ptr<Client>& client);

	bool IsDead();

public:
	int index;
	int locX;
	int locY;
	int hp;
	int str;
	Logic::PotionType potionType;

	static int slimeIndex;
	static const int MAX_X_ATTACK_RANGE = 1;
	static const int MIN_X_ATTACK_RANGE = -1;
	static const int MAX_Y_ATTACK_RANGE = 1;
	static const int MIN_Y_ATTACK_RANGE = -1;
	static const int ATTACK_PERIOD = 5;
	static const int MIN_HP = 5;
	static const int MAX_HP = 10;
	static const int MIN_STR = 3;
	static const int MAX_STR = 5;
};
int Slime::slimeIndex = 0;

// 난수 관련 네임스페이스
namespace Rand
{
	// random
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> locDis(0, Logic::NUM_DUNGEON_X - 1);
	uniform_int_distribution<int> slimeHpDis(Slime::MIN_HP, Slime::MAX_HP);
	uniform_int_distribution<int> slimeStrDis(Slime::MIN_STR, Slime::MAX_STR);
	uniform_int_distribution<int> potionTypeDis(0, Logic::NUM_POTION_TYPE - 1);

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

// json 관련 네임스페이스
namespace Json
{
	enum class M_Type
	{
		E_DIE,
		E_DUP_CONNECTION,
	};

	// json recv key
	static const char* TEXT = "text";
	static const char* PARAM1 = "first";
	static const char* PARAM2 = "second";

	// json recv value
	static const char* LOGIN = "login";
	static const char* MOVE = "move";
	static const char* ATTACK = "attack";
	static const char* MONSTERS = "monsters";
	static const char* USERS = "users";
	static const char* CHAT = "chat";

	// Slime attack to user
	string GetSlimeAttackUserJson(int slimeIndex, const string& userID, int slimeStr);

	// User attack to slime
	string GetUserAttackSlimeJson(const string& userID, int slimeIndex, int userStr);

	// Slime die
	string GetSlimeDieJson(int slimeIndex, const string& userID);

	// User die
	string GetUserDieJson(const string& userID, int slimeIndex);

	// Move response
	string GetMoveRespJson(const string& userID);

	// Text only
	string GetTextOnlyJson(const string& text);

	// Error Notify
	string GetDupConnectionJson();

	// Users response
	string GetUsersRespJson(const string& userID);

	// Monsters response
	string GetMonstersRespJson();

	// Chat response
	string GetChatRespJson(const string& userID, const string& text);
}

// redis 관련 네임스페이스
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

	// TODO : 키 만료 시간 수정 필요
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

	string GetHpPotion(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + POTION_HP;
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

	string GetStrPotion(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + POTION_STR;
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
			// TODO : HP의 하한, 상한을 정해야 함
			int newHp = Logic::Clamp(hp, 0, 30);
			setCmd = "SET USER:" + ID + HP + " " + to_string(newHp);
		}
		else
		{
			// TODO : HP의 하한, 상한을 정해야 함
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

	void SetHpPotion(const string& ID, int numOfPotion, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == F_Type::E_ABSOLUTE)
		{
			setCmd = "SET USER:" + ID + POTION_HP + " " + to_string(numOfPotion);
		}
		else
		{
			int newNumOfPotion = stoi(GetHpPotion(ID)) + numOfPotion;
			setCmd = "SET USER:" + ID + POTION_HP + " " + to_string(newNumOfPotion);
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

	void SetStrPotion(const string& ID, int numOfPotion, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == F_Type::E_ABSOLUTE)
		{
			setCmd = "SET USER:" + ID + POTION_STR + " " + to_string(numOfPotion);
		}
		else
		{
			int newNumOfPotion = stoi(GetStrPotion(ID)) + numOfPotion;
			setCmd = "SET USER:" + ID + POTION_STR + " " + to_string(newNumOfPotion);
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
	string RegisterUser(const string& ID)
	{
		// 반환할 문자열
		string ret;

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
					ret = Json::GetTextOnlyJson("기존의 접속을 종료하고 로그인했습니다.");

					// 기존 접속들을 강제 종료했으므로 redis의 key들은 expire 된다.
					// 이를 다시 영구적인 값으로 되돌려 새로 로그인한 클라만이 사용하도록 한다.
					Server::TerminateRemainUser(ID);
					PersistUser(ID);
				}
				// 종료 후 5분이 지나기 전에 재접속
				else if (strcmp(GetUserConnection(ID).c_str(), EXPIRED) == 0)
				{
					cout << ID + " 재접속함\n";
					ret = Json::GetTextOnlyJson("로그인에 성공했습니다.");
					PersistUser(ID);
				}
			}
			// USER:ID가 존재하지 않을 때
			else
			{
				cout << "New User : " << ID << '\n';
				ret = Json::GetTextOnlyJson("로그인에 성공했습니다.");

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

		return ret;
	}
	
	// 한 유저의 모든 키를 삭제한다.
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
	// 기존의 접속된 클라이언트를 찾는다.
	for (auto& entry : Server::activeClients)
	{
		if (entry.second->ID == ID)
		{
			entry.second->shouldTerminate = true;

			// 기존 접속에서 예정되어 있던 보내야하는 메시지들을 모두 없애고,
			// 중복 로그인에 의해 접속 종료되는 것을 알리는 메시지를 마지막으로 전송하기 위해 메시지를 예약한다.
			Logic::shouldSendPackets[entry.first].clear();

			Logic::shouldSendPackets[entry.first].push_back(Json::GetDupConnectionJson());
		}
	}
}


// class Client Definition
Client::Client(SOCKET sock) : sock(sock), sendTurn(false), doingProc(false), lenCompleted(false), packetLen(0), offset(0), ID(""), sendPacket(""), shouldTerminate(false)
{}

Client::~Client()
{
	// 유저가 접속 종료할 때 Expire
	if (ID != "" && !shouldTerminate)
	{
		Redis::ExpireUser(ID);
	}

	cout << "Client destroyed. Socket: " << sock << endl;
}

int Client::OnAttack(const shared_ptr<Slime>& slime)
{
	cout << ID << " is On Attack by slime" << slime->index << '\n';

	Redis::SetHp(ID, -1 * slime->str, Redis::F_Type::E_RELATIVE);

	cout << "hayoon's remain hp : " << stoi(Redis::GetHp(ID)) << '\n';

	int nowHp = stoi(Redis::GetHp(ID));
	if (nowHp <= 0)
	{
		shouldTerminate = true;
	}

	return nowHp;
}

// namespace Logic Definition
void Logic::BroadcastToClients(const string& message, const string& exceptUser)
{
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		for (auto& entry : Server::activeClients)
		{
			// 브로드캐스트에서 제외하고 싶은 유저가 있다면 스킵
			if (entry.second->ID == exceptUser)
				continue;

			shouldSendPackets[entry.first].push_back(message);
		}
	}
}

string Logic::ProcessMove(const shared_ptr<Client>& client, const ParamsForProc& job)
{
	cout << "ProcessMove is called\n";
	Redis::SetLocation(client->ID, stoi(job.param1), stoi(job.param2), Redis::F_Type::E_RELATIVE);

	return Json::GetMoveRespJson(client->ID);
}

string Logic::ProcessAttack(const shared_ptr<Client>& client, const ParamsForProc& job)
{
	cout << "ProcessAttack is called\n";

	string ret = "";			// 반환할 문자열
	bool canAttack = false;		// 공격할 수 있는지

	// 유저의 스텟
	int userLocX = stoi(Redis::GetLocationX(client->ID));
	int userLocY = stoi(Redis::GetLocationY(client->ID));
	int userStr = stoi(Redis::GetStr(client->ID));

	// 죽은 슬라임의 이터레이터를 저장하는 리스트
	list<list<shared_ptr<Slime>>::iterator> deadSlimeIts;
	for (list<shared_ptr<Slime>>::iterator it = slimes.begin(); it != slimes.end(); ++it)
	{
		// 이미 죽은 슬라임이면 스킵
		if ((*it)->IsDead())
			continue;

		// 슬라임의 스텟
		int slimeLocX = (*it)->locX;
		int slimeLocY = (*it)->locY;

		// 유저의 공격 범위 안에 슬라임이 있으면
		if ((userLocX - slimeLocX <= Client::MAX_X_ATTACK_RANGE && userLocX - slimeLocX >= Client::MIN_X_ATTACK_RANGE) &&
			(userLocY - slimeLocY <= Client::MAX_Y_ATTACK_RANGE && userLocY - slimeLocY >= Client::MIN_Y_ATTACK_RANGE))
		{
			// 공격 가능
			canAttack = true;

			// 공격 메시지를 브로드캐스트한다.
			string msg = Json::GetUserAttackSlimeJson(client->ID, (*it)->index, userStr);
			BroadcastToClients(msg);

			// 유저의 공격으로 슬라임이 죽었으면 삭제를 위해 deadSlimes 리스트에 넣는다.
			if ((*it)->OnAttack(client) <= 0)
			{
				// 슬라임 사망 메시지를 브로드캐스트한다.
				deadSlimeIts.push_back(it);
				string dieMsg = Json::GetSlimeDieJson((*it)->index, client->ID);
				BroadcastToClients(dieMsg);
			}
		}
	}

	// 죽은 슬라임을 없앤다.
	{
		lock_guard<mutex> lg(slimesMutex);

		for (auto& deadSlimeIt : deadSlimeIts)
		{
			// 죽은 슬라임이 가지고 있던 포션을 유저가 획득한다.
			if ((*deadSlimeIt)->potionType == Logic::PotionType::E_POTION_HP)
			{
				Redis::SetHpPotion(client->ID, 1, Redis::F_Type::E_RELATIVE);
			}
			else if ((*deadSlimeIt)->potionType == Logic::PotionType::E_POTION_STR)
			{
				Redis::SetStrPotion(client->ID, 1, Redis::F_Type::E_RELATIVE);
			}

			slimes.erase(deadSlimeIt);
		}
	}

	// 공격할 대상이 없다면 클라이언트에게 알린다.
	if (!canAttack)
		ret = Json::GetTextOnlyJson("공격할 대상이 없습니다.");

	return ret;
}

string Logic::ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& job)
{
	cout << "ProcessMonsters is called\n";

	return Json::GetMonstersRespJson();
}

string Logic::ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& job)
{
	cout << "ProcessUsers is called\n";

	// 클라이언트의 위치
	return Json::GetUsersRespJson(client->ID);
}

string Logic::ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& job)
{
	cout << "ProcessChat is called\n";

	SOCKET toSock = -1;

	// 유저를 찾기
	for (auto& entry : Server::activeClients)
		if (entry.second->ID == job.param1)
		{
			toSock = entry.first;
			break;
		}

	// 유저가 존재하지 않을 때
	if (toSock == -1)
		return Json::GetTextOnlyJson("존재하지 않는 유저입니다.");

	// 유저가 존재할 때
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		shouldSendPackets[toSock].push_back(Json::GetChatRespJson(job.param1, job.param2));
	}

	return Json::GetTextOnlyJson("메시지를 전송했습니다.");
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


// class Slime Definition
Slime::Slime()
{
	index = slimeIndex++;
	locX = Rand::GetRandomLoc();
	locY = Rand::GetRandomLoc();
	hp = Rand::GetRandomSlimeHp();
	str = Rand::GetRandomSlimeStr();
	potionType = Logic::PotionType(Rand::GetRandomPotionType());
}

int Slime::OnAttack(const shared_ptr<Client>& client)
{
	cout << "slime" << index << " is On Attack by " << client->ID << '\n';

	int userStr = stoi(Redis::GetStr(client->ID));
	hp = Logic::Clamp(hp - userStr, 0, MAX_HP);

	return hp;
}

bool Slime::IsDead()
{
	return (hp <= 0);
}

// namespace Json definition
string Json::GetSlimeAttackUserJson(int slimeIndex, const string& userID, int slimeStr)
{
	return "{\"text\":\"슬라임" + to_string(slimeIndex) + " 이/가 " + userID + " 을/를 공격해서 데미지 " + to_string(slimeStr) + " 을/를 가했습니다.\"}";
}

string Json::GetUserAttackSlimeJson(const string& userID, int slimeIndex, int userStr)
{
	return "{\"text\":\"" + userID + " 이/가 슬라임" + to_string(slimeIndex) + " 을/를 공격해서 데미지 " + to_string(userStr) + " 을/를 가했습니다.\"}";
}

string Json::GetSlimeDieJson(int slimeIndex, const string& userID)
{
	return "{\"text\":\"슬라임" + to_string(slimeIndex) + " 이/가 " + userID + " 에 의해 죽었습니다.\"}";
}

string Json::GetUserDieJson(const string& userID, int slimeIndex)
{
	return "{\"text\":\"" + userID + " 이/가 슬라임" + to_string(slimeIndex) + " 에 의해 죽었습니다.\",\"userid\":\"" + userID + "\", \"error\":\"" + to_string(int(M_Type::E_DIE)) + "\"}";
}

string Json::GetMoveRespJson(const string& userID)
{
	return "{\"text\":\"(" + Redis::GetLocationX(userID) + "," + Redis::GetLocationY(userID) + ")로 이동했다.\"}";
}

string Json::GetTextOnlyJson(const string& text)
{
	return "{\"text\":\"" + text + "\"}";
}

string Json::GetDupConnectionJson()
{
	return "{\"text\":\"다른 클라이언트에서 동일한 아이디로 로그인했습니다.\",\"error\":\"" + to_string(int(M_Type::E_DUP_CONNECTION)) + "\"}";
}

string Json::GetUsersRespJson(const string& userID)
{
	// 해당 명령 보낸 클라이언트 우선 출력
	string msg = "{\"text\":\"" + userID + " : (" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")\\r\\n";

	// 다른 클라이언트들의 위치
	for (auto& entry : Server::activeClients)
	{
		// 해당 명령 보낸 클라이언트와 아직 로그인안된 클라이언트 제외
		if (entry.second->ID == userID || entry.second->ID == "")
		{
			continue;
		}
		msg += entry.second->ID + " : (" + Redis::GetLocationX(entry.second->ID) + ", " + Redis::GetLocationY(entry.second->ID) + ")\\r\\n";
	}
	msg += "\"}";

	return msg;
}

string Json::GetMonstersRespJson()
{
	string msg = "{\"text\":\"";
	for (auto& slime : Logic::slimes)
	{
		msg += "Slime" + to_string(slime->index) + " : (" + to_string(slime->locX) + ", " + to_string(slime->locY) + ")\\r\\n";
	}
	msg += "\"}";

	return msg;
}

string Json::GetChatRespJson(const string& userID, const string& text)
{
	return "{\"text\":\"" + userID + "(으)로 부터 온 메시지 : " + text + "\"}";
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
