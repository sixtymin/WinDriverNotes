#同步#

###R3的同步###



###R0的同步###

**自旋锁**

它是一种同步处理机制，能够保证某个资源只能被一个线程所拥有。自旋锁如其名字含义，如果有程序申请获取的自旋锁被锁住了，那么这个线程就进入了自旋状态。所谓自旋状态就是不停询问是否可以获取自旋锁。

它与等待事件不同，自旋状态不会让当前线程进入休眠，而是一直处于自旋，这不太适合与长时间占用锁的逻辑。


```
KSPIN_LOCK MySpinLock;  // 定义自旋锁对象，一般要定义为全局的。

KIRQL oldirql;
KeAcquireSpinLock(&MySpinLock, &oldirql);

KeReleaseSpinLock(&MySpinLock, &oldirql);
```
