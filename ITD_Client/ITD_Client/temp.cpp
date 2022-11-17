//#include "ITD_Client.h"
//
//bool processResponse(Response* response)
//{
//    SOCKET responseSock = response->sock;
//
//    // ��Ŷ�� �޴´�.
//    if (response->sendTurn == false)
//    {
//        if (response->lenCompleted == false)
//        {
//            // network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
//            int r = recv(responseSock, (char*)&(response->packetLen) + response->offset, 4 - response->offset, 0);
//            if (r == SOCKET_ERROR)
//            {
//                cerr << "recv failed with error " << WSAGetLastError() << endl;
//                return false;
//            }
//            else if (r == 0)
//            {
//                // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
//                cerr << "Socket closed: " << responseSock << endl;
//                return false;
//            }
//            response->offset += r;
//
//            // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
//            if (response->offset < 4)
//            {
//                return true;
//            }
//
//            // host byte order �� �����Ѵ�.
//            int dataLen = ntohl(response->packetLen);
//            cout << "[" << responseSock << "] Received length info: " << dataLen << endl;
//            response->packetLen = dataLen;
//
//            // Ȥ�� �츮�� ���� �����Ͱ� 64KB���� ū�� Ȯ���Ѵ�.
//            if (response->packetLen > sizeof(response->packet))
//            {
//                cerr << "[" << responseSock << "] Too big data: " << response->packetLen << endl;
//                return false;
//            }
//
//            // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
//            response->lenCompleted = true;
//            response->offset = 0;
//        }
//
//        // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
//        if (response->lenCompleted == false)
//        {
//            return true;
//        }
//
//        int r = recv(responseSock, response->packet + response->offset, response->packetLen - response->offset, 0);
//        if (r == SOCKET_ERROR)
//        {
//            cerr << "recv failed with error " << WSAGetLastError() << endl;
//            return false;
//        }
//        else if (r == 0)
//        {
//            // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
//            return false;
//        }
//        response->offset += r;
//
//        if (response->offset == response->packetLen)
//        {
//            cout << "[" << responseSock << "] Received " << response->packetLen << " bytes" << endl;
//
//            // TODO: Ŭ���̾�Ʈ�κ��� ���� �ؽ�Ʈ�� ���� ���� ����
//            const string json = string(response->packet).substr(0, response->packetLen);
//
//            cout << json << '\n';
//
//            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
//            response->lenCompleted = false;
//            response->offset = 0;
//            response->packetLen = 0;
//            memset
//        }
//
//        return true;
//    }
//
//    // ���� ��Ŷ�� ���� ������ ������.
//    if (response->sendTurn)
//    {
//        cout << "Send Start : " << response->ID << '\n';
//
//        if (response->lenCompleted == false)
//        {
//            int r = send(responseSock, (char*)&(response->packetLen) + response->offset, 4 - response->offset, 0);
//            if (r == SOCKET_ERROR)
//            {
//                cerr << "send failed with error " << WSAGetLastError() << endl;
//                return false;
//            }
//            response->offset += r;
//
//            // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
//            if (response->offset < 4)
//            {
//                return true;
//            }
//
//            // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
//            response->lenCompleted = true;
//            response->offset = 0;
//            response->packetLen = ntohl(response->packetLen);
//        }
//
//        // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
//        if (response->lenCompleted == false)
//        {
//            return true;
//        }
//
//        int r = send(responseSock, response->sendPacket.c_str() + response->offset, response->packetLen - response->offset, 0);
//        if (r == SOCKET_ERROR)
//        {
//            cerr << "send failed with error " << WSAGetLastError() << endl;
//            return false;
//        }
//        response->offset += r;
//
//        if (response->offset == response->packetLen)
//        {
//            cout << "[" << responseSock << "] Sent " << response->packetLen << " bytes" << endl;
//
//            // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
//            response->sendTurn = false;
//            response->lenCompleted = false;
//            response->offset = 0;
//            response->packetLen = 0;
//        }
//
//        return true;
//    }
//}
//
//void GetInput()
//{
//    while (true)
//    {
//        string input;
//        cin >> input;
//
//        {
//            shared_ptr<Response> newResponse(new Response(sock));
//            newResponse->sendTurn = true;
//            newResponse->sendPacket = input;
//            newResponse->packetLen = htonl(input.length());
//
//            //Client::activeResponses.insert(make_pair(newResponse->count, newResponse));
//        }
//    }
//}
//
//void workerThreadProc() {
//    while (true) {
//        {
//            unique_lock<mutex> ul(serverConnectMutex);
//
//            while (serverConnect->doingRecv.load() == true) {
//                canProcessServerConnect.wait(ul);
//            }
//            serverConnect->doingRecv.store(true);
//        }
//
//        if (serverConnect) {
//            bool successful = processResponse(serverConnect);
//            if (successful == false)
//            {
//                {
//                    lock_guard<mutex> lg(serverConnectMutex);
//
//                    // serverConnect ��ü �ʱ�ȭ
//                    InitServerConnect();
//                    //Client::activeResponses.erase(response->count);
//                }
//            }
//            else
//            {
//                lock_guard<mutex> lg(serverConnectMutex);
//
//                serverConnect->doingRecv.store(false);
//            }
//        }
//    }
//}
//
//int main()
//{
//    int r = 0;
//
//    // Winsock �� �ʱ�ȭ�Ѵ�.
//    WSADATA wsaData;
//    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
//    if (r != NO_ERROR) {
//        cerr << "WSAStartup failed with error " << r << endl;
//        return 1;
//    }
//
//    // passive socket �� ������ش�.
//    //SOCKET passiveSock = createPassiveSocket();
//
//    struct sockaddr_in serverAddr;
//
//    // TCP socket �� �����.
//    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    if (sock == INVALID_SOCKET) {
//        std::cerr << "socket failed with error " << WSAGetLastError() << std::endl;
//        return 1;
//    }
//
//    // TCP �� ���� ����̴�. ���� �ּҸ� ���ϰ� connect() �� �����Ѵ�.
//    // connect �Ŀ��� ������ ���� �ּҸ� �������� �ʰ� send/recv �� �Ѵ�.
//    serverAddr.sin_family = AF_INET;
//    serverAddr.sin_port = htons(Client::SERVER_PORT);
//    inet_pton(AF_INET, Client::SERVER_ADDRESS, &serverAddr.sin_addr);
//    r = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
//    if (r == SOCKET_ERROR) {
//        std::cerr << "connect failed with error " << WSAGetLastError() << std::endl;
//        return 1;
//    }
//
//    InitServerConnect();
//
//    list<shared_ptr<thread> > threads;
//    for (int i = 0; i < Client::NUM_WORKER_THREADS; ++i) {
//        shared_ptr<thread> workerThread(new thread(workerThreadProc));
//        threads.push_back(workerThread);
//    }
//    // �����͸� �޴� �����带 �߰��Ѵ�.
//    shared_ptr<thread> receiveThread(new thread(workerThreadProc));
//    // �Է��� �޴� �����带 �߰��Ѵ�.
//    shared_ptr<thread> inputThread(new thread(GetInput));
//
//    while (true) {
//        int sendCount = 0;
//
//        fd_set readSet, exceptionSet;
//
//        // ���� socket set �� �ʱ�ȭ�Ѵ�.
//        FD_ZERO(&readSet);
//        FD_ZERO(&exceptionSet);
//
//        SOCKET maxSock = -1;
//
//        FD_SET(sock, &readSet);
//        FD_SET(sock, &exceptionSet);
//        maxSock = max(maxSock, sock);
//
//        // ���� �����ִ� active socket �鿡 ���ؼ��� ��� set �� �־��ش�.
//        /*for (auto& entry : Client::activeResponses) {
//            SOCKET responseSock = entry.second->sock;
//            shared_ptr<Response> response = entry.second;
//
//            if (response->doingRecv.load() == false) {
//                FD_SET(responseSock, &readSet);
//                FD_SET(responseSock, &exceptionSet);
//                maxSock = max(maxSock, responseSock);
//
//                if (response->sendTurn)
//                    ++sendCount;
//            }
//        }*/
//
//        struct timeval timeout;
//        timeout.tv_sec = 0;
//        timeout.tv_usec = 100;
//        r = select(maxSock + 1, &readSet, NULL, &exceptionSet, &timeout);
//
//        // ȸ���� �� ���� �����̴�. Ŭ���̾�Ʈ�� �ߴ��Ѵ�.
//        if (r == SOCKET_ERROR) {
//            cerr << "select failed: " << WSAGetLastError() << endl;
//            break;
//        }
//
//        // �ƹ��͵� ��ȯ�� ���� ���� �Ʒ��� ó������ �ʰ� �ٷ� �ٽ� select �� �ϰ� �Ѵ�.
//        // select �� ��ȯ���� ������ �� SOCKET_ERROR, �� ���� ��� �̺�Ʈ�� �߻��� ���� �����̴�.
//        if (r == 0 && !serverConnect->sendTurn) {
//            continue;
//        }
//
//        // socket �� readable �ϴٸ� �̴� �� ������ ���Դٴ� ���̴�.
//        if (FD_ISSET(sock, &readSet)) {
//            // passive socket �� �̿��� accept() �� �Ѵ�.
//            /*cout << "Waiting for a connection" << endl;
//            struct sockaddr_in clientAddr;
//            int clientAddrSize = sizeof(clientAddr);
//            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);*/
//
//            //shared_ptr<Response> newResponse(new Response(sock));
//
//            //Client::activeResponses.insert(make_pair(newResponse->count, newResponse));
//
//            /*char strBuf[1024];
//            inet_ntop(AF_INET, &(serverAddr.sin_addr), strBuf, sizeof(strBuf));
//            cout << "New response from " << strBuf << ":" << ntohs(serverAddr.sin_port) << ". "
//                << "Socket: " << sock << endl;*/
//
//            if (serverConnect->doingRecv.load() == false)
//            {
//                // ���� �ٽ� select ����� ���� �ʵ��� client �� flag �� ���ش�.
//
//                {
//                    lock_guard<mutex> lg(serverConnectMutex);
//
//                    canProcessServerConnect.notify_one();
//                }
//            }
//        }
//
//        // ���� �̺�Ʈ�� �߻��ϴ� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
//        if (FD_ISSET(sock, &exceptionSet))
//        {
//            cerr << "Exception on socket " << sock << endl;
//            {
//                lock_guard<mutex> lg(serverConnectMutex);
//
//                InitServerConnect();
//            }
//            /*{
//                lock_guard<mutex> lg(Client::activeResponsesMutex);
//
//                for (auto& entry : Client::activeResponses)
//                    Client::activeResponses.erase(entry.first);
//                continue;
//            }*/
//        }
//
//
//
//        //for (auto& entry : Client::activeResponses) 
//        //{
//        //    shared_ptr<Response> response = entry.second;
//
//        //    // ������ �ݴ´�.
//        //    //closesocket(responseSock);
//
//        //    // ���� ��� ���Խ�Ų��.
//        //    //toDelete.push_back(entry.first);
//
//        //    // ������ ���� ��� �� �̻� ó���� �ʿ䰡 ������ �Ʒ� read �۾��� ���� �ʴ´�.
//        //    //continue;
//
//        //    // �б� �̺�Ʈ�� �߻��ϴ� ������ ��� recv() �� ó���Ѵ�.
//        //    //if (FD_ISSET(sock, &readSet) || response->sendTurn) {
//        //    if (response->doingRecv.load() == false)
//        //    {
//        //        // ���� �ٽ� select ����� ���� �ʵ��� client �� flag �� ���ش�.
//        //        response->doingRecv.store(true);
//
//        //        {
//        //            lock_guard<mutex> lg(Client::jobQueueMutex);
//
//        //            bool wasEmpty = Client::jobQueue.empty();
//        //            Client::jobQueue.push(response);
//
//        //            // ���ǹ��ϰ� CV �� notify���� �ʵ��� ť�� ���̰� 0���� 1�� �Ǵ� ���� notify �� �ϵ��� ����.
//        //            if (wasEmpty) {
//        //                Client::jobQueueFilledCv.notify_one();
//        //            }
//        //        }
//        //    }
//        //}
//    }
//
//    // ���� threads ���� join �Ѵ�.
//    /*for (shared_ptr<thread>& workerThread : threads)
//    {
//        workerThread->join();
//    }*/
//    receiveThread->join();
//    inputThread->join();
//
//    // ������ ��ٸ��� passive socket �� �ݴ´�.
//    r = closesocket(sock);
//    if (r == SOCKET_ERROR)
//    {
//        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
//        return 1;
//    }
//
//    // Winsock �� �����Ѵ�.
//    WSACleanup();
//    return 0;
//}