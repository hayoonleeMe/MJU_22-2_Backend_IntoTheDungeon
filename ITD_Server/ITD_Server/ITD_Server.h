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

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

// Slime 전방선언
class Slime;

// 서버로 로그인된 유저 클라이언트
class Client
{
	struct Restful
	{
		bool isRestful;							// RESTful API 연결인지를 나타내는 상태변수
		string method;							// Http 요청의 method
		string params;							// Http 요청의 파라미터
	};

public:
	Client(SOCKET sock);

	~Client();

	// 유저가 공격받을 때 호출되는 메소드
	int OnAttack(const shared_ptr<Slime>& slime);

public:
	struct Restful restful;

	SOCKET sock;								// 이 클라이언트의 active socket
	string ID;									// 로그인된 유저의 ID

	atomic<bool> doingProc;						// 현재 클라이언트가 처리 중인지를 나타내는 atomic 변수
	static const int PACKET_SIZE = 65536;		// packet 버퍼의 크기, 64KB
	char packet[PACKET_SIZE];					// 최대 64KB 로 패킷 사이즈 고정
	string sendPacket;							// 클라이언트로 보내질 데이터
	bool sendTurn;								// 클라이언트로 보낼 차례인지 나타내는 상태변수
	bool lenCompleted;							// 클라이언트가 보낼 데이터의 길이를 모두 받았는지 나타내는 상태변수
	int packetLen;								// 받은/보낼 데이터의 길이
	int offset;									// 현재까지 받은/보낸 길이 오프셋
	bool shouldTerminate;						// 클라이언트가 접속 종료되어야 하는지 나타내는 상태변수

	static const int DEFAULT_HP = 30;			// 기본 hp
	static const int DEFAULT_STR = 3;			// 기본 str
	static const int DEFAULT_POTION_HP = 1;		// 기본 hp potion 개수
	static const int DEFAULT_POTION_STR = 1;	// 기본 str potion 개수

	static const int MAX_HP = 30;				// 최대 hp
	static const int MIN_HP = 0;				// 최소 hp

	static const int MAX_X_ATTACK_RANGE = 1;	// X축 최대 공격 범위
	static const int MIN_X_ATTACK_RANGE = -1;	// X축 최소 공격범위
	static const int MAX_Y_ATTACK_RANGE = 1;	// Y축 최대 공격범위
	static const int MIN_Y_ATTACK_RANGE = -1;	// Y축 최소 공격범위
};

// 서버 구동 관련 네임스페이스
namespace Server
{
	static const char* SERVER_ADDRESS = "127.0.0.1";	// 서버 주소
	static const unsigned short SERVER_PORT = 27015;	// 서버 포트
	static const int NUM_WORKER_THREADS = 40;			// 쓰레드 개수
	map<SOCKET, shared_ptr<Client>> activeClients;		// 연결된 클라이언트
	mutex activeClientsMutex;							// activeClients 뮤텍스
	queue<shared_ptr<Client>> jobQueue;					// 작업 큐
	mutex jobQueueMutex;								// jobQueue 뮤텍스
	condition_variable jobQueueFilledCv;				// jobQueue condition_variable

	// ID를 가진 기존의 접속을 모두 종료한다.
	void TerminateRemainUser(const string& ID);
}

namespace Rest
{
	static const char* REST_SERVER_ADDRESS = "127.0.0.1";	// 서버 주소
	static const unsigned short REST_SERVER_PORT = 27016;	// 서버 포트

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

	static const int NUM_DUNGEON_X = 30;			// 던전 X축 크기
	static const int NUM_DUNGEON_Y = 30;			// 던전 Y축 크기

	static const int SLIME_GEN_PERIOD = 60;			// 슬라임 생성 주기 (초)
	static const int MAX_NUM_OF_SLIME = 10;			// 최대 슬라임 수

	static const int NUM_POTION_TYPE = 2;			// 포션 타입 수
	static const int HP_POTION_HEAL = 10;			// hp 포션 체력 증가량
	static const int STR_POTION_DAMAGE = 3;			// str 포션 공격력 증가량
	static const int STR_POTION_DURATION = 60;		// str 포션 지속시간 (초)

	// 포션 타입을 나타내는 enum class
	enum class PotionType
	{
		E_POTION_HP,
		E_POTION_STR,
	};

	map<SOCKET, list<string>> shouldSendPackets;	// 보내져야하는 예약된 메시지
	mutex shouldSendPacketsMutex;					// shouldSendPackets 뮤텍스

	map<string, Handler> handlers;					// Process 함수 맵

	list<shared_ptr<Slime>> slimes;					// 슬라임들을 저장하는 리스트
	mutex slimesMutex;								// slimes 뮤텍스

	// value를 minValue~maxValue 사이로 조정하여 반환하는 함수
	int Clamp(int value, int minValue, int maxValue)
	{
		int result = min(max(minValue, value), maxValue);
		return result;
	}

	// message를 모든 클라이언트에게 보내도록 예약하는 함수
	void BroadcastToClients(const string& message);

	// move 명령어를 처리하는 함수
	string ProcessMove(const shared_ptr<Client>& client, const ParamsForProc& params);

	// attack 명령어를 처리하는 함수
	string ProcessAttack(const shared_ptr<Client>& client, const ParamsForProc& params);

	// monsters 명령어를 처리하는 함수
	string ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& jobparams);

	// users 명령어를 처리하는 함수
	string ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& params);

	// chat 명령어를 처리하는 함수
	string ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& params);

	// usepotion 명령어를 처리하는 함수
	string ProcessUsePotion(const shared_ptr<Client>& client, const ParamsForProc& params);

	// info 명령어를 처리하는 함수
	string ProcessInfo(const shared_ptr<Client>& client, const ParamsForProc& params);

	// handlers 초기화하는 함수
	void InitHandlers();

	// 슬라임을 num만큼 스폰하는 함수
	void SpawnSlime(int num);

	// Str 포션의 효과를 구현하는 함수
	void StrPotion(const shared_ptr<Client>& client);

	// 슬라임이 5초마다 공격할 수 있는 유저에게 공격을 수행하는 함수
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

// 슬라임 클래스
class Slime
{
public:
	Slime();

	// 슬라임이 공격받을 때 호출되는 함수
	int OnAttack(const shared_ptr<Client>& client);

	// 슬라임이 죽었는지를 반환하는 함수
	bool IsDead();

public:
	int index;									// 슬라임을 구별하는 인덱스
	int locX;									// x축 위치
	int locY;									// y축 위치
	int hp;										// hp, 5~10 사이 랜덤
	int str;									// str, 3~5 사이 랜덤
	Logic::PotionType potionType;				// 지니고 있는 포션의 타입

	static int slimeIndex;						// 생성될 때마다 1씩 증가하는 static 인덱스 변수
	static const int MAX_X_ATTACK_RANGE = 1;	// x축 최대 공격범위
	static const int MIN_X_ATTACK_RANGE = -1;	// x축 최소 공격범위
	static const int MAX_Y_ATTACK_RANGE = 1;	// y축 최대 공격범위
	static const int MIN_Y_ATTACK_RANGE = -1;	// y축 최소 공격범위
	static const int ATTACK_PERIOD = 5;			// 공격 주기 (초)
	static const int RAND_MIN_HP = 5;			// hp 최소 랜덤 범위
	static const int RAND_MAX_HP = 10;			// hp 최대 랜덤 범위
	static const int RAND_MIN_STR = 3;			// str 최소 랜덤 범위
	static const int RAND_MAX_STR = 5;			// str 최대 랜덤 범위
};
// static 변수 slimeIndex 초기화
int Slime::slimeIndex = 0;

// 난수 관련 네임스페이스
namespace Rand
{
	// random 변수들
	random_device rd;
	mt19937 gen(rd());

	// 던전 범위 내의 정수
	uniform_int_distribution<int> locDis(0, Logic::NUM_DUNGEON_X - 1);

	// 슬라임 hp 범위 내의 정수
	uniform_int_distribution<int> slimeHpDis(Slime::RAND_MIN_HP, Slime::RAND_MAX_HP);

	// 슬라임 str 범위 내의 정수
	uniform_int_distribution<int> slimeStrDis(Slime::RAND_MIN_STR, Slime::RAND_MAX_STR);

	// 포션 타입 범위 내의 정수
	uniform_int_distribution<int> potionTypeDis(0, Logic::NUM_POTION_TYPE - 1);

	// 던전 범위 내의 랜덤 위치 반환
	int GetRandomLoc() 
	{
		return locDis(gen);
	}

	// 슬라임 랜덤 hp 반환
	int GetRandomSlimeHp()
	{
		return slimeHpDis(gen);
	}

	// 슬라임 랜덤 str 반환
	int GetRandomSlimeStr()
	{
		return slimeStrDis(gen);
	}

	// 랜덤 포션 타입 반환
	int GetRandomPotionType()
	{
		return potionTypeDis(gen);
	}
}

/// <summary> json 관련 네임스페이스
/// json 문자열의 첫번째 키는 TEXT, 이후 키들은 순서대로 PARAM1, PARAM2이다.
/// 메시지 타입이 있는 경우 두번째 키인 PARAM1의 value로 제공된다.
/// </summary>
namespace Json
{
	// 메시지 타입을 나타내는 enum class
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

	// 유저가 포션을 주울 때 나타나는 메시지를 json 형식으로 변환하는 함수
	string GetPickUpPotionJson(const string& userID, Logic::PotionType potionType);
}

// redis 관련 네임스페이스
namespace Redis
{
	// hiredis 
	redisContext* redis;
	mutex redisMutex;
	static const int DEFAULT_REDIS_PORT = 6379;

	// 사용할 함수 타입
	enum class F_Type
	{
		E_ABSOLUTE,
		E_RELATIVE
	};

	// ID가 존재할 때의 반환값
	static const int EXIST_ID = 1;

	static const char* EXPIRE_TIME = "300";			// 키 만료 시간 (초)
	static const char* LOGINED = "1";				// USER:ID의 이미 로그인 중일 때의 value 값
	static const char* EXPIRED = "0";				// USER:ID의 Expire 됐을 때의 value 값

	static const char* LOC_X = ":location:x";		// x축 위치의 redis 키
	static const char* LOC_Y = ":location:y";		// y축 위치의 redis 키
	static const char* HP = ":hp";					// hp의 redis 키
	static const char* STR = ":str";				// str의 redis 키
	static const char* POTION_HP = ":potions:hp";	// hp 포션의 redis 키
	static const char* POTION_STR = ":potions:str";	// str 포션의 redis 키

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
	// t == E_ABSOLUTE : 바로 세팅
	// t == E_RELATIVE : 현재값을 기준으로 세팅
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
	// t == E_ABSOLUTE : 바로 세팅
	// t == E_RELATIVE : 현재값을 기준으로 세팅
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
	// t == E_ABSOLUTE : 바로 세팅
	// t == E_RELATIVE : 현재값을 기준으로 세팅
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
	// t == E_ABSOLUTE : 바로 세팅
	// t == E_RELATIVE : 현재값을 기준으로 세팅
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
	// t == E_ABSOLUTE : 바로 세팅
	// t == E_RELATIVE : 현재값을 기준으로 세팅
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
	string RegisterUser(const string& ID, const SOCKET& sock)
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
				if (GetUserConnection(ID) == LOGINED)
				{
					cout << "[시스템] " << ID + " 이미 로그인 되어 있음\n";
					string raw = "기존의 접속을 종료하고 로그인했습니다.\\r\\n" + ID + " 님이 접속했습니다.";
					ret = Json::GetLoginSuccessJson(raw);

					// 기존 접속들을 강제 종료한다.
					Server::TerminateRemainUser(ID);
				}
				// 종료 후 5분이 지나기 전에 재접속
				else if (GetUserConnection(ID) == EXPIRED)
				{
					cout << "[시스템] " << ID + " 재접속함\n";
					string raw = "로그인에 성공했습니다.\\r\\n" + ID + " 님이 접속했습니다.";
					ret = Json::GetLoginSuccessJson(raw);

					// 접속 종료하여 expire된 키를 복구한다.
					PersistUser(ID);
				}
			}
			// USER:ID가 존재하지 않을 때
			else
			{
				cout << "[시스템] [" << sock << "] New User : " << ID << '\n';
				string raw = "로그인에 성공했습니다.\\r\\n" + ID + " 님이 접속했습니다.";
				ret = Json::GetLoginSuccessJson(raw);

				// redis에 등록
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
			return GetHttpResponse(Response_Status::E_FAILED, "로그인을 먼저 해야합니다.");
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
				return GetHttpResponse(Response_Status::E_FAILED, "잘못된 method입니다.");
			}
		}
	}
	// login, move
	else if (client->restful.method == POST)
	{
		string body = client->packet;

		if (text == Json::LOGIN)
		{
			// 이미 로그인 됐을 때
			if (client->ID != "")
			{
				return GetHttpResponse(Response_Status::E_FAILED, "이미 로그인에 성공했습니다.");
			}
			else
			{
				// 옳은 요청 (id = ID)
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
				// 잘못된 요청
				else
				{
					return GetHttpResponse(Response_Status::E_FAILED, "Body의 key-value가 잘못되었습니다.");
				}
			}
		}
		else if (text == Json::MOVE)
		{
			// 로그인 필요
			if (client->ID == "")
			{
				return GetHttpResponse(Response_Status::E_FAILED, "로그인을 먼저 해야합니다.");
			}
			// 로그인 완료 후
			else
			{
				// 옳은 요청 (x = X, y = Y)
				if (body.find('?') != string::npos)
				{
					vector<string> bodySplited;
					Logic::Split(body, bodySplited, '?');

					if (bodySplited.size() != 2)
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body의 key-value가 잘못되었습니다.");
					}

					// x 좌표 추출
					vector<string> xSplited;
					Logic::Split(bodySplited[0], xSplited, '=');

					// 잘못된 요청
					if (xSplited[0] != "x")
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body의 key-value가 잘못되었습니다.");
					}

					string x = xSplited[1];

					// y 좌표 추출
					vector<string> ySplited;
					Logic::Split(bodySplited[1], ySplited, '=');

					// 잘못된 요청
					if (ySplited[0] != "y")
					{
						return GetHttpResponse(Response_Status::E_FAILED, "Body의 key-value가 잘못되었습니다.");
					}

					string y = ySplited[1];

					return GetHttpResponse(Response_Status::E_SUCCESSFUL, Logic::ProcessMove(client, { x, y }));
				}
				// 잘못된 요청
				else
				{
					return GetHttpResponse(Response_Status::E_FAILED, "Body의 key-value가 잘못되었습니다.");
				}
			}
		}
		else
		{
			return GetHttpResponse(Response_Status::E_FAILED, "잘못된 method입니다.");
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

			if (client->restful.isRestful)
			{
				ret = msg;
			}

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

	// 공격할 대상이 없다면 클라이언트에게 알린다.
	if (!canAttack)
		ret = Json::GetTextOnlyJson("공격할 대상이 없습니다.");

	return ret;
}

string Logic::ProcessMonsters(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	return Json::GetMonstersRespJson();
}

string Logic::ProcessUsers(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	// 클라이언트의 위치
	return Json::GetUsersRespJson(client->ID);
}

string Logic::ProcessChat(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	SOCKET toSock = -1;

	// 유저를 찾기
	for (auto& entry : Server::activeClients)
		if (entry.second->ID == params.param1)
		{
			toSock = entry.first;
			break;
		}

	// 유저가 존재하지 않을 때 반환됨
	if (toSock == -1)
	{
		string raw = params.param1 + " 은/는 존재하지 않는 유저입니다.";
		return Json::GetTextOnlyJson(raw);
	}

	// 유저가 존재할 때
	if (!client->shouldTerminate)
	{
		lock_guard<mutex> lg(shouldSendPacketsMutex);

		shouldSendPackets[toSock].push_back(Json::GetChatRespJson(client->ID, params.param2));
	}

	string raw = params.param1 + " 에게 귓속말을 전송했습니다.";
	return Json::GetTextOnlyJson(raw);
}

string Logic::ProcessUsePotion(const shared_ptr<Client>& client, const ParamsForProc& params)
{
	string ret = "";

	// hp potion 사용
	if (params.param1 == Json::HP_POTION)
	{
		int numOfPotion = stoi(Redis::GetHpPotion(client->ID));

		// hp potion이 존재할 때
		if (numOfPotion > 0)
		{
			// 체력 증가
			Redis::SetHp(client->ID, HP_POTION_HEAL, Redis::F_Type::E_RELATIVE);

			// hp 포션 개수 차감
			Redis::SetHpPotion(client->ID, -1, Redis::F_Type::E_RELATIVE);

			string raw = "HP 포션을 1개 사용해 체력이 " + to_string(Logic::HP_POTION_HEAL) + "만큼 증가했습니다.";
			ret = Json::GetTextOnlyJson(raw);
		}
		// hp potion이 존재하지 않을 때
		else
		{
			ret = Json::GetTextOnlyJson("HP 포션이 존재하지 않습니다.");
		}
	}
	// str potion 사용
	else if (params.param1 == Json::STR_POTION)
	{
		int numOfPotion = stoi(Redis::GetStrPotion(client->ID));

		// str potion이 존재할 때
		if (numOfPotion > 0)
		{
			// 지속시간동안 공격력을 증가시키는 StrPotion 함수를 호출하는 쓰레드 생성
			shared_ptr<thread> strPotionThread(new thread(StrPotion, client));
			strPotionThread->detach();

			// str 포션 개수 차감
			Redis::SetStrPotion(client->ID, -1, Redis::F_Type::E_RELATIVE);

			string raw = "STR 포션 1개를 사용해 " + to_string(Logic::STR_POTION_DURATION) + "초 간 공격력이 " + to_string(Logic::STR_POTION_DAMAGE) + "만큼 증가합니다.";
			ret = Json::GetTextOnlyJson(raw);
		}
		// str potion이 존재하지 않을 때
		else
		{
			ret = Json::GetTextOnlyJson("STR 포션이 존재하지 않습니다.");
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
	// str 증가
	Redis::SetStr(client->ID, STR_POTION_DAMAGE, Redis::F_Type::E_RELATIVE);

	// 지속시간 동안 대기
	this_thread::sleep_for(chrono::seconds(STR_POTION_DURATION));

	// 증가량 만큼 str 감소
	Redis::SetStr(client->ID, -1 * STR_POTION_DAMAGE, Redis::F_Type::E_RELATIVE);

	// 현재 유저가 접속중인 상태라면 종료 메시지 예약
	if (Redis::GetUserConnection(client->ID) == Redis::LOGINED)
	{
		// str 포션을 사용한 클라이언트가 접속 종료했다가 다시 접속했으면 기존 소켓과 다를 수 있기 때문에 새로 들어온 클라이언트 중에서 찾는다.
		for (auto& entry : Server::activeClients)
		{
			if (entry.second->shouldTerminate == false && entry.second->ID == client->ID)
			{
				// 찾은 클라이언트에게 종료 메시지 예약
				{
					lock_guard<mutex> lg(shouldSendPacketsMutex);

					shouldSendPackets[entry.first].push_back(Json::GetTextOnlyJson("Str 증가 효과가 종료되었습니다."));
				}
				break;
			}
		}
	}
	// 효과가 끝나기 전에 유저가 나가서 키가 Expire 된 상태일 때
	else if (Redis::GetUserConnection(client->ID) == Redis::EXPIRED)
	{
		// Expire된 상태에서 str을 감소시켰기 때문에 Persist 상태가 된다.
		// 따라서 str키를 다른 키의 남은 만료시간(TTL)만큼 다시 Expire한다.
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
			// 아직 로그인되지 않은 클라이언트라면 스킵
			if (entry.second->ID == "")
				continue;

			// 없어져야하는 클라이언트면 스킵
			if (entry.second->shouldTerminate)
				continue;

			int userLocX = stoi(Redis::GetLocationX(entry.second->ID));
			int userLocY = stoi(Redis::GetLocationY(entry.second->ID));

			// 슬라임의 공격 범위 안에 유저가 있으면
			if ((slimeLocX - userLocX <= Slime::MAX_X_ATTACK_RANGE && slimeLocX - userLocX >= Slime::MIN_X_ATTACK_RANGE) &&
				(slimeLocY - userLocY <= Slime::MAX_Y_ATTACK_RANGE && slimeLocY - userLocY >= Slime::MIN_Y_ATTACK_RANGE))
			{
				Logic::BroadcastToClients(Json::GetSlimeAttackUserJson(slime->index, entry.second->ID, slime->str));

				// 공격 받은 클라이언트의 hp가 0일 때
				if (entry.second->OnAttack(slime) <= 0)
				{
					Logic::BroadcastToClients(Json::GetUserDieJson(entry.second->ID, slime->index));
					entry.second->shouldTerminate = true;
				}
			}
		}

		// 슬라임의 공격 주기만큼 sleep
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
	cout << "[시스템] 슬라임" << index << " is On Attack by " << client->ID << '\n';

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
	return "{\"" + string(TEXT) + "\":\"슬라임" + to_string(slimeIndex) + " 이/가 " + userID + " 을/를 공격해서 데미지 " + to_string(slimeStr) + " 을/를 가했습니다.\"}";
}

string Json::GetUserAttackSlimeJson(const string& userID, int slimeIndex, int userStr)
{
	return "{\"" + string(TEXT) + "\":\"" + userID + " 이/가 슬라임" + to_string(slimeIndex) + " 을/를 공격해서 데미지 " + to_string(userStr) + " 을/를 가했습니다.\"}";
}

string Json::GetSlimeDieJson(int slimeIndex, const string& userID)
{
	return "{\"" + string(TEXT) + "\":\"슬라임" + to_string(slimeIndex) + " 이/가 " + userID + " 에 의해 죽었습니다.\"}";
}

string Json::GetUserDieJson(const string& userID, int slimeIndex)
{
	return "{\"" + string(TEXT) + "\":\"" + userID + " 이/가 슬라임" + to_string(slimeIndex) + " 에 의해 죽었습니다.\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_DIE)) + "\",\"" + string(PARAM2) + "\":\"" + userID + "\"}";
}

string Json::GetMoveRespJson(const string& userID)
{
	return "{\"" + string(TEXT) + "\":\"(" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")로 이동했습니다.\"}";
}

string Json::GetTextOnlyJson(const string& text)
{
	return "{\"" + string(TEXT) + "\":\"" + text + "\"}";
}

string Json::GetDupConnectionJson()
{
	return "{\"" + string(TEXT) + "\":\"다른 클라이언트에서 동일한 아이디로 로그인했습니다.\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_DUP_CONNECTION)) + "\"}";
}

string Json::GetUsersRespJson(const string& userID)
{
	// 해당 명령 보낸 클라이언트 우선 출력
	string msg = "{\"" + string(TEXT) + "\":\"유저 위치정보\\r\\n" + userID + " : (" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")";

	// 다른 클라이언트들의 위치
	for (auto& entry : Server::activeClients)
	{
		// 해당 명령 보낸 클라이언트와 아직 로그인안된 클라이언트 제외
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
	string msg = "{\"" + string(TEXT) + "\":\"슬라임 위치정보";

	for (list<shared_ptr<Slime>>::iterator it = Logic::slimes.begin(); it != Logic::slimes.end(); ++it)
	//for (auto& slime : Logic::slimes)
	{
		msg += "\\r\\n슬라임" + to_string((*it)->index) + " : (" + to_string((*it)->locX) + ", " + to_string((*it)->locY) + ")";
	}
	msg += "\"}";

	return msg;
}

string Json::GetChatRespJson(const string& fromUserID, const string& text)
{
	return "{\"" + string(TEXT) + "\":\"" + fromUserID + " (으)로 부터 온 메시지 : " + text + "\",\"" + string(PARAM1) + "\":\"" + to_string(int(M_Type::E_CHAT)) + "\"}";
}

string Json::GetInfoRespJson(const string& userID)
{
	string ret = "{\"" + string(TEXT) + "\":\"유저 정보\\r\\n유저 ID : " + userID + "\\r\\n유저 좌표 : (" + Redis::GetLocationX(userID) + ", " + Redis::GetLocationY(userID) + ")\\r\\n유저 HP : " + Redis::GetHp(userID) + "\\r\\n유저 STR : " + Redis::GetStr(userID) + "\\r\\n유저 HP 포션 개수 : " + Redis::GetHpPotion(userID) + "\\r\\n유저 STR 포션 개수 : " + Redis::GetStrPotion(userID) + "\"}";

	return ret;
}

string Json::GetPickUpPotionJson(const string& userID, Logic::PotionType potionType)
{
	string ret = "{\"" + string(TEXT) + "\":\"";
	if (potionType == Logic::PotionType::E_POTION_HP)
	{
		ret += "HP 포션을 획득했습니다.\"}";
	}
	else if (potionType == Logic::PotionType::E_POTION_STR)
	{
		ret += "STR 포션을 획득했습니다.\"}";
	}

	return ret;
}