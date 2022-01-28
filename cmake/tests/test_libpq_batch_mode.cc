#include <libpq-fe.h>

int main()
{
    PQenterPipelineMode(NULL);
    PQexitPipelineMode(NULL);
    PQpipelineSync(NULL);
    PQpipelineStatus(NULL);
}
