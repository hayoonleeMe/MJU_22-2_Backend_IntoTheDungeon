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
    // ���α׷� �̸� ����
    system("title Into The Dungeon Client");

    // Handler Map �ʱ�ȭ
    Json::InitHandlers();

    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "[����] WSAStartup failed with error " << r << endl;
        return 1;
    }

    // TCP ������ �����.
    Client::sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Client::sock == INVALID_SOCKET) {
        cerr << "[����] socket failed with error " << WSAGetLastError() << endl;
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
        cerr << "[����] connect failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // ù �α���
    // �α��� ���� �޽����� ������ ���� ������ ����Ѵ�.
    Logic::Login();

    // ���� ���� ���
    Logic::Info();

    // cin���� �Է¹��� �ؽ�Ʈ�� JSON���� ������ ������ �����Ѵ�.
    while (true) {
        // bot ��ɾ �Է¹޾� bot ��尡 Ȱ��ȭ�Ǹ� �ݺ����� ����������.
        if (Bot::isBotMode)
            break;

        string text;

        // ��ȯ�� �����ϸ� �ٽ� �Է¹޴´�.
        if (!Logic::GetInputTextJson(text))
        {
            continue;
        }

        if (!Logic::SendData(text))
            Logic::ExitProgram();
    }

    // bot ��ɾ ���� bot ��尡 Ȱ��ȭ�Ǹ�
    if (Bot::isBotMode)
    {
        Bot::InitBotHandlers();

        // �� ����� ������ �����Ѵ�.
        while (true)
        {
            // ������ ��ɾ �����Ѵ�.
            string text = Bot::CMD_BOT[Rand::GetRandomCmd()];

            // ��ɾ� ���� json ���ڿ��� ��ȯ�Ͽ� ������.
            string msg = Bot::botHandlers[text](text);

            if (!Logic::SendData(msg))
                Logic::ExitProgram();

            // CMD_PERIOD �� ���� ��ɾ �����Ѵ�.
            this_thread::sleep_for(chrono::seconds(Bot::CMD_PERIOD));
        }
    }

    // recvThread�� join�Ѵ�.
    recvThread->join();

    // Socket �� �ݴ´�.
    r = closesocket(Client::sock);
    if (r == SOCKET_ERROR) {
        cerr << "[����] closesocket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}