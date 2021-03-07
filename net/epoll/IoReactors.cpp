#include "IoReactors.h"

#include <unistd.h>
#include <cstring>
#include <thread>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EPOLL_EVENT_NUM 50000
#define DEBUG_BUFF_LEN      100
#define SUCCESS     "success"
#define FAIL        "failed"

class DataBuffer
{
public:
    DataBuffer(int len)
        : mLen(len)
    {
        mBuffer = new char[len];
        memset(mBuffer, 0, len);
    }

    ~DataBuffer()
    {
        delete [] mBuffer;
    }

    int Length() { return mLen; }
    char* GetBuffer() { return mBuffer; }

private:
    char* mBuffer;
    int mLen;
};

class IoReactorsImp : public IoReactors
{
public:
    IoReactorsImp(int port, int listenNum, int timeWaitMs, bool debug)
        : mStopped(false)
        , mRunning(false)
        , mDebugMode(debug)
        , mEpollTimeWaitMs(timeWaitMs)
    {
        InitAcceptFd(port, listenNum);
    }

    ~IoReactorsImp() override
    {
        Stop();
        ::close(mMainEpollFd);
        ::close(mAcceptEpollFd);
    }

    void Stop() override
    {
        mStopped = true;

        if (mRunning) {
            mAcceptThread->join();
            mAcceptThread->detach();
            mReactorThread->join();
            mReactorThread->detach();
            mRunning = false;
        }
    }

    void Run() override
    {
        if (mRunning) {
            return;
        }
        mRunning = true;

        mReactorThread = std::make_shared<std::thread>(std::bind(&IoReactorsImp::MainEventLoop, this));
        mAcceptThread = std::make_shared<std::thread>(std::bind(&IoReactorsImp::AcceptEventLoop, this));
    }

    // todo: maybe thread unsafe.
    void SetCallBack(EventCallback callBack) override
    {
        mCallBack = std::move(callBack);
    }

    void SendAndClosedFd(int fd, const char* buff, int len) override
    {
        char* data = new char[len];
        memcpy(data, buff, len);

        struct epoll_event ev;
        ev.data.fd = fd;
        ev.data.ptr = (void*)data;
        ev.data.u32 = len;
        ev.events = EPOLLOUT;
        epoll_ctl(mMainEpollFd, EPOLL_CTL_MOD, fd, &ev);
    }

protected:
    void InitAcceptFd(int port, int listenNum)
    {
        mListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (mListenFd < 0) {
            printf("socket error. \n");
            exit(-1);
        }

        int opt = 1;
        if (setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt))) {
            perror("set socket opt");
            exit(-1);
        }

        struct sockaddr_in addr;
        ::memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(mListenFd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
            printf("bind error. \n");
            exit(-2);
        }

        if (::listen(mListenFd, listenNum) < 0) {
            printf("listen error. \n");
            exit(-3);
        }
    }

    void EventLoop(int epollFd)
    {
        struct epoll_event events[MAX_EPOLL_EVENT_NUM] = {0};
        while (!mStopped) {
            int nReady = epoll_wait(epollFd, events, MAX_EPOLL_EVENT_NUM, mEpollTimeWaitMs);
            if (nReady < 0) {
                printf("some error happened, errno: %d.\n", errno);
                break;
            } else if (nReady == 0) {
                printf("nothing happened.\n");
            } else {
                HandleEvents(nReady, events);
            }
        }
    }

    void MainEventLoop()
    {
        mMainEpollFd = ::epoll_create1(EPOLL_CLOEXEC);
        if (mMainEpollFd < 0) {
            printf("epoll_create1 main epoll fd failed. \n");
            exit(-1);
        }

        EventLoop(mMainEpollFd);
    }

    void AcceptEventLoop()
    {
        mAcceptEpollFd = ::epoll_create1(EPOLL_CLOEXEC);
        if (mAcceptEpollFd < 0) {
            printf("epoll_create1 listen epoll fd failed. \n");
            exit(-1);
        }

        AddEpollEvent(mAcceptEpollFd, mListenFd, EPOLLIN);

        EventLoop(mAcceptEpollFd);
    }

    void HandleEvents(int n, struct epoll_event* events) {
        for (int i = 0; i < n; ++i) {
            if (mListenFd == events[i].data.fd) {
                HandleAccept();
                continue;
            }

            uint32_t r = events[i].events;
            if ((r & EPOLLHUP) && !(r & EPOLLIN)) {
                printf("fd = %d Channel::handle_event() EPOLL HUP.\n", events[i].data.fd);
            }
            if (r & (EPOLLERR)) {
            }
            if (r & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
                HandleRead(events[i].data.fd);
            }
            if (r & EPOLLOUT) {
                HandleWrite(&events[i]);
            }
        }
    }

    void HandleRead(int fd)
    {
        // todo: difference between recv/read
        if (mDebugMode) {
            DataBuffer buf(DEBUG_BUFF_LEN);
            int n = ::recv(fd, buf.GetBuffer(), DEBUG_BUFF_LEN, MSG_DONTWAIT);

            if (n < 0) {
                printf("Recv msg err, errno: %d.\n", errno);
            } else if (0 == n) {
                printf("socket closed, fd: %d.\n", fd);
                ModifyEpollEvent(mMainEpollFd, fd, 0);
                close(fd);
                return;
            }

            if (mCallBack) {
                mCallBack(fd, buf.GetBuffer());
            }

            printf("closed socket, fd: %d.\n", fd);
            ModifyEpollEvent(mMainEpollFd, fd, 0);
            close(fd);
            return;
        }

        // todo: the length of len maybe different in other platforms.
        int len = 0;
        int nRecv = 0;
        DataBuffer buff(len);
        int n = ::recv(fd, &len, sizeof(int), MSG_WAITALL);
        if (n < 1 || len < 1) {
            return;
        }

        while (nRecv < len) {
            n = ::recv(fd, buff.GetBuffer() + nRecv, len - nRecv, MSG_DONTWAIT);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
            } else if (0 == n) {
                ModifyEpollEvent(mMainEpollFd, fd, 0);
                close(fd);
                return;

            }
            nRecv += n;
        }

        if (nRecv != len) {
            printf("Recv data length: %d, should be: %d. \n", nRecv, len);
            SendAndClosedFd(fd, FAIL, strlen(FAIL));
        }

        if (mCallBack) {
            mCallBack(fd, buff.GetBuffer());
        }
    }

    void HandleAccept()
    {
        struct sockaddr_in clientAddr;
        ::memset(&clientAddr, 0, sizeof(struct sockaddr));
        socklen_t len;
        int clientFd = ::accept(mListenFd, (struct sockaddr*)&clientAddr, &len);
        if (clientFd <= 0) {
            return;
        }
        char str[INET_ADDRSTRLEN] = {0};
        printf("recv from %s at port %d\n", ::inet_ntop(AF_INET, &clientAddr.sin_addr, str, sizeof(str)),
               ntohs(clientAddr.sin_port));

        AddEpollEvent(mAcceptEpollFd, mListenFd, EPOLLIN);
        AddEpollEvent(mMainEpollFd, clientFd, EPOLLIN | EPOLLET);
    }

    void HandleWrite(struct epoll_event* event)
    {
        int fd = event->data.fd;
        char* buff = (char*)event->data.ptr;
        int len = event->data.u32;
        int n = sizeof(len);

        do {
            if (n != ::send(fd, &len, n, 0)) {
                printf("Send msg len may miss. \n");
                break;
            }
            if (len != ::send(fd, buff, len, 0)) {
                printf("Send msg data may miss. \n");
                break;
            }
            printf("Send msg success. \n");
        } while (0);

        ModifyEpollEvent(mMainEpollFd,event->data.fd, 0);
        close(fd);
    }

    int ModifyEpollEvent(int epFd, int fd, int event)
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        return epoll_ctl(epFd, EPOLL_CTL_MOD, fd, &ev);
    }

    int AddEpollEvent(int epFd, int fd, int event)
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        return epoll_ctl(epFd, EPOLL_CTL_ADD, fd, &ev);
    }

    int DelEpollEvent(int epFd, int fd,  int event)
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        return epoll_ctl(epFd, EPOLL_CTL_DEL, fd, &ev);
    }

protected:
    int     mListenFd;
    int     mMainEpollFd;
    int     mAcceptEpollFd;
    int     mEpollTimeWaitMs;
    bool    mStopped;
    bool    mRunning;
    bool    mDebugMode;
    std::shared_ptr<std::thread>    mAcceptThread;
    std::shared_ptr<std::thread>    mReactorThread;
    EventCallback                   mCallBack;
};


std::shared_ptr<IoReactors> Create(int port, int maxListenFdNum, int epollTimeWaitMs, bool debug)
{
    return std::shared_ptr<IoReactors>(new IoReactorsImp(port, maxListenFdNum, epollTimeWaitMs, debug));
}