### 一、select
- 得通过read返回0来判断连接断开
- 在事件到达时，得通过循环遍历得到具体文件描述符
- select中nfds为最大文件描述符加一，故循环开头得搜索maxfd


### 二、poll
- 可以通过POLLRDHUP时间来判断连接断开
- 可以在pollfd中绑定文件描述符和相应的事件


### 三、epoll
