# 一、前言

在逻辑处理模块中，响应HTTP请求采用主从状态机来完成。

传统的控制流程都是按照顺序执行的，状态机能处理任意顺序的事件，并能提供有意义的响应---即使这些时间发生的顺序和预计的不同。

# 二、如何响应收到HTTP请求的报文

## **(1) http连接请求处理**

在启动服务器时，先创建好线程池。当浏览器端发出http连接请求，主线程创建http类对象数组用来接收请求并将所有数据读入各个对象对应buffer，然后将该对象插入任务队列；如果是连接请求，那么就将他注册到内核事件表中（通过静态成员变量完成）。线程池中的工作线程从任务队列中取出一个任务进行处理（解析请求报文）。

## (2) **http响应报文处理流程**

当上述报文解析完成后，服务器子线程调用process_write完成响应报文，响应报文包括

1.状态行：http/1.1 状态码 状态消息；

2.消息报头，内部调用add_content_length和add_linger函数

l content-length记录响应报文长度，用于浏览器端判断服务器是否发送完数据

l connection记录连接状态，用于告诉浏览器端保持长连接

3.空行

随后注册epollout事件。服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端。至此整个http请求和响应全部完成。

## (3) **GET和POST的区别**

- 最直观的区别就是GET把参数包含在URL中，POST通过request body传递参数。
- GET请求参数会被完整保留在浏览器历史记录里，而POST中的参数不会被保留。
- GET请求在URL中传送的参数是有长度限制。（大多数）浏览器通常都会限制url长度在2K个字节，而（大多数）服务器最多处理64K大小的url。
- GET产生一个TCP数据包；POST产生两个TCP数据包。对于GET方式的请求，浏览器会把http header和data一并发送出去，服务器响应200（返回数据）；而对于POST，浏览器先发送header，服务器响应100（指示信息—表示请求已接收，继续处理）continue，浏览器再发送data，服务器响应200 ok（返回数据）。

## (4) HTTP 状态码

- 1xx：指示信息--表示请求已接收，继续处理。
- 2xx：成功--表示请求正常处理完毕。
  - 200 OK：客户端请求被正常处理。
  - 206 Partial content：客户端进行了范围请求。
- 3xx：重定向--要完成请求必须进行更进一步的操作。
  - 301 Moved Permanently：永久重定向，该资源已被永久移动到新位置，将来任何对该资源的访问都要使用本响应返回的若干个URI之一。
  - 302 Found：临时重定向，请求的资源现在临时从不同的URI中获得。
- 4xx：客户端错误--请求有语法错误，服务器无法处理请求。
  - 400 Bad Request：请求报文存在语法错误。
  - 403 Forbidden：请求被服务器拒绝。
  - 404 Not Found：请求不存在，服务器上找不到请求的资源。
- 5xx：服务器端错误--服务器处理请求出错。
  - 500 Internal Server Error：服务器在执行请求时出现错误。





## **(5) 主从状态机的模式**

### 1 为什么要用状态机？

传统的控制流程都是按照顺序执行的，状态机能处理任意顺序的事件，并能提供有意义的响应---即使这些时间发生的顺序和预计的不同。

项目中使用**主从状态机**的模式进行解析，从状态机（`parse_line`）负责读取报文的一行，主状态机负责对该行数据进行解析，主状态机内部调用从状态机，从状态机驱动主状态机。每解析一部分都会将整个请求的`m_check_state`状态改变，状态机也就是根据这个状态来进行不同部分的解析跳转的：

> 以下图片来源于《两猿社》~ 非常棒的服务器讲解！

![图片](https://ydlin.oss-cn-guangzhou.aliyuncs.com/blog-img/640.webp)

#### **主状态机**

三种状态，标识解析位置。

- CHECK_STATE_REQUESTLINE，解析请求行
- CHECK_STATE_HEADER，解析请求头
- CHECK_STATE_CONTENT，解析消息体，仅用于解析POST请求

#### **从状态机**

三种状态，标识解析一行的读取状态。

- LINE_OK，完整读取一行
- LINE_BAD，报文语法有误
- LINE_OPEN，读取的行不完整





```cpp
void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret)
        close_conn();
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
```

HTTP请求报文：请求行（request line）、请求头部（header）、空行和请求数据

响应报文:状态行、消息报头、空行和响应正文。

### 2 有没有想过状态机会给项目带来哪些危害？

缺点：状态机的缺点就是性能比较低，一般一个状态做一个事情，性能比较差，在追求高性能的场景下一般不用，高性能场景一般使用流水线设计。

### 3 你的项目http请求怎么做的？如何保证http请求完整解析

该项目使用线程池（半同步半反应堆模式）并发处理用户请求，主线程负责读写，工作线程（线程池中的线程）负责处理逻辑（HTTP请求报文的解析等等）

主从状态机可以保证完整解析。

如何响应

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBG9ibQZ4SgllXZqrkObpUHNKNoh8SsGMyOSGIgaE8nZdGhYua3E84VojicmKuJoict9s3ibraK6Lux1dQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)



## 小结

HTTP请求处理中主从状态机的思想非常有趣，简化了项目处理流程。数据库连接池是如何运行的，我们将放在下一章进行讲解。



# 四、参考资料：

- 《两猿社》
- 《Linux高性能服务器》