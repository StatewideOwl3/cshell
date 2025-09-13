#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int fd;
    char buf[100];
    int n;

    // Call getreadcount before reading
    uint64 before = getreadcount();
    printf("Read count before: %d\n", (int)before);

    // Open a file
    if(argc < 2){
        printf("Usage: readcount filename\n");
        exit(1);
    }

    fd = open(argv[1], 0);
    if(fd < 0){
        printf("Cannot open %s\n", argv[1]);
        exit(1);
    }

    // Read 100 bytes
    n = read(fd, buf, sizeof(buf));
    printf("Read %d bytes from file %s\n", n, argv[1]);

    close(fd);

    // Call getreadcount after reading
    uint64 after = getreadcount();
    printf("Read count after: %d\n", (int)after);

    exit(0);
}
