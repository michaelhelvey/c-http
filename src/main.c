#include "common.h"
#include <getopt.h>

struct option long_options[] = { { "help", no_argument, 0, 'h' },
    { "version", no_argument, 0, 'v' }, { "port", required_argument, 0, 'p' } };

u16 port = 8080;

int main(int argc, char* argv[])
{
    switch (getopt_long(argc, argv, "hvp:", long_options, NULL)) {
    case 'h':
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Async http server for fun.\n\n");
        printf("  -p, --port=PORT specify the port to listen on\n");
        printf("  -h, --help      display this help and exit\n");
        printf("  -v, --version   output version information and exit\n");
        return 0;
    case 'v':
        printf("c-http %s\n", VERSION);
        return 0;
    case 'p':
        port = atoi(optarg);
        break;
    case -1:
        break;
    default:
        return 1;
    }

    println("starting server on port %d", port);
}
