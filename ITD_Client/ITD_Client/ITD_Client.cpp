#include "ITD_Client.h"

int main()
{
    Client::bExit = false;

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

    shared_ptr<thread> recvThread(new thread(Logic::RecvThreadProc));

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