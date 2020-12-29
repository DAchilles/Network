/*
TODO:下载时组装顺序(data包可能乱序)
    发送想要的ACK
TODO:超时重传
*/
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

timeval nNetTimeout = {3,0};

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

void add_log(fstream &log_in, string str)
{
    time_t now = time(0);
    string now_s = ctime(&now);
    now_s.erase(now_s.length()-1);
    log_in <<"[" << now_s <<"]" <<str <<endl;
    return ;
}

int main(int argc, char *argv[])
{
    //打开日志
    time_t now = time(0);
    string now_s = ctime(&now);
    now_s.erase(now_s.length()-1);
    fstream log(now_s+".log", ios::app);

    //client <-r/-w> <-n/-o> <server_addr> <filename>
    if(argc==5)
    {
        //创建socket
        int op_code, modes;
        int sock=socket(AF_INET, SOCK_DGRAM, 0);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &nNetTimeout, sizeof(nNetTimeout));

        //选择读or写
        if(!strcmp(argv[1], "-r"))
            op_code=0x01;
        else if(!strcmp(argv[1], "-w"))
            op_code=0x02;
        else
        {
            //输出错误提示
            cout <<"Illegal input!" <<endl;
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
            cout <<"Illegal input!" <<endl;
            goto USAGE;
        }

        //向服务器构建请求包
        sockaddr_in ask_addr = get_addr(argv[3], 69);
        int datalen;
        char *r_pack=request_pack(argv[4], op_code, modes, datalen);

        //日志记录
        if(op_code == 1)
        {
            string filename = argv[4];
            add_log(log, "Download " + filename);
            add_log(log, "Send RRQ");
        }
        else
        {
            string filename = argv[4];
            add_log(log, "Upload " + filename);
            add_log(log, "Send WRQ");
        }

        //读请求（下载）
        if(op_code==0x01)
        {
            //创建文件
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
                add_log(log, "Create local file failed!");
                goto ERROR;
            }

            //开始传输文件
            time_t start_time = time(0);

            //发送请求包
            int res;
            char buf[1024];
            sockaddr_in server_addr;
            socklen_t len = sizeof(server_addr);
            do
            {
                res = sendto(sock, r_pack, datalen, 0, (sockaddr*)&ask_addr, sizeof(ask_addr));
                if(res != datalen)
                {
                    cout <<"Send requre error" <<endl;
                    add_log(log, "Send requre error");
                    goto ERROR;
                    break;
                }
                //接收ACK0
                memset(buf, 0, sizeof(buf));
                res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
            }while(res<0 && (errno == EWOULDBLOCK || errno == EAGAIN));

            //接包失败
            if(res<0)
            {
                cout <<"recvfrom() error" <<endl;
                add_log(log, "Recieve data packet error");
                goto ERROR;
            }

            unsigned short packet_want=0;
            while (true)
            {
                //用flag来取服务器发送的包的操作码
                short op_code;
                memcpy(&op_code, buf, 2);
                op_code = ntohs(op_code);     //网络字节序->主机字节序
                //操作码等于3，数据包
                if(op_code == 3)
                {
                    //用index来取数据块的编号
                    unsigned short index;
                    memcpy(&index, buf+2, 2);
                    
                    index = ntohs(index);
                    cout <<"Recieve data packet No." <<index <<endl;
                    add_log(log, "Recieve data packet No." + to_string(index));
                    //如果是想要的包，则写进buf
                    if(index == packet_want+1)
                    {
                        packet_want++;
                        //把包里的数据写入本地文件
                        fwrite(buf+4, res-4, 1, local_file);
                    }
                    //判断是否为最后一个包
                    if(res<516)
                    {
                        cout <<"Download finish!" <<endl;
                        add_log(log, "Download finish");
                        //组装ACK
                        char ack[4];
                        ack[0]=0x00;
                        ack[1]=0x04;
                        ack[2]=packet_want>>8;
                        ack[3]=packet_want&0xff;
                        /*
                        do{
                            res = sendto(sock, ack, 4, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                            add_log(log, "Send ACK No." + to_string(packet_want));
                            if(res != 4)
                            {
                                cout <<"sendto() error" <<endl;
                                add_log(log, "Send ACK No." + to_string(packet_want) +" error");
                                goto ERROR;
                            }
                            memset(buf, 0, sizeof(buf));
                            res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
                        }while (res<0 && (errno != EWOULDBLOCK && errno != EAGAIN));
                        */
                        //发送ACK
                        res = sendto(sock, ack, 4, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                        add_log(log, "Send ACK No." + to_string(packet_want));
                        if(res != 4)
                        {
                            cout <<"sendto() error" <<endl;
                            add_log(log, "Send ACK No." + to_string(packet_want) +" error");
                            goto ERROR;
                        }
                        break;
                    }
                    //组装ACK
                    char ack[4];
                    ack[0]=0x00;
                    ack[1]=0x04;
                    ack[2]=packet_want>>8;
                    ack[3]=packet_want&0xff;
                    do{
                        //发送ACK
                        res = sendto(sock, ack, 4, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                        add_log(log, "Send ACK No." + to_string(packet_want));
                        if(res != 4)
                        {
                            cout <<"sendto() error" <<endl;
                            add_log(log, "Send ACK No." + to_string(packet_want) +" error");
                            goto ERROR;
                        }
                        memset(buf, 0, sizeof(buf));
                        res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
                    }while (res<0 && (errno == EWOULDBLOCK || errno == EAGAIN));
                
                }
                    //操作码等于5，error包
                else if(op_code == 5)
                {
                    //用error_code取错误码
                    short error_code;
                    memcpy(&error_code, buf+2, 2);
                    error_code = ntohs(error_code);
                    //用error_str存储错误信息
                    string error_str;
                    int iter=0;
                    while(*(buf+4+iter) != '\0')
                    {
                        error_str += *(buf+4+iter);
                        iter++;
                    }
                    cout <<"Error " <<error_code <<"! " <<error_str <<endl;
                    add_log(log, "Error " + to_string(error_code) + "! " + error_str);
                    goto ERROR;
                    break;
                }
            }
            time_t end_time = time(0);
            cout <<"Speed: " <<(double)packet_want*512/1024/(end_time-start_time) <<"KB/s\n";
            fclose(local_file);
        }

        /*写请求(上传)*/
        else if(op_code==0x02)
        {
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
                add_log(log, "Open local file failed");
                goto ERROR;
            }
            time_t start_time = time(0);
            //发送请求包
            int res;
            char buf[1024];
            sockaddr_in server_addr;
            socklen_t len = sizeof(server_addr);
            do
            {
                res = sendto(sock, r_pack, datalen, 0, (sockaddr*)&ask_addr, sizeof(ask_addr));
                if(res != datalen)
                {
                    cout <<"Send requre error" <<endl;
                    add_log(log, "Send requre error");
                    goto ERROR;
                    break;
                }
                //接收ACK0
                memset(buf, 0, sizeof(buf));
                res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
            }while(res<0 && (errno == EWOULDBLOCK || errno == EAGAIN));

            //接包失败
            if(res<0)
            {
                cout <<"recvfrom() error" <<endl;
                add_log(log, "Recieve ACK packet error");
                goto ERROR;
            }

            //开始循环收发
            int pack_cnt=0;
            int last_index=0;       //最后一个包的index
            int overflow=0;         //编号是否超出2字节
            bool send_finish=false;
            while(true)
            {
                //是否为ACK包
                short op_code;
                memcpy(&op_code, buf, 2);
                op_code = ntohs(op_code);
                //收到一个ACK包
                if(op_code==4)
                {
                    //提取ACK编号
                    unsigned short index;
                    int true_index;

                    memcpy(&index, buf+2, 2);
                    index = ntohs(index);
                    true_index = 65535*overflow + index;
                    add_log(log, "Recieve ACK No." + to_string(index));

                    //如果是最后一个ACK，则发送成功，退出
                    if(send_finish && true_index==last_index)
                    {
                        cout <<"Upload finish!" <<endl;
                        add_log(log, "Upload finish");
                        break;
                    }
                    send_finish = false;
                    //制作data包
                    if(index == 65535)
                        overflow += 1;
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
                    fseek(local_file, true_index*512, SEEK_SET);
                    //复制文件信息到内存中
                    for(data_len=4; data_len<516; data_len++)
                    {
                        if(feof(local_file))
                        {
                            send_finish = true;
                            last_index = data_index + 65535*overflow;
                            break;
                        }
                        fread(data_pack+data_len, sizeof(char), 1, local_file);
                    }
                    do
                    {
                        //发送数据包
                        res = sendto(sock, data_pack, data_len, 0, (sockaddr*)&server_addr, sizeof(server_addr));
                        cout <<"Send data packet No." <<true_index+1 <<endl;
                        add_log(log, "Send data packet No." + to_string(true_index+1));
                        //发送失败则输出错误信息
                        if(res != data_len)
                        {
                            //输出错误信息
                            cout <<"sendto() error" <<endl;
                            add_log(log, "Send data packet error");
                            goto ERROR;
                        }
                        pack_cnt++;
                        memset(buf, 0, sizeof(buf));
                        res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server_addr, &len);
                    } while (res<0 && (errno == EWOULDBLOCK || errno == EAGAIN));
                }
                    //收到的ERROR包
                else if(op_code == 5)
                {
                    //用error_code取错误码
                    short error_code;
                    memcpy(&error_code, buf+2, 2);
                    error_code = ntohs(error_code);
                    //用error_str存储错误信息
                    string error_str;
                    int iter=0;
                    while(*(buf+4+iter) != '\0')
                    {
                        error_str += *(buf+4+iter);
                        iter++;
                    }
                    cout <<"Error " <<error_code <<"! " <<error_str <<endl;
                    add_log(log, "Error " + to_string(error_code) + "! " + error_str);
                    goto ERROR;
                    break;
                }
            }
            time_t end_time = time(0);
            cout <<"Speed: " <<(double)pack_cnt*512/1024/(end_time-start_time) <<"KB/s\n";
            fclose(local_file);
        }

        goto END;
    }

    USAGE:
    cout <<"USAGE:\n";
    cout <<"\tclient <-r read|-w write> <-n netascii| -o octet> <server_addr> <filename>";
    ERROR:
    add_log(log, "Break");
    return -1;
    END:
    return 0;
}