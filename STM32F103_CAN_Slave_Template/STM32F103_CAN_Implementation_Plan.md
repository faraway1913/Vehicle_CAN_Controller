# STM32F103 CAN从节点实现计划

## Context
本计划针对STM32F103 CAN从节点（仪表端）的开发需求，作为车载域控制器模拟系统的一部分，与STM32F407主节点（ECU端）通过CAN总线通信。当前已完成CubeMX基础配置，需要实现CAN接收过滤、LED心跳指示功能，并完成双板联调前的硬件核查。

## Implementation Plan

### 1. 优化CAN过滤器配置
**文件路径**: [Core/Src/main.c](Core/Src/main.c)
- 替换原有全通过滤器配置为只接收主节点ID范围`0x100~0x10F`
- 过滤器参数：32位掩码模式，过滤ID`0x1000`，掩码`0xFFF0`
- 确保CAN启动和接收中断激活配置正确

### 2. 实现CAN接收回调函数
**文件路径**: [Core/Src/freertos.c](Core/Src/freertos.c)
- 定义报文ID宏和接收数据结构体
- 实现`HAL_CAN_RxFifo0MsgPendingCallback`函数
- 功能：数据接收、按ID解析、串口格式化打印

### 3. 实现FreeRTOS LED心跳任务
**文件路径**: [Core/Src/freertos.c](Core/Src/freertos.c)
- 添加LED任务定义和属性配置
- 实现500ms周期的LED闪烁任务函数
- 在`MX_FREERTOS_Init`中创建LED任务

### 4. 硬件核查（联调前执行）
按照以下清单逐一检查：
1. 电源供电：两板供电稳定，纹波<100mV
2. CAN收发器：TJA1050模块接线正确，供电正常
3. 终端电阻：总线两端120Ω电阻已安装，总电阻≈60Ω
4. 信号接线：CAN_H/CAN_L交叉连接正确，RX/TX引脚配置对应
5. 共地检查：两板GND已连接，压差<0.5V
6. 配置一致性：两板波特率均为500kbps，时钟配置匹配

## Verification
1. **单独功能验证**：
   - 烧录程序后LED以500ms周期闪烁，说明FreeRTOS运行正常
   - 串口打印开机信息，说明printf重定向正常

2. **CAN功能验证**：
   - 主节点发送测试报文，从节点串口正确打印接收数据
   - 发送非0x100~0x10F范围的ID，从节点不接收，说明过滤器工作正常

3. **联调验证**：
   - 主节点发送ADC油门值和温湿度数据，从节点正确解析并打印
   - 使用逻辑分析仪抓取CAN报文，确认ACK位正常，数据帧结构正确