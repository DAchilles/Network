#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
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
    int len=strlen(content);
    char *buf=new char[1024];

    buf[0] = 0x00;
    buf[1] = op_code;
    memcpy(buf + 2, content, len);
    memcpy(buf + 2 + len, "\0", 1);
    memcpy(buf + 2 + len + 1, mode[modes].c_str(), mode[modes].length());
    memcpy(buf+2+len+1+mode[modes].length(), "\0", 1);

    datalen = 2 + len + 1 + mode[modes].length() + 1;
    return buf;
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
                        cout <<"Pack No." <<index <<endl;
                        //把包里的数据写入本地文件
                        fwrite(buf+4, res-4, 1, local_file);
                        //组装ACK
                        char ack[4];
                        ack[0]=0x00;
                        ack[1]=0x04;
                        ack[2]=index>>8;
                        ack[3]=index&0xff;
                        //memcpy(ack+2, &index, 2);
                        //FIXME:发送ACK
                        int ack_len=sendto(sock, ack, 4, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                        if(ack_len != 4)
                        {
                            cout <<"ACK send error!" <<errno <<endl;
                        }
                        //判断是否为最后一个包
                        if(res<516)
                        {
                            cout <<"Download finish!" <<endl;
                            break;
                        }
                    }
                        //操作码等于5，error包
                    else if(flag == 5)
                    {
                        //用error_code取错误码
                        short error_code;
                        memcpy(&error_code, buf+2, 2);
                        error_code = ntohs(error_code);
                        //用error_str存储错误信息
                        char error_str[1024];
                        int iter=0;
                        while(*(buf+4+iter) != 0)
                        {
                            memcpy(error_str+iter, buf+4+iter, 1);
                            iter++;
                        }
                        cout <<"Error " <<error_code <<"!\t" <<error_str <<endl;
                        break;
                    }
                }
            }
            fclose(local_file);
        }
            /*写请求(上传):
                打开文件
                while(ture)
                {
                    接收服务器发的ACK
                    提取ACK编号index
                    发送data[index+1]
                    if(data[index+1])
                        break;
                }
            */
        else if(op_code==0x02)
        {
            //打开本地文件
            FILE *local_file;
            int last_index=0;
            bool send_finish=false;
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
            //开始循环收发
            while(true)
            {
                //接受ACK
                char buf[1024];
                sockaddr_in server_addr;
                socklen_t len = sizeof(server_addr);
                res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
                //TODO:接包失败
                if(res<=0)
                {
                    //输出错误提示
                    break;
                }
                //是否为ACK包
                short op_code;
                memcpy(&op_code, buf, 2);
                op_code = ntohs(op_code);
                //收到一个ACK包
                if(op_code==4)
                {
                    //提取ACK编号
                    short index;
                    memcpy(&index, buf+2, 2);
                    index = ntohs(index);
                    //如果是最后一个ACK，则发送成功，退出
                    if(send_finish && index==last_index)
                    {
                        cout <<"Upload finish!" <<endl;
                        break;
                    }
                    send_finish = false;
                    //制作data包
                    int data_len;
                    int data_index = index+1;
                    char data_pack[1024];
                    //op_code为03
                    data_pack[0]=0x00;
                    data_pack[1]=0x03;
                    //index为ACK的index+1
                    data_pack[2]=(data_index)>>8;
                    data_pack[3]=(data_index)&0xff;
                    //移动文件指针
                    fseek(local_file, index*512, SEEK_SET);
                    //复制文件信息到内存中
                    for(data_len=4; data_len<516; data_len++)
                    {
                        if(feof(local_file))
                        {
                            send_finish = true;
                            last_index = data_index;
                            break;
                        }
                        fread(data_pack+data_len, sizeof(char), 1, local_file);
                    }
                    //发送数据包
                    int res = sendto(sock, data_pack, data_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                    //发送失败则输出错误信息
                    if(res != data_len)
                    {
                        //输出错误信息
                    }
                }
                    //TODO:收到的ERROR包
                else if(op_code == 5)
                {
                    //用error_code取错误码
                    short error_code;
                    memcpy(&error_code, buf+2, 2);
                    error_code = ntohs(error_code);
                    //用error_str存储错误信息
                    char error_str[1024];
                    int iter=0;
                    while(*(buf+4+iter) != 0)
                    {
                        memcpy(error_str+iter, buf+4+iter, 1);
                        iter++;
                    }
                    cout <<"Error " <<error_code <<"!\t" <<error_str <<endl;
                    break;
                }
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