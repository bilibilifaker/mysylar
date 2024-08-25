#include "../src/hook.h"
#include "../src/IOManager.h"
#include "../src/log.h"
#include "../src/timer.h"
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <arpa/inet.h>

sylar::Logger::ptr hook_logger = SYLAR_LOG_ROOT();

void test_sleep(){
    sylar::IOManager iom(1);

    iom.schedule([](){
        sleep(4);
        SYLAR_LOG_INFO(hook_logger) << "sleep 4";
    });

    iom.schedule([](){
        sleep(8);
        SYLAR_LOG_INFO(hook_logger) << "sleep 8";
    });

    SYLAR_LOG_INFO(hook_logger) << "test_sleep";
}

void test_sock() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "39.156.66.18", &addr.sin_addr.s_addr);
    SYLAR_LOG_INFO(hook_logger) << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    SYLAR_LOG_INFO(hook_logger) << "connect rt = " << rt << " ,errno = " << errno;
    if(rt) {
        return ;
    }
    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_INFO(hook_logger) << "send rt = " << rt << " ,errno = " << errno;

    if(rt <= 0) {
        return;
    }

    std::string buff;
    buff.resize(40960);

    rt = recv(sock, &buff[0], buff.size(), 0);
    SYLAR_LOG_INFO(hook_logger) << "recv rt = " << rt << " ,errno = " << errno;
    if(rt <= 0) {
        return;
    }

    buff.resize(rt);
    SYLAR_LOG_INFO(hook_logger) << buff;
}


int main() {
    //test_sleep();
    sylar::IOManager iom;
    iom.schedule(test_sock);
    return 0;
}
