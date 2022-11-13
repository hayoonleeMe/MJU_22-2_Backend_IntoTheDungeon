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

char recvBuf[65536];

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
            return false;
        }
        offset += r;
    }
    cout << "Sent length info: " << dataLen << endl;

    offset = 0;
    while (offset < dataLen) {
        r = send(sock, text.c_str() + offset, dataLen - offset, 0);
        if (r == SOCKET_ERROR) {
            cerr << "send failed with error " << WSAGetLastError() << endl;
            return false;
        }
        cout << "Sent " << r << " bytes" << endl;
        offset += r;
    }

    return true;
}

bool ReceiveData(SOCKET activeSock)
{
    int r = 0;

    // 길이 정보를 받기 위해서 4바이트를 읽는다.
    // network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
    std::cout << "Receiving length info" << std::endl;
    int dataLenNetByteOrder;
    int offset = 0;
    while (offset < 4) {
        r = recv(activeSock, ((char*)&dataLenNetByteOrder) + offset, 4 - offset, 0);
        if (r == SOCKET_ERROR) {
            std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
            return false;
        }
        else if (r == 0) {
            // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
            // 따라서 r == 0 인 경우도 loop 을 탈출하게 해야된다.
            std::cerr << "socket closed while reading length" << std::endl;
            return false;
        }
        offset += r;
    }
    int dataLen = ntohl(dataLenNetByteOrder);
    std::cout << "Received length info: " << dataLen << std::endl;

    // 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
    if (dataLen > sizeof(recvBuf))
    {
        cerr << "[" << activeSock << "] Too big data: " << dataLen << endl;
        return false;
    }

    // socket 으로부터 데이터를 받는다.
    // TCP 는 연결 기반이므로 누가 보냈는지는 accept 시 결정되고 그 뒤로는 send/recv 만 호출한다.
    std::cout << "Receiving stream" << std::endl;
    offset = 0;
    while (offset < dataLen) {
        r = recv(activeSock, recvBuf + offset, dataLen - offset, 0);
        if (r == SOCKET_ERROR) {
            std::cerr << "recv failed with error " << WSAGetLastError() << std::endl;
            return false;
        }
        else if (r == 0) {
            // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
            // 따라서 r == 0 인 경우도 loop 을 탈출하게 해야된다.
            return false;
        }
        std::cout << "Received " << r << " bytes" << std::endl;
        offset += r;
    }

    string text = string(recvBuf).substr(0, dataLen);

    cout << "Received text : " << text << '\n';

    return true;
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
        
        if (!SendData(sock, text))
        {
            return 1;
        }
        
        if (!ReceiveData(sock))
        {
            return 1;
        }
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
