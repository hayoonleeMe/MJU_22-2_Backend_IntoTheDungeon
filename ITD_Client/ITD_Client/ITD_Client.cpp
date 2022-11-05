#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

const char* SERVER_ADDRESS = "127.0.0.1";
static unsigned short SERVER_PORT = 27015; 

using namespace std;

// cin���� �Է¹��� �ؽ�Ʈ�� json ���ڿ��� ��ȯ�� ��ȯ�ϴ� �Լ�
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

int main()
{
    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    struct sockaddr_in serverAddr;

    // TCP socket �� �����.
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // TCP �� ���� ����̴�. ���� �ּҸ� ���ϰ� connect() �� �����Ѵ�.
    // connect �Ŀ��� ������ ���� �ּҸ� �������� �ʰ� send/recv �� �Ѵ�.
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr);
    r = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "connect failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // cin���� �Է¹��� �ؽ�Ʈ�� JSON���� ������ ������ �����Ѵ�.
    while (true) {
        string text = GetInputTextJson();
        if (text == "")
        {
            cout << "Wrong Text, Try Again\n";
            continue;
        }
        cout << text << endl;
        int dataLen = text.length();

        // ���̸� ���� ������.
        // binary �� 4bytes �� ���̷� encoding �Ѵ�.
        // �� �� network byte order �� ��ȯ�ϱ� ���ؼ� htonl �� ȣ���ؾߵȴ�.
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

    // Socket �� �ݴ´�.
    r = closesocket(sock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}
