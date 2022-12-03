#include "ITD_Client.h"

// ����ؼ� �����κ��� �޽����� �޴� �Լ�
void RecvThreadProc()
{
    while (true)
    {
        if (!Logic::ReceiveData())
        {
            Logic::ExitProgram();
        }
    }
}

int main()
{
    // Handler Map �ʱ�ȭ
    Json::InitHandlers();

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

    // �����κ��� �޽����� �޴� ������ ����
    shared_ptr<thread> recvThread(new thread(RecvThreadProc));

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
    while (true) {
        string text = Logic::GetInputTextJson();
        if (text == "")
        {
            cout << "Wrong Text, Try Again\n";
            continue;
        }

        if (!Logic::SendData(text))
            Logic::ExitProgram();
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