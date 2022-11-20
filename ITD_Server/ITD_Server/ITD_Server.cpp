#include "ITD_Server.h"

SOCKET createPassiveSocket() 
{
    // TCP socket �� �����.
    SOCKET passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passiveSock == INVALID_SOCKET) {
        cerr << "socket failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // socket �� Ư�� �ּ�, ��Ʈ�� ���ε� �Ѵ�.
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

    // ��Ŷ�� �޴´�.
    if (client->sendTurn == false)
    {
        if (client->lenCompleted == false) 
        {
            // network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
            int r = recv(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "recv failed with error " << WSAGetLastError() << endl;
                return false;
            }
            else if (r == 0) 
            {
                // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
                cerr << "Socket closed: " << activeSock << endl;
                return false;
            }
            client->offset += r;

            // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
            if (client->offset < 4) 
            {
                return true;
            }

            // host byte order �� �����Ѵ�.
            int dataLen = ntohl(client->packetLen);
            cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
            client->packetLen = dataLen;

            // Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
            if (client->packetLen > sizeof(client->packet)) 
            {
                cerr << "[" << activeSock << "] Too big data: " << client->packetLen << endl;
                return false;
            }

            // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
            client->lenCompleted = true;
            client->offset = 0;
        }

        // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
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
            // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
            return false;
        }
        client->offset += r;

        if (client->offset == client->packetLen) 
        {
            cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

            // TODO: Ŭ���̾�Ʈ�κ��� ���� �ؽ�Ʈ�� ���� ���� ����
            const string json = string(client->packet).substr(0, client->packetLen);

            Document document;
            document.Parse(json);

            // ��ɾ�
            Value& text = document[Json::TEXT];

            // ù �α���
            if (strcmp(text.GetString(), Json::LOGIN) == 0)
            {
                Redis::RegisterUser(string(document[Json::PARAM1].GetString()));

                if (client->ID == "")
                    client->ID = string(document[Json::PARAM1].GetString());

                client->sendPacket = "{\"text\":\"�α��� ����\"}";
            }
            // �̹� �α��ε� �����κ��� ��ɾ� ����
            else
            {
                // ��ɺ� ���� ����
                Job job;
                if (document.HasMember(Json::PARAM1))
                {
                    job.param1 = document[Json::PARAM1].GetString();
                    job.param2 = document[Json::PARAM2].GetString();
                }

                client->sendPacket = (Logic::handlers[text.GetString()])(client, job);
            }

            // ���� ��Ŷ ���� 
            if (client->sendPacket != "")
                client->sendTurn = true;
            else
                client->sendTurn = false;

            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
            client->lenCompleted = false;
            client->offset = 0;
            if (client->sendTurn)
                client->packetLen = htonl(client->sendPacket.length());
            else
                client->packetLen = 0;
        }

        return true;
    }

    // ���� ��Ŷ�� ���� ������ ������.
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

            // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
            if (client->offset < 4) 
            {
                return true;
            }

            // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
            client->lenCompleted = true;
            client->offset = 0;
            client->packetLen = ntohl(client->packetLen);
        }

        // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
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

            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
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
    cout << "In SlimeGenerateThread, " << Logic::MAX_NUM_OF_SLIME << " ���� ����\n";
    Logic::SpawnSlime(Logic::MAX_NUM_OF_SLIME);

    //int curNumOfSlime = 0;

    //// 1�и��� ������ ���� 10������ �ǵ��� ���Ѵ�.
    //while (true)
    //{
    //    cout << "In SlimeGenerateThread, " << Logic::MAX_NUM_OF_SLIME - curNumOfSlime << " ���� ����\n";
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
                // ���� �α��ε��� ���� Ŭ���̾�Ʈ��� ��ŵ
                if (entry.second->ID == "")
                    continue;

                // �̹� ���� Ŭ���̾�Ʈ��� ��ŵ
                if (entry.second->IsDead())
                    continue;

                int userLocX = stoi(Redis::GetLocationX(entry.second->ID));
                int userLocY = stoi(Redis::GetLocationY(entry.second->ID));

                // �������� ���� ���� �ȿ� ������ ������
                if ((slimeLocX - userLocX <= Slime::MAX_X_ATTACK_RANGE && slimeLocX - userLocX >= Slime::MIN_X_ATTACK_RANGE) &&
                    (slimeLocY - userLocY <= Slime::MAX_Y_ATTACK_RANGE && slimeLocY - userLocY >= Slime::MIN_Y_ATTACK_RANGE))
                /*if ((slimeLocX - userLocX <= 29 && slimeLocX - userLocX >= -29) &&
                    (slimeLocY - userLocY <= 29 && slimeLocY - userLocY >= -29))*/
                {
                    string attackMsg = "{\"text\":\"������" + to_string(slime->index) + " ��/�� " + entry.second->ID + " ��/�� �����ؼ� ������ " + to_string(slime->str) + " ��/�� ���߽��ϴ�.\"}";
                    Logic::BroadcastToClients(attackMsg);

                    entry.second->OnAttack(slime);
                    // ���� ���� Ŭ���̾�Ʈ�� hp�� 0�� ��
                    if (entry.second->IsDead())
                    {
                        // TODO : Ŭ���̾�Ʈ�� ������ �˸��� �޽����� ���� �� �׿����� ���α׷� ���� �غ� �ʿ�
                        string dieMsg = "{\"text\":\"" + entry.second->ID + " ��/�� ������" + to_string(slime->index) + " �� ���� �׾����ϴ�.\"}";
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
    // hiredis ����
    Redis::redis = redisConnect(Server::SERVER_ADDRESS, 6379);
    if (Redis::redis == NULL || Redis::redis->err)
    {
        if (Redis::redis)
            printf("Error: %s\n", Redis::redis->errstr);
        else
            printf("Can't allocate redis context\n");
        return 1;
    }

    // Handler Map �ʱ�ȭ
    Logic::InitHandlers();

    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) 
    {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // passive socket �� ������ش�.
    SOCKET passiveSock = createPassiveSocket();

    // �۾��� ó���ϴ� �����带 �߰��Ѵ�.
    list<shared_ptr<thread> > threads;
    for (int i = 0; i < Server::NUM_WORKER_THREADS; ++i) 
    {
        shared_ptr<thread> workerThread(new thread(workerThreadProc));
        threads.push_back(workerThread);
    }
    // ������ ���� �� ���� �۾��� ó���ϴ� �����带 �߰��Ѵ�.
    threads.push_back(shared_ptr<thread>(new thread(SlimeGenerateThread)));
    threads.push_back(shared_ptr<thread>(new thread(SlimeAttackCheckThread)));

    while (true) 
    {
        int sendCount = 0;

        fd_set readSet, exceptionSet;

        // ���� socket set �� �ʱ�ȭ�Ѵ�.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        SOCKET maxSock = -1;

        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // ���� �����ִ� active socket �鿡 ���ؼ��� ��� set �� �־��ش�.
        for (auto& entry : Server::activeClients) 
        {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (client->doingProc.load() == false) 
            {
                FD_SET(activeSock, &readSet);
                FD_SET(activeSock, &exceptionSet);
                maxSock = max(maxSock, activeSock);

                // �� Ŭ���̾�Ʈ�� �����͸� ���� �����̸� jobQueue�� ���� �� �ֵ��� �Ѵ�.
                if (client->sendTurn)
                    ++sendCount;
                // �� Ŭ���̾�Ʈ�� �����͸� ���� ���ʰ� �ƴϸ�
                else
                {
                     // �۾��ؾ��� ���� �ƹ��͵� ���� Ŭ���̾�Ʈ�̹Ƿ� �������ϴ� ��Ŷ�� �ִٸ� �������� �������ش�.
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

        // ȸ���� �� ���� �����̴�. ������ �ߴ��Ѵ�.
        if (r == SOCKET_ERROR) 
        {
            cerr << "select failed: " << WSAGetLastError() << endl;
            break;
        }

        // �ƹ��͵� ��ȯ�� ���� ���� �Ʒ��� ó������ �ʰ� �ٷ� �ٽ� select �� �ϰ� �Ѵ�.
        // select �� ��ȯ���� ������ �� SOCKET_ERROR, �� ���� ��� �̺�Ʈ�� �߻��� ���� �����̴�.
        if (r == 0 && sendCount == 0) 
        {
            continue;
        }

        // passive socket �� readable �ϴٸ� �̴� �� ������ ���Դٴ� ���̴�.
        if (FD_ISSET(passiveSock, &readSet)) 
        {                                                                       
            // passive socket �� �̿��� accept() �� �Ѵ�.
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

        // ���� �̺�Ʈ�� �߻��ϴ� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
        list<SOCKET> toDelete;
        for (auto& entry : Server::activeClients)
        {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (FD_ISSET(activeSock, &exceptionSet)) 
            {
                cerr << "Exception on socket " << activeSock << endl;

                // ������ �ݴ´�.
                closesocket(activeSock);

                // ���� ��� ���Խ�Ų��.
                toDelete.push_back(activeSock);

                // ������ ���� ��� �� �̻� ó���� �ʿ䰡 ������ �Ʒ� read �۾��� ���� �ʴ´�.
                continue;
            }

            // �ش� Ŭ���̾�Ʈ�� �������ϴ� �޽����� ��� ���� �����̰� �� Ŭ���̾�Ʈ�� �׾��ٸ� ���ش�.
            if (client->sendTurn == false && Logic::shouldSendPackets[activeSock].empty() && client->IsDead())
            {
                // ������ �ݴ´�.
                closesocket(client->sock);

                // �ش� ������ ������ Redis�� ��� Ű�� �����Ѵ�.
                Redis::DeleteAllUserKeys(client->ID);

                // ���� ��� ���Խ�Ų��.
                toDelete.push_back(activeSock);

                continue;
            }

            // �б� �̺�Ʈ�� �߻��ϴ� ������ ��� recv() �� ó���Ѵ�.
            if (FD_ISSET(activeSock, &readSet) || client->sendTurn) 
            {
                // ���� �ٽ� select ����� ���� �ʵ��� client �� flag �� ���ش�.
                client->doingProc.store(true);

                {
                    lock_guard<mutex> lg(Server::jobQueueMutex);

                    bool wasEmpty = Server::jobQueue.empty();
                    Server::jobQueue.push(client);

                    // ���ǹ��ϰ� CV �� notify���� �ʵ��� ť�� ���̰� 0���� 1�� �Ǵ� ���� notify �� �ϵ��� ����.
                    if (wasEmpty) {
                        Server::jobQueueFilledCv.notify_one();
                    }
                }
            }
        }

        // ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ش�.
        {
            lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

            for (auto& closedSock : toDelete)
            {
                while (!Logic::shouldSendPackets[closedSock].empty())
                    Logic::shouldSendPackets[closedSock].pop_front();
            }
        }

        // ���� ���� �־��ٸ� �����.
        {
            lock_guard<mutex> lg(Server::activeClientsMutex);

            for (auto& closedSock : toDelete)
            {
                // �ʿ��� ����� ��ü�� �����ش�.
                Server::activeClients.erase(closedSock);
            }
        }
    }

    // ���� threads ���� join �Ѵ�.
    for (shared_ptr<thread>& workerThread : threads) 
        workerThread->join();

    // ������ ��ٸ��� passive socket �� �ݴ´�.
    r = closesocket(passiveSock);
    if (r == SOCKET_ERROR) 
    {
        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}