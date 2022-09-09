#include "../serverHeaders/server.h"

int main(int argc, char const *argv[])
{
    //捕捉SIGINT信号关闭服务器
    if (signal(SIGINT, CloseServer) == SIG_ERR)
    {
        ERRLOG("signal");

        return FAILURE;
    }
    InitServer();
    
    return 0;
}
