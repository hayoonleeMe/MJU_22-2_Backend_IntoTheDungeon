#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;

// ws2_32.lib �� ��ũ�Ѵ�.
#pragma comment(lib, "Ws2_32.lib")


static const unsigned short SERVER_PORT = 27015;
static const int NUM_WORKER_THREADS = 10;


class Client {
public:
    SOCKET sock;  // �� Ŭ���̾�Ʈ�� active socket

    atomic<bool> doingRecv;

    bool lenCompleted;
    int packetLen;
    char packet[65536];  // �ִ� 64KB �� ��Ŷ ������ ����
    int offset;

    Client(SOCKET sock) : sock(sock), doingRecv(false), lenCompleted(false), packetLen(0), offset(0) {
    }

    ~Client() {
        cout << "Client destroyed. Socket: " << sock << endl;
    }

    // TODO: ���⸦ ä���.
};


// �������κ��� Client ��ü �����͸� ���� ���� map
// ������ key �� Client ��ü �����͸� value �� ����ִ´�. (shared_ptr �� ����Ѵ�.)
// ���߿� key �� �������� ã���� ����� Client ��ü �����Ͱ� ���´�.
// key �� �������� ����� �ش� ��Ʈ���� �������.
// key ����� ���� ����̹Ƿ� ���� �����ִ� ���ϵ��̶�� ������ �� �ִ�.
map<SOCKET, shared_ptr<Client> > activeClients;
mutex activeClientsMutex;

// ��Ŷ�� ������ client ���� ť
queue<shared_ptr<Client> > jobQueue;
mutex jobQueueMutex;
condition_variable jobQueueFilledCv;


SOCKET createPassiveSocket() {
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

    // TCP �� ������ �޴� passive socket �� ���� ����� �� �� �ִ� active socket ���� ���еȴ�.
    // passive socket �� socket() �ڿ� listen() �� ȣ�������ν� ���������.
    // active socket �� passive socket �� �̿��� accept() �� ȣ�������ν� ���������.
    r = listen(passiveSock, 10);
    if (r == SOCKET_ERROR) {
        cerr << "listen faijled with error " << WSAGetLastError() << endl;
        return 1;
    }

    return passiveSock;
}


bool processClient(shared_ptr<Client> client) {
    SOCKET activeSock = client->sock;

    // ������ ������ �۾��ߴ����� ���� �ٸ��� ó���Ѵ�.
    // ������ packetLen �� �ϼ����� ���ߴ�. �װ� �ϼ��ϰ� �Ѵ�.
    if (client->lenCompleted == false) {
        // ���� ������ �ޱ� ���ؼ� 4����Ʈ�� �д´�.
        // network byte order �� �����Ǳ� ������ ntohl() �� ȣ���Ѵ�.
        int r = recv(activeSock, (char*)&(client->packetLen) + client->offset, 4 - client->offset, 0);
        if (r == SOCKET_ERROR) {
            cerr << "recv failed with error " << WSAGetLastError() << endl;
            return false;
        }
        else if (r == 0) {
            // �޴����� ���� recv() �� ������ ���� ��� 0 �� ��ȯ���� �� �� �ִ�.
            // ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
            cerr << "Socket closed: " << activeSock << endl;
            return false;
        }
        client->offset += r;

        // �ϼ� ���ߴٸ� �������� ��� �õ��� ���̴�.
        if (client->offset < 4) {
            return true;
        }

        // network byte order �� �����߾���.
        // ���� �̸� host byte order �� �����Ѵ�.
        int dataLen = ntohl(client->packetLen);
        cout << "[" << activeSock << "] Received length info: " << dataLen << endl;
        client->packetLen = dataLen;

        // �츮�� Client class �ȿ� packet ���̸� �ִ� 64KB �� �����ϰ� �ִ�.
        // Ȥ�� �츮�� ���� �����Ͱ� �̰ͺ��� ū�� Ȯ���Ѵ�.
        // ���� ũ�ٸ� ó�� �Ұ����̹Ƿ� ������ ó���Ѵ�.
        if (client->packetLen > sizeof(client->packet)) {
            cerr << "[" << activeSock << "] Too big data: " << client->packetLen << endl;
            return false;
        }

        // ���� packetLen �� �ϼ��ߴٰ� ����ϰ� offset �� �ʱ�ȭ���ش�.
        client->lenCompleted = true;
        client->offset = 0;
    }

    // ������� �����ߴٴ� ���� packetLen �� �ϼ��� ����. (== lenCompleted �� true)
    // packetLen ��ŭ �����͸� �����鼭 �ϼ��Ѵ�.
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
        // ���� r == 0 �� ��쵵 loop �� Ż���ϰ� �ؾߵȴ�.
        return false;
    }
    client->offset += r;

    // �ϼ��� ���� partial recv �� ��츦 �����ؼ� �α׸� ��´�.
    if (client->offset == client->packetLen) {
        cout << "[" << activeSock << "] Received " << client->packetLen << " bytes" << endl;

        // ���� ��Ŷ�� ���� ��Ŷ ���� ������ �ʱ�ȭ�Ѵ�.
        client->lenCompleted = false;
        client->offset = 0;
        client->packetLen = 0;
    }
    else {
        cout << "[" << activeSock << "] Partial recv " << r << "bytes. " << client->offset << "/" << client->packetLen << endl;
    }

    return true;
}


void workerThreadProc(int workerId) {
    cout << "Worker thread is starting. WorkerId: " << workerId << endl;

    while (true) {
        // lock_guard Ȥ�� unique_lock �� ��� scope ������ lock ������ �����ǹǷ�,
        // �Ʒ�ó�� ���� scope �� ���� lock �� ��� ���� ����.
        shared_ptr<Client> client;
        {
            unique_lock<mutex> ul(jobQueueMutex);

            // job queue �� �̺�Ʈ�� �߻��� ������ condition variable �� ���� ���̴�.
            while (jobQueue.empty()) {
                jobQueueFilledCv.wait(ul);
            }

            // while loop �� ���Դٴ� ���� job queue �� �۾��� �ִٴ� ���̴�.
            // queue �� front �� ����ϰ� front �� pop �ؼ� ť���� ����.
            client = jobQueue.front();
            jobQueue.pop();

        }

        // ���� block �� �������� client �� ������ ���̴�.
        // �׷��� Ȥ�� ���߿� �ڵ尡 ����� ���� �ְ� �׷��� client �� null �� �ƴ����� Ȯ�� �� ó���ϵ��� ����.
        // shared_ptr �� boolean �� �ʿ��� ���� ���� ���� null ���� ���θ� Ȯ�����ش�.
        if (client) {
            SOCKET activeSock = client->sock;
            bool successful = processClient(client);
            if (successful == false) {
                closesocket(activeSock);

                // ��ü ���� Ŭ���̾�Ʈ ����� activeClients ���� �����Ѵ�.
                // activeClients �� ���� �����忡���� �����Ѵ�. ���� mutex ���� ��ȣ�ؾߵ� ����̴�.
                // lock_guard �� scope ������ �����ϹǷ� lock ������ ������ �ּ�ȭ�ϱ� ���ؼ� ���� scope �� ����.
                {
                    lock_guard<mutex> lg(activeClientsMutex);

                    // activeClients �� key �� SOCKET Ÿ���̰�, value �� shared_ptr<Client> �̹Ƿ� socket ���� �����.
                    activeClients.erase(activeSock);
                }
            }
            else {
                // �ٽ� select ����� �� �� �ֵ��� �÷��׸� ���ش�.
                // ����� ���� ������ ��츸 �� flag �� �ٷ�� �ִ�.
                // �� ������ ������ �߻��� ���� ������ ���� ����Ʈ���� ������ ���̰� select �� �� ���� ���� �����̴�.
                client->doingRecv.store(false);
            }
        }
    }

    cout << "Worker thread is quitting. Worker id: " << workerId << endl;
}


int main()
{
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

    // �Ź� worker thread ������ �����ϴ°� �������� ���� ũ�� �迭 ��� ���⼭�� list �� ���.
    // loop �� �� �� worker thread ������ �ѹ��� �����ϸ� �� �ڿ��� list �� ��ȸ�ϴ� ������� ���� ���� ���� �����ϰ� �Ѵ�.
    // new thread(workerThreadProc) ���� ���� ���� thread �� ���� ���� ������,
    // ���⼭�� ���������� worker id �� ���ڷ� �Ѱܺ��Ҵ�.
    list<shared_ptr<thread> > threads;
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        shared_ptr<thread> workerThread(new thread(workerThreadProc, i));
        threads.push_back(workerThread);
    }

    // ������ ����ڰ� �ߴ��� ������ ���α׷��� ��� �����ؾߵȴ�.
    // ���� loop ���� �ݺ� ó���Ѵ�.
    while (true) {
        // select �� �̿��� �б� �̺�Ʈ�� ���� �̺�Ʈ�� �߻��ϴ� ������ �˾Ƴ� ���̴�.
        // fd_set �� C/C++ ���� ���� ���� �ƴ϶� typedef �� ������ custom type �̴�.
        // �׷��� �츮�� ��ü���� ������ �Ű澲�� �ʾƵ� �ǰ� ��� FD_XXX() �� ��ũ�� �Լ��� �̿��� ������ ���̴�.
        fd_set readSet, exceptionSet;

        // ���� socket set �� �ʱ�ȭ�Ѵ�.
        FD_ZERO(&readSet);
        FD_ZERO(&exceptionSet);

        // select �� ù��° ���ڴ� max socket ��ȣ�� 1�� ���� ���̴�.
        // ���� max socket ��ȣ�� ����Ѵ�.
        SOCKET maxSock = -1;

        // passive socket �� �⺻���� �� socket set �� ���ԵǾ�� �Ѵ�.
        FD_SET(passiveSock, &readSet);
        FD_SET(passiveSock, &exceptionSet);
        maxSock = max(maxSock, passiveSock);

        // ���� �����ִ� active socket �鿡 ���ؼ��� ��� set �� �־��ش�.
        for (auto& entry : activeClients) {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            // �̹� readable �ϴٰ� �ؼ� job queue �� ���� ��� �ٽ� select �� �ϸ� �ٽ� readable �ϰ� ���´�.
            // �̷��� �Ǹ� job queue �ȿ� �ߺ����� client �� ���� �ǹǷ�,
            // ���� job queue �ȿ� �ȵ� Ŭ���̾�Ʈ�� select Ȯ�� ������� �Ѵ�.
            if (client->doingRecv.load() == false) {
                FD_SET(activeSock, &readSet);
                FD_SET(activeSock, &exceptionSet);
                maxSock = max(maxSock, activeSock);
            }
        }

        // select �� ���ش�. ������ �ִ��� doingRecv �� ���� �͵��� �������� �ʾҾ���.
        // �̷� �͵��� worker thread �� ó�� �� doingRecv �� ���� �ٽ� select ����� �Ǿ�� �ϴµ�,
        // �Ʒ��� timeout ���� ���� ���� select �� ��ٸ��Ƿ� doingRecv �������� �ٽ� select �Ǿ�� �ϴ� �͵���
        // ������ ���� �ɸ� �� �ִ�. �׷� ������ �ذ��ϱ� ���ؼ� select �� timeout �� 100 msec ������ �����Ѵ�.
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        r = select(maxSock + 1, &readSet, NULL, &exceptionSet, &timeout);

        // ȸ���� �� ���� �����̴�. ������ �ߴ��Ѵ�.
        if (r == SOCKET_ERROR) {
            cerr << "select failed: " << WSAGetLastError() << endl;
            break;
        }

        // �������� �̺�Ʈ�� �߻��� ������ ���� ���� ��ٷ��� �Ʊ� ������ select �� ��ȯ���� ���������� Ȯ���ߴ�.
        // �׷��� ������ 100msec timeout �� �ɱ� ������ �ƹ� �̺�Ʈ�� �߻����� �ʴ��� select �� ����ȴ�.
        // �� ���� ��� socket ���� FD_ISSET �� �ϰ� �Ǹ�, ���� ������ ���� �� ���ǹ��ϰ� 
        // loop �� ���� �Ǵ� ���� �ȴ�.
        // ���� �ƹ��͵� ��ȯ�� ���� ���� �Ʒ��� ó������ �ʰ� �ٷ� �ٽ� select �� �ϰ� �Ѵ�.
        // ������ select �� ��ȯ���� ������ �� SOCKET_ERROR, �� ���� ��� �̺�Ʈ�� �߻��� ���� �����̴�.
        // ���� ��ȯ�� r �� 0�� ���� �Ʒ��� ��ŵ�ϰ� �Ѵ�.
        if (r == 0) {
            continue;
        }

        // passive socket �� readable �ϴٸ� �̴� �� ������ ���Դٴ� ���̴�.
        // �� Ŭ���̾�Ʈ ��ü�� �������� ����� 
        if (FD_ISSET(passiveSock, &readSet)) {
            // passive socket �� �̿��� accept() �� �Ѵ�.
            // accept() �� blocking ������ �츮�� �̹� select() �� ���� �� ������ ������ �˰� accept() �� ȣ���Ѵ�.
            // ���� ���⼭�� blocking ���� �ʴ´�.
            // ������ �Ϸ�ǰ� ��������� ������ active socket �̴�.
            cout << "Waiting for a connection" << endl;
            struct sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            SOCKET activeSock = accept(passiveSock, (sockaddr*)&clientAddr, &clientAddrSize);

            // accpet() �� �����ϸ� �ش� ������ �̷������ �ʾ����� �ǹ��Ѵ�.
            // �� ������ �߸��ȴٰ� �ϴ��� �ٸ� ������� ó���ؾߵǹǷ� ������ �߻��ߴٰ� �ϴ��� ��� �����Ѵ�.
            if (activeSock == INVALID_SOCKET) {
                cerr << "accept failed with error " << WSAGetLastError() << endl;
                return 1;
            }
            else {
                // ���� client ��ü�� �����.
                shared_ptr<Client> newClient(new Client(activeSock));

                // socket �� key �� �ϰ� �ش� ��ü �����͸� value �� �ϴ� map �� ���� �ִ´�.
                activeClients.insert(make_pair(activeSock, newClient));

                // �α׸� ��´�.
                char strBuf[1024];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), strBuf, sizeof(strBuf));
                cout << "New client from " << strBuf << ":" << ntohs(clientAddr.sin_port) << ". "
                    << "Socket: " << activeSock << endl;
            }
        }

        // ���� �̺�Ʈ�� �߻��ϴ� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
        // activeClients �� ��ȸ�ϴ� ���� �� ������ �����ϸ� �ȵǴ� ����� ��츦 ���� ������ list �� ����.
        list<SOCKET> toDelete;
        for (auto& entry : activeClients) {
            SOCKET activeSock = entry.first;
            shared_ptr<Client> client = entry.second;

            if (FD_ISSET(activeSock, &exceptionSet)) {
                cerr << "Exception on socket " << activeSock << endl;

                // ������ �ݴ´�.
                closesocket(activeSock);

                // ���� ��� ���Խ�Ų��.
                // ���⼭ activeClients ���� �ٷ� ������ �ʴ� ������ ���� activeClients �� ��ȸ���̱� �����̴�.
                toDelete.push_back(activeSock);

                // ������ ���� ��� �� �̻� ó���� �ʿ䰡 ������ �Ʒ� read �۾��� ���� �ʴ´�.
                continue;
            }

            // �б� �̺�Ʈ�� �߻��ϴ� ������ ��� recv() �� ó���Ѵ�.
            // ����: �Ʒ��� ������ recv() �� ���� blocking �� �߻��� �� �ִ�.
            //       �츮�� �̸� producer-consumer ���·� �ٲ� ���̴�.
            if (FD_ISSET(activeSock, &readSet)) {
                // ���� �ٽ� select ����� ���� �ʵ��� client �� flag �� ���ش�.
                client->doingRecv.store(true);

                // �ش� client �� job queue �� ����. lock_guard �� �ᵵ �ǰ� unique_lock �� �ᵵ �ȴ�.
                // lock �ɸ��� ������ ��������� �����ϱ� ���ؼ� ���� scope �� �����ش�.
                {
                    lock_guard<mutex> lg(jobQueueMutex);

                    bool wasEmpty = jobQueue.empty();
                    jobQueue.push(client);

                    // �׸��� worker thread �� �����ش�.
                    // ������ condition variable �� notify �ص� �Ǵµ�,
                    // �ش� condition variable �� queue �� ������ ���� �� �̻� �� ť�� �ƴ� �� ���̹Ƿ�
                    // ���⼭�� ���ǹ��ϰ� CV �� notify���� �ʵ��� ť�� ���̰� 0���� 1�� �Ǵ� ���� notify �� �ϵ��� ����.
                    if (wasEmpty) {
                        jobQueueFilledCv.notify_one();
                    }

                    // lock_guard �� scope �� ��� �� Ǯ�� ���̴�.
                }
            }
        }

        // ���� ���� ���� �־��ٸ� �����.
        for (auto& closedSock : toDelete) {

            // �ʿ��� ����� ��ü�� �����ش�.
            // shared_ptr �� ��� ������ �ʿ��� ������ �� �̻� ����ϴ� ���� �������� ��ü�� ��������.
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
