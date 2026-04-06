# 车载环境感知与数据采集系统

基于 STM32F407 + FreeRTOS 的车载 CAN 总线双机通信项目，实现传感器数据采集与实时传输。

## 项目概述

本项目实现了一个完整的车载环境感知与数据采集系统，包含：

- **Master（STM32F407）**：采集油门踏板电位器数据（ADC）和车内温湿度数据（AHT20传感器），通过 CAN 总线发送给 Slave
- **Slave（STM32F103）**：接收 CAN 报文，解析并通过串口实时打印数据

## 硬件配置

| 板子 | 型号 | 主频 | CAN引脚 | 功能 |
|------|------|------|---------|------|
| Master | STM32F407ZGT6 | 168MHz | PA12(TX)/PA11(RX) | 传感器采集 + CAN发送 |
| Slave | STM32F103C8T6 | 72MHz | PB9(TX)/PB8(RX) | CAN接收 + 串口打印 |

**外设连接**：
- AHT20 温湿度传感器 → I2C1 (PB6 SCL / PB7 SDA)
- 电位器（油门模拟） → ADC1_IN1 (PA1)
- TJA1050 CAN 收发器 × 2（总线两侧各一颗）
- 120Ω 终端电阻 × 2（总线两端各一颗）

## 通信规格

| 参数 | 值 |
|------|-----|
| CAN 波特率 | 250 kbps |
| 油门数据周期 | 200 ms |
| 温湿度数据周期 | 2 s |
| CAN ID（油门） | 0x100 |
| CAN ID（温湿度） | 0x101 |
| 数据长度 | 油门 4 字节，温湿度 8 字节 |

## 项目结构

```
Vehicle_Domain_Controller_Simulator/
├── STM32F407_AutoEnvDAQ/          # Master 工程（Keil MDK）
│   └── Core/
│       └── Src/
│           ├── freertos.c         # 核心业务逻辑（任务定义、CAN发送、ADC采集）
│           ├── can.c              # CAN 外设初始化
│           └── aht20.c            # AHT20 温湿度驱动
├── STM32F103_CAN_Slave_Template/  # Slave 工程（Keil MDK）
│   └── Core/
│       └── Src/
│           ├── freertos.c         # CAN 接收回调、串口打印
│           ├── can.c              # CAN 外设初始化（PB8/PB9重映射）
│           └── main.c             # 过滤器配置、CAN启动
└── DevelopmentRecord/             # 开发记录文档
```

## 快速开始

### 1. 硬件接线

**CAN 总线连接**：
```
[F407 + TJA1050]  ←── CAN_H / CAN_L ──→  [F103 + TJA1050]
       ↓                                          ↓
   120Ω 终端电阻                        120Ω 终端电阻
       ↓                                          ↓
      GND ─────────────────────────────────── GND（共地）
```

**TJA1050 接线**：
- Master侧：RXD → PA12，TXD → PA11，VCC → 3.3V
- Slave侧：RXD → PB9，TXD → PB8，VCC → 3.3V

### 2. 编译烧录

1. 用 Keil MDK 打开 `STM32F407_AutoEnvDAQ.uvprojx`，编译烧录到 F407
2. 打开 `STM32F103_CAN_Slave_Template.uvprojx`，编译烧录到 F103

### 3. 验证

- F407 串口：每秒打印系统状态，每200ms打印ADC值，每2s打印温湿度
- F103 串口：每秒打印油门ADC值（约2400~2650）和温湿度数据

## 开发环境

- **IDE**：Keil MDK 5.x
- **HAL库**：STM32CubeHAL
- **RTOS**：FreeRTOS v10.x（CMSIS_V2接口）
- **配置工具**：STM32CubeMX
- **调试工具**：逻辑分析仪（24M 8CH）、ST-Link

## 关键技术点

### CAN总线通信
- 标准 CAN 2.0A 协议，11位标准帧 ID
- 硬件过滤器配置，只接收目标 ID 范围（0x100~0x10F）
- ACK 应答机制和错误状态处理
- CAN引脚重映射（PB8/PB9）解决引脚冲突

### FreeRTOS多任务
- 5个并发任务，优先级调度
- 互斥锁保护共享数据（传感器数据）
- 互斥锁保护串口打印（防止乱序）

### 传感器驱动
- ADC 连续转换模式，12位精度
- AHT20 I2C 驱动，已适配 FreeRTOS（非阻塞延时）

## 踩坑记录

完整的踩坑记录和技术细节请参考 [DevelopmentRecord/](DevelopmentRecord/) 目录下的文档，包括：

- [传感器集成阶段完成总结.md](DevelopmentRecord/2026-03-11_传感器集成阶段完成总结.md) — ADC + I2C 阶段
- [CAN总线联调完成记录.md](DevelopmentRecord/2026-04-06_CAN总线联调完成记录.md) — CAN 通信全流程记录（含14个踩坑点和调试方法）

### 核心踩坑点

1. **CAN波特率匹配**：F407(42MHz APB1) 和 F103(36MHz APB1) 时钟不同，500kbps 通信失败，降为 250kbps 后稳定
2. **引脚冲突**：F103 的 PA11/PA12 被 USB 电路占用，通过重映射到 PB8/PB9 解决
3. **JTAG占用**：STM32F103 默认启用 JTAG，PB8 被 TDI 占用，需调用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 释放
4. **数据字节序**：CAN 帧的数据字节顺序需要发送端和接收端严格匹配

## 适用场景

本项目可作为以下场景的参考：

- 车载 ECU 与仪表之间的 CAN 通信
- 多 MCU 分布式传感器数据采集系统
- FreeRTOS + HAL 库的嵌入式开发入门
- CAN 总线协议调试和故障排查学习

## License

MIT License
