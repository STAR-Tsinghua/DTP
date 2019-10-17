DTP 是一个面向确定性时延的传输协议，应用将传输数据块的时延需求告知DTP，DTP通过调度和FEC等手段尽力满足，可以减轻应用开发者的负担并提升传输的性能。DTP发表在[APNet 2019](https://dl.acm.org/citation.cfm?id=3343191)上，其设计见[slides](https://conferences.sigcomm.org/events/apnet2019/slides/Session_1_1.pptx)。

## 特色
### 易于部署和使用
首先，DTP 不依赖中间网络设备的协助，是纯粹的端的解决方案，谁部署谁受益。其次，DTP 在QUIC的基础上修改而来，是纯用户态的协议栈，应用将其打包成library来使用，不需要修改kernel。最后，DTP的API是在BSD socket 基础上拓展而来，应用修改起来很容易，DTP实现的方式是State Machine(用Rust实现)+平台特定的Socket IO API，易于跨平台。

### 通过发送端调度来优先保证高优先级数据的传输
应用将数据块的优先级、截止时间、依赖关系等属性告知DTP，DTP的调度综合考虑多种因素来决定发送的数据块。调度器的目标是优先保证高优先级的数据块在截止时间之前的到达。

### 通过冗余模块来避免重传
网络丢包引起的重传同样会增大传输的时延，针对这个问题，DTP的冗余模块会生成冗余包，在丢包之后无需重传即可在接收端恢复出原始数据包。DTP的冗余模块只会在重传时延过大会引起截止时间内无法传到时候触发。

## 应用
本项目在第二届AITrans智能网络技术挑战赛中作为基础环境使用。基于DTP开发的Live Video System见[Live Evaluation System](https://github.com/STAR-Tsinghua/LiveEvaluationSystem)。