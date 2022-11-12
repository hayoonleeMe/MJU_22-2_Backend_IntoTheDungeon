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
    serverAddr.sin_port = htons(SERVER_PORT);
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

    if (client->lenCompleted == false) {
        // network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
        int r = recv(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
        if (r == SOCKET_ERROR) {
            cerr << "recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        else if (r == 0) {
            // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
            cerr << "Socket closed: " << activeSock << endl;
            return false;
        }
        client->offset += r;

        // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
        if (client->offset < 4) {
            return true;
        }

        // host byte order �� �����Ѵ�.
        int dataLen = ntohl(client->packetLen);
        cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
        client->packetLen = dataLen;

        // Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
        if (client->packetLen > sizeof(client->packet)) {
            cerr << "[" << activeSock << "] Too big data: " << client->packetLen << endl;
            return false;
        }

        // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
        client->lenCompleted = true;
        client->offset = 0;
    }

    // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
    if (client->lenCompleted == false) {
        return true;
    }

    int r = recv(activeSock, client->packet + client->offset, client->packetLen - client->offset, 0);
    if (r == SOCKET_ERROR) {
        cerr << "recv failed with error " << WSAGetLastError() << endl;
        return false;
    }
    else if (r == 0) {
        // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
        return false;
    }
    client->offset += r;
        
    if (client->offset == client->packetLen) {
        cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

        // TODO: Ŭ���̾�Ʈ�κ��� ���� �ؽ�Ʈ�� ���� ���� ����
        const string json = string(client->packet).substr(0, client->packetLen);

        Document d;
        d.Parse(json);

        // ��ɾ�
        Value& text = d[Json::TEXT];

        // ù �α���
        if (strcmp(text.GetString(), Json::LOGIN) == 0)
        {
            {
                lock_guard<mutex> lg(Redis::redisMutex);

                Redis::RegisterUser(string(d[Json::PARAM1].GetString()));
            }

            if (client->ID == NONE)
                client->ID = string(d[Json::PARAM1].GetString());
        }
        // �̹� �α��ε� �����κ��� ��ɾ� ����
        else
        {
            // TODO : ��ɺ� ���� ����
        }

        // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
        client->lenCompleted = false;
        client->offset = 0;
        client->packetLen = 0;
    }

    return true;
}

void workerThreadProc() {

    while (true) {
        shared_ptr<Client> client;
        {
            unique_lock<mutex> ul(jobQueueMutex);

            while (jobQueue.empty()) {
                jobQueueFilledCv.wait(ul);
            }

            client = jobQueue.front();
            jobQueue.pop();

        }

        if (client) {
            SOCKET activeSock = client->sock;
            bool successful = processClient(client);
            if (successful == false) {
                closesocket(activeSock);

                {
                    lock_guard<mutex> lg(activeClientsMutex);

                    activeClients.erase(activeSock);
                }
            }
            else {
                client->doingRecv.store(false);
            }
        }
    }
}

int main()
{
    // hiredis ����
    Redis::redis = redisConnect(SERVER_ADDRESS, 6379);
    if (Redis::redis == NULL || Redis::redis->err)
    {
        if (Redis::redis)
            printf("Error: %s\n", Redis::redis->errstr);
        else
            printf("Can't allocate redis context\n");
        return 1;
    }

    int r = 0;

    // Winsock �� �ʱ�ȭ�Ѵ�.
    WSADATA wsaData;
    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != NO_ERROR) {
        cerr << "WSAStartup failed with error " << r << endl;
        return 1;
    }

    // passive socket �� ������ش�.
    SOCKET passiveSock = createPassiveSocket();

    list<shared_ptr<thread> > threads;
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        shared_ptr<thread> workerThread(new thread(workerThreadProc));
        threads.push_back(workerThread);
    }

    while (true) {
        fd_set readSet, exceptionSet;

        // ���� socket set �� �ʱ�ȭ�Ѵ�.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        SOCKET maxSock = -1;

        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // ���� �����ִ� active socket �鿡 ���ؼ��� ��� set �� �־��ش�.
        for (auto& entry : activeClients) {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (client->doingRecv.load() == false) {
                FD_SET(activeSock, &readSet);
                FD_SET(activeSock, &exceptionSet);
                maxSock = max(maxSock, activeSock);
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        r = select(maxSock + 1, &readSet, NULL, &exceptionSet, &timeout);

        // ȸ���� �� ���� �����̴�. ������ �ߴ��Ѵ�.
        if (r == SOCKET_ERROR) {
            cerr << "select failed: " << WSAGetLastError() << endl;
            break;
        }

        // �ƹ��͵� ��ȯ�� ���� ���� �Ʒ��� ó������ �ʰ� �ٷ� �ٽ� select �� �ϰ� �Ѵ�.
        // select �� ��ȯ���� ������ �� SOCKET_ERROR, �� ���� ��� �̺�Ʈ�� �߻��� ���� �����̴�.
        if (r == 0) {
            continue;
        }

        // passive socket �� readable �ϴٸ� �̴� �� ������ ���Դٴ� ���̴�.
        if (FD_ISSET(passiveSock, &readSet)) {
            // passive socket �� �̿��� accept() �� �Ѵ�.
            cout << "Waiting for a connection" << endl;
            struct sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);

            if (activeSock == INVALID_SOCKET) {
                cerr << "accept failed with error " << WSAGetLastError() << endl;
                return 1;
            }
            else {
                shared_ptr<Client> newClient(new Client(activeSock));

                activeClients.insert(make_pair(activeSock, newClient));

                char strBuf[1024];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), strBuf, sizeof(strBuf));
                cout << "New client from " << strBuf << ":" << ntohs(clientAddr.sin_port) << ". "
                    << "Socket: " << activeSock << endl;
            }
        }

        // ���� �̺�Ʈ�� �߻��ϴ� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
        list<SOCKET> toDelete;
        for (auto& entry : activeClients) {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (FD_ISSET(activeSock, &exceptionSet)) {
                cerr << "Exception on socket " << activeSock << endl;

                // ������ �ݴ´�.
                closesocket(activeSock);

                // ���� ��� ���Խ�Ų��.
                toDelete.push_back(activeSock);

                // ������ ���� ��� �� �̻� ó���� �ʿ䰡 ������ �Ʒ� read �۾��� ���� �ʴ´�.
                continue;
            }

            // �б� �̺�Ʈ�� �߻��ϴ� ������ ��� recv() �� ó���Ѵ�.
            if (FD_ISSET(activeSock, &readSet)) {
                // ���� �ٽ� select ����� ���� �ʵ��� client �� flag �� ���ش�.
                client->doingRecv.store(true);

                {
                    lock_guard<mutex> lg(jobQueueMutex);

                    bool wasEmpty = jobQueue.empty();
                    jobQueue.push(client);

                    // ���ǹ��ϰ� CV �� notify���� �ʵ��� ť�� ���̰� 0���� 1�� �Ǵ� ���� notify �� �ϵ��� ����.
                    if (wasEmpty) {
                        jobQueueFilledCv.notify_one();
                    }
                }
            }
        }

        // ���� ���� �־��ٸ� �����.
        for (auto& closedSock : toDelete) 
        {
            // �ʿ��� ����� ��ü�� �����ش�.
            activeClients.erase(closedSock);
        }
    }

    // ���� threads ���� join �Ѵ�.
    for (shared_ptr<thread>& workerThread : threads) {
        workerThread->join();
    }

    // ������ ��ٸ��� passive socket �� �ݴ´�.
    r = closesocket(passiveSock);
    if (r == SOCKET_ERROR) {
        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
        return 1;
    }

    // Winsock �� �����Ѵ�.
    WSACleanup();
    return 0;
}