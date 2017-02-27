# mod_event_zeromq

FreeSWITCH 的 ZeroMQ 接口模块。
官方的mod_event_zmq模块长时间没有维护和升级，并且采用的是非常老的ZeroMQ 2.1.9版本。

mod_event_zeromq 参照官方版本模块用C语言重新编写，支持新版本的ZeroMQ V4.2.2 在FreeSWITCH V1.6 版本下开发。

编译前需要安装libzmq。

ZeroMQ安装：
git clone https://github.com/zeromq/libzmq.git
指定版本：git checkout  v4.2.2
./autogen.sh
./configure
./make
./make install

## V0.1
增加event_zeromq.conf.xml可以对ZeroMQ bind endpoint进行配置

TODO：
1. 增加API访问接口
2. Event消息过滤