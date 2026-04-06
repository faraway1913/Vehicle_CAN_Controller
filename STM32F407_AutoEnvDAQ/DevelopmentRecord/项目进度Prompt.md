# CAN总线全链路联调实现计划

## Context
本计划针对STM32F407(Master/ECU)与STM32F103(Slave/仪表)的CAN总线双机通信联调场景，硬件已准备就绪（TJA1050收发器、120Ω终端电阻、逻辑分析仪），两端均已配置500kbps波特率。需要完成：
1. F407侧CAN发送任务实现，将油门ADC采样数据和AHT20温湿度数据封装为标准CAN帧发送
2. 逻辑分析仪总线信号验证方法，特别是ACK应答位的识别
3. F103无接收反应的全流程排查方案

## Implementation Plan
### 1. 前置配置
- **文件**：`/STM32F407_AutoEnvDAQ/Core/Inc/stm32f4xx_hal_conf.h`
- 操作：取消`#define HAL_CAN_MODULE_ENABLED`的注释，开启CAN模块HAL库支持
- 确认CubeMX已配置CAN1：PA11(RX)/PA12(TX)、500kbps波特率、自动重传开启，生成全局句柄`hcan1`

### 2. 全局资源定义
- **文件**：`/STM32F407_AutoEnvDAQ/Core/Src/freertos.c`
- 添加内容：
  - 全局共享采样变量：`g_adc_throttle_value`(uint32_t)、`g_aht20_temperature`(float)、`g_aht20_humidity`(float)
  - 传感器数据互斥锁：`sensorDataMutexHandle`，保护共享数据的线程安全
  - CAN发送任务句柄：`myCanTxTaskHandle`，优先级设置为osPriorityAboveNormal
  - CAN ID定义：`CAN_ID_THROTTLE 0x100`、`CAN_ID_AHT20 0x101`

### 3. 采样任务修改
- **文件**：`/STM32F407_AutoEnvDAQ/Core/Src/freertos.c`
- 修改`StartAdcTask`：ADC采样完成后，获取互斥锁更新全局`g_adc_throttle_value`变量，每200ms执行一次
- 修改`StartI2cTask`：温湿度读取完成后，获取互斥锁更新全局`g_aht20_temperature`和`g_aht20_humidity`变量，每2秒执行一次

### 4. CAN发送任务实现
- **文件**：`/STM32F407_AutoEnvDAQ/Core/Src/freertos.c`
- 实现`StartCanTxTask`函数：
  - 配置CAN发送帧头：标准ID、数据帧、DLC根据数据类型动态设置
  - 200ms周期发送油门数据：ID=0x100，数据长度4字节，存储ADC采样值
  - 2秒周期发送温湿度数据：ID=0x101，数据长度8字节，前4字节温度、后4字节湿度
  - 添加发送失败错误打印，方便调试

### 5. 任务初始化
- **文件**：`/STM32F407_AutoEnvDAQ/Core/Src/freertos.c`
- 在`MX_FREERTOS_Init`函数中：
  - 先创建传感器数据互斥锁
  - 再创建CAN发送任务

## Verification Steps
### 逻辑分析仪信号验证
1. **硬件连接**：逻辑分析仪GND接开发板共地，CH1接CAN_H，CH2接CAN_L，采样率设置为24MS/s
2. **软件配置**：添加CAN协议解析器，波特率设为500kbps，采样点位置80%
3. **帧结构验证**：
   - 应周期性出现ID=0x100(200ms周期)和ID=0x101(2s周期)的CAN帧
   - 解析数据域确认数值与采样值一致
4. **ACK位验证**：
   - 查看帧末尾ACK时隙，正常通信时ACK槽位置应为低电平（由F103发送应答）
   - 若ACK槽始终为高电平，说明无节点应答，接收端存在问题

## Troubleshooting Flow
### F103无接收反应排查流程（按优先级）
1. **物理层检查**：
   - 测量总线两端电阻：应为~60Ω，确认两个120Ω终端电阻正确连接
   - 测量静态电压：空闲时CAN_H/CAN_L均应为2.5V±0.1V，发送时差分电压1.5~2.5V
   - 检查接线：CAN_H接CAN_H、CAN_L接CAN_L、GND共地，无接反或虚焊
2. **配置层检查**：
   - 确认两端波特率完全一致，均为500kbps，时钟配置相同
   - 确认F103 CAN过滤器配置正确，允许接收0x100~0x10F范围的标准ID
   - 确认F103处于正常工作模式，接收中断已开启且优先级正确
3. **软件层检查**：
   - F407侧：调试`HAL_CAN_AddTxMessage`返回值是否为HAL_OK，查看发送邮箱空闲状态和错误寄存器
   - F103侧：查看接收FIFO填充等级，确认接收中断回调函数已正确实现，无总线关闭/错误被动状态
4. **递进定位**：物理层→链路层→应用层逐层排查，使用逻辑分析仪定位故障节点

---

## 下一步操作指引
### 阶段1：硬件接线确认（优先完成）
1. 按照以下要求完成物理连线：
   - F407：PA12(TX) → TJA1050(TXD)，PA11(RX) → TJA1050(RXD)
   - F103：PA12(TX) → TJA1050(TXD)，PA11(RX) → TJA1050(RXD)
   - 总线：CAN_H连CAN_H，CAN_L连CAN_L，两端各并联120Ω终端电阻
   - 必须连接F407 GND与F103 GND，保证共地

### 阶段2：CubeMX配置确认
1. 打开STM32F407项目的CubeMX配置：
   - 启用CAN1外设，配置PA11为CAN_RX，PA12为CAN_TX
   - 波特率配置为500kbps：APB1时钟42MHz，分频器设为2，BS1=13TQ，BS2=2TQ
   - 工作模式设为正常模式，开启自动重传
   - 生成代码，确认CAN初始化代码已生成到main.c和can.c

### 阶段3：代码实现（按顺序执行）
1. 确认`stm32f4xx_hal_conf.h`中`HAL_CAN_MODULE_ENABLED`已取消注释
2. 按照上述计划修改freertos.c文件，添加全局变量、修改采样任务、实现CAN发送任务
3. 编译项目，确认无编译错误

### 阶段4：联调验证
1. 烧录程序到F407和F103
2. 使用逻辑分析仪抓取CAN总线信号，验证ACK位是否正常
3. 查看F103串口输出，确认是否接收到ID为0x100和0x101的报文
