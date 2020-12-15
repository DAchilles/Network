#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#define LOCAL_PORT 1234
using namespace std;

//组装地址信息
sockaddr_in get_addr(char *ip, int port)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    return addr;
}

//组装请求报文
char* request_pack(const char *content, int op_code, int mode, int &datalen)
{
    string mode_s[2]={"netascii", "octet"};
    string buf;
    
    buf[0] = 0x00;
    buf[1] = op_code;
    buf += content;
    buf += "\0";
    buf += mode_s[mode]+"\0";

    datalen = buf.length();
    char *pack=new char[datalen];
    memcpy(pack, buf.c_str(), datalen);
    return pack;
}

int main(int argc, char *argv[])
{
    //client -[r/w] -[n/o] server_addr filename
    if(argc==5)
    {
        int op_code, mode;
        int sock=socket(AF_INET, SOCK_DGRAM, 0);
        
        //选择读or写
        if(!strcmp(argv[1], "-r"))
            op_code=0x01;
        else if(!strcmp(argv[1], "-w"))
            op_code=0x02;
        else
        {
            //TODO:输出错误提示
        }
        
        //选择netascii or octet
        if(!strcmp(argv[2], "-n"))
            mode=0;
        else if(!strcmp(argv[2], "-o"))
            mode=1;
        else
        {
            //TODO:输出错误提示
        }
        //向服务器构建请求包
        sockaddr_in addr = get_addr(argv[3], 69);
        int datalen;
        char *r_pack=request_pack(argv[4], op_code, mode, datalen);
        //发送请求包
        int res = sendto(sock, r_pack, datalen, 0, (sockaddr*)&addr, sizeof(addr));
        if(res != datalen)
        {
            cout <<"Send request_pack failed!" <<endl;
            return -1;
        }
        


    }
    else
    {
        //TODO:输出错误信息
    }
    
    
}