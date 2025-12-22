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

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

using namespace std;

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
    
    // [1] 실제 통신을 담당할 소켓 변수 선언
    // ListenSocket은 '연결 대기용'이고, 이 ClientSocket이 '실제 대화용'입니다.
    SOCKET ClientSocket = INVALID_SOCKET;

    // [2] accept 함수: 대기열에서 손님을 한 명 끄집어냄 (Blocking)
    // ListenSocket: 문지기 소켓 (누가 왔나 감시)
    // NULL, NULL: 접속한 클라이언트의 IP/Port 정보를 안 받겠다는 뜻 (받으려면 구조체 넣으면 됨)
    // ★중요★: 연결이 성립되면, accept는 '새로운 소켓(ClientSocket)'을 만들어서 반환
    ClientSocket = accept(ListenSocket, NULL, NULL);
    
    if (ClientSocket == INVALID_SOCKET) 
    {
        printf("accept failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket); 
        WSACleanup();
        return 1;
    }
    
    // [4] 문지기 소켓 닫기 (선택 사항이지만 중요!)
    // "나는 손님 한 명(ClientSocket)이랑만 대화하면 돼. 더 이상 다른 손님은 안 받을래."
    // 라는 뜻입니다. 1:1 통신만 할 거라면 리소스 절약을 위해 닫는 게 좋습니다.
    // (만약 채팅 서버처럼 계속 여러 명을 받아야 한다면 이걸 닫으면 안 됩니다!) ★★★★★
    closesocket(ListenSocket);
    
    cout << "Accept Success" << endl;
    
    //----------------------------------------------------------------------------------------------------------
    
    char recvbuf[DEFAULT_BUFLEN];     // 데이터를 받을 그릇(버퍼)
    int iSendResult;                 // send 함수의 결과(보낸 바이트 수) 저장
    int recvbuflen = DEFAULT_BUFLEN; // 버퍼의 최대 크기

    // do-while 루프: 클라이언트가 연결을 끊을 때까지 계속 대화
    do 
    {
        // [1] 데이터 수신 (듣기)
        // ClientSocket: accept로 만든 '담당자 소켓'
        // recvbuf: 데이터를 저장할 메모리 공간
        // recvbuflen: 버퍼의 최대 크기 (이 이상은 한 번에 못 받음)
        // 0: 플래그 (기본값)
        // ★특징: 데이터가 올 때까지 여기서 프로그램이 멈춰있습니다(Blocking).
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

        // [2] 상황별 처리
        if (iResult > 0) // A. 데이터를 성공적으로 받음 (iResult = 받은 바이트 수)
        {
            printf("Bytes received: %d\n", iResult);

            // [3] 에코(Echo) 전송 (말 따라하기)
            // 받은 데이터(recvbuf)를 그대로 다시 클라이언트에게 보냅니다.
            // ★중요: 보낼 크기는 'recvbuflen(512)'이 아니라 'iResult(실제 받은 크기)'여야 합니다.
            iSendResult = send(ClientSocket, recvbuf, iResult, 0);
            
            if (iSendResult == SOCKET_ERROR) 
            {
                printf("send failed: %d\n", WSAGetLastError());
                closesocket(ClientSocket); // 소켓 닫고
                WSACleanup();              // 퇴근
                return 1;
            }
            printf("Bytes sent: %d\n", iSendResult);
        } 
        else if (iResult == 0) // B. 연결 종료 요청 (Client가 shutdown 함수 호출함)
        {
            // 클라이언트가 "나 이제 할 말 없어(FIN)"라고 했을 때 0이 리턴
            printf("Connection closing...\n");
        }
        else // C. 에러 발생 (iResult < 0)
        {
            printf("recv failed: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

        // iResult가 0보다 크면(데이터가 계속 오면) 루프를 계속 돕니다.
        // iResult가 0이면(연결 끊김) 루프를 탈출
    } while (iResult > 0);
    
    //----------------------------------------------------------------------------------------------------------
    
    // [1] 서버 측 종료 선언 (Half-Close)
    // 클라이언트가 먼저 끊었지만(recv == 0), TCP는 양방향 통신입니다.
    // 서버 입장에서도 "나도 너한테 보낼 데이터가 더 이상 없어"라고 확실하게 말해주는(FIN 패킷 전송) 과정입니다.
    iResult = shutdown(ClientSocket, SD_SEND);
    
    if (iResult == SOCKET_ERROR) 
    {
        printf("shutdown failed: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // [3] 담당자 소켓 소멸 (리소스 해제)
    // 클라이언트와 연결되어 있던 'ClientSocket'을 메모리에서 삭제
    // 이제 이 클라이언트와의 연결은 완전히 끊어졌습니다.
    closesocket(ClientSocket);

    // [4] 윈도우 소켓 라이브러리 종료
    // 프로그램이 완전히 끝나는 시점이므로, "이제 네트워크 기능 안 씁니다"라고 OS에 보고합니다.
    WSACleanup();

    return 0;
}
