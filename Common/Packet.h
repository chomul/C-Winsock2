#pragma once  

// **가능:** `int`, `float`, `double`, `bool`, `char`, `long` 그리고 이것들의 **고정 배열(`[]`)**.
// **불가능:** `string`, `vector`, `포인터(*)`.
struct Packet 
{
    int  cmd;          // 명령 (0:로그인, 1:채팅, 2:귓속발)
    char name[20];     // 이름
    char target[20];   // 받는 사람
    char msg[100];     // 메시지
};

struct ClientInfo {
    SOCKET sock;
    char name[20];
};