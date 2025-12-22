#pragma once  

struct Packet 
{
    int  cmd;          // 명령 (0:로그인, 1:채팅)
    char name[20];     // 이름
    char msg[100];     // 메시지
};