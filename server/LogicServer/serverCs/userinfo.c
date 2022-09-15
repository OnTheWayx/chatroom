#include "../serverHeaders/userinfo.h"

void strcpy_usrinfo(usrinfo *dest, const usrinfo *src)
{
    strncpy(dest->id, src->id, 6);
    strncpy(dest->name, src->name, 13);
    strncpy(dest->passwd, src->passwd, 15);
}

void init_usrinfo(usrinfo *dest, const char *id, const char *passwd, const char *name)
{
    if (id != NULL)     strncpy(dest->id, id, 6);
    if (passwd != NULL) strncpy(dest->passwd, passwd, 13);
    if (name != NULL)   strncpy(dest->name, name, 15);

    return;
}