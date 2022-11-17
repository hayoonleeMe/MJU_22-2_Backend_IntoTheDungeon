#include "ITD_Client.h"

int main()
{
    Client::bExit = false;

    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // TCP ������ �����.
    Client::sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Client::sock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    shared_ptr<thread> recvThread(new thread(Logic::RecvThreadProc));

    // TCP �� ���� ����̴�. ���� �ּҸ� ���ϰ� connect() �� �����Ѵ�.
    // connect �Ŀ��� ������ ���� �ּҸ� �������� �ʰ� send/recv �� �Ѵ�.
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Client::SERVER_PORT);
    inet_pton(AF_INET, Client::SERVER_ADDRESS, &serverAddr.sin_addr);

    // TCP ������ �����Ѵ�.
    r = connect(Client::sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "connect failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // ù �α���
    Logic::Login();

    // cin���� �Է¹��� �ؽ�Ʈ�� JSON���� ������ ������ �����Ѵ�.
    while (!Client::bExit) {
        string text = Logic::GetInputTextJson();
        if (text == "")
        {
            cout << "Wrong Text, Try Again\n";
            continue;
        }
        cout << text << endl;

        if (!Logic::SendData(text))
            return 1;
    }

    // recvThread�� join�Ѵ�.
    recvThread->join();

    // Socket �� �ݴ´�.
    r = closesocket(Client::sock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}