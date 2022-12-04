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
    // 프로그램 이름 설정
    system("title Into The Dungeon Client");

    // Handler Map 초기화
    Json::InitHandlers();

    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "[오류] WSAStartup failed with error " << r << endl;
        return 1;
    }

    // TCP 소켓을 만든다.
    Client::sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Client::sock == INVALID_SOCKET) {
        cerr << "[오류] socket failed with error " << WSAGetLastError() << endl;
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
        cerr << "[오류] connect failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // 첫 로그인
    // 로그인 성공 메시지를 받으면 조작 도움말을 출력한다.
    Logic::Login();

    // 유저 정보 출력
    Logic::Info();

    // cin으로 입력받은 텍스트를 JSON으로 변경해 서버로 전송한다.
    while (true) {
        // bot 명령어를 입력받아 bot 모드가 활성화되면 반복문을 빠져나간다.
        if (Bot::isBotMode)
            break;

        string text;

        // 변환이 실패하면 다시 입력받는다.
        if (!Logic::GetInputTextJson(text))
        {
            continue;
        }

        if (!Logic::SendData(text))
            Logic::ExitProgram();
    }

    // bot 명령어를 통해 bot 모드가 활성화되면
    if (Bot::isBotMode)
    {
        Bot::InitBotHandlers();

        // 봇 모드의 로직을 수행한다.
        while (true)
        {
            // 랜덤한 명령어를 선택한다.
            string text = Bot::CMD_BOT[Rand::GetRandomCmd()];

            // 명령어 별로 json 문자열로 변환하여 보낸다.
            string msg = Bot::botHandlers[text](text);

            if (!Logic::SendData(msg))
                Logic::ExitProgram();

            // CMD_PERIOD 초 마다 명령어를 실행한다.
            this_thread::sleep_for(chrono::seconds(Bot::CMD_PERIOD));
        }
    }

    // recvThread를 join한다.
    recvThread->join();

    // Socket 을 닫는다.
    r = closesocket(Client::sock);
    if (r == SOCKET_ERROR) {
        cerr << "[오류] closesocket failed with error " << WSAGetLastError() << endl;
        return false;
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}