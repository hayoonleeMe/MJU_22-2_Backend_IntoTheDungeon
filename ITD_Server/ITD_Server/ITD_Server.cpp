#include "ITD_Server.h"

SOCKET createPassiveSocket() 
{
    // TCP socket 을 만든다.
    SOCKET passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passiveSock == INVALID_SOCKET) {
        cerr << "[오류] socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // socket 을 특정 주소, 포트에 바인딩 한다.
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Server::SERVER_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int r = ::bind(passiveSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "[오류] bind failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    r = listen(passiveSock, 10);
    if (r == SOCKET_ERROR) {
        cerr << "[오류] listen failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    return passiveSock;
}

bool processClient(shared_ptr<Client>& client)
{
    SOCKET activeSock = client->sock;

    // 패킷을 받는다.
    if (client->sendTurn == false)
    {
        if (client->lenCompleted == false) 
        {
            // network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
            int r = recv(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "[오류] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
                return false;
            }
            else if (r == 0) 
            {
                // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
                cerr << "[오류] [" << activeSock << "] Socket closed" << endl;
                return false;
            }
            client->offset += r;

            // 완성 못했다면 다음번에 계속 시도할 것이다.
            if (client->offset < 4) 
            {
                return true;
            }

            // host byte order 로 변경한다.
            int dataLen = ntohl(client->packetLen);
            //cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
            client->packetLen = dataLen;

            // 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
            if (client->packetLen > sizeof(client->packet)) 
            {
                cerr << "[오류] [" << activeSock << "] Too big data: " << client->packetLen << endl;
                return false;
            }

            // 이제 packetLen 을 완성했다고 기록하고 offset 을 초기화해준다.
            client->lenCompleted = true;
            client->offset = 0;
        }

        // 여기까지 도달했다는 것은 packetLen 을 완성한 경우다. (== lenCompleted 가 true)
        if (client->lenCompleted == false) 
        {
            return true;
        }

        int r = recv(activeSock, client->packet + client->offset, client->packetLen - client->offset, 0);
        if (r == SOCKET_ERROR) 
        {
            cerr << "[오류] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        else if (r == 0) 
        {
            // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
            cerr << "[오류] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        client->offset += r;

        if (client->offset == client->packetLen) 
        {
            //cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

            // 클라이언트로부터 받은 명령어에 따라 로직 수행
            const string json = string(client->packet).substr(0, client->packetLen);

            Document document;
            document.Parse(json);

            // 명령어
            Value& text = document[Json::TEXT];

            // 클라이언트의 로그인 시도
            if (strcmp(text.GetString(), Json::LOGIN) == 0)
            {
                client->sendPacket = Redis::RegisterUser(string(document[Json::PARAM1].GetString()), client->sock);

                if (client->ID == "")
                    client->ID = string(document[Json::PARAM1].GetString());
            }
            // 이미 로그인된 유저로부터 명령어 받음
            else
            {
                // 명령별 로직 수행
                Logic::ParamsForProc job;
                if (document.HasMember(Json::PARAM1))
                {
                    job.param1 = document[Json::PARAM1].GetString();

                    if (document.HasMember(Json::PARAM2))
                    {
                        job.param2 = document[Json::PARAM2].GetString();
                    }
                }

                client->sendPacket = (Logic::handlers[text.GetString()])(client, job);
            }

            // 보낼 패킷이 존재하면 보내도록 설정 
            if (client->sendPacket != "")
                client->sendTurn = true;
            else
                client->sendTurn = false;

            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
            memset(client->packet, 0, Client::PACKET_SIZE);
            client->lenCompleted = false;
            client->offset = 0;
            if (client->sendTurn)
                client->packetLen = htonl(client->sendPacket.length());
            else
                client->packetLen = 0;
        }

        return true;
    }

    // 받은 패킷에 대한 응답을 보낸다.
    if (client->sendTurn)
    {   
        //cout << "[" << activeSock << "] Send Start to " << client->ID << ", send msg : " << client->sendPacket << '\n';

        // 데이터 길이를 보낸다.
        if (client->lenCompleted == false) 
        {
            int r = send(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "[오류] [" << activeSock << "] send failed with error " << WSAGetLastError() << endl;
                return false;
            }
            client->offset += r;

            // 모두 보내지 못했다면 다음번에 계속 시도할 것이다.
            if (client->offset < 4) 
            {
                return true;
            }

            // 이제 packetLen 을 모두 보냈다고 기록하고 offset 을 초기화해준다.
            client->lenCompleted = true;
            client->offset = 0;
            client->packetLen = ntohl(client->packetLen);
        }

        // 여기까지 도달했다는 것은 packetLen 을 모두 보낸 경우이다. (== lenCompleted 가 true)
        if (client->lenCompleted == false) 
        {
            return true;
        }

        // 데이터를 보낸다.
        int r = send(activeSock, client->sendPacket.c_str() + client->offset, client->packetLen - client->offset, 0);
        if (r == SOCKET_ERROR) 
        {
            cerr << "[오류] [" << activeSock << "] send failed with error " << WSAGetLastError() << endl;
            return false;
        }
        client->offset += r;

        // 데이터를 모두 보냈다면
        if (client->offset == client->packetLen) 
        {
            //cout << "[" << activeSock << "] Sent " << client->packetLen << " bytes" << endl;

            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
            client->sendTurn = false;
            client->lenCompleted = false;
            client->offset = 0;
            client->packetLen = 0;
        }
    }

    return true;
}

void workerThreadProc() 
{
    while (true) 
    {
        shared_ptr<Client> client;
        {
            unique_lock<mutex> ul(Server::jobQueueMutex);

            while (Server::jobQueue.empty()) 
            {
                Server::jobQueueFilledCv.wait(ul);
            }

            client = Server::jobQueue.front();
            Server::jobQueue.pop();
        }

        if (client) 
        {
            SOCKET activeSock = client->sock;
            bool successful = processClient(client);
            if (successful == false) 
            {
                // 기존 접속에서 예정되어 있던 보내야하는 메시지들을 모두 없앤다.
                {
                    lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                    Logic::shouldSendPackets[activeSock].clear();
                }

                // 해당 클라이언트의 모든 키를 expire한다.
                Redis::ExpireUser(client->ID);

                // 해당 클라이언트를 지운다.
                {
                    lock_guard<mutex> lg(Server::activeClientsMutex);

                    Server::activeClients.erase(activeSock);
                }

                // 소켓을 닫는다.
                if (activeSock != INVALID_SOCKET)
                {
                    cout << "[processClient is false] socket closed : " << activeSock << '\n';
                    closesocket(activeSock);
                }
            }
            else 
            {
                client->doingProc.store(false);
            }
        }
    }
}

// 1분마다 슬라임의 수가 최대 수만큼 존재하도록 생성하는 함수
void SlimeGenerateThread()
{
    // 1분마다 슬라임 수가 10마리가 되도록 젠한다.
    while (true)
    {
        cout << "[시스템] 슬라임 " << Logic::MAX_NUM_OF_SLIME - Logic::slimes.size() << "마리 생성\n";
        Logic::SpawnSlime(Logic::MAX_NUM_OF_SLIME - Logic::slimes.size());

        this_thread::sleep_for(chrono::seconds(Logic::SLIME_GEN_PERIOD));
    }
}

int main()
{
    // 프로그램 이름 설정
    system("title Into The Dungeon Server");

    // hiredis 연결
    Redis::redis = redisConnect(Server::SERVER_ADDRESS, Redis::DEFAULT_REDIS_PORT);
    if (Redis::redis == NULL || Redis::redis->err)
    {
        if (Redis::redis)
            cerr << "[오류] " << Redis::redis->errstr << '\n';
        else
            cout << "[오류] Can't allocate redis context\n";
        return 1;
    }

    // 서버 구동 시 Redis 초기화
    Redis::Flushall();

    // Handler Map 초기화
    Logic::InitHandlers();

    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) 
    {
        cerr << "[오류] WSAStartup failed with error " << r << endl;
        return 1;
    }

    // passive socket 을 만들어준다.
    SOCKET passiveSock = createPassiveSocket();

    // 작업을 처리하는 쓰레드를 추가한다.
    list<shared_ptr<thread> > threads;
    for (int i = 0; i < Server::NUM_WORKER_THREADS; ++i) 
    {
        shared_ptr<thread> workerThread(new thread(workerThreadProc));
        threads.push_back(workerThread);
    }
    // 슬라임 생성 및 공격 작업을 처리하는 쓰레드를 추가한다.
    threads.push_back(shared_ptr<thread>(new thread(SlimeGenerateThread)));

    while (true) 
    {
        // 데이터 보내기 혹은 제거되어야할 클라이언트가 존재하는지 나타내는 변수
        bool shouldPass = false;

        fd_set readSet, exceptionSet;

        // 위의 socket set 을 초기화한다.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        SOCKET maxSock = -1;

        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // 현재 남아있는 active socket 들에 대해서도 모두 set 에 넣어준다.
        {
            lock_guard<mutex> lg(Server::activeClientsMutex);

            for (auto& entry : Server::activeClients)
            {
                SOCKET activeSock = entry.first;
                shared_ptr<Client> client = entry.second;

                if (client->doingProc.load() == false)
                {
                    FD_SET(activeSock, &readSet);
                    FD_SET(activeSock, &exceptionSet);
                    maxSock = max(maxSock, activeSock);

                    // 만약 해당 클라이언트가 죽었거나 중복 로그인으로 인해 제거되어야 하거나 데이터를 보내기 위해 작업 큐에 넣어져야 한다면 shouldPass를 true로 세팅
                    if (client->shouldTerminate || client->sendTurn)
                    {
                        shouldPass = true;
                    }
                    // 이 클라이언트가 데이터를 보낼 차례가 아니면
                    else
                    {
                        // 로그인된 클라이언트가 작업해야할 것이 아무것도 없는 클라이언트이므로 보내야하는 패킷이 있다면 보내도록 설정해준다.
                        if (client->ID != "" && !Logic::shouldSendPackets[client->sock].empty())
                        {
                            {
                                lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                                client->sendPacket = Logic::shouldSendPackets[client->sock].front();
                                Logic::shouldSendPackets[client->sock].pop_front();
                                client->sendTurn = true;
                                client->packetLen = htonl(client->sendPacket.length());
                                shouldPass = true;
                            }
                        }
                    }
                }
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        r = select(maxSock + 1, &readSet, NULL, &exceptionSet, &timeout);

        // 회복할 수 없는 오류이다. 서버를 중단한다.
        if (r == SOCKET_ERROR) 
        {
            cerr << "[오류] select failed: " << WSAGetLastError() << endl;
            break;
        }

        // 아무것도 반환을 안한 경우는 아래를 처리하지 않고 바로 다시 select 를 하게 한다.
        // select 의 반환값은 오류일 때 SOCKET_ERROR, 그 외의 경우 이벤트가 발생한 소켓 갯수이다.
        if (r == 0 && !shouldPass) 
        {
            continue;
        }

        // passive socket 이 readable 하다면 이는 새 연결이 들어왔다는 것이다.
        if (FD_ISSET(passiveSock, &readSet)) 
        {                                                                       
            // passive socket 을 이용해 accept() 를 한다.
            cout << "[시스템] Waiting for a connection" << endl;
            struct sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);
            if (activeSock == INVALID_SOCKET) 
            {
                cerr << "[오류] accept failed with error " << WSAGetLastError() << endl;
                return 1;
            }
            else 
            {
                shared_ptr<Client> newClient(new Client(activeSock));

                Server::activeClients.insert(make_pair(activeSock, newClient));

                char strBuf[1024];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), strBuf, sizeof(strBuf));
                cout << "[시스템] New client from " << strBuf << ":" << ntohs(clientAddr.sin_port) << ". "
                    << "Socket: " << activeSock << endl;
            }
        }

        // 오류 이벤트가 발생하는 소켓의 클라이언트는 제거한다.
        {
            lock_guard<mutex> lg(Server::activeClientsMutex);

            for (auto it = Server::activeClients.begin(); it != Server::activeClients.end();)
            {
                SOCKET activeSock = it->first;
                shared_ptr<Client> client = it->second;

                if (FD_ISSET(activeSock, &exceptionSet))
                {
                    cerr << "[오류] Exception on socket " << activeSock << endl;

                    // 기존 접속에서 예정되어 있던 보내야하는 메시지들을 모두 없앤다.
                    {
                        lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                        Logic::shouldSendPackets[activeSock].clear();
                    }

                    // 모든 키를 Expire한다.
                    Redis::ExpireUser(client->ID);

                    // 해당 클라이언트를 지운다.
                    Server::activeClients.erase(it++);

                    // 소켓을 닫는다.
                    if (activeSock != INVALID_SOCKET)
                    {
                        cout << "[Exception on socket] socket closed : " << activeSock << '\n';
                        closesocket(activeSock);
                    }

                    // 소켓을 닫은 경우 더 이상 처리할 필요가 없으니 아래 read 작업은 하지 않는다.
                    continue;
                }

                // 해당 클라이언트가 처리해야 할 작업을 모두 완료한 상태이고 그 클라이언트가 죽었거나 삭제되어야하면 없앤다.
                if (client->shouldTerminate && Logic::shouldSendPackets[activeSock].empty() && client->doingProc.load() == false)
                {
                    // 기존 접속에서 예정되어 있던 보내야하는 메시지들을 모두 없앤다.
                    {
                        lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                        Logic::shouldSendPackets[activeSock].clear();
                    }

                    // 유저가 죽었다면 해당 유저에 설정된 Redis의 모든 키를 제거한다.
                    if (stoi(Redis::GetHp(client->ID)) <= 0)
                    {
                        Redis::DeleteAllUserKeys(client->ID);
                    }

                    // 해당 클라이언트를 지운다.
                    Server::activeClients.erase(it++);

                    // 소켓을 닫는다.
                    if (activeSock != INVALID_SOCKET)
                    {
                        cout << "[toDelete] socket closed : " << client->sock << '\n';
                        closesocket(client->sock);
                    }

                    continue;
                }

                // 읽기 이벤트가 발생하는 소켓의 경우 recv() 를 처리한다.
                if (FD_ISSET(activeSock, &readSet) || client->sendTurn)
                {
                    // 이제 다시 select 대상이 되지 않도록 client 의 flag 를 켜준다.
                    client->doingProc.store(true);

                    {
                        lock_guard<mutex> lg(Server::jobQueueMutex);

                        bool wasEmpty = Server::jobQueue.empty();
                        Server::jobQueue.push(client);

                        // 무의미하게 CV 를 notify하지 않도록 큐의 길이가 0에서 1이 되는 순간 notify 를 하도록 하자.
                        if (wasEmpty) 
                        {
                            Server::jobQueueFilledCv.notify_one();
                        }
                    }
                }

                ++it;
            }
        }
    }

    // 이제 threads 들을 join 한다.
    for (shared_ptr<thread>& workerThread : threads) 
        workerThread->join();

    // 연결을 기다리는 passive socket 을 닫는다.
    if (passiveSock != INVALID_SOCKET)
    {
        cout << "[final] close socket : " << passiveSock << '\n';
        r = closesocket(passiveSock);
        if (r == SOCKET_ERROR) 
        {
            cerr << "[오류] closesocket(passive) failed with error " << WSAGetLastError() << endl;
            return 1;
        }
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}