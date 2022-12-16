#include "ITD_Server.h"

SOCKET createPassiveSocket() 
{
    // TCP socket �� �����.
    SOCKET passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passiveSock == INVALID_SOCKET) {
        cerr << "[����] socket failed with error " << WSAGetLastError() << endl;
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
        cerr << "[����] bind failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    r = listen(passiveSock, 10);
    if (r == SOCKET_ERROR) {
        cerr << "[����] listen failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    return passiveSock;
}

bool processClient(shared_ptr<Client>& client)
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
                cerr << "[����] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
                return false;
            }
            else if (r == 0) 
            {
                // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
                cerr << "[����] [" << activeSock << "] Socket closed" << endl;
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
            //cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
            client->packetLen = dataLen;

            // Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
            if (client->packetLen > sizeof(client->packet)) 
            {
                cerr << "[����] [" << activeSock << "] Too big data: " << client->packetLen << endl;
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
            cerr << "[����] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        else if (r == 0) 
        {
            // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
            cerr << "[����] [" << activeSock << "] recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        client->offset += r;

        if (client->offset == client->packetLen) 
        {
            //cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

            // Ŭ���̾�Ʈ�κ��� ���� ��ɾ ���� ���� ����
            const string json = string(client->packet).substr(0, client->packetLen);

            Document document;
            document.Parse(json);

            // ��ɾ�
            Value& text = document[Json::TEXT];

            // Ŭ���̾�Ʈ�� �α��� �õ�
            if (strcmp(text.GetString(), Json::LOGIN) == 0)
            {
                client->sendPacket = Redis::RegisterUser(string(document[Json::PARAM1].GetString()), client->sock);

                if (client->ID == "")
                    client->ID = string(document[Json::PARAM1].GetString());
            }
            // �̹� �α��ε� �����κ��� ��ɾ� ����
            else
            {
                // ��ɺ� ���� ����
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

            // ���� ��Ŷ�� �����ϸ� �������� ���� 
            if (client->sendPacket != "")
                client->sendTurn = true;
            else
                client->sendTurn = false;

            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
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

    // ���� ��Ŷ�� ���� ������ ������.
    if (client->sendTurn)
    {   
        //cout << "[" << activeSock << "] Send Start to " << client->ID << ", send msg : " << client->sendPacket << '\n';

        // ������ ���̸� ������.
        if (client->lenCompleted == false) 
        {
            int r = send(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
            if (r == SOCKET_ERROR) 
            {
                cerr << "[����] [" << activeSock << "] send failed with error " << WSAGetLastError() << endl;
                return false;
            }
            client->offset += r;

            // ��� ������ ���ߴٸ� �������� ��� �õ��� ���̴�.
            if (client->offset < 4) 
            {
                return true;
            }

            // ���� packetLen �� ��� ���´ٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
            client->lenCompleted = true;
            client->offset = 0;
            client->packetLen = ntohl(client->packetLen);
        }

        // ������� �����ߴٴ� ���� packetLen �� ��� ���� ����̴�. (== lenCompleted �� true)
        if (client->lenCompleted == false) 
        {
            return true;
        }

        // �����͸� ������.
        int r = send(activeSock, client->sendPacket.c_str() + client->offset, client->packetLen - client->offset, 0);
        if (r == SOCKET_ERROR) 
        {
            cerr << "[����] [" << activeSock << "] send failed with error " << WSAGetLastError() << endl;
            return false;
        }
        client->offset += r;

        // �����͸� ��� ���´ٸ�
        if (client->offset == client->packetLen) 
        {
            //cout << "[" << activeSock << "] Sent " << client->packetLen << " bytes" << endl;

            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
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
                // ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ش�.
                {
                    lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                    Logic::shouldSendPackets[activeSock].clear();
                }

                // �ش� Ŭ���̾�Ʈ�� ��� Ű�� expire�Ѵ�.
                Redis::ExpireUser(client->ID);

                // �ش� Ŭ���̾�Ʈ�� �����.
                {
                    lock_guard<mutex> lg(Server::activeClientsMutex);

                    Server::activeClients.erase(activeSock);
                }

                // ������ �ݴ´�.
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

// 1�и��� �������� ���� �ִ� ����ŭ �����ϵ��� �����ϴ� �Լ�
void SlimeGenerateThread()
{
    // 1�и��� ������ ���� 10������ �ǵ��� ���Ѵ�.
    while (true)
    {
        cout << "[�ý���] ������ " << Logic::MAX_NUM_OF_SLIME - Logic::slimes.size() << "���� ����\n";
        Logic::SpawnSlime(Logic::MAX_NUM_OF_SLIME - Logic::slimes.size());

        this_thread::sleep_for(chrono::seconds(Logic::SLIME_GEN_PERIOD));
    }
}

int main()
{
    // ���α׷� �̸� ����
    system("title Into The Dungeon Server");

    // hiredis ����
    Redis::redis = redisConnect(Server::SERVER_ADDRESS, Redis::DEFAULT_REDIS_PORT);
    if (Redis::redis == NULL || Redis::redis->err)
    {
        if (Redis::redis)
            cerr << "[����] " << Redis::redis->errstr << '\n';
        else
            cout << "[����] Can't allocate redis context\n";
        return 1;
    }

    // ���� ���� �� Redis �ʱ�ȭ
    Redis::Flushall();

    // Handler Map �ʱ�ȭ
    Logic::InitHandlers();

    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) 
    {
        cerr << "[����] WSAStartup failed with error " << r << endl;
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

    while (true) 
    {
        // ������ ������ Ȥ�� ���ŵǾ���� Ŭ���̾�Ʈ�� �����ϴ��� ��Ÿ���� ����
        bool shouldPass = false;

        fd_set readSet, exceptionSet;

        // ���� socket set �� �ʱ�ȭ�Ѵ�.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        SOCKET maxSock = -1;

        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // ���� �����ִ� active socket �鿡 ���ؼ��� ��� set �� �־��ش�.
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

                    // ���� �ش� Ŭ���̾�Ʈ�� �׾��ų� �ߺ� �α������� ���� ���ŵǾ�� �ϰų� �����͸� ������ ���� �۾� ť�� �־����� �Ѵٸ� shouldPass�� true�� ����
                    if (client->shouldTerminate || client->sendTurn)
                    {
                        shouldPass = true;
                    }
                    // �� Ŭ���̾�Ʈ�� �����͸� ���� ���ʰ� �ƴϸ�
                    else
                    {
                        // �α��ε� Ŭ���̾�Ʈ�� �۾��ؾ��� ���� �ƹ��͵� ���� Ŭ���̾�Ʈ�̹Ƿ� �������ϴ� ��Ŷ�� �ִٸ� �������� �������ش�.
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

        // ȸ���� �� ���� �����̴�. ������ �ߴ��Ѵ�.
        if (r == SOCKET_ERROR) 
        {
            cerr << "[����] select failed: " << WSAGetLastError() << endl;
            break;
        }

        // �ƹ��͵� ��ȯ�� ���� ���� �Ʒ��� ó������ �ʰ� �ٷ� �ٽ� select �� �ϰ� �Ѵ�.
        // select �� ��ȯ���� ������ �� SOCKET_ERROR, �� ���� ��� �̺�Ʈ�� �߻��� ���� �����̴�.
        if (r == 0 && !shouldPass) 
        {
            continue;
        }

        // passive socket �� readable �ϴٸ� �̴� �� ������ ���Դٴ� ���̴�.
        if (FD_ISSET(passiveSock, &readSet)) 
        {                                                                       
            // passive socket �� �̿��� accept() �� �Ѵ�.
            cout << "[�ý���] Waiting for a connection" << endl;
            struct sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);
            if (activeSock == INVALID_SOCKET) 
            {
                cerr << "[����] accept failed with error " << WSAGetLastError() << endl;
                return 1;
            }
            else 
            {
                shared_ptr<Client> newClient(new Client(activeSock));

                Server::activeClients.insert(make_pair(activeSock, newClient));

                char strBuf[1024];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), strBuf, sizeof(strBuf));
                cout << "[�ý���] New client from " << strBuf << ":" << ntohs(clientAddr.sin_port) << ". "
                    << "Socket: " << activeSock << endl;
            }
        }

        // ���� �̺�Ʈ�� �߻��ϴ� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
        {
            lock_guard<mutex> lg(Server::activeClientsMutex);

            for (auto it = Server::activeClients.begin(); it != Server::activeClients.end();)
            {
                SOCKET activeSock = it->first;
                shared_ptr<Client> client = it->second;

                if (FD_ISSET(activeSock, &exceptionSet))
                {
                    cerr << "[����] Exception on socket " << activeSock << endl;

                    // ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ش�.
                    {
                        lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                        Logic::shouldSendPackets[activeSock].clear();
                    }

                    // ��� Ű�� Expire�Ѵ�.
                    Redis::ExpireUser(client->ID);

                    // �ش� Ŭ���̾�Ʈ�� �����.
                    Server::activeClients.erase(it++);

                    // ������ �ݴ´�.
                    if (activeSock != INVALID_SOCKET)
                    {
                        cout << "[Exception on socket] socket closed : " << activeSock << '\n';
                        closesocket(activeSock);
                    }

                    // ������ ���� ��� �� �̻� ó���� �ʿ䰡 ������ �Ʒ� read �۾��� ���� �ʴ´�.
                    continue;
                }

                // �ش� Ŭ���̾�Ʈ�� ó���ؾ� �� �۾��� ��� �Ϸ��� �����̰� �� Ŭ���̾�Ʈ�� �׾��ų� �����Ǿ���ϸ� ���ش�.
                if (client->shouldTerminate && Logic::shouldSendPackets[activeSock].empty() && client->doingProc.load() == false)
                {
                    // ���� ���ӿ��� �����Ǿ� �ִ� �������ϴ� �޽������� ��� ���ش�.
                    {
                        lock_guard<mutex> lg(Logic::shouldSendPacketsMutex);

                        Logic::shouldSendPackets[activeSock].clear();
                    }

                    // ������ �׾��ٸ� �ش� ������ ������ Redis�� ��� Ű�� �����Ѵ�.
                    if (stoi(Redis::GetHp(client->ID)) <= 0)
                    {
                        Redis::DeleteAllUserKeys(client->ID);
                    }

                    // �ش� Ŭ���̾�Ʈ�� �����.
                    Server::activeClients.erase(it++);

                    // ������ �ݴ´�.
                    if (activeSock != INVALID_SOCKET)
                    {
                        cout << "[toDelete] socket closed : " << client->sock << '\n';
                        closesocket(client->sock);
                    }

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

    // ���� threads ���� join �Ѵ�.
    for (shared_ptr<thread>& workerThread : threads) 
        workerThread->join();

    // ������ ��ٸ��� passive socket �� �ݴ´�.
    if (passiveSock != INVALID_SOCKET)
    {
        cout << "[final] close socket : " << passiveSock << '\n';
        r = closesocket(passiveSock);
        if (r == SOCKET_ERROR) 
        {
            cerr << "[����] closesocket(passive) failed with error " << WSAGetLastError() << endl;
            return 1;
        }
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}