# P2P-TCP
## A Peer to Peer Protocol using TCP for simulating FC ASM

VER1.00

 最近一个项目需要模拟FC的流消息协议。该协议最大一包数据可以发送16MB。本机用UDP最大一包只能发送16KB。所以改成用TCP+自己修改的PPP协议去模拟。
 TCP协议最大一包可以满足16MB的发送形式的要求。而PPP协议可以结局粘包问题，保证发送端发送多少字节，接收端调用一次接收，收到的就是多少字节
 本项目的TCP和PPP基于C语言开发，采用面向对象的方式，每种协议都抽象为一个句柄。
 TCP目前为了简单处理，只支持点对点的方式，一个服务端socket对应一个客户端的socket。如果多个客户端访问，需要创建多个服务端SOCKET。当然也能满足当前项目需求，但是并未利用TCP的特性。
 TCP最好运行在非阻塞状态下，这样用户使用的时候只需要关注初始化，接收和发送三个功能即可，无需手动connect和accept。当然connect和accept的接口也是开放的，可以手动调用。

VER2.00

屏蔽connect和accept接口，接收方或者发送方任意一方不论调用recv还是send就能实现accept或者send功能。
增加多路复用模式，待后续优化。
