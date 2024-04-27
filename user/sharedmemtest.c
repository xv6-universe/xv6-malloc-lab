#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    mkshpg(114514);
    mkshpg(0);
    int pid = fork();
    if (pid == 0) {
        uint64 *p = (uint64 *)bdshpg(114514);
        uint64 *q = (uint64 *)bdshpg(0);
        int creator = qyshct(114514);
        printf("original creator of p: %d\n", creator);
        chshct(114514);
        *p = 1919810;
        *q = 19260817;
        printf("child\n");
    } else {
        sleep(10);
        uint64 *q = (uint64 *)bdshpg(0);
        uint64 *p = (uint64 *)bdshpg(114514);
        int creator = qyshct(114514);
        printf("q's value is: %d\n", *q);
        printf("p's value is: %d\n", *p);
        printf("creator of p: %d\n", creator);

        int num = qyshn(0);
        printf("number of processes sharing 0: %d\n", num);
        rmshpg(0);
        num = qyshn(0);
        printf("number of processes sharing 0 after unlinking: %d\n", num);
        printf("parent\n");
    }
    exit(0);
}