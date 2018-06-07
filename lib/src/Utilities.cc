#include "Utilities.h"
#include <string.h>
//解url编码实现
int urldecode(const char* encd,char* decd)
{
    int j;
    char *cd =(char*) encd;
    char p[2];

    j=0;

    for( size_t i = 0; i < strlen(cd); i++ )
    {
        memset( p,0,2);
        if( cd[i] != '%' )
        {
            if(cd[i]=='+')
                decd[j++]=' ';
            else
                decd[j++] = cd[i];
            continue;
        }

        p[0] = cd[++i];
        p[1] = cd[++i];

        p[0] = p[0] - 48 - ((p[0] >= 'A') ? 7 : 0) - ((p[0] >= 'a') ? 32 : 0);
        p[1] = p[1] - 48 - ((p[1] >= 'A') ? 7 : 0) - ((p[1] >= 'a') ? 32 : 0);
        decd[j++] = (unsigned char)(p[0] * 16 + p[1]);

    }

    decd[j] = 0;
    return j;
}