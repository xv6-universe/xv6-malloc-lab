#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_getclk(void)
{
  return *(uint64*) CLINT_MTIME;
}

uint64 sys_mkshpg(void) {
  uint64 key;
  argaddr(0, &key);

  return mkshpg(key);
}

uint64 sys_rmshpg(void) {
  uint64 key;
  argaddr(0, &key);

  return rmshpg(key);
}

uint64 sys_bdshpg(void) {
  uint64 key;
  argaddr(0, &key);

  return bdshpg(key);
}

uint64 sys_ivshpg(void) {
  uint64 key;
  argaddr(0, &key);

  return ivshpg(key);
}

uint64 sys_qyshct(void) {
  uint64 key;
  argaddr(0, &key);

  return qyshct(key);
}

uint64 sys_qyshn(void) {
  uint64 key;
  argaddr(0, &key);

  return qyshn(key);
}

uint64 sys_chshct(void) {
  uint64 key;
  argaddr(0, &key);

  return chshct(key);
}

uint64 sys_gtshmd(void) {
  uint64 key;
  argaddr(0, &key);

  return gtshmd(key);
}

uint64 sys_chshmd(void) {
  uint64 key;
  uint64 value;
  argaddr(0, &key);
  argaddr(1, &value);

  return chshmd(key, value);
}