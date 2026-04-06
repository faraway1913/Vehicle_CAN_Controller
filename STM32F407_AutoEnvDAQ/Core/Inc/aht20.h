/**
 * @file    aht20.h
 * @brief   AHT20 温湿度传感器驱动
 * @details 基于HAL库的I2C驱动，适用于STM32F4系列
 */
#ifndef __AHT20_H
#define __AHT20_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

/* AHT20 I2C地址 */
#define AHT20_ADDR          0x38  // 有些模块地址是0x39，根据实际模块修改

/* AHT20 命令 */
#define AHT20_CMD_INIT      0xBE  // 通用初始化命令，兼容更多版本
#define AHT20_CMD_MEASURE   0xAC  // 触发测量，参数0x33, 0x00
#define AHT20_CMD_SOFT_RESET 0xBA // 软件复位

/* 状态位 */
#define AHT20_STATUS_BUSY   0x80  //忙碌标志
#define AHT20_STATUS_CAL    0x08  //校准使能标志

/* AHT20数据结构体 */
typedef struct {
    float temperature;  // 温度值，摄氏度
    float humidity;     // 湿度值，百分比
} AHT20_DataTypeDef;

/**
 * @brief  初始化AHT20传感器
 * @param  hi2c: I2C句柄
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  读取AHT20温湿度数据
 * @param  hi2c: I2C句柄
 * @param  data: 存储温湿度数据的结构体
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_ReadData(I2C_HandleTypeDef *hi2c, AHT20_DataTypeDef *data);

/**
 * @brief  软件复位AHT20
 * @param  hi2c: I2C句柄
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_SoftReset(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_H */

