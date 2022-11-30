#include "ITD_Client.h"

// 계속해서 서버로부터 메시지를 받는 함수
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
    // Handler Map 초기화
    Json::InitHandlers();

    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // TCP 소켓을 만든다.
    Client::sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Client::sock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // 서버로부터 메시지를 받는 쓰레드 생성
    shared_ptr<thread> recvThread(new thread(RecvThreadProc));

    // TCP 는 연결 기반이다. 서버 주소를 정하고 connect() 로 연결한다.
    // connect 후에는 별도로 서버 주소를 기재하지 않고 send/recv 를 한다.
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Client::SERVER_PORT);
    inet_pton(AF_INET, Client::SERVER_ADDRESS, &serverAddr.sin_addr);

    // TCP 소켓을 연결한다.
    r = connect(Client::sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "connect failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // 첫 로그인
    Logic::Login();

    // cin으로 입력받은 텍스트를 JSON으로 변경해 서버로 전송한다.
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

    // recvThread를 join한다.
    recvThread->join();

    // Socket 을 닫는다.
    r = closesocket(Client::sock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}