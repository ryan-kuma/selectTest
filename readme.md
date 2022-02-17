### 一、select
- 得通过read返回0来判断连接断开
- 在事件到达时，得通过循环遍历得到具体文件描述符
- select中nfds为最大文件描述符加一，故循环开头得搜索maxfd
- 由于无法正常判断连接断开，所以有时候会select会报描述符错误的error

### 二、poll
- 可以通过POLLRDHUP时间来判断连接断开
- 可以在pollfd中绑定文件描述符和相应的事件

### 三、epoll
- 相比于poll需要遍历所有文件描述符确定已到达的感兴趣事件，epoll可以对到达事件结构体数组epoll_events进行遍历处理,故epoll可以对事件分组处理，处理方式更灵活，而poll则以文件描述符为单位
- epoll中epoll_wait(int epollfd,struct epoll_event *events, int maxevents, int timeout),通常对event做分组处理,而poll中poll(struct pollfd* fds, nfds_t nfds, int timeout)通常对fd进行分组处理
