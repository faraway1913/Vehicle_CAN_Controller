/**
 * @file    aht20.c
 * @brief   AHT20 温湿度传感器驱动实现
 * @details 基于HAL库的I2C驱动，适用于STM32F4系列，FreeRTOS环境适配
 */
#include "aht20.h"
#include "cmsis_os.h"
#include "stdio.h"

/**
 * @brief  软件复位AHT20
 * @param  hi2c: I2C句柄
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_SoftReset(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd = AHT20_CMD_SOFT_RESET;
    return HAL_I2C_Master_Transmit(hi2c, AHT20_ADDR << 1, &cmd, 1, 100) == HAL_OK ? 0 : 1;
}

/**
 * @brief  初始化AHT20传感器
 * @param  hi2c: I2C句柄
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd[3];
    uint8_t status;
    uint8_t retry = 5;

    // 软件复位
    AHT20_SoftReset(hi2c);
    osDelay(20);  // 复位后需要等待20ms

    // 发送初始化命令 0xE1, 0x08, 0x00
    cmd[0] = AHT20_CMD_INIT;
    cmd[1] = 0x08;
    cmd[2] = 0x00;

    while (retry--)
    {
        if (HAL_I2C_Master_Transmit(hi2c, AHT20_ADDR << 1, cmd, 3, 100) == HAL_OK)
        {
            osDelay(100);  // 等待初始化完成

            // 读取状态确认校准使能
            HAL_I2C_Master_Receive(hi2c, AHT20_ADDR << 1, &status, 1, 100);
            if ((status & AHT20_STATUS_CAL) != 0)
            {
                return 0;  // 初始化成功
            }
        }
        osDelay(10);
    }

    return 1;  // 初始化失败
}

/**
 * @brief  读取AHT20温湿度数据
 * @param  hi2c: I2C句柄
 * @param  data: 存储温湿度数据的结构体
 * @retval 0:成功  1:失败
 */
uint8_t AHT20_ReadData(I2C_HandleTypeDef *hi2c, AHT20_DataTypeDef *data)
{
    uint8_t cmd[3];
    uint8_t recv_data[6];
    uint32_t humidity_raw;
    uint32_t temperature_raw;
    uint8_t status;
    uint8_t retry = 5;

    // 触发测量命令: 0xAC, 0x33, 0x00
    cmd[0] = AHT20_CMD_MEASURE;
    cmd[1] = 0x33;
    cmd[2] = 0x00;

    while (retry--)
    {
        // 发送测量命令
        if (HAL_I2C_Master_Transmit(hi2c, AHT20_ADDR << 1, cmd, 3, 100) != HAL_OK)
        {
            HAL_Delay(5);
            continue;
        }

        // 等待测量完成 (大约80ms)
        osDelay(80);

        // 读取状态
        if (HAL_I2C_Master_Receive(hi2c, AHT20_ADDR << 1, &status, 1, 100) != HAL_OK)
        {
            osDelay(5);
            continue;
        }

        // 检查忙碌标志
        if ((status & AHT20_STATUS_BUSY) == 0)
        {
            // 读取6字节数据
            if (HAL_I2C_Master_Receive(hi2c, AHT20_ADDR << 1, recv_data, 6, 100) == HAL_OK)
            {
                // 解析数据
                // 湿度: H[19:12] H[11:4] = H[19:0]
                humidity_raw = ((uint32_t)recv_data[1] << 12) | ((uint32_t)recv_data[2] << 4) | ((recv_data[3] >> 4) & 0x0F);
                // 温度: T[19:12] T[11:4] = T[19:0]
                temperature_raw = (((uint32_t)recv_data[3] & 0x0F) << 16) | ((uint32_t)recv_data[4] << 8) | recv_data[5];

                // 转换为实际值
                // 湿度 = (humidity_raw / 2^20) * 100%
                data->humidity = ((float)humidity_raw / 1048576.0f) * 100.0f;
                // 温度 = (temperature_raw / 2^20) * 200 - 50
                data->temperature = (((float)temperature_raw / 1048576.0f) * 200.0f) - 50.0f;

                // 限幅处理
                if (data->humidity > 100) data->humidity = 100;
                if (data->humidity < 0) data->humidity = 0;
                if (data->temperature > 120) data->temperature = 120;
                if (data->temperature < -40) data->temperature = -40;

                return 0;  // 读取成功
            }
        }

        osDelay(10);
    }

    return 1;  // 读取失败
}

