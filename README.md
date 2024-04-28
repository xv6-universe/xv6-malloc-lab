# OS-MinorHW2

## 1. 作业介绍

### 1.1 malloc

我们实现了 segregated free list + best fit 的架构。具体地说，我们将所有大小在 $[2^{i-1}, 2^i)$ 范围内的空闲块用一个链表维护，并且链表是按照空闲块的大小排序的。

另外，针对 `realloc(int ptr)`，我们也进行了对应的优化，在能与前后块合并的前提下进行了合并，防止了新开空间和 `memcpy()` 的开销。

当然，我们也可以为每个 bucket 开一个全局数组来服务 second fit，但由于时间关系这一功能并未实现。

### 1.2. 共享内存页

我们设计了如下的系统调用：

```c++
int             mkshpg(uint64); // make **one** shared page for a given key
int             rmshpg(uint64); // unbind a shared page for a given process
uint64         bdshpg(uint64); // bind a shared page for a process
int             ivshpg(uint64); // free the shared page under certain key
int             qyshct(uint64); // query the creator of a given shared page
int             qyshn(uint64); // query the refcnt of a given shared page
int             chshct(uint64); // change the creator
uint64         gtshmd(uint64); // get the permission of a shared page for a process
int             chshmd(uint64, uint64); // change permission of a shared page for a process
```

另外，我们的设计有如下 feature:

1. 用于共享内存的虚拟地址是通过增加每个进程的 `p->sz` 来实现的，另外为了防止在 `freeproc()` 的时候出现 `unmapped error`，我们删掉了该错误
2. 进程之间可以通过约定相同的 key 来进行页共享
3. `fork()` 之后默认会继承共享内存页

## 2. 测试

user-level malloc 已经通过 @KelvinMYYZJ 设计的 allocation_checker 并通过 baseline，数据如下 （在 intel CORE i5 上完成测试）：

```
finishing test: amptjp-bal.rep
heap used : 2023740 bytes
time : 60031009
finishing test: binary2-bal.rep
heap used : 1119948 bytes
time : 326360047
finishing test: binary-bal.rep
heap used : 2096108 bytes
time : 150494588
finishing test: cccp-bal.rep
heap used : 1685428 bytes
time : 70763900
finishing test: coalescing-bal.rep
heap used : 12372 bytes
time : 57203
finishing test: cp-decl-bal.rep
heap used : 3182812 bytes
time : 85247942
finishing test: expr-bal.rep
heap used : 3434236 bytes
time : 69217524
finishing test: random2-bal.rep
heap used : 15188156 bytes
time : 68932294
finishing test: random-bal.rep
heap used : 15445276 bytes
time : 68650201
finishing test: realloc2-bal.rep
heap used : 62612 bytes
time : 198953246
finishing test: realloc-bal.rep
heap used : 1316772 bytes
time : 253366007
finishing test: short1-bal.rep
heap used : 12324 bytes
time : 71400
finishing test: short2-bal.rep
heap used : 20428 bytes
time : 82459

```

另外其余几种设计的结果也放在了 `./test_result` 中。

----

关于共享内存页，我们设计了一个非常简单的 tester （见 `./user/sharedmemtest.c`）

其主要测试了如下功能：

1. 基本的基于共享内存页的进程通信
2. 测试了 creator 的查询和修改功能
3. 测试了 ref_cnt 只有在**等于 0**的情况下才会 `kfree()` 的功能
4. 测试了对共享内存页的 link/unlink 功能

关于读写权限，由于权限不正确会导致 `usertrap()`，并没有在测试程序中测试