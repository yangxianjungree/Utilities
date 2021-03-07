#include "IoReactors.h"

#include <unistd.h>

void PrintInfo(int fd, const char* buff)
{
    printf("call back recv msg: %s. \n", buff);
}

int main()
{
    auto io = Create(9999, 100000, -1, true);
    io->SetCallBack(IoReactors::EventCallback(PrintInfo));
    io->Run();
    while (true) {
        sleep(10);
        printf("Wake up after 10s. \n");
    }
    return 0;
}