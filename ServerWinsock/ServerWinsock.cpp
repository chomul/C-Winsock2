#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ostream>
#include <stdio.h>
#include <thread>
#include "../Common/Packet.h"
#include <vector>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

using namespace std;

// 일단 전역 변수 -> 나중에 서버 클래스 만들어서 관리할 예정
vector<SOCKET> ClientSockets;
mutex client_lock; 

void SendPacketToAll(Packet* p, SOCKET senderSock)
{
    // 보내고 있는 동안은 명단 접근 금지
    lock_guard<mutex> lock(client_lock);
    for (SOCKET sock : ClientSockets)
    {
        if (sock == senderSock) continue;
        send(sock, (char*)p, sizeof(Packet), 0);
    }
}

// 대화 받는 작업 쓰레드에서 진행
void RecvThread(SOCKET sock)
{
    char recvbuf[DEFAULT_BUFLEN];
    ZeroMemory(recvbuf, DEFAULT_BUFLEN);
    int iResult;
    while (true)
    {
        iResult = recv(sock, recvbuf, DEFAULT_BUFLEN - 1, 0);
        
        if (iResult > 0)
        {
            Packet* p = (Packet*)recvbuf;
            if (p->cmd == 0) cout << "--- [" << p->name << "] Login ---" << endl;
            else if (p->cmd == 1) cout << "[" << p->name << "]: " << p->msg << endl;
            
            SendPacketToAll(p, sock);
        }
        else
        {
            printf("Client disconnected.\n");

            // ★ 명단에서 제거하는 작업 추가
            client_lock.lock();
            for (int i = 0; i < ClientSockets.size(); i++)
            {
                if (ClientSockets[i] == sock)
                {
                    ClientSockets.erase(ClientSockets.begin() + i);
                    break; 
                }
            }
            client_lock.unlock();
    
            // 소켓 닫기
            closesocket(sock);
            // detach 이용해서 쓰레드를 처리했기에 따로 처리 안해도됨!!
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    
    int iResult;
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) 
    {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    // 서버 소켓 생성
    struct addrinfo *result = NULL, 
                    *ptr = NULL, 
                    hints;
    
    ZeroMemory(&hints, sizeof (hints));
    hints.ai_family = AF_INET;       // IPv4 주소 체계만 사용 (IPv6 안 씀)
    hints.ai_socktype = SOCK_STREAM; // TCP 사용
    hints.ai_protocol = IPPROTO_TCP; // TCP 프로토콜    
    hints.ai_flags = AI_PASSIVE;     // [서버 전용 옵션] ★★★ 
                                     // 어디에 접속하러 가는 게 아니라, 들어오는 접속을 기다리는(Passive) 입장이다

    // 주소 정보 가져오기
    // 첫 번째 인자가 NULL인 것이 핵심!
    // 내 컴퓨터의 모든 네트워크 인터페이스(IP)로 들어오는 연결을 허용
    // 내 IP 어디로 들어오든 27015 포트면 다 받아주겠다
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    
    // 데이터를 주고 받는 역할 X
    // 문지기 역할
    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) 
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    
    cout << "Create Server Socket" << endl;
        
    //----------------------------------------------------------------------------------------------------------
    
    // [1] bind 함수 호출: 소켓과 주소 정보를 묶어줍니다.
    // ListenSocket: 앞서 socket()으로 만든 '빈 껍데기' 소켓
    // result->ai_addr: getaddrinfo()로 받아온 내 IP와 포트 정보 (0.0.0.0 : 27015)
    // result->ai_addrlen: 주소 정보의 크기
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed: %d\n", WSAGetLastError());
        // [중요] 에러가 났더라도 할당받은 메모리와 리소스는 깔끔하게 정리하고 나가야 함
        freeaddrinfo(result);      // 주소 정보 메모리 해제
        closesocket(ListenSocket); // 소켓 닫기
        WSACleanup();              // 윈도우 소켓 라이브러리 종료
        return 1;                  // 비정상 종료
    }
    
    // [3] 주소 정보 메모리 해제 (성공 시)
    // bind가 성공했으면, 더 이상 'result' (주소 정보 리스트)는 필요 없습니다.
    // 소켓(ListenSocket)이 이미 그 정보를 가져갔기 때문
    // 따라서 메모리 누수를 막기 위해 해제
    freeaddrinfo(result);
    
    cout << "Bind Success" << endl;
    
    //----------------------------------------------------------------------------------------------------------
    
    // [1] listen 함수 호출: 연결 수신 대기 상태로 전환
    // ListenSocket: bind가 완료된 서버 소켓
    // SOMAXCONN: 연결 요청 대기열(Queue)의 최대 크기 (운영체제가 알아서 최댓값으로 설정해줌)
    if ( listen( ListenSocket, SOMAXCONN ) == SOCKET_ERROR ) 
    {
        printf( "Listen failed with error: %ld\n", WSAGetLastError() );
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // 클라이언트가 connect()를 하면 대기열에 들어옵니다.
    
    cout << "Listening Success" << endl;
    
    //----------------------------------------------------------------------------------------------------------
    
    while (true)
    {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        
        if (ClientSocket == INVALID_SOCKET) 
        {
            printf("accept failed: %d\n", WSAGetLastError());
            continue;
        }
        
        client_lock.lock();
        ClientSockets.push_back(ClientSocket);
        client_lock.unlock();
        
        cout << "Client Connected! (Socket: " << ClientSocket << ")" << endl;
        
        thread(RecvThread, ClientSocket).detach(); // 매우 중요!!!
    }
    
    WSACleanup();
    return 0;
}
