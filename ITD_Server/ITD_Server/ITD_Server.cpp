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

    int r = ::bind(passiveSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
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
    if (client->sendTurn == false)
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

            Document document;
            document.Parse(json);

            // 명령어
            Value& text = document[Json::TEXT];

            // 첫 로그인
            if (strcmp(text.GetString(), Json::LOGIN) == 0)
            {
                Redis::RegisterUser(string(document[Json::PARAM1].GetString()));

                if (client->ID == "")
                    client->ID = string(document[Json::PARAM1].GetString());

                client->sendPacket = "{\"text\":\"로그인 성공\"}";
            }
            // 이미 로그인된 유저로부터 명령어 받음
            else
            {
                // 명령별 로직 수행
                Job job;
                if (document.HasMember(Json::PARAM1))
                {
                    job.param1 = document[Json::PARAM1].GetString();
                    job.param2 = document[Json::PARAM2].GetString();
                }

                client->sendPacket = (Logic::handlers[text.GetString()])(client, job);
            }

            // 보낼 패킷 설정 
            if (client->sendPacket != "")
                client->sendTurn = true;
            else
                client->sendTurn = false;

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
        cout << "Send Start to " << client->ID << '\n' << "send msg : " << client->sendPacket << '\n';

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
                client->doingProc.store(false);
            }
        }
    }
}

void SlimeGenerateThread()
{
    cout << "In SlimeGenerateThread, " << Logic::MAX_NUM_OF_SLIME << " 마리 생성\n";
    Logic::SpawnSlime(Logic::MAX_NUM_OF_SLIME);

    //int curNumOfSlime = 0;

    //// 1분마다 슬라임 수가 10마리가 되도록 젠한다.
    //while (true)
    //{
    //    cout << "In SlimeGenerateThread, " << Logic::MAX_NUM_OF_SLIME - curNumOfSlime << " 마리 생성\n";
    //    Logic::SpawnSlime(Logic::MAX_NUM_OF_SLIME - curNumOfSlime);

    //    this_thread::sleep_for(chrono::seconds(Logic::SLIME_GEN_PERIOD));
    //}
}

void SlimeAttackCheckThread()
{
    while (true)
    {
        for (auto& slime : Logic::slimes)
        {
            int slimeLocX = slime->locX;
            int slimeLocY = slime->locY;
            int slimeStr = slime->str;

            for (auto& entry : Server::activeClients)
            {
                // 아직 로그인되지 않은 클라이언트라면 스킵
                if (entry.second->ID == "")
                    continue;

                // 이미 죽은 클라이언트라면 스킵
                if (entry.second->IsDead())
                    continue;

                int userLocX = stoi(Redis::GetLocationX(entry.second->ID));
                int userLocY = stoi(Redis::GetLocationY(entry.second->ID));

                // 슬라임의 공격 범위 안에 유저가 있으면
                if ((slimeLocX - userLocX <= Slime::MAX_X_ATTACK_RANGE && slimeLocX - userLocX >= Slime::MIN_X_ATTACK_RANGE) &&
                    (slimeLocY - userLocY <= Slime::MAX_Y_ATTACK_RANGE && slimeLocY - userLocY >= Slime::MIN_Y_ATTACK_RANGE))
                /*if ((slimeLocX - userLocX <= 29 && slimeLocX - userLocX >= -29) &&
                    (slimeLocY - userLocY <= 29 && slimeLocY - userLocY >= -29))*/
                {
                    string attackMsg = "{\"text\":\"슬라임" + to_string(slime->index) + " 이/가 " + entry.second->ID + " 을/를 공격해서 데미지 " + to_string(slime->str) + " 을/를 가했습니다.\"}";
                    Logic::BroadcastToClients(attackMsg);

                    entry.second->OnAttack(slime);
                    // 공격 받은 클라이언트의 hp가 0일 때
                    if (entry.second->IsDead())
                    {
                        // TODO : 클라이언트가 죽음을 알리는 메시지를 받을 때 그에따라 프로그램 종료 준비 필요
                        string dieMsg = "{\"text\":\"" + entry.second->ID + " 이/가 슬라임" + to_string(slime->index) + " 에 의해 죽었습니다.\"}";
                        cout << dieMsg << '\n';
                        Logic::BroadcastToClients(dieMsg);
                    }
                }
            }
        }
        
        this_thread::sleep_for(chrono::seconds(Logic::SLIME_ATTACK_PERIOD));
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

    // Handler Map 초기화
    Logic::InitHandlers();

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

    // 작업을 처리하는 쓰레드를 추가한다.
    list<shared_ptr<thread> > threads;
    for (int i = 0; i < Server::NUM_WORKER_THREADS; ++i) 
    {
        shared_ptr<thread> workerThread(new thread(workerThreadProc));
        threads.push_back(workerThread);
    }
    // 슬라임 생성 및 공격 작업을 처리하는 쓰레드를 추가한다.
    threads.push_back(shared_ptr<thread>(new thread(SlimeGenerateThread)));
    threads.push_back(shared_ptr<thread>(new thread(SlimeAttackCheckThread)));

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

            if (client->doingProc.load() == false) 
            {
                FD_SET(activeSock, &readSet);
                FD_SET(activeSock, &exceptionSet);
                maxSock = max(maxSock, activeSock);

                // 이 클라이언트가 데이터를 보낼 차례이면 jobQueue에 넣을 수 있도록 한다.
                if (client->sendTurn)
                    ++sendCount;
                // 이 클라이언트가 데이터를 보낼 차례가 아니면
                else
                {
                     // 작업해야할 것이 아무것도 없는 클라이언트이므로 보내야하는 패킷이 있다면 보내도록 설정해준다.
                    if (!Logic::shouldSendPackets[client->sock].empty())
                    {
                        {
                            lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                            client->sendPacket = Logic::shouldSendPackets[client->sock].front();
                            Logic::shouldSendPackets[client->sock].pop_front();
                            client->sendTurn = true;
                            client->packetLen = htonl(client->sendPacket.length());
                            sendCount++;
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

            // 해당 클라이언트가 보내야하는 메시지를 모두 보낸 상태이고 그 클라이언트가 죽었다면 없앤다.
            if (client->sendTurn == false && Logic::shouldSendPackets[activeSock].empty() && client->IsDead())
            {
                // 소켓을 닫는다.
                closesocket(client->sock);

                // 해당 유저에 설정된 Redis의 모든 키를 제거한다.
                Redis::DeleteAllUserKeys(client->ID);

                // 지울 대상에 포함시킨다.
                toDelete.push_back(activeSock);

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
                    if (wasEmpty) {
                        Server::jobQueueFilledCv.notify_one();
                    }
                }
            }
        }

        // 기존 접속에서 예정되어 있던 보내야하는 메시지들을 모두 없앤다.
        {
            lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

            for (auto& closedSock : toDelete)
            {
                while (!Logic::shouldSendPackets[closedSock].empty())
                    Logic::shouldSendPackets[closedSock].pop_front();
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
        workerThread->join();

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