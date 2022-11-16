//#include "ITD_Client.h"
//
//bool processResponse(Response* response)
//{
//    SOCKET responseSock = response->sock;
//
//    // 패킷을 받는다.
//    if (response->sendTurn == false)
//    {
//        if (response->lenCompleted == false)
//        {
//            // network byte order 로 전성되기 때문에 ntohl() 을 호출한다.
//            int r = recv(responseSock, (char*)&(response->packetLen) + response->offset, 4 - response->offset, 0);
//            if (r == SOCKET_ERROR)
//            {
//                cerr << "recv failed with error " << WSAGetLastError() << endl;
//                return false;
//            }
//            else if (r == 0)
//            {
//                // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
//                cerr << "Socket closed: " << responseSock << endl;
//                return false;
//            }
//            response->offset += r;
//
//            // 완성 못했다면 다음번에 계속 시도할 것이다.
//            if (response->offset < 4)
//            {
//                return true;
//            }
//
//            // host byte order 로 변경한다.
//            int dataLen = ntohl(response->packetLen);
//            cout << "[" << responseSock << "] Received length info: " << dataLen << endl;
//            response->packetLen = dataLen;
//
//            // 혹시 우리가 받을 데이터가 64KB보다 큰지 확인한다.
//            if (response->packetLen > sizeof(response->packet))
//            {
//                cerr << "[" << responseSock << "] Too big data: " << response->packetLen << endl;
//                return false;
//            }
//
//            // 이제 packetLen 을 완성했다고 기록하고 offset 을 초기화해준다.
//            response->lenCompleted = true;
//            response->offset = 0;
//        }
//
//        // 여기까지 도달했다는 것은 packetLen 을 완성한 경우다. (== lenCompleted 가 true)
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
//            // 메뉴얼을 보면 recv() 는 소켓이 닫힌 경우 0 을 반환함을 알 수 있다.
//            return false;
//        }
//        response->offset += r;
//
//        if (response->offset == response->packetLen)
//        {
//            cout << "[" << responseSock << "] Received " << response->packetLen << " bytes" << endl;
//
//            // TODO: 클라이언트로부터 받은 텍스트에 따라 로직 수행
//            const string json = string(response->packet).substr(0, response->packetLen);
//
//            cout << json << '\n';
//
//            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
//            response->lenCompleted = false;
//            response->offset = 0;
//            response->packetLen = 0;
//            memset
//        }
//
//        return true;
//    }
//
//    // 받은 패킷에 대한 응답을 보낸다.
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
//            // 완성 못했다면 다음번에 계속 시도할 것이다.
//            if (response->offset < 4)
//            {
//                return true;
//            }
//
//            // 이제 packetLen 을 완성했다고 기록하고 offset 을 초기화해준다.
//            response->lenCompleted = true;
//            response->offset = 0;
//            response->packetLen = ntohl(response->packetLen);
//        }
//
//        // 여기까지 도달했다는 것은 packetLen 을 완성한 경우다. (== lenCompleted 가 true)
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
//            // 다음 패킷을 위해 패킷 관련 정보를 초기화한다.
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
//                    // serverConnect 객체 초기화
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
//    // Winsock 을 초기화한다.
//    WSADATA wsaData;
//    r = WSAStartup(MAKEWORD(2, 2), &wsaData);
//    if (r != NO_ERROR) {
//        cerr << "WSAStartup failed with error " << r << endl;
//        return 1;
//    }
//
//    // passive socket 을 만들어준다.
//    //SOCKET passiveSock = createPassiveSocket();
//
//    struct sockaddr_in serverAddr;
//
//    // TCP socket 을 만든다.
//    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    if (sock == INVALID_SOCKET) {
//        std::cerr << "socket failed with error " << WSAGetLastError() << std::endl;
//        return 1;
//    }
//
//    // TCP 는 연결 기반이다. 서버 주소를 정하고 connect() 로 연결한다.
//    // connect 후에는 별도로 서버 주소를 기재하지 않고 send/recv 를 한다.
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
//    // 데이터를 받는 쓰레드를 추가한다.
//    shared_ptr<thread> receiveThread(new thread(workerThreadProc));
//    // 입력을 받는 쓰레드를 추가한다.
//    shared_ptr<thread> inputThread(new thread(GetInput));
//
//    while (true) {
//        int sendCount = 0;
//
//        fd_set readSet, exceptionSet;
//
//        // 위의 socket set 을 초기화한다.
//        FD_ZERO(&readSet);
//        FD_ZERO(&exceptionSet);
//
//        SOCKET maxSock = -1;
//
//        FD_SET(sock, &readSet);
//        FD_SET(sock, &exceptionSet);
//        maxSock = max(maxSock, sock);
//
//        // 현재 남아있는 active socket 들에 대해서도 모두 set 에 넣어준다.
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
//        // 회복할 수 없는 오류이다. 클라이언트를 중단한다.
//        if (r == SOCKET_ERROR) {
//            cerr << "select failed: " << WSAGetLastError() << endl;
//            break;
//        }
//
//        // 아무것도 반환을 안한 경우는 아래를 처리하지 않고 바로 다시 select 를 하게 한다.
//        // select 의 반환값은 오류일 때 SOCKET_ERROR, 그 외의 경우 이벤트가 발생한 소켓 갯수이다.
//        if (r == 0 && !serverConnect->sendTurn) {
//            continue;
//        }
//
//        // socket 이 readable 하다면 이는 새 연결이 들어왔다는 것이다.
//        if (FD_ISSET(sock, &readSet)) {
//            // passive socket 을 이용해 accept() 를 한다.
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
//                // 이제 다시 select 대상이 되지 않도록 client 의 flag 를 켜준다.
//
//                {
//                    lock_guard<mutex> lg(serverConnectMutex);
//
//                    canProcessServerConnect.notify_one();
//                }
//            }
//        }
//
//        // 오류 이벤트가 발생하는 소켓의 클라이언트는 제거한다.
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
//        //    // 소켓을 닫는다.
//        //    //closesocket(responseSock);
//
//        //    // 지울 대상에 포함시킨다.
//        //    //toDelete.push_back(entry.first);
//
//        //    // 소켓을 닫은 경우 더 이상 처리할 필요가 없으니 아래 read 작업은 하지 않는다.
//        //    //continue;
//
//        //    // 읽기 이벤트가 발생하는 소켓의 경우 recv() 를 처리한다.
//        //    //if (FD_ISSET(sock, &readSet) || response->sendTurn) {
//        //    if (response->doingRecv.load() == false)
//        //    {
//        //        // 이제 다시 select 대상이 되지 않도록 client 의 flag 를 켜준다.
//        //        response->doingRecv.store(true);
//
//        //        {
//        //            lock_guard<mutex> lg(Client::jobQueueMutex);
//
//        //            bool wasEmpty = Client::jobQueue.empty();
//        //            Client::jobQueue.push(response);
//
//        //            // 무의미하게 CV 를 notify하지 않도록 큐의 길이가 0에서 1이 되는 순간 notify 를 하도록 하자.
//        //            if (wasEmpty) {
//        //                Client::jobQueueFilledCv.notify_one();
//        //            }
//        //        }
//        //    }
//        //}
//    }
//
//    // 이제 threads 들을 join 한다.
//    /*for (shared_ptr<thread>& workerThread : threads)
//    {
//        workerThread->join();
//    }*/
//    receiveThread->join();
//    inputThread->join();
//
//    // 연결을 기다리는 passive socket 을 닫는다.
//    r = closesocket(sock);
//    if (r == SOCKET_ERROR)
//    {
//        cerr << "closesocket(passive) failed with error " << WSAGetLastError() << endl;
//        return 1;
//    }
//
//    // Winsock 을 정리한다.
//    WSACleanup();
//    return 0;
//}