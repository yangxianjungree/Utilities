#include <functional>
#include <memory>

class IoReactors
{
public:
    typedef std::function<void(int, char*)> EventCallback;

    explicit IoReactors() = default;
    virtual ~IoReactors() = default;

    virtual void Stop() = 0;
    virtual void Run() = 0;
    virtual void SetCallBack(EventCallback callBack) = 0;
    virtual void SendAndClosedFd(int fd, const char* buff, int len) = 0;
};

std::shared_ptr<IoReactors> Create(int port, int maxListenFdNum = 1024, int epollTimeWaitMs = -1, bool debug = false);