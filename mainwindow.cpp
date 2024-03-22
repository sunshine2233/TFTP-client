#include "mainwindow.h"
#include "ui_mainwindow.h"
// 服务端和客户端的ip地址
sockaddr_in serverAddr, clientAddr;
// 客户端socket
SOCKET clientsock;
// ip地址长短
unsigned int addr_len = sizeof(struct sockaddr_in);
// 日志文件
FILE* logFp; 
char logBuf[512];
time_t rawTime;
tm* info;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->Exit2, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->Exit1, SIGNAL(clicked()), this, SLOT(close()));
    initUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI(){
    //打开日志文件
    logFp = fopen("tftp.log", "a");
    if (logFp == NULL) {
        ui->output->append("open log file failed");
        return;
    }
}


void MainWindow::on_upload_clicked()
{



    QByteArray Qfilename = ui->FilePath->text().toLatin1();
    QByteArray QserverIP = ui->uploadServerIP->text().toLatin1();
    QByteArray QclientIP = ui->uploadLocalIP->text().toLatin1();
    char * filePath = Qfilename.data();
    char * serverIP = QserverIP.data();
    char * clientIP = QclientIP.data();
    char filename[512];
    wchar_t buf[512];   //输出错误信息
    int r_size; // UDP传输函数返回值
    // 记录重传传输次数
    int resent = 0;
    // 记录坏包数量
    int badpacket = 0;

    // 截取文件名称
    int temp = 0;
    for(int i = 0; filePath[i]!='\0'; i++, temp++){
        if(filePath[i] == '/'){
            i++;
            temp = 0;
            filename[temp] = filePath[i];
        }
        else{
            filename[temp] = filePath[i];
        }
    }
    filename[temp] = '\0';


    //初始化 winsock
    WSADATA wsaData;
    int nRC = WSAStartup(0x0101, &wsaData);
    if (nRC)
    {
        //Winsock 初始化错误
        ui->output->append("Client initialize winsock error!\n");
        return;
    }
    if (wsaData.wVersion != 0x0101)
    {
        //版本支持不够
        //报告错误给用户，清除 Winsock，返回
        ui->output->append("Client's winsock version error!\n");
        WSACleanup();
        return;
    }
    else
        ui->output->append("Client's winsock initialized !\n");


    // 创建客户端socket
    clientsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//指定传输协议为UDP
    if (clientsock == INVALID_SOCKET)
    {
        // 创建失败，输出错误信息
        wchar_t *s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errno,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&s, 0, NULL);
        swprintf(buf,L"Client create socket with error %d%ws\n", errno, s);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        WSACleanup();
        return;
    }
    ui->output->append("Client socket create OK!\n");


    // 设置接收超时断开
    DWORD timeout = PKT_RCV_TIMEOUT;
    setsockopt(clientsock, SOL_SOCKET, SO_RCVTIMEO,  (const char*)&timeout, sizeof(timeout));


    // 将socket与主机地址绑定
    // 设置客户端 地址族(ipv4) 端口 ip
    clientAddr.sin_family = AF_INET;
    //htons函数把主机字节顺序转换为网络字节顺序
    clientAddr.sin_port = htons(0); //端口号设为0，Winsock自动为应用程序分配1024-5000端口号
    clientAddr.sin_addr.S_un.S_addr = inet_addr(clientIP); //inet_addr将字符串形式转换为unsigned long形式
    r_size = ::bind(clientsock,(LPSOCKADDR)&clientAddr, sizeof(clientAddr));
    if (r_size == SOCKET_ERROR){
        //绑定失败，输出错误信息
        wchar_t *s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errno,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&s, 0, NULL);
        swprintf(buf,L"Bind Client with error %d%ws\n", errno, s);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        WSACleanup();
        return;
    }


    // 准备服务器的信息，这里需要指定服务器的地址
    // 设置服务器 地址族(ipv4) 端口 ip
    serverAddr.sin_family = AF_INET;
    //htons和htonl函数把主机字节顺序转换为网络字节数据
    serverAddr.sin_port = htons(69);
    serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIP);


    // 发送数据报文
    // 发送包,接收包
    tftpPacket sendPacket, rcv_packet;

    // 发送请求数据包
    // 写请求数据包
    // 写入操作码
    sendPacket.cmd = htons(CMD_WRQ);
    // 写入文件名以及传输格式
    int choose = ui->uploadMode->currentIndex();
    if(choose == 0){
        sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "netascii", 0);
        ui->output->append("uploadMode=netascii\n");
    }
    else{
        sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "octet", 0);
        ui->output->append("uploadMode=octet\n");
    }

    sockaddr_in sender; // 接收服务器端口信息
    ui->output->append("Send upload Filename\n");
    // 最多重传3次，rxmt为重传次数
    int rxmt;
    for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++) {
        r_size = sendto(clientsock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);
        // 发送错误，输出错误信息
        if (r_size == SOCKET_ERROR){
            wchar_t *s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, errno,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPWSTR)&s, 0, NULL);
            swprintf(buf,L"Client send request to Server with error %d%ws\n", errno, s);
            QString out;
            out = out.fromWCharArray(buf);
            ui->output->append(out);
            WSACleanup();
            // 写入日志
            // 获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
                asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                "Client send request to Server error.");
            fwrite(logBuf, 1, strlen(logBuf), logFp);
            fclose(logFp);
            return;
        }
        // 接收信息，限时3s
        clock_t startones = clock();
        int ifack = 0;
        while(1){
            if ((clock()-startones) >= PKT_RCV_TIMEOUT) break;
            // 尝试接收
            r_size = recvfrom(clientsock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
            // 接收到ACK
            if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0)) {
                ifack = 1;
                break;
            }
            // 接收错误
            if (r_size == SOCKET_ERROR && errno != 10060){
                wchar_t *s = NULL;
                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, errno,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPWSTR)&s, 0, NULL);
                swprintf(buf, L"Request Recv Fail with error %d%ws\n", errno, s);
                QString out;
                out = out.fromWCharArray(buf);
                ui->output->append(out);
                // 写入日志
                // 获取时间
                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
                    asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                    "Request Recv Fail.");
                fwrite(logBuf, 1, strlen(logBuf), logFp);
                fclose(logFp);
                return;
            }
            // 接收超时
            if (r_size == SOCKET_ERROR && errno == 10060) {
                ui->output->append("RECV TIMEOUT!\n");
                break;
            }
            // 接收包错误
            else{
                swprintf(buf, L"Bad packet: r_size=%d, rcv_packet_cmd=%d, rcv_packet_block=%d\n", r_size, rcv_packet.cmd, rcv_packet.block);
                QString out;
                out = out.fromWCharArray(buf);
                ui->output->append(out);
            }
            Sleep(20);
        }
        //重传
        if (! ifack){
            ui->output->append("Can't receive ACK, resent\n");
            // 写入日志
            // 获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s WARN: upload %s, mode: %s, %s\n",
                asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                "Can't receive ACK, resent");
            fwrite(logBuf, 1, strlen(logBuf), logFp);
        }
        else break;
        resent++;
    }
    // 重传仍未接受到ACK
    if (rxmt >= PKT_MAX_RXMT) {
        ui->output->append("Could not receive request response from server.\n");
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
            asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
            "Could not receive from server.");
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }

    // 打开文件
    FILE* fp = NULL;
    if(choose == 0)
        fp = fopen(filePath, "r");
    else
        fp = fopen(filePath, "rb");
    if (fp == NULL) {
        ui->output->append("File not exists!\n");
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
            asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
            "File not exists!");
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }

    // 发送数据包
    // 记录块编号
    unsigned short block = 1;
    // 记录传输字节大小
    double transByte = 0;
    sendPacket.cmd = htons(CMD_DATA);
    // 记录时间
    clock_t start = clock();
    double s_size;
    do {
        memset(sendPacket.data, 0, sizeof(sendPacket.data));
        // 写入块编号
        sendPacket.block = htons(block);
        // 读入数据
        s_size = fread(sendPacket.data, 1, DATA_SIZE, fp);
        transByte += s_size;
        swprintf(buf,L"Send the %d block\n", block);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        // 重传3次
        for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++) {
            //发送数据包
            r_size = sendto(clientsock, (char*)&sendPacket, s_size + 4, 0, (struct sockaddr*) & sender, addr_len);
            // 发送错误
            if (r_size == SOCKET_ERROR){
                wchar_t *s = NULL;
                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, errno,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPWSTR)&s, 0, NULL);
                swprintf(buf,L"Send Data block %d to Server with error %d%ws\n", block, errno, s);
                QString out;
                out = out.fromWCharArray(buf);
                ui->output->append(out);
                WSACleanup();
                // 写入日志
                // 获取时间
                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s WARN: upload %s, mode: %s, %s\n",
                    asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                    "Send Data block %d to Server error");
                fwrite(logBuf, 1, strlen(logBuf), logFp);
                fclose(logFp);
                return;
            }
            clock_t startones = clock();
            int ifack = 0;
            while(1){
                if ((clock() - startones) >= PKT_RCV_TIMEOUT) break;
                //尝试接收
                r_size = recvfrom(clientsock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
                if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(block)) {
                    // 成功收到ACK
                    ifack = 1;
                    break;
                }
                // 接收错误
                if (r_size == SOCKET_ERROR && errno != 10060){
                    wchar_t *s = NULL;
                    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, errno,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPWSTR)&s, 0, NULL);
                    swprintf(buf, L"Data Recv Fail with error %d%ws\n", errno, s);
                    QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    // 写入日志
                    // 获取时间
                    time(&rawTime);
                    // 转化为当地时间
                    info = localtime(&rawTime);
                    sprintf(logBuf, "%s WARN: upload %s, mode: %s, %s\n",
                        asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                        "Data Recv Fail");
                    fwrite(logBuf, 1, strlen(logBuf), logFp);
                    fclose(logFp);
                    return;
                }
                //接收超时
                if (r_size == SOCKET_ERROR && errno == 10060){
                    ui->output->append("RECV TIMEOUT!\n");
                    break;
                }
                //接受包错误
                else{
                    swprintf(buf, L"Bad packet: r_size=%d, rcv_packet_cmd=%d, rcv_packet_block=%d\n", r_size, ntohs(rcv_packet.cmd), ntohs(rcv_packet.block));
                    QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    badpacket++;
                }
                Sleep(20);
            }
            //重传
            if (! ifack){
                ui->output->append("Can't receive ACK, resent\n");
                // 写入日志
                // 获取时间
                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s WARN: upload %s, mode: %s, %s\n",
                    asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                    "Can't receive ACK, resent");
                fwrite(logBuf, 1, strlen(logBuf), logFp);
            }
            else break;
            resent++;
        }
        if (rxmt >= PKT_MAX_RXMT) {
            // 3次重传失败
            ui->output->append("Could not receive Data from server.\n");
            fclose(fp);
            // 写入日志
            // 获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s ERROR: upload %s, mode: %s, Wait for ACK timeout\n",
                asctime(info), filename, choose == 0 ? ("netascii") : ("octet"));
            fwrite(logBuf, 1, strlen(logBuf), logFp);
            fclose(logFp);
            return;
        }
        // 传输下一个数据块
        block++;
    } while (s_size == DATA_SIZE);	// 当数据块未装满时认为时最后一块数据，结束循环
    clock_t end = clock();
    ui->output->append("Send file end.\n");

    // 关闭文件
    fclose(fp);
    QString out;

    //显示传输的吞吐量
    double consumeTime = ((double)(end - start)) / CLK_TCK;
    swprintf(buf, L"Upload file size: %.1f kB\nConsuming time: %.2f s", transByte/ 1024, consumeTime);
    out = out.fromWCharArray(buf);
    ui->output->append(out);
    swprintf(buf, L"Upload speed: %.1f kB/s", transByte/(1024 * consumeTime));
    out = out.fromWCharArray(buf);
    ui->output->append(out);
    swprintf(buf, L"Client resent packet:%d\nPacket loss probability:%.2f%%", resent, 100 * ((double)resent / (resent + block - 1)));
    out = out.fromWCharArray(buf);
    ui->output->append(out);
    swprintf(buf, L"Client recieve bad packet:%d\nBad packet probability:%.2f%%\n", badpacket, 100 * ((double)badpacket / (badpacket + block - 1)));
    out = out.fromWCharArray(buf);
    ui->output->append(out);


    //写入日志
    // 获取时间
    time(&rawTime);
    // 转化为当地时间
    info = localtime(&rawTime);
    sprintf(logBuf, "%s INFO: upload %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
        asctime(info), filename, choose == 0 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
    fwrite(logBuf, 1, strlen(logBuf), logFp);
    fclose(logFp);
}


void MainWindow::on_download_clicked()
{
    QByteArray QremoteFile = ui->downloadServerFilename->text().toLatin1();
    QByteArray QlocalFile = ui->downloadLocalFilename->text().toLatin1();
    QByteArray QserverIP = ui->downloadServerIP->text().toLatin1();
    QByteArray QclientIP = ui->downloadLocalIP->text().toLatin1();
    char * localFile = QlocalFile.data();
    char * remoteFile = QremoteFile.data();
    char * serverIP = QserverIP.data();
    char * clientIP = QclientIP.data();
    wchar_t buf[512];   //输出错误信息
    int r_size; // UDP传输函数返回值
    // 记录重传传输次数
    int resent = 0;
    // 记录坏包次数
    int badpacket = 0;

    WSADATA wsaData;
    //初始化 winsock
    int nRC = WSAStartup(0x0101, &wsaData);
    if (nRC)
    {
        //Winsock 初始化错误
        ui->output->append("Client initialize winsock error!\n");
        return;
    }
    if (wsaData.wVersion != 0x0101)
    {
        //版本支持不够
        //报告错误给用户，清除 Winsock，返回
        ui->output->append("Client's winsock version error!\n");
        WSACleanup();
        return;
    }
    else
        ui->output->append("Client's winsock initialized !\n");


    // 创建客户端socket
    clientsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//指定传输协议为UDP
    if (clientsock == INVALID_SOCKET)
    {
        // 创建失败
        wchar_t *s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errno,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&s, 0, NULL);
        swprintf(buf,L"Client create socket with error %d%ws\n", errno, s);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        WSACleanup();
        return;
    }
    ui->output->append("Client socket create OK!\n");


    // 设置超时
    DWORD timeout = PKT_RCV_TIMEOUT;
    setsockopt(clientsock, SOL_SOCKET, SO_RCVTIMEO,  (const char*)&timeout, sizeof(timeout));


    // 将socket与主机地址绑定
    // 设置客户端 地址族(ipv4) 端口 ip
    clientAddr.sin_family = AF_INET;
    //htons函数把主机字节顺序转换为网络字节顺序
    clientAddr.sin_port = htons(0); //端口号设为0，Winsock自动为应用程序分配1024-5000端口号
    clientAddr.sin_addr.S_un.S_addr = inet_addr(clientIP); //inet_addr将字符串形式转换为unsigned long形式
    r_size = ::bind(clientsock,(LPSOCKADDR)&clientAddr, sizeof(clientAddr));
    if (r_size == SOCKET_ERROR){
        //Bind失败
        wchar_t *s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errno,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&s, 0, NULL);
        swprintf(buf,L"Bind Client with error %d%ws\n", errno, s);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        WSACleanup();
        return;
    }

    // 准备服务器的信息，这里需要指定服务器的地址
    // 设置服务器 地址族(ipv4) 端口 ip
    serverAddr.sin_family = AF_INET;
    //htons和htonl函数把主机字节顺序转换为网络字节数据
    serverAddr.sin_port = htons(69);
    serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIP);


    // 接收数据报文
    // 发送包,接收包
    tftpPacket sendPacket, rcv_packet;

    // 读请求数据包
    // 读取操作码
    sendPacket.cmd = htons(CMD_RRQ);
    // 写入文件名以及传输格式
    int choose = ui->uploadMode->currentIndex();
    if(choose == 0){
        sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "netascii", 0);
        ui->output->append("uploadMode=netascii\n");
    }
    else{
        sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "octet", 0);
        ui->output->append("uploadMode=octet\n");
    }
    sockaddr_in sender; // 接收服务器端口信息
    ui->output->append("Send Download Request\n");
    r_size = sendto(clientsock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);
    if (r_size == SOCKET_ERROR){
        wchar_t *s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errno,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&s, 0, NULL);
        swprintf(buf,L"Send Request to Server with error %d%ws\n", errno, s);
        QString out;
        out = out.fromWCharArray(buf);
        ui->output->append(out);
        WSACleanup();
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Send Request to Server with error\n",
            asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"));
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }

    // 创建本地写入文件
    FILE* fp = NULL;
    char file[512];
    sprintf(file, "D:\\111-studyfile\\ClientFiles\\%s", localFile);
    if (choose == 0)
        fp = fopen(file, "w");
    else
        fp = fopen(file, "wb");
    if (fp == NULL) {
        sprintf(file, "Create file \"%s\" error.\n", localFile);
        ui->output->append(file);
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Create file \"%s\" error.\n",
            asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
            localFile);
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }
    else{
        sprintf(file, "Create file \"%s\" Success.\n", localFile);
        ui->output->append(file);
    }

    // 接收数据
    clock_t start = clock();
    sendPacket.cmd = htons(CMD_ACK);
    // 记录传输字节大小
    int transByte = 0;
    unsigned short block = 1;
    do {
        int rxmt;
        int ifack = 0;
        //3次重传
        for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++) {
            clock_t startones = clock();
            ifack = 0;
            while(1){
                if ((clock()-startones) >= PKT_RCV_TIMEOUT) break;
                // 尝试接收
                r_size = recvfrom(clientsock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
                // 接收到数据包
                if (r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block)) {
                    swprintf(buf, L"DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
                    QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    // 发送ACK
                    swprintf(buf, L"Sent ACK block=%d\n", ntohs(rcv_packet.block));
                    //QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    sendPacket.cmd = htons(CMD_ACK);
                    sendPacket.block = rcv_packet.block;
                    sendto(clientsock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
                    ifack = 1;
                    // 写入文件
                    fwrite(rcv_packet.data, 1, r_size - 4, fp);
                    break;
                }
                // 接收错误
                if (r_size == SOCKET_ERROR && errno != 10060){
                    wchar_t *s = NULL;
                    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, errno,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPWSTR)&s, 0, NULL);
                    swprintf(buf, L"Request Recv Fail with error %d%ws\n", errno, s);
                    QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    // 写入日志
                    // 获取时间
                    time(&rawTime);
                    // 转化为当地时间
                    info = localtime(&rawTime);
                    sprintf(logBuf, "%s WARN: download %s as %s, mode: %s, Request Recv Fail\n",
                        asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"));
                    fwrite(logBuf, 1, strlen(logBuf), logFp);
                    fclose(logFp);
                    return;
                }
                // 接收超时
                if (r_size == SOCKET_ERROR && errno == 10060) {
                    ui->output->append("RECV TIMEOUT!\n");
                    break;
                }
                // 接收包错误
                else{
                    swprintf(buf, L"Bad packet: r_size=%d, rcv_packet_cmd=%d, rcv_packet_block=%d\n", r_size, ntohs(rcv_packet.cmd), ntohs(rcv_packet.block));
                    QString out;
                    out = out.fromWCharArray(buf);
                    ui->output->append(out);
                    badpacket++;
                }
                Sleep(20);
            }
            // 重传
            if (! ifack && block != 1){
                ui->output->append("Can't receive DATA, resent\n");
                // 重发ACK
                swprintf(buf, L"Sent ACK block=%d\n", block);
                QString out;
                out = out.fromWCharArray(buf);
                ui->output->append(out);
                sendPacket.cmd = htons(CMD_ACK);
                sendto(clientsock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
                resent++;
                // 写入日志
                // 获取时间
                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s WARN: download %s as %s, mode: %s, Can't receive DATA #%d, resent\n",
                    asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
                    block);
                fwrite(logBuf, 1, strlen(logBuf), logFp);
            }
            else if (! ifack && block == 1){
                //当前block为第一个，重发RRQ写请求
                ui->output->append("Can't receive DATA, resent\n");
                sendPacket.cmd = htons(CMD_RRQ);
                ui->output->append("Send Download Request\n");
                sendto(clientsock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
                resent++;
                // 写入日志
                // 获取时间
                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s WARN: download %s as %s, mode: %s, Can't receive DATA #%d, resent\n",
                    asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
                    block);
                fwrite(logBuf, 1, strlen(logBuf), logFp);
            }
            else break;
        }
        if (rxmt >= PKT_MAX_RXMT || (! ifack && block == 1))
        {
            // 3次重传失败
            // 未能连接上服务器
            // 获取时间
            ui->output->append("Could not receive from server.\n");
            //写入日志
            //获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Could not receive from server.\n",
                asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"));
            fwrite(logBuf, 1, strlen(logBuf), logFp);
            fclose(logFp);
            return;
        }
        transByte += (r_size - 4);
        block++;
    } while (r_size == DATA_SIZE + 4);
    clock_t end = clock();


    //显示传输的吞吐量
    double consumeTime = ((double)(end - start)) / CLK_TCK;
    sprintf(file, "Download file size: %.1f kB\nConsuming time: %.2f s", (double)transByte / 1024, consumeTime);
    ui->output->append(file);
    sprintf(file, "Download speed: %.1f kB/s", transByte/(1024 * consumeTime));
    ui->output->append(file);
    sprintf(file, "Client resent packet:%d\nPacket loss probability:%.2f%%", resent, 100 * ((double)resent / (resent + block - 1)));
    ui->output->append(file);
    sprintf(file, "Client recieve bad packet:%d\nBad packet probability:%.2f%%\n", badpacket, 100 * ((double)badpacket / (badpacket + block - 1)));
    ui->output->append(file);
    fclose(fp);

    //写入日志
    // 获取时间
    time(&rawTime);
    // 转化为当地时间
    info = localtime(&rawTime);
    sprintf(logBuf, "%s INFO: download %s as %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
        asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
    fwrite(logBuf, 1, strlen(logBuf), logFp);
    fclose(logFp);
}


void MainWindow::on_FileChoose_clicked()
{
    QDir dir;
    QString PathName = QFileDialog::getOpenFileName(this,tr(""),"",tr("file(*)")); //选择路径
    ui->FilePath->setText(PathName);    //文件名称显示

}


void MainWindow::on_ClearButton_clicked()
{
    ui->output->clear();
}

