#include "../fileserverHeaders/fileserver.h"

int main(void)
{
    fileserver_t fileserver;

    // 47.96.75.101
    if (InitFileServer(&fileserver, "192.168.0.103", 10926) != 0)
    {
        return -1;
    }
    if (StartReadyFileServer(&fileserver) != 0)
    {
        return -1;
    }
    Start(&fileserver);

    return 0;
}