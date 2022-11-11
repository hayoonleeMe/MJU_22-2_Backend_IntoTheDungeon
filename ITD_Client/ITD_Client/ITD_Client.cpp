#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

// ws2_32.lib 를 링크한다.
#pragma comment(lib, "Ws2_32.lib")

static const char* SERVER_ADDRESS = "127.0.0.1";
static unsigned short SERVER_PORT = 27015; 

using namespace std;
using namespace rapidjson;

// cin으로 입력받은 텍스트를 json 문자열로 변환해 반환하는 함수
static string GetInputTextJson()
{
    string input;
    cin >> input;

    string text = "{\"text\":\"" + input + "\"";

    if (input == "move")
    {
        int x, y;
        cin >> x >> y;

        text += ",\"first\":" + to_string(x) + ",\"second\":" + to_string(y);
    }
    else if (input == "chat")
    {
        string name, msg;
        cin >> name >> msg;

        text += ",\"first\":\"" + name + "\",\"second\":\"" + msg + "\"";
    }
    else if (input != "attack" && input != "monsters" && input != "users" && input != "bot")
    {
        return "";
    }

    text += "}";
    return text;
}

// 서버로 데이터를 보낸다.
bool SendData(SOCKET sock, const string& text)
{
    int r = 0;

    int dataLen = text.length();

    // 길이를 먼저 보낸다.
    // binary 로 4bytes 를 길이로 encoding 한다.
    // 이 때 network byte order 로 변환하기 위해서 htonl 을 호출해야된다.
    int dataLenNetByteOrder = htonl(dataLen);
    int offset = 0;
    while (offset < 4) {
        r = send(sock, ((char*)&dataLenNetByteOrder) + offset, 4 - offset, 0);
        if (r == SOCKET_ERROR) {
            cerr << "failed to send length: " << WSAGetLastError() << endl;
            return 1;
        }
        offset += r;
    }
    cout << "Sent length info: " << dataLen << endl;

    offset = 0;
    while (offset < dataLen) {
        r = send(sock, text.c_str() + offset, dataLen - offset, 0);
        if (r == SOCKET_ERROR) {
            cerr << "send failed with error " << WSAGetLastError() << endl;
            return 1;
        }
        cout << "Sent " << r << " bytes" << endl;
        offset += r;
    }
}

// 로그인한다.
void Login(SOCKET sock)
{
    cout << "로그인 시작\n" << "아이디를 입력하세요.\n";
    string ID;
    cin >> ID;

    string text = "{\"text\":\"login\",\"first\":\"" + ID + "\"}";

    SendData(sock, text);
}

int main()
{
    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    struct sockaddr_in serverAddr;

    // TCP socket 을 만든다.
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // TCP 는 연결 기반이다. 서버 주소를 정하고 connect() 로 연결한다.
    // connect 후에는 별도로 서버 주소를 기재하지 않고 send/recv 를 한다.
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr);
    r = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "connect failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // 첫 로그인
    Login(sock);

    // cin으로 입력받은 텍스트를 JSON으로 변경해 서버로 전송한다.
    while (true) {
        string text = GetInputTextJson();
        if (text == "")
        {
            cout << "Wrong Text, Try Again\n";
            continue;
        }
        cout << text << endl;
        
        SendData(sock, text);
    }

    // Socket 을 닫는다.
    r = closesocket(sock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}
