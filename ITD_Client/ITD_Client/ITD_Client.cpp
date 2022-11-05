#include <chrono>
#include <iostream>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")

static unsigned short SERVER_PORT = 27015;

int main()
{
    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        std::cerr << "WSAStartup failed with error " << r << std::endl;
        return 1;
    }

    struct sockaddr_in serverAddr;

    // TCP socket �� �����.
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket failed with error " << WSAGetLastError() << std::endl;
        return 1;
    }

    // TCP �� ���� ����̴�. ���� �ּҸ� ���ϰ� connect() �� �����Ѵ�.
    // connect �Ŀ��� ������ ���� �ּҸ� �������� �ʰ� send/recv �� �Ѵ�.
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    r = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        std::cerr << "connect failed with error " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 1�� �������� ��� ��Ŷ�� ��������.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int dataLen = rand() % 5001 + 10000;  // 10000 ~ 15000 ����Ʈ������ �����ϰ� ������. 

        // ���̸� ���� ������.
        // binary �� 4bytes �� ���̷� encoding �Ѵ�.
        // �� �� network byte order �� ��ȯ�ϱ� ���ؼ� htonl �� ȣ���ؾߵȴ�.
        int dataLenNetByteOrder = htonl(dataLen);
        int offset = 0;
        while (offset < 4) {
            r = send(sock, ((char*)&dataLenNetByteOrder) + offset, 4 - offset, 0);
            if (r == SOCKET_ERROR) {
                std::cerr << "failed to send length: " << WSAGetLastError() << std::endl;
                return 1;
            }
            offset += r;
        }
        std::cout << "Sent length info: " << dataLen << std::endl;

        // send �� �����͸� ������. ���⼭�� �ʱ�ȭ���� ���� ������ �����͸� ������.

        char data[32768];
        offset = 0;
        while (offset < dataLen) {
            r = send(sock, data + offset, dataLen - offset, 0);
            if (r == SOCKET_ERROR) {
                std::cerr << "send failed with error " << WSAGetLastError() << std::endl;
                return 1;
            }
            std::cout << "Sent " << r << " bytes" << std::endl;
            offset += r;
        }
    }

    // Socket �� �ݴ´�.
    r = closesocket(sock);
    if (r == SOCKET_ERROR) {
        std::cerr << "closesocket failed with error " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}
