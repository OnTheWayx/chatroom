#include "../fileserverHeaders/fileserver.h"

int main(void)
{
    fileserver_t fileserver;

    if (InitFileServer(&fileserver, "127.0.0.1", 9999) != 0)
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