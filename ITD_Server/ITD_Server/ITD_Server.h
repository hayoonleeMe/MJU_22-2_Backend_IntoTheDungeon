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
#include <sstream>
#include <thread>
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace rapidjson;
using namespace std;

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

// Slime ���漱��
class Slime;

// ������ �α��ε� ���� Ŭ���̾�Ʈ
class Client
{
	struct Restful
	{
		bool isRestful;							// RESTful API ���������� ��Ÿ���� ���º���
		string method;							// Http ��û�� method
		string params;							// Http ��û�� �Ķ����
	};

public:
	Client(SOCKET sock);

	~Client();

	// ������ ���ݹ��� �� ȣ��Ǵ� �޼ҵ�
	int OnAttack(const shared_ptr<Slime>& slime);

public:
	struct Restful restful;

	SOCKET sock;								// �� Ŭ���̾�Ʈ�� active socket
	string ID;									// �α��ε� ������ ID

	atomic<bool> doingProc;						// ���� Ŭ���̾�Ʈ�� ó�� �������� ��Ÿ���� atomic ����
	static const int PACKET_SIZE = 65536;		// packet ������ ũ��, 64KB
	char packet[PACKET_SIZE];					// �ִ� 64KB �� ��Ŷ ������ ����
	string sendPacket;							// Ŭ���̾�Ʈ�� ������ ������
	bool sendTurn;								// Ŭ���̾�Ʈ�� ���� �������� ��Ÿ���� ���º���
	bool lenCompleted;							// Ŭ���̾�Ʈ�� ���� �������� ���̸� ��� �޾Ҵ��� ��Ÿ���� ���º���
	int packetLen;								// ����/���� �������� ����
	int offset;									// ������� ����/���� ���� ������
	bool shouldTerminate;						// Ŭ���̾�Ʈ�� ���� ����Ǿ�� �ϴ��� ��Ÿ���� ���º���

	static const int DEFAULT_HP = 30;			// �⺻ hp
	static const int DEFAULT_STR = 3;			// �⺻ str
	static const int DEFAULT_POTION_HP = 1;		// �⺻ hp potion ����
	static const int DEFAULT_POTION_STR = 1;	// �⺻ str potion ����

	static const int MAX_HP = 30;				// �ִ� hp
	static const int MIN_HP = 0;				// �ּ� hp

	static const int MAX_X_ATTACK_RANGE = 1;	// X�� �ִ� ���� ����
	static const int MIN_X_ATTACK_RANGE = -1;	// X�� �ּ� ���ݹ���
	static const int MAX_Y_ATTACK_RANGE = 1;	// Y�� �ִ� ���ݹ���
	static const int MIN_Y_ATTACK_RANGE = -1;	// Y�� �ּ� ���ݹ���
};

// ���� ���� ���� ���ӽ����̽�
namespace Server
{
	static const char* SERVER_ADDRESS = "127.0.0.1";	// ���� �ּ�
	static const unsigned short SERVER_PORT = 27015;	// ���� ��Ʈ
	static const int NUM_WORKER_THREADS = 40;			// ������ ����
	map<SOCKET, shared_ptr<Client>> activeClients;		// ����� Ŭ���̾�Ʈ
	mutex activeClientsMutex;							// activeClients ���ؽ�
	queue<shared_ptr<Client>> jobQueue;					// �۾� ť
	mutex jobQueueMutex;								// jobQueue ���ؽ�
	condition_variable jobQueueFilledCv;				// jobQueue condition_variable

	// ID�� ���� ������ ������ ��� �����Ѵ�.
	void TerminateRemainUser(const string& ID);
}

namespace Rest
{
	static const char* REST_SERVER_ADDRESS = "127.0.0.1";	// ���� �ּ�
	static const unsigned short REST_SERVER_PORT = 27016;	// ���� ��Ʈ

	static const char* GET = "GET";
	static const char* POST = "POST";
	static const char* CONTENT_LENGTH = "Content-Length";

	static const char* RESPONSE_CONTENT_TYPE = "\r\nContent-Type: application/json\r\n\r\n";
	static const char* RESPONSE_STATUS_SUCCESSFUL = "HTTP / 1.1 200 OK\r\nContent-Length: ";
	static const char* RESPONSE_STATUS_FAILED = "HTTP / 1.1 400 Bad Request\r\nContent-Length: ";

	enum class Response_Status
	{
		E_SUCCESSFUL,
		E_FAILED,
	};

	string ProcessHttpCmd(const string& text, shared_ptr<Client>& client);
	string GetHttpResponse(Response_Status rStatus, const string& content);
}

// ���� ���� ���� ���ӽ����̽�
namespace Logic
{
	// Process �Լ����� ����� �����͸� ��Ÿ���� ����ü
	struct ParamsForProc
	{
		string param1;
		string param2;
	};

	typedef function<string(shared_ptr<Client>, Logic::ParamsForProc)> Handler;

	static const int NUM_DUNGEON_X = 30;			// ���� X�� ũ��
	static const int NUM_DUNGEON_Y = 30;			// ���� Y�� ũ��

	static const int SLIME_GEN_PERIOD = 60;			// ������ ���� �ֱ� (��)
	static const int MAX_NUM_OF_SLIME = 10;			// �ִ� ������ ��

	static const int NUM_POTION_TYPE = 2;			// ���� Ÿ�� ��
	static const int HP_POTION_HEAL = 10;			// hp ���� ü�� ������
	static const int STR_POTION_DAMAGE = 3;			// str ���� ���ݷ� ������
	static const int STR_POTION_DURATION = 60;		// str ���� ���ӽð� (��)

	// ���� Ÿ���� ��Ÿ���� enum class
	enum class PotionType
	{
		E_POTION_HP,
		E_POTION_STR,
	};

	map<SOCKET, list<string>> shouldSendPackets;	// ���������ϴ� ����� �޽���
	mutex shouldSendPacketsMutex;					// shouldSendPackets ���ؽ�

	map<string, Handler> handlers;					// Process �Լ� ��

	list<shared_ptr<Slime>> slimes;					// �����ӵ��� �����ϴ� ����Ʈ
	mutex slimesMutex;								// slimes ���ؽ�

	// value�� minValue~maxValue ���̷� �����Ͽ� ��ȯ�ϴ� �Լ�
	int Clamp(int value, int minValue, int maxValue)
	{
		int result = min(max(minValue, value), maxValue);
		return result;
	}

	// message�� ��� Ŭ���̾�Ʈ���� �������� �����ϴ� �Լ�
	void BroadcastToClients(const string& message);

	// move ��ɾ ó���ϴ� �Լ�
	string ProcessMove(const shared_ptr<Client>& client, const ParamsForProc& params);

	// attack ��ɾ ó���ϴ� �Լ�
	string ProcessAttack(const shared_ptr<Client>& client, const ParamsForProc& params);

	// monsters ��ɾ ó���ϴ� �Լ�
	string ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& jobparams);

	// users ��ɾ ó���ϴ� �Լ�
	string ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& params);

	// chat ��ɾ ó���ϴ� �Լ�
	string ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& params);

	// usepotion ��ɾ ó���ϴ� �Լ�
	string ProcessUsePotion(const shared_ptr<Client>& client, const ParamsForProc& params);

	// info ��ɾ ó���ϴ� �Լ�
	string ProcessInfo(const shared_ptr<Client>& client, const ParamsForProc& params);

	// handlers �ʱ�ȭ�ϴ� �Լ�
	void InitHandlers();

	// �������� num��ŭ �����ϴ� �Լ�
	void SpawnSlime(int num);

	// Str ������ ȿ���� �����ϴ� �Լ�
	void StrPotion(const shared_ptr<Client>& client);

	// �������� 5�ʸ��� ������ �� �ִ� �������� ������ �����ϴ� �Լ�
	void SlimeAttackCheck(const shared_ptr<Slime>& slime);

	void Split(const string& text, vector<string>& splited, char delimiter)
	{
		istringstream ss(text);
		string stringBuffer;
		splited.clear();
		while (getline(ss, stringBuffer, delimiter))
		{
			splited.push_back(stringBuffer);
		}
	}

	// trim from left 
	string& Ltrim(string& s, const char* t = " \t\n\r\f\v")
	{
		s.erase(0, s.find_first_not_of(t));
		return s;
	}

	// trim from right 
	string& Rtrim(string& s, const char* t = " \t\n\r\f\v")
	{
		s.erase(s.find_last_not_of(t) + 1);
		return s;
	}

	// trim from left & right 
	string& Trim(string& s, const char* t = " \t\n\r\f\v")
	{
		return Ltrim(Rtrim(s, t), t);
	}

	string convertToJson() {
		char jsonData[8192];
		sprintf_s(jsonData, sizeof(jsonData), "{\"tag\": \"position\", \"x\": %d, \"y\": %d}", 10, 10);
		return jsonData;
	}
}

// ������ Ŭ����
class Slime
{
public:
	Slime();

	// �������� ���ݹ��� �� ȣ��Ǵ� �Լ�
	int OnAttack(const shared_ptr<Client>& client);

	// �������� �׾������� ��ȯ�ϴ� �Լ�
	bool IsDead();

public:
	int index;									// �������� �����ϴ� �ε���
	int locX;									// x�� ��ġ
	int locY;									// y�� ��ġ
	int hp;										// hp, 5~10 ���� ����
	int str;									// str, 3~5 ���� ����
	Logic::PotionType potionType;				// ���ϰ� �ִ� ������ Ÿ��

	static int slimeIndex;						// ������ ������ 1�� �����ϴ� static �ε��� ����
	static const int MAX_X_ATTACK_RANGE = 1;	// x�� �ִ� ���ݹ���
	static const int MIN_X_ATTACK_RANGE = -1;	// x�� �ּ� ���ݹ���
	static const int MAX_Y_ATTACK_RANGE = 1;	// y�� �ִ� ���ݹ���
	static const int MIN_Y_ATTACK_RANGE = -1;	// y�� �ּ� ���ݹ���
	static const int ATTACK_PERIOD = 5;			// ���� �ֱ� (��)
	static const int RAND_MIN_HP = 5;			// hp �ּ� ���� ����
	static const int RAND_MAX_HP = 10;			// hp �ִ� ���� ����
	static const int RAND_MIN_STR = 3;			// str �ּ� ���� ����
	static const int RAND_MAX_STR = 5;			// str �ִ� ���� ����
};
// static ���� slimeIndex �ʱ�ȭ
int Slime::slimeIndex = 0;

// ���� ���� ���ӽ����̽�
namespace Rand
{
	// random ������
	random_device rd;
	mt19937 gen(rd());

	// ���� ���� ���� ����
	uniform_int_distribution<int> locDis(0, Logic::NUM_DUNGEON_X - 1);

	// ������ hp ���� ���� ����
	uniform_int_distribution<int> slimeHpDis(Slime::RAND_MIN_HP, Slime::RAND_MAX_HP);

	// ������ str ���� ���� ����
	uniform_int_distribution<int> slimeStrDis(Slime::RAND_MIN_STR, Slime::RAND_MAX_STR);

	// ���� Ÿ�� ���� ���� ����
	uniform_int_distribution<int> potionTypeDis(0, Logic::NUM_POTION_TYPE - 1);

	// ���� ���� ���� ���� ��ġ ��ȯ
	int GetRandomLoc() 
	{
		return locDis(gen);
	}

	// ������ ���� hp ��ȯ
	int GetRandomSlimeHp()
	{
		return slimeHpDis(gen);
	}

	// ������ ���� str ��ȯ
	int GetRandomSlimeStr()
	{
		return slimeStrDis(gen);
	}

	// ���� ���� Ÿ�� ��ȯ
	int GetRandomPotionType()
	{
		return potionTypeDis(gen);
	}
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
		E_CHAT,
	};

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

	// login success notify
	string GetLoginSuccessJson(const string& text);

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

	// Monsters response
	string GetMonstersRespJson();

	// Users response
	string GetUsersRespJson(const string& userID);

	// Chat response
	string GetChatRespJson(const string& fromUserID, const string& text);

	// Info response
	string GetInfoRespJson(const string& userID);

	// ������ ������ �ֿ� �� ��Ÿ���� �޽����� json �������� ��ȯ�ϴ� �Լ�
	string GetPickUpPotionJson(const string& userID, Logic::PotionType potionType);
}

// redis ���� ���ӽ����̽�
namespace Redis
{
	// hiredis 
	redisContext* redis;
	mutex redisMutex;
	static const int DEFAULT_REDIS_PORT = 6379;

	// ����� �Լ� Ÿ��
	enum class F_Type
	{
		E_ABSOLUTE,
		E_RELATIVE
	};

	// ID�� ������ ���� ��ȯ��
	static const int EXIST_ID = 1;

	static const char* EXPIRE_TIME = "300";			// Ű ���� �ð� (��)
	static const char* LOGINED = "1";				// USER:ID�� �̹� �α��� ���� ���� value ��
	static const char* EXPIRED = "0";				// USER:ID�� Expire ���� ���� value ��

	static const char* LOC_X = ":location:x";		// x�� ��ġ�� redis Ű
	static const char* LOC_Y = ":location:y";		// y�� ��ġ�� redis Ű
	static const char* HP = ":hp";					// hp�� redis Ű
	static const char* STR = ":str";				// str�� redis Ű
	static const char* POTION_HP = ":potions:hp";	// hp ������ redis Ű
	static const char* POTION_STR = ":potions:str";	// str ������ redis Ű

	// flushall
	void Flushall()
	{
		string cmd = "flushall";
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, cmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << cmd << '\n';

		freeReplyObject(reply);
	}

	// get USER:ID
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

		freeReplyObject(reply);

		return "";
	}

	// get USER:ID:location:x
	string GetLocationX(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + LOC_X;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// get USER:ID:location:y
	string GetLocationY(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + LOC_Y;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// get USER:ID:hp
	string GetHp(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + HP;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// get USER:ID:str
	string GetStr(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + STR;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// get USER:ID:potions:hp
	string GetHpPotion(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + POTION_HP;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// get USER:ID:potions:str
	string GetStrPotion(const string& ID)
	{
		string ret = "";
		string getCmd = "GET USER:" + ID + POTION_STR;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, getCmd.c_str());
			if (reply->type == REDIS_REPLY_STRING)
				ret = reply->str;
		}

		freeReplyObject(reply);

		return ret;
	}

	// ttl USER:ID 
	int GetTTL(const string& ID)
	{
		int ret = 0;
		string ttlCmd = "TTL USER:" + ID;
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, ttlCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_INTEGER)
			ret = reply->integer;

		freeReplyObject(reply);

		return ret;
	}

	// set USER:ID status
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

	// set UESR:ID:location:x/y x/y
	// t == E_ABSOLUTE : �ٷ� ����
	// t == E_RELATIVE : ���簪�� �������� ����
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

		redisReply* reply; 
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, setCmd1.c_str());
			if (reply->type == REDIS_REPLY_ERROR)
				cout << "Redis Command Error : " << setCmd1 << '\n';

			reply = (redisReply*)redisCommand(redis, setCmd2.c_str());
			if (reply->type == REDIS_REPLY_ERROR)
				cout << "Redis Command Error : " << setCmd2 << '\n';
		}

		freeReplyObject(reply);
	}
	
	// set USER:ID:hp hp
	// t == E_ABSOLUTE : �ٷ� ����
	// t == E_RELATIVE : ���簪�� �������� ����
	void SetHp(const string& ID, int hp, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == F_Type::E_ABSOLUTE)
		{
			int newHp = Logic::Clamp(hp, Client::MIN_HP, Client::MAX_HP);
			setCmd = "SET USER:" + ID + HP + " " + to_string(newHp);
		}
		else
		{
			int newHp = Logic::Clamp(stoi(GetHp(ID)) + hp, Client::MIN_HP, Client::MAX_HP);
			setCmd = "SET USER:" + ID + HP + " " + to_string(newHp);
		}
		
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	// set USER:ID:str str
	// t == E_ABSOLUTE : �ٷ� ����
	// t == E_RELATIVE : ���簪�� �������� ����
	void SetStr(const string& ID, int str, F_Type t = F_Type::E_ABSOLUTE)
	{
		string setCmd;
		if (t == F_Type::E_ABSOLUTE)
		{
			int newStr = max(0, str);
			setCmd = "SET USER:" + ID + STR + " " + to_string(newStr);
		}
		else
		{
			int newStr = max(0, stoi(GetStr(ID)) + str);
			setCmd = "SET USER:" + ID + STR + " " + to_string(newStr);
		}

		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	// set USER:ID:potions:hp numOfPotion
	// t == E_ABSOLUTE : �ٷ� ����
	// t == E_RELATIVE : ���簪�� �������� ����
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

			reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	// set USER:ID:potions:str numOfPotion
	// t == E_ABSOLUTE : �ٷ� ����
	// t == E_RELATIVE : ���簪�� �������� ����
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

			reply = (redisReply*)redisCommand(redis, setCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << setCmd << '\n';

		freeReplyObject(reply);
	}

	// expire specific key
	void ExpireKey(const string& ID, const string& key, int expire_time)
	{
		string expireCmd = "EXPIRE USER:" + ID + key + " " + to_string(expire_time);
		redisReply* reply;
		{
			lock_guard<mutex> lg(redisMutex);

			reply = (redisReply*)redisCommand(redis, expireCmd.c_str());
		}

		if (reply->type == REDIS_REPLY_ERROR)
			cout << "Redis Command Error : " << expireCmd << '\n';

		freeReplyObject(reply); 
	}

	// expire all key
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
	string RegisterUser(const string& ID, const SOCKET& sock)
	{
		// ��ȯ�� ���ڿ�
		string ret;

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
				if (GetUserConnection(ID) == LOGINED)
				{
					cout << "[�ý���] " << ID + " �̹� �α��� �Ǿ� ����\n";
					string raw = "������ ������ �����ϰ� �α����߽��ϴ�.\\r\\n" + ID + " ���� �����߽��ϴ�.";
					ret = Json::GetLoginSuccessJson(raw);

					// ���� ���ӵ��� ���� �����Ѵ�.
					Server::TerminateRemainUser(ID);
				}
				// ���� �� 5���� ������ ���� ������
				else if (GetUserConnection(ID) == EXPIRED)
				{
					cout << "[�ý���] " << ID + " ��������\n";
					string raw = "�α��ο� �����߽��ϴ�.\\r\\n" + ID + " ���� �����߽��ϴ�.";
					ret = Json::GetLoginSuccessJson(raw);

					// ���� �����Ͽ� expire�� Ű�� �����Ѵ�.
					PersistUser(ID);
				}
			}
			// USER:ID�� �������� ���� ��
			else
			{
				cout << "[�ý���] [" << sock << "] New User : " << ID << '\n';
				string raw = "�α��ο� �����߽��ϴ�.\\r\\n" + ID + " ���� �����߽��ϴ�.";
				ret = Json::GetLoginSuccessJson(raw);

				// redis�� ���
				SetUserConnection(ID, LOGINED);
				SetLocation(ID, Rand::GetRandomLoc(), Rand::GetRandomLoc());
				SetHp(ID, Client::DEFAULT_HP);
				SetStr(ID, Client::DEFAULT_STR);
				SetHpPotion(ID, Client::DEFAULT_POTION_HP);
				SetStrPotion(ID, Client::DEFAULT_POTION_STR);
			}
		}

		freeReplyObject(reply);

		return ret;
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
	// ������ ���ӵ� Ŭ���̾�Ʈ�� ã�´�.
	for (auto& entry : Server::activeClients)
	{
		if (entry.second->ID == ID)
		{
			entry.second->shouldTerminate = true;

			// ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ְ�,
			// �ߺ� �α��ο� ���� ���� ����Ǵ� ���� �˸��� �޽����� ���������� �����ϱ� ���� �޽����� �����Ѵ�.
			Logic::shouldSendPackets[entry.first].clear();

			Logic::shouldSendPackets[entry.first].push_back(Json::GetDupConnectionJson());
		}
	}
}


// class Client Definition
Client::Client(SOCKET sock) : sock(sock), sendTurn(false), doingProc(false), lenCompleted(false), packetLen(0), offset(0), ID(""), sendPacket(""), shouldTerminate(false)
{
	memset(packet, 0, Client::PACKET_SIZE);
	restful = { false };
}

Client::~Client()
{
	cout << "[" << sock << "] Client destroyed\n";
}

int Client::OnAttack(const shared_ptr<Slime>& slime)
{
	Redis::SetHp(ID, -1 * slime->str, Redis::F_Type::E_RELATIVE);

	cout << "[" << sock << "] " << ID << " is On Attack by slime" << slime->index << ", remain hp : " << stoi(Redis::GetHp(ID)) << '\n';

	int nowHp = stoi(Redis::GetHp(ID));

	return nowHp;
}

// namespace Rest Definition
string Rest::ProcessHttpCmd(const string& text, shared_ptr<Client>& client)
{
	// attack, monsters, users
	if (client->restful.method == GET)
	{
		if (client->ID == "")
		{
			return GetHttpResponse(Response_Status::E_FAILED, "�α����� ���� �ؾ��մϴ�.");
		}
		else
		{
			if (text == Json::ATTACK)
			{
				return GetHttpResponse(Response_Status::E_SUCCESSFUL, Logic::ProcessAttack(client, {}));
			}
			else if (text == Json::MONSTERS)
			{
				return GetHttpResponse(Response_Status::E_SUCCESSFUL, Logic::ProcessMonsters(client, {}));
			}
			else if (text == Json::USERS)
			{
				return GetHttpResponse(Response_Status::E_SUCCESSFUL, Logic::ProcessUsers(client, {}));
			}
			else
			{
				return GetHttpResponse(Response_Status::E_FAILED, "�߸��� method�Դϴ�.");
			}
		}
	}
	// login, move
	else if (client->restful.method == POST)
	{
		string body = client->packet;

		if (text == Json::LOGIN)
		{
			// �̹� �α��� ���� ��
			if (client->ID != "")
			{
				return GetHttpResponse(Response_Status::E_FAILED, "�̹� �α��ο� �����߽��ϴ�.");
			}
			else
			{
				// ���� ��û (id = ID)
				if (body.find('?') == string::npos)
				{
					vector<string> idSplited;
					Logic::Split(body, idSplited, '=');
					string ID = idSplited[1];

					string content = Redis::RegisterUser(ID, client->sock);

					if (client->ID == "")
						client->ID = ID;

					return GetHttpResponse(Response_Status::E_SUCCESSFUL, content);
				}
				// �߸��� ��û
				else
				{
					return GetHttpResponse(Response_Status::E_FAILED, "Body�� key-value�� �߸��Ǿ����ϴ�.");
				}
			}
		}
		else if (text == Json::MOVE)
		{
			// �α��� �ʿ�
			if (client->ID == "")
			{
				return GetHttpResponse(Response_Status::E_FAILED, "�α����� ���� �ؾ��մϴ�.");
			}
			// �α��� �Ϸ� ��
			else
			{
				// ���� ��û (x = X, y = Y)
				if (body.find('?') != string::npos)
				{
					vector<string> bodySplited;
					Logic::Split(body, bodySplited, '?');

					if (bodySplited.size() != 2)
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body�� key-value�� �߸��Ǿ����ϴ�.");
					}

					// x ��ǥ ����
					vector<string> xSplited;
					Logic::Split(bodySplited[0], xSplited, '=');

					// �߸��� ��û
					if (xSplited[0] != "x")
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body�� key-value�� �߸��Ǿ����ϴ�.");
					}

					string x = xSplited[1];

					// y ��ǥ ����
					vector<string> ySplited;
					Logic::Split(bodySplited[1], ySplited, '=');

					// �߸��� ��û
					if (ySplited[0] != "y")
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body�� key-value�� �߸��Ǿ����ϴ�.");
					}

					string y = ySplited[1];

					return GetHttpResponse(Response_Status::E_SUCCESSFUL, Logic::ProcessMove(client, { x, y }));
				}
				// �߸��� ��û
				else
				{
					return GetHttpResponse(Response_Status::E_FAILED, "Body�� key-value�� �߸��Ǿ����ϴ�.");
				}
			}
		}
		else
		{
			return GetHttpResponse(Response_Status::E_FAILED, "�߸��� method�Դϴ�.");
		}
	}
}

string Rest::GetHttpResponse(Response_Status rStatus, const string& content)
{
	if (rStatus == Response_Status::E_SUCCESSFUL)
	{
		return RESPONSE_STATUS_SUCCESSFUL + to_string(content.length()) + RESPONSE_CONTENT_TYPE + content;
	}

	return RESPONSE_STATUS_FAILED + to_string(content.length()) + RESPONSE_CONTENT_TYPE + content;
}


// namespace Logic Definition
void Logic::BroadcastToClients(const string& message)
{
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		for (auto& entry : Server::activeClients)
		{
			if (entry.second->shouldTerminate)
			{
				continue;
			}

			shouldSendPackets[entry.first].push_back(message);
		}
	}
}

string Logic::ProcessMove(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	Redis::SetLocation(client->ID, stoi(params.param1), stoi(params.param2), Redis::F_Type::E_RELATIVE);

	return Json::GetMoveRespJson(client->ID);
}

string Logic::ProcessAttack(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	string ret = "";			// ��ȯ�� ���ڿ�
	bool canAttack = false;		// ������ �� �ִ���

	// ������ ����
	int userLocX = stoi(Redis::GetLocationX(client->ID));
	int userLocY = stoi(Redis::GetLocationY(client->ID));
	int userStr = stoi(Redis::GetStr(client->ID));

	// ���� �������� ���ͷ����͸� �����ϴ� ����Ʈ
	list<list<shared_ptr<Slime>>::iterator> deadSlimeIts;
	for (list<shared_ptr<Slime>>::iterator it = slimes.begin(); it != slimes.end(); ++it)
	{
		// �̹� ���� �������̸� ��ŵ
		if ((*it)->IsDead())
			continue;

		// �������� ����
		int slimeLocX = (*it)->locX;
		int slimeLocY = (*it)->locY;

		// ������ ���� ���� �ȿ� �������� ������
		if ((userLocX - slimeLocX <= Client::MAX_X_ATTACK_RANGE && userLocX - slimeLocX >= Client::MIN_X_ATTACK_RANGE) &&
			(userLocY - slimeLocY <= Client::MAX_Y_ATTACK_RANGE && userLocY - slimeLocY >= Client::MIN_Y_ATTACK_RANGE))
		{
			// ���� ����
			canAttack = true;

			// ���� �޽����� ��ε�ĳ��Ʈ�Ѵ�.
			string msg = Json::GetUserAttackSlimeJson(client->ID, (*it)->index, userStr);
			BroadcastToClients(msg);

			if (client->restful.isRestful)
			{
				ret = msg;
			}

			// ������ �������� �������� �׾����� ������ ���� deadSlimes ����Ʈ�� �ִ´�.
			if ((*it)->OnAttack(client) <= 0)
			{
				// ������ ��� �޽����� ��ε�ĳ��Ʈ�Ѵ�.
				deadSlimeIts.push_back(it);
				string dieMsg = Json::GetSlimeDieJson((*it)->index, client->ID);
				BroadcastToClients(dieMsg);
			}
		}
	}

	// ���� �������� ���ش�.
	{
		lock_guard<mutex> lg(slimesMutex);

		for (auto& deadSlimeIt : deadSlimeIts)
		{
			// ���� �������� ������ �ִ� ������ ������ ȹ���Ѵ�.
			if ((*deadSlimeIt)->potionType == PotionType::E_POTION_HP)
			{
				Redis::SetHpPotion(client->ID, 1, Redis::F_Type::E_RELATIVE);
			}
			else if ((*deadSlimeIt)->potionType == PotionType::E_POTION_STR)
			{
				Redis::SetStrPotion(client->ID, 1, Redis::F_Type::E_RELATIVE);
			}

			if (!client->shouldTerminate)
			{
				lock_guard<mutex> lg(shouldSendPacketsMutex);

				shouldSendPackets[client->sock].push_back(Json::GetPickUpPotionJson(client->ID, (*deadSlimeIt)->potionType));
			}

			slimes.erase(deadSlimeIt);
		}
	}

	// ������ ����� ���ٸ� Ŭ���̾�Ʈ���� �˸���.
	if (!canAttack)
		ret = Json::GetTextOnlyJson("������ ����� �����ϴ�.");

	return ret;
}

string Logic::ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	return Json::GetMonstersRespJson();
}

string Logic::ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	// Ŭ���̾�Ʈ�� ��ġ
	return Json::GetUsersRespJson(client->ID);
}

string Logic::ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	SOCKET toSock = -1;

	// ������ ã��
	for (auto& entry : Server::activeClients)
		if (entry.second->ID == params.param1)
		{
			toSock = entry.first;
			break;
		}

	// ������ �������� ���� �� ��ȯ��
	if (toSock == -1)
	{
		string raw = params.param1 + " ��/�� �������� �ʴ� �����Դϴ�.";
		return Json::GetTextOnlyJson(raw);
	}

	// ������ ������ ��
	if (!client->shouldTerminate)
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		shouldSendPackets[toSock].push_back(Json::GetChatRespJson(client->ID, params.param2));
	}

	string raw = params.param1 + " ���� �ӼӸ��� �����߽��ϴ�.";
	return Json::GetTextOnlyJson(raw);
}

string Logic::ProcessUsePotion(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	string ret = "";

	// hp potion ���
	if (params.param1 == Json::HP_POTION)
	{
		int numOfPotion = stoi(Redis::GetHpPotion(client->ID));

		// hp potion�� ������ ��
		if (numOfPotion > 0)
		{
			// ü�� ����
			Redis::SetHp(client->ID, HP_POTION_HEAL, Redis::F_Type::E_RELATIVE);

			// hp ���� ���� ����
			Redis::SetHpPotion(client->ID, -1, Redis::F_Type::E_RELATIVE);

			string raw = "HP ������ 1�� ����� ü���� " + to_string(Logic::HP_POTION_HEAL) + "��ŭ �����߽��ϴ�.";
			ret = Json::GetTextOnlyJson(raw);
		}
		// hp potion�� �������� ���� ��
		else
		{
			ret = Json::GetTextOnlyJson("HP ������ �������� �ʽ��ϴ�.");
		}
	}
	// str potion ���
	else if (params.param1 == Json::STR_POTION)
	{
		int numOfPotion = stoi(Redis::GetStrPotion(client->ID));

		// str potion�� ������ ��
		if (numOfPotion > 0)
		{
			// ���ӽð����� ���ݷ��� ������Ű�� StrPotion �Լ��� ȣ���ϴ� ������ ����
			shared_ptr<thread> strPotionThread(new thread(StrPotion, client));
			strPotionThread->detach();

			// str ���� ���� ����
			Redis::SetStrPotion(client->ID, -1, Redis::F_Type::E_RELATIVE);

			string raw = "STR ���� 1���� ����� " + to_string(Logic::STR_POTION_DURATION) + "�� �� ���ݷ��� " + to_string(Logic::STR_POTION_DAMAGE) + "��ŭ �����մϴ�.";
			ret = Json::GetTextOnlyJson(raw);
		}
		// str potion�� �������� ���� ��
		else
		{
			ret = Json::GetTextOnlyJson("STR ������ �������� �ʽ��ϴ�.");
		}
	}

	return ret;
}

string Logic::ProcessInfo(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	return Json::GetInfoRespJson(client->ID);
}

void Logic::InitHandlers()
{
	handlers[Json::MOVE] = ProcessMove;
	handlers[Json::ATTACK] = ProcessAttack;
	handlers[Json::MONSTERS] = ProcessMonsters;
	handlers[Json::USERS] = ProcessUsers;
	handlers[Json::CHAT] = ProcessChat;
	handlers[Json::USE_POTION] = ProcessUsePotion;
	handlers[Json::INFO] = ProcessInfo;
}

void Logic::SpawnSlime(int num)
{
	if (num <= 0)
		return;

	{
		lock_guard<mutex> lg(slimesMutex);

		for (int i = 0; i < num; ++i)
		{
			shared_ptr<Slime> slime(new Slime());
			slimes.push_back(slime);
			shared_ptr<thread> slimeAttackThread(new thread(SlimeAttackCheck, slime));
			slimeAttackThread->detach();
		}
	}
}

void Logic::StrPotion(const shared_ptr<Client>& client)
{
	// str ����
	Redis::SetStr(client->ID, STR_POTION_DAMAGE, Redis::F_Type::E_RELATIVE);

	// ���ӽð� ���� ���
	this_thread::sleep_for(chrono::seconds(STR_POTION_DURATION));

	// ������ ��ŭ str ����
	Redis::SetStr(client->ID, -1 * STR_POTION_DAMAGE, Redis::F_Type::E_RELATIVE);

	// ���� ������ �������� ���¶�� ���� �޽��� ����
	if (Redis::GetUserConnection(client->ID) == Redis::LOGINED)
	{
		// str ������ ����� Ŭ���̾�Ʈ�� ���� �����ߴٰ� �ٽ� ���������� ���� ���ϰ� �ٸ� �� �ֱ� ������ ���� ���� Ŭ���̾�Ʈ �߿��� ã�´�.
		for (auto& entry : Server::activeClients)
		{
			if (entry.second->shouldTerminate == false && entry.second->ID == client->ID)
			{
				// ã�� Ŭ���̾�Ʈ���� ���� �޽��� ����
				{
					lock_guard<mutex> lg(shouldSendPacketsMutex);

					shouldSendPackets[entry.first].push_back(Json::GetTextOnlyJson("Str ���� ȿ���� ����Ǿ����ϴ�."));
				}
				break;
			}
		}
	}
	// ȿ���� ������ ���� ������ ������ Ű�� Expire �� ������ ��
	else if (Redis::GetUserConnection(client->ID) == Redis::EXPIRED)
	{
		// Expire�� ���¿��� str�� ���ҽ��ױ� ������ Persist ���°� �ȴ�.
		// ���� strŰ�� �ٸ� Ű�� ���� ����ð�(TTL)��ŭ �ٽ� Expire�Ѵ�.
		Redis::ExpireKey(client->ID , Redis::STR, Redis::GetTTL(client->ID));
	}
}

void Logic::SlimeAttackCheck(const shared_ptr<Slime>& slime)
{
	while (true)
	{
		if (slime->IsDead() || slime == nullptr)
		{
			return;
		}

		int slimeLocX = slime->locX;
		int slimeLocY = slime->locY;
		int slimeStr = slime->str;

		for (auto& entry : Server::activeClients)
		{
			// ���� �α��ε��� ���� Ŭ���̾�Ʈ��� ��ŵ
			if (entry.second->ID == "")
				continue;

			// ���������ϴ� Ŭ���̾�Ʈ�� ��ŵ
			if (entry.second->shouldTerminate)
				continue;

			int userLocX = stoi(Redis::GetLocationX(entry.second->ID));
			int userLocY = stoi(Redis::GetLocationY(entry.second->ID));

			// �������� ���� ���� �ȿ� ������ ������
			if ((slimeLocX - userLocX <= Slime::MAX_X_ATTACK_RANGE && slimeLocX - userLocX >= Slime::MIN_X_ATTACK_RANGE) &&
				(slimeLocY - userLocY <= Slime::MAX_Y_ATTACK_RANGE && slimeLocY - userLocY >= Slime::MIN_Y_ATTACK_RANGE))
			{
				Logic::BroadcastToClients(Json::GetSlimeAttackUserJson(slime->index, entry.second->ID, slime->str));

				// ���� ���� Ŭ���̾�Ʈ�� hp�� 0�� ��
				if (entry.second->OnAttack(slime) <= 0)
				{
					Logic::BroadcastToClients(Json::GetUserDieJson(entry.second->ID, slime->index));
					entry.second->shouldTerminate = true;
				}
			}
		}

		// �������� ���� �ֱ⸸ŭ sleep
		this_thread::sleep_for(chrono::seconds(Slime::ATTACK_PERIOD));
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
	cout << "[�ý���] ������" << index << " is On Attack by " << client->ID << '\n';

	int userStr = stoi(Redis::GetStr(client->ID));
	hp = Logic::Clamp(hp - userStr, 0, RAND_MAX_HP);

	return hp;
}

bool Slime::IsDead()
{
	return (hp <= 0);
}

// namespace Json definition
string Json::GetLoginSuccessJson(const string& text)
{
	return "{\"" + string(TEXT) + "\":\"" + text + "\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_LOGIN_SUCCESS)) + "\"}";
}

string Json::GetSlimeAttackUserJson(int slimeIndex, const string& userID, int slimeStr)
{
	return "{\"" + string(TEXT) + "\":\"������" + to_string(slimeIndex) + " ��/�� " + userID + " ��/�� �����ؼ� ������ " + to_string(slimeStr) + " ��/�� ���߽��ϴ�.\"}";
}

string Json::GetUserAttackSlimeJson(const string& userID, int slimeIndex, int userStr)
{
	return "{\"" + string(TEXT) + "\":\"" + userID + " ��/�� ������" + to_string(slimeIndex) + " ��/�� �����ؼ� ������ " + to_string(userStr) + " ��/�� ���߽��ϴ�.\"}";
}

string Json::GetSlimeDieJson(int slimeIndex, const string& userID)
{
	return "{\"" + string(TEXT) + "\":\"������" + to_string(slimeIndex) + " ��/�� " + userID + " �� ���� �׾����ϴ�.\"}";
}

string Json::GetUserDieJson(const string& userID, int slimeIndex)
{
	return "{\"" + string(TEXT) + "\":\"" + userID + " ��/�� ������" + to_string(slimeIndex) + " �� ���� �׾����ϴ�.\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_DIE)) + "\",\"" + string(PARAM2) + "\":\"" + userID + "\"}";
}

string Json::GetMoveRespJson(const string& userID)
{
	return "{\"" + string(TEXT) + "\":\"(" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")�� �̵��߽��ϴ�.\"}";
}

string Json::GetTextOnlyJson(const string& text)
{
	return "{\"" + string(TEXT) + "\":\"" + text + "\"}";
}

string Json::GetDupConnectionJson()
{
	return "{\"" + string(TEXT) + "\":\"�ٸ� Ŭ���̾�Ʈ���� ������ ���̵�� �α����߽��ϴ�.\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_DUP_CONNECTION)) + "\"}";
}

string Json::GetUsersRespJson(const string& userID)
{
	// �ش� ��� ���� Ŭ���̾�Ʈ �켱 ���
	string msg = "{\"" + string(TEXT) + "\":\"���� ��ġ����\\r\\n" + userID + " : (" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")";

	// �ٸ� Ŭ���̾�Ʈ���� ��ġ
	for (auto& entry : Server::activeClients)
	{
		// �ش� ��� ���� Ŭ���̾�Ʈ�� ���� �α��ξȵ� Ŭ���̾�Ʈ ����
		if (entry.second->ID == userID || entry.second->ID == "")
		{
			continue;
		}
		msg += "\\r\\n" + entry.second->ID + " : (" + Redis::GetLocationX(entry.second->ID) + ", " + Redis::GetLocationY(entry.second->ID) + ")";
	}
	msg += "\"}";

	return msg;
}

string Json::GetMonstersRespJson()
{
	string msg = "{\"" + string(TEXT) + "\":\"������ ��ġ����";

	for (list<shared_ptr<Slime>>::iterator it = Logic::slimes.begin(); it != Logic::slimes.end(); ++it)
	//for (auto& slime : Logic::slimes)
	{
		msg += "\\r\\n������" + to_string((*it)->index) + " : (" + to_string((*it)->locX) + ", " + to_string((*it)->locY) + ")";
	}
	msg += "\"}";

	return msg;
}

string Json::GetChatRespJson(const string& fromUserID, const string& text)
{
	return "{\"" + string(TEXT) + "\":\"" + fromUserID + " (��)�� ���� �� �޽��� : " + text + "\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_CHAT)) + "\"}";
}

string Json::GetInfoRespJson(const string& userID)
{
	string ret = "{\"" + string(TEXT) + "\":\"���� ����\\r\\n���� ID : " + userID + "\\r\\n���� ��ǥ : (" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")\\r\\n���� HP : " + Redis::GetHp(userID) + "\\r\\n���� STR : " + Redis::GetStr(userID) + "\\r\\n���� HP ���� ���� : " + Redis::GetHpPotion(userID) + "\\r\\n���� STR ���� ���� : " + Redis::GetStrPotion(userID) + "\"}";

	return ret;
}

string Json::GetPickUpPotionJson(const string& userID, Logic::PotionType potionType)
{
	string ret = "{\"" + string(TEXT) + "\":\"";
	if (potionType == Logic::PotionType::E_POTION_HP)
	{
		ret += "HP ������ ȹ���߽��ϴ�.\"}";
	}
	else if (potionType == Logic::PotionType::E_POTION_STR)
	{
		ret += "STR ������ ȹ���߽��ϴ�.\"}";
	}

	return ret;
}