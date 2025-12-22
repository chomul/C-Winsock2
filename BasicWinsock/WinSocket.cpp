#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // 윈도우 헤더에서 잘 쓰이지 않는 API들을 제외하여 컴파일 속도를 높임
#endif

#include <iostream>
#include <windows.h>
#include <winsock2.h>  // 윈도우 소켓 2.0 헤더
#include <ws2tcpip.h>  // getaddrinfo와 같은 최신 IP 주소 변환 함수용 헤더
#include <iphlpapi.h>
#include <ostream>
#include <stdio.h>
#include <thread>
#include "../Common/Packet.h"

// 링커에게 Ws2_32.lib 라이브러리를 링크하라고 지시 (Visual Studio 전용)
// 이 라이브러리가 있어야 윈도우 소켓 함수들을 실행 파일에 포함시킬 수 있음
#pragma comment(lib, "Ws2_32.lib")

// 접속할 서버의 포트 번호 (문자열로 정의)
#define DEFAULT_PORT "27015"
// 버퍼 길이
#define DEFAULT_BUFLEN 512

using namespace std;

// 대화 받는 작업 쓰레드에서 진행
void RecvThread(SOCKET sock)
{
    char recvbuf[DEFAULT_BUFLEN];
    ZeroMemory(recvbuf, DEFAULT_BUFLEN);
    int iResult;
    while (true)
    {
        iResult = recv(sock, recvbuf, DEFAULT_BUFLEN, 0);
        
        if (iResult > 0)
        {
            recvbuf[iResult] = '\0';
            cout << "Server: " << recvbuf << endl;
        }
        else if (iResult == 0)
        {
            printf("Connection closing...\n");
            break;
        }
        else
        {
            printf("recv failed: %d\n", WSAGetLastError());
            break;
        }
    }
}

// 외부에서 값을 던져주는 통로
// argc : 던져진 데이터의 개수
// argv : 던져진 데이터들의 문자열 배열(리스트)
// 예시 : cmd에서 프로그램 실행 [ C:\> client.exe 192.168.0.1 ]
// argc : 2   argv[0] : "client.exe"   argv[1] : "192.168.0.1" (우리 코드에서 argv[1]을 사용하는 이유!!)
int __cdecl main(int argc, char **argv) 
{
    // [1] 윈도우 소켓 초기화 단계
    WSADATA wsaData; // 윈도우 소켓 구현 정보를 담을 구조체
    int iResult;
    
    // 프로그램 실행 시 인자(서버 주소)가 없으면 에러 처리 (예: 실행 시 "client.exe 127.0.0.1" 처럼 IP를 줘야 함)
    if (argc != 2) 
    {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    // WSAStartup: Winsock DLL(Ws2_32.dll) 사용을 시작하겠다고 운영체제에 알림
    // MAKEWORD(2, 2): 소켓 버전 2.2를 사용하겠다고 요청
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }
    
    // [2] 접속할 서버의 주소 정보 설정 단계
    struct addrinfo *result = NULL, // 결과로 받을 주소 정보 구조체 포인터             [연결 리스트]
                    *ptr = NULL,    // 결과 리스트를 탐색할 포인터                    [연결 리스트]
                    hints;          // 어떤 타입의 소켓을 원하는지 설정하는 힌트 구조체  [단일 설정값]

    // hints 구조체를 0으로 초기화 (쓰레기 값 방지)
    ZeroMemory( &hints, sizeof(hints) );
    
    hints.ai_family   = AF_INET;     // AF_UNSPEC;   // IPv4(AF_INET)와 IPv6(AF_INET6) 둘 다 허용 (Unspecified)
    hints.ai_socktype = SOCK_STREAM; // TCP와 같은 연결 지향형 소켓 사용
    hints.ai_protocol = IPPROTO_TCP; // 프로토콜은 TCP 사용
    
    // getaddrinfo: 도메인 이름(argv[1])과 포트(DEFAULT_PORT)를 이용해 
    // 한 서버의 접속 가능한 주소 목록(List of Addresses)을 얻어와 'result'에 저장함.
    // 예: google.com이라고 쳤을 때, 접속 가능한 IPv4 주소, IPv6 주소 등을 모두 가져와서 result
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup(); // 에러 발생 시 소켓 라이브러리 해제
        return 1;
    }
    
    // [3] 소켓 생성 단계
    SOCKET ConnectSocket = INVALID_SOCKET; // 소켓 변수 초기화
    
    // getaddrinfo가 반환한 주소 목록 중 첫 번째 주소 정보를 사용 (예시 : 일단 첫 번째꺼 사용)
    // 복사하는 이유 : 나중에 메모리 해제(freeaddrinfo(result))할 때 사용.
    // result 자체를 움직여버리면 시작점을 잃어버리니까, ptr이라는 **복사본(대리인)**을 만들어서 움직이는 것
    ptr = result;
    
    // socket(): 실제 통신을 위한 엔드포인트(소켓) 생성
    // ptr->ai_family: 주소 체계 (IPv4 or IPv6)
    // ptr->ai_socktype: 소켓 타입 (TCP Stream)
    // ptr->ai_protocol: 프로토콜 (TCP)
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    
    // 소켓 생성 실패 검사
    if (ConnectSocket == INVALID_SOCKET)
    {
        // WSAGetLastError(): 가장 최근에 발생한 소켓 에러 코드를 반환
        printf("socket() failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result); // 할당받은 주소 메모리 해제
        WSACleanup();         // 소켓 라이브러리 해제
        return 1;
    }
    
    cout << "Create Socket!" << endl; // 소켓 생성까지 성공함
    
    //----------------------------------------------------------------------------------------------------------

    // [1] 서버에 연결 요청 (Connect)
    // ConnectSocket: 앞서 socket()으로 만든 소켓
    // ptr->ai_addr: 접속할 서버의 IP 주소와 포트 정보가 담긴 구조체
    // ptr->ai_addrlen: 그 주소 정보의 크기 (byte 단위)
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

    // [2] 연결 실패 확인
    if (iResult == SOCKET_ERROR) // 연결에 실패했다면 (예: 서버 꺼짐, 네트워크 단절)
    {
        // 실패한 소켓은 재사용할 수 없으므로 닫아버림 (자원 반납)
        closesocket(ConnectSocket);
        // 이 변수를 INVALID_SOCKET으로 설정하여 "현재 연결된 소켓 없음"을 표시
        ConnectSocket = INVALID_SOCKET;
    }

    // [3] 주소 정보 메모리 해제
    // 연결에 성공했든 실패했든, getaddrinfo로 할당받은 주소 목록(result)은 더 이상 필요 없음.
    // 메모리 누수를 막기 위해 반드시 해제해야 함.
    freeaddrinfo(result);

    // [4] 최종 연결 성공 여부 확인
    // 만약 위에서 연결이 실패해서 ConnectSocket이 INVALID_SOCKET이 되었다면?
    if (ConnectSocket == INVALID_SOCKET)
    {
        printf("connect() failed with error: %d\n", WSAGetLastError());
        WSACleanup(); // 윈도우 소켓 라이브러리 정리
        return 1;     // 프로그램 비정상 종료
    }

    // 여기까지 왔다면 연결 성공!
    cout << "Connect Socket!" << endl;    
    
    //----------------------------------------------------------------------------------------------------------
    
    thread recv_worker(RecvThread, ConnectSocket);
    
    // 패킷 생성
    Packet ClientPacket;
    ClientPacket.cmd = 0;
    cout << "Your name: ";
    cin >> ClientPacket.name;
    cin.ignore();
    
    // 로그인 과정
    iResult = send(ConnectSocket, (char*)&ClientPacket, sizeof(Packet), 0);
    
    if (iResult == SOCKET_ERROR)
    {
        printf("send() failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        if (recv_worker.joinable()) recv_worker.join();
        WSACleanup();
        return 1;
    }
    
    cout << "--- Send Login! Start Chat ---" << endl;
    
    // 대화 시작
    ClientPacket.cmd = 1;
    
    while (true)
    {
        cin.getline(ClientPacket.msg, DEFAULT_BUFLEN);
        
        if (strcmp(ClientPacket.msg, "exit\n") == 0)
        {
            shutdown(ConnectSocket, SD_SEND);
            break;
        }
        
        iResult = send(ConnectSocket, (char*)&ClientPacket, sizeof(Packet), 0);
        if (iResult == SOCKET_ERROR)
        {
            printf("send() failed with error: %d\n", WSAGetLastError());
            break;
        }
    }
        
    //----------------------------------------------------------------------------------------------------------
    
    // [1] 듣기/말하기 모두 불가 -> 남은 연결을 정리 -> 소켓 자원을 OS에 반납함
    
    // 무조건 closesocket -> join -> WSACleanup 순서 진행 : 안그러면 데드락 발생
    closesocket(ConnectSocket);
    if (recv_worker.joinable()) recv_worker.join();
    WSACleanup();
    
    cout << "Connection Closed" << endl;
    
    return 0;
}