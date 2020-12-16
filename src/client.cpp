#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
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
char* request_pack(const char *content, int op_code, int modes, int &datalen)
{
    string mode[2]={"netascii", "octet"};
    string buf;
    
    buf[0] = 0x00;
    buf[1] = op_code;
    buf += content;
    buf += "\0";
    buf += mode[modes]+"\0";

    datalen = buf.length();
    char *pack=new char[datalen];
    memcpy(pack, buf.c_str(), datalen);
    return pack;
}

int main(int argc, char *argv[])
{
    //tftp-client <-r/-w> <-n/-o> <server_addr> <filename>
    if(argc==5)
    {
        int op_code, modes;
        int sock=socket(AF_INET, SOCK_DGRAM, 0);
        
        //选择读or写
        if(!strcmp(argv[1], "-r"))
            op_code=0x01;
        else if(!strcmp(argv[1], "-w"))
            op_code=0x02;
        else
        {
            //输出错误提示
            cout <<"Error input!" <<endl;
            goto USAGE;
        }
        
        //选择netascii or octet
        if(!strcmp(argv[2], "-n"))
            modes=0;
        else if(!strcmp(argv[2], "-o"))
            modes=1;
        else
        {
            //输出错误提示
            cout <<"Error input!" <<endl;
            goto USAGE;
        }
        
        //向服务器构建请求包
        sockaddr_in ask_addr = get_addr(argv[3], 69);
        int datalen;
        char *r_pack=request_pack(argv[4], op_code, modes, datalen);
        
        //发送请求包
        int res = sendto(sock, r_pack, datalen, 0, (sockaddr*)&ask_addr, sizeof(ask_addr));
        if(res != datalen)
        {
            cout <<"Send request pack failed!" <<endl;
            return -1;
        }
        
        //读请求（下载）
        if(op_code==0x01)
        {
            FILE *local_file;
            //文本形式（netascii）
            if(modes==0)
                local_file=fopen(argv[4], "w");
            //二进制文件（octet）
            else if(modes==1)
                local_file=fopen(argv[4], "wb");
            //创建文件失败
            if(local_file == NULL)
            {
                cout <<"Create local file failed!" <<endl;
                return -1;
            }
            //开始传输文件
            while (true)
            {
                char buf[1024];
                //发送RRQ到服务器的69端口之后，服务器会找一个随机端口发送数据到客户端，客户端返回ACK时应该发给此端口，因此需要一个server_addr来保存地址信息
                sockaddr_in server_addr;
                socklen_t len = sizeof(server_addr);
                res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
                if(res > 0)
                {
                    //用flag来取服务器发送的包的操作码
                    short flag;
                    memcpy(&flag, buf, 2);
                    flag = ntohs(flag);     //网络字节序 转 主机字节序
                    //操作码等于3，数据包
                    if(flag == 3)
                    {
                        //用index来取数据块的编号
                        short index;
                        memcpy(&index, buf+2, 2);
                        index = ntohs(index);
                        
                    }
                    //操作码等于5，error包
                    else if(flag == 5)
                    {

                    }
                }
            }
            fclose(local_file);
            


        }
        //写请求（上传）
        else if(op_code==0x02)
        {
            char buf[1024];
            //发送WRQ到服务器的69端口之后，服务器会找一个随机端口发送ACK0到客户端，客户端发送数据包应该发给此端口，因此需要一个server_addr来保存地址信息
            sockaddr_in server_addr;
            socklen_t len = sizeof(server_addr);
            res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
            if(res<=0)
            {
                //TODO:输出错误提示
            }
            short flag;
            memcpy(&flag, buf, 2);
            flag = ntohs(flag);
            //收到一个ACK包
            if(flag==4)
            {
                short index;
                memcpy(&index, buf+2, 2);
                index = ntohs(index);
                if(index != 0)
                {
                    //TODO:输出错误信息
                }
            }
            else
            {
                //TODO:输出错误信息
            }
            //打开本地文件
            FILE *local_file;
            //文本形式（netascii）
            if(modes==0)
                local_file=fopen(argv[4], "r");
            //二进制文件（octet）
            else if(modes==1)
                local_file=fopen(argv[4], "rb");
            //打开文件失败
            if(local_file == NULL)
            {
                cout <<"Open local file failed!" <<endl;
                return -1;
            }
            //发送本地文件
            while(true)
            {
                //TODO:制作数据包
                char data_pack[1024];
                //TODO:发送数据包
                int res = sendto(sock, data_pack, datalen, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                //TODO:接受ACK


            }
            fclose(local_file);
        }
        goto END;
    }
    

    
USAGE:
    {
        //TODO:输出错误信息
        cout <<"usage:\t";
        cout <<"tftp-client <-r read|-w write> <-n netascii| -o octet> <server_addr> <filename>";
        return -1;
    }
END:
    return 0; 
}