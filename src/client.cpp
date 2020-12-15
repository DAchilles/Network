#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>

//组装地址信息
sockaddr_in get_addr(char *ip, int port)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    return addr;
}

//组装下载请求报文
char* request_down_pack(const char *content, int &datalen)
{
    int len=strlen(content);
    char *buf=new char[len + 2 + 2 +5];
}

int main()
{
    int sock=socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr = get_addr();
}