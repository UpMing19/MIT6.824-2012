# MIT6.824-2012

lab4 
set用queue代替，也许能解决锁的抢占问题
retryQueue和releaseQueue合并为一个retryQueue，在client端，retryQueue代表要像服务器发送请求的线程，WaitQueue是本地线程