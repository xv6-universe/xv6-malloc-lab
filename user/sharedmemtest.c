#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"

void fail(char *msg) {
  printf("%s\n", msg);
  printf("test failed\n");
  exit(1);
}

int creator = 0;

int main(int argc, char *argv[]) {
  mkshpg(114514);
  mkshpg(0);
  int pid = fork();
  if (pid == 0) {
    printf("---------------- child process ----------------\n");
    uint64 *p = (uint64 *) bdshpg(114514);
    uint64 *q = (uint64 *) bdshpg(0);
    printf("process %d created a shared page with key %d\n", qyshct(114514), 114514);
    printf("process %d created a shared page with key %d\n", qyshct(0), 0);
    printf("change creator to the current process......\n");
    chshct(114514);
    chshct(0);
    printf("connecting...writing a value to both pages\n");
    *p = 1919810;
    *q = 19260817;
  } else {
    sleep(2);

    printf("--------------- parent process ----------------\n");

    uint64 *q = (uint64 *) bdshpg(0);
    uint64 *p = (uint64 *) bdshpg(114514);
    printf("process %d created a shared page with key %d\n", qyshct(114514), 114514);
    printf("process %d created a shared page with key %d\n", qyshct(0), 0);
    printf("\nconnection test passed!\n\n");
    int num = qyshn(0);
    printf("number of processes sharing 0: %d\n", num);

    printf("reading values from shared pages...\n");

    if (*p != 1919810) {
      fail("value read from shared page 114514 is incorrect");
    } else {
      printf("value read from shared page 114514: %d\n", *p);
    }

    if (*q != 19260817) {
      fail("value read from shared page 0 is incorrect");
    } else {
      printf("value read from shared page 0: %d\n", *q);
    }


    rmshpg(0);
    printf("unlinking...\n");
    num = qyshn(0);
    if (num != 1) {
      fail("unlink failed");
    } else {
      printf("number of processes sharing 0 after unlinking: %d\n", num);
      printf("\nunlinking test passed!\n\n");
    }

    wait(0);

    printf("child process dies, check validity of shared pages...\n");
    if (*p != 1919810) {
      fail("value read from shared page 114514 is incorrect");
    } else {
      printf("persistent value read from shared page 114514: %d\n", *p);
    }

    printf("\ntest passed!!!\n");
  }
  exit(0);
}