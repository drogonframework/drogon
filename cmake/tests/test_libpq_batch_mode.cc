#include <libpq-fe.h>

int main()
{
    PQisInBatchMode(NULL);
    PQbatchIsAborted(NULL);
    PQqueriesInBatch(NULL);
    PQbeginBatchMode(NULL);
    PQendBatchMode(NULL);
    PQsendEndBatch(NULL);
    PQgetNextQuery(NULL);
}
