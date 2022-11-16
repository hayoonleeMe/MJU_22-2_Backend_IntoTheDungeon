#include "ITD_Server.h"

SOCKET createPassiveSocket() 
{
    // TCP socket 을 만든다.
    SOCKET passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passiveSock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // socket 을 특정 주소, 포트에 바인딩 한다.
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Server::SERVER_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int r = bind(passiveSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (r == SOCKET_ERROR) {
        cerr << "bind failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    r = listen(passiveSock, 10);
    if (r == SOCKET_ERROR) {
        cerr << "listen failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    return passiveSock;
}

bool processClient(shared_ptr<Client> client)
{
    SOCKET activeSock = client->sock;

    // 패킷을 받는다.
    if (client->sendTurn == false && !isRepeat)
    {
        if (client->lenCompleted == false) 
        {
            // network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
            int r = recv(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "recv failed with error " << WSAGetLastError() << endl;
                return false;
            }
            else if (r == 0) 
            {
                // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
                cerr << "Socket closed: " << activeSock << endl;
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
            cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
            client->packetLen = dataLen;

            // 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
            if (client->packetLen > sizeof(client->packet)) 
            {
                cerr << "[" << activeSock << "] Too big data: " << client->packetLen << endl;
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
            cerr << "recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        else if (r == 0) 
        {
            // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
            return false;
        }
        client->offset += r;

        if (client->offset == client->packetLen) 
        {
            cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

            // TODO: 클라이언트로부터 받은 텍스트에 따라 로직 수행
            const string json = string(client->packet).substr(0, client->packetLen);

            Document d;
            d.Parse(json);

            // 명령어
            Value& text = d[Json::TEXT];

            // 첫 로그인
            if (strcmp(text.GetString(), Json::LOGIN) == 0)
            {
                Redis::RegisterUser(string(d[Json::PARAM1].GetString()));

                if (client->ID == "")
                    client->ID = string(d[Json::PARAM1].GetString());
            }
            // 이미 로그인된 유저로부터 명령어 받음
            else
            {
                // 명령별 로직 수행
                /*if (strcmp(text.GetString(), Json::MOVE) == 0)
                {
                    client->sendPacket = Logic::ProcessMove(client->ID, "", "");
                }
                else if (strcmp(text.GetString(), Json::ATTACK) == 0)
                {
                    client->sendPacket = Logic::ProcessAttack(client->ID);
                }
                else if (strcmp(text.GetString(), Json::MONSTERS) == 0)
                {
                    client->sendPacket = Logic::ProcessMonsters(client->ID);
                }
                else if (strcmp(text.GetString(), Json::USERS) == 0)
                {
                    client->sendPacket = Logic::ProcessUsers(client->ID, "", "");
                }
                else if (strcmp(text.GetString(), Json::CHAT) == 0)
                {
                    client->sendPacket = Logic::ProcessChat(client->ID);
                }
                else if (strcmp(text.GetString(), Json::BOT) == 0)
                {
                    client->sendPacket = Logic::ProcessBot(client->ID);
                }*/
            }

            client->sendPacket = json;

            // 보낼 패킷 설정 
            client->sendTurn = true;

            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
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
        cout << "Send Start to " << client->ID << '\n';

        if (client->lenCompleted == false) 
        {
            int r = send(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "send failed with error " << WSAGetLastError() << endl;
                return false;
            }
            client->offset += r;

            // 완성 못했다면 다음번에 계속 시도할 것이다.
            if (client->offset < 4) 
            {
                return true;
            }

            // 이제 packetLen 을 완성했다고 기록하고 offset 을 초기화해준다.
            client->lenCompleted = true;
            client->offset = 0;
            client->packetLen = ntohl(client->packetLen);
        }

        // 여기까지 도달했다는 것은 packetLen 을 완성한 경우다. (== lenCompleted 가 true)
        if (client->lenCompleted == false) 
        {
            return true;
        }

        int r = send(activeSock, client->sendPacket.c_str() + client->offset, client->packetLen - client->offset, 0);
        if (r == SOCKET_ERROR) 
        {
            cerr << "send failed with error " << WSAGetLastError() << endl;
            return false;
        }
        client->offset += r;

        if (client->offset == client->packetLen) 
        {
            cout << "[" << activeSock << "] Sent " << client->packetLen << " bytes" << endl;

            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
            client->sendTurn = false;
            client->lenCompleted = false;
            client->offset = 0;
            client->packetLen = 0;

            if (isRepeat)
                isRepeat = false;
        }

        return true;
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
                closesocket(activeSock);
                {
                    lock_guard<mutex> lg(Server::activeClientsMutex);

                    Server::activeClients.erase(activeSock);
                }
            }
            else 
            {
                client->doingRecv.store(false);
            }
        }
    }
}

int main()
{
    // hiredis 연결
    Redis::redis = redisConnect(Server::SERVER_ADDRESS, 6379);
    if (Redis::redis == NULL || Redis::redis->err)
    {
        if (Redis::redis)
            printf("Error: %s\n", Redis::redis->errstr);
        else
            printf("Can't allocate redis context\n");
        return 1;
    }

    int r = 0;

    // Winsock 을 초기화한다.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) 
    {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // passive socket 을 만들어준다.
    SOCKET passiveSock = createPassiveSocket();

    list<shared_ptr<thread> > threads;
    for (int i = 0; i < Server::NUM_WORKER_THREADS; ++i) 
    {
        shared_ptr<thread> workerThread(new thread(workerThreadProc));
        threads.push_back(workerThread);
    }
    shared_ptr<thread> sendThread(new thread(SendDataRepeat));
    threads.push_back(sendThread);


    while (true) 
    {
        int sendCount = 0;

        fd_set readSet, exceptionSet;

        // 위의 socket set 을 초기화한다.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        SOCKET maxSock = -1;

        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // 현재 남아있는 active socket 들에 대해서도 모두 set 에 넣어준다.
        for (auto& entry : Server::activeClients) 
        {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (client->doingRecv.load() == false) 
            {
                FD_SET(activeSock, &readSet);
                FD_SET(activeSock, &exceptionSet);
                maxSock = max(maxSock, activeSock);

                if (client->sendTurn)
                    ++sendCount;
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        r = select(maxSock + 1, &readSet, NULL, &exceptionSet, &timeout);

        // 회복할 수 없는 오류이다. 서버를 중단한다.
        if (r == SOCKET_ERROR) 
        {
            cerr << "select failed: " << WSAGetLastError() << endl;
            break;
        }

        // 아무것도 반환을 안한 경우는 아래를 처리하지 않고 바로 다시 select 를 하게 한다.
        // select 의 반환값은 오류일 때 SOCKET_ERROR, 그 외의 경우 이벤트가 발생한 소켓 갯수이다.
        if (r == 0 && sendCount == 0) 
        {
            continue;
        }

        // passive socket 이 readable 하다면 이는 새 연결이 들어왔다는 것이다.
        if (FD_ISSET(passiveSock, &readSet)) 
        {                                                                       
            // passive socket 을 이용해 accept() 를 한다.
            cout << "Waiting for a connection" << endl;
            struct sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);

            if (activeSock == INVALID_SOCKET) 
            {
                cerr << "accept failed with error " << WSAGetLastError() << endl;
                return 1;
            }
            else 
            {
                shared_ptr<Client> newClient(new Client(activeSock));

                Server::activeClients.insert(make_pair(activeSock, newClient));

                char strBuf[1024];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), strBuf, sizeof(strBuf));
                cout << "New client from " << strBuf << ":" << ntohs(clientAddr.sin_port) << ". "
                    << "Socket: " << activeSock << endl;
            }
        }

        // 오류 이벤트가 발생하는 소켓의 클라이언트는 제거한다.
        list<SOCKET> toDelete;
        for (auto& entry : Server::activeClients)
        {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (FD_ISSET(activeSock, &exceptionSet)) 
            {
                cerr << "Exception on socket " << activeSock << endl;

                // 소켓을 닫는다.
                closesocket(activeSock);

                // 지울 대상에 포함시킨다.
                toDelete.push_back(activeSock);

                // 소켓을 닫은 경우 더 이상 처리할 필요가 없으니 아래 read 작업은 하지 않는다.
                continue;
            }

            // 읽기 이벤트가 발생하는 소켓의 경우 recv() 를 처리한다.
            if (FD_ISSET(activeSock, &readSet) || client->sendTurn) 
            {
                // 이제 다시 select 대상이 되지 않도록 client 의 flag 를 켜준다.
                client->doingRecv.store(true);

                {
                    lock_guard<mutex> lg(Server::jobQueueMutex);

                    bool wasEmpty = Server::jobQueue.empty();
                    Server::jobQueue.push(client);

                    // 무의미하게 CV 를 notify하지 않도록 큐의 길이가 0에서 1이 되는 순간 notify 를 하도록 하자.
                    if (wasEmpty) {
                        Server::jobQueueFilledCv.notify_one();
                    }
                }
            }
        }

        // 지울 것이 있었다면 지운다.
        {
            lock_guard<mutex> lg(Server::activeClientsMutex);

            for (auto& closedSock : toDelete)
            {
                // 맵에서 지우고 객체도 지워준다.
                Server::activeClients.erase(closedSock);
            }
        }
    }

    // 이제 threads 들을 join 한다.
    for (shared_ptr<thread>& workerThread : threads) 
    {
        workerThread->join();
    }
    sendThread->join();

    // 연결을 기다리는 passive socket 을 닫는다.
    r = closesocket(passiveSock);
    if (r == SOCKET_ERROR) 
    {
        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock 을 정리한다.
    WSACleanup();
    return 0;
}