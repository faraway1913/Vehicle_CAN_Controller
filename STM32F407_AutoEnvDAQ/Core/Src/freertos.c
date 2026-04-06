/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "adc.h"
#include "i2c.h"
#include "aht20.h"
#include "can.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* 定义四个任务的句柄 */
osThreadId_t myLedTaskHandle;
osThreadId_t myPrintTaskHandle;
osThreadId_t myAdcTaskHandle;
osThreadId_t myI2cTaskHandle;

/* 定义互斥锁句柄 */
osMutexId_t printfMutexHandle;

/* 定义互斥锁属性 */
const osMutexAttr_t printfMutex_attributes = {
  .name = "printfMutex"
};

/* 定义任务属性 */
const osThreadAttr_t LedTask_attributes = {
  .name = "LedTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t PrintTask_attributes = {
  .name = "PrintTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t AdcTask_attributes = { .name = "AdcTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal };
const osThreadAttr_t I2cTask_attributes = { .name = "I2cTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal };

/* AHT20传感器状态标志 */
uint8_t aht20_init_ok = 0;

/* CAN ID定义 */
#define CAN_ID_THROTTLE  0x100  // 油门ADC数据
#define CAN_ID_TEMP_HUM  0x101  // 温湿度数据

/* 全局共享采样数据 */
uint32_t g_adc_throttle_value = 0;
float g_aht20_temperature = 0.0f;
float g_aht20_humidity = 0.0f;

/* 传感器数据互斥锁 */
osMutexId_t sensorDataMutexHandle;
const osMutexAttr_t sensorDataMutex_attributes = {
  .name = "sensorDataMutex"
};

/* CAN发送任务属性 */
const osThreadAttr_t CanTxTask_attributes = {
  .name = "CanTxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal, // 发送任务优先级高于采样任务
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartLedTask(void *argument);
void StartPrintTask(void *argument);
void StartAdcTask(void *argument);
void StartI2cTask(void *argument);
void StartCanTxTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* 创建互斥锁 */
  printfMutexHandle = osMutexNew(&printfMutex_attributes);
  /* 创建传感器数据互斥锁 */
  sensorDataMutexHandle = osMutexNew(&sensorDataMutex_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  myLedTaskHandle = osThreadNew(StartLedTask, NULL, &LedTask_attributes);
  myPrintTaskHandle = osThreadNew(StartPrintTask, NULL, &PrintTask_attributes);
  myAdcTaskHandle = osThreadNew(StartAdcTask, NULL, &AdcTask_attributes);
  myI2cTaskHandle = osThreadNew(StartI2cTask, NULL, &I2cTask_attributes);
  /* 创建CAN发送任务 */
  osThreadNew(StartCanTxTask, NULL, &CanTxTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* --- 任务 1：LED 闪烁 --- */
void StartLedTask(void *argument)
{
  for(;;)
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    
    // 加锁打印，保证串口数据不乱序
    if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK)
    {
      printf("Led Task: PC13 Toggled!\r\n");
      osMutexRelease(printfMutexHandle);
    }
    
    osDelay(500);
  }
}

/* --- 任务 2：串口打印任务 --- */
void StartPrintTask(void *argument)
{
  for(;;)
  {
    if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK)
    {
      printf("--- Print Task: System Running Smoothly ---\r\n");
      osMutexRelease(printfMutexHandle);
    }
    osDelay(1000);
  }
}

/* --- 任务 3：电位器读取 (ADC) --- */
void StartAdcTask(void *argument) {
  uint32_t adc_value;
  for(;;) {
    HAL_ADC_Start(&hadc1);
    if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
       adc_value = HAL_ADC_GetValue(&hadc1);

       // 更新全局ADC油门值
       if(osMutexAcquire(sensorDataMutexHandle, osWaitForever) == osOK) {
         g_adc_throttle_value = adc_value;
         osMutexRelease(sensorDataMutexHandle);
       }

       if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
         printf("ADC Value: %lu\r\n", adc_value);
         osMutexRelease(printfMutexHandle);
       }
    }
    osDelay(200); // 200ms 刷新一次
  }
}

/* --- 任务 4：温湿度读取 (I2C) --- */
void StartI2cTask(void *argument) {
  AHT20_DataTypeDef aht20_data;

  // 先打印，确认任务已经启动
  if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
    printf("I2C Task: Started!\r\n");
    osMutexRelease(printfMutexHandle);
  }

  osDelay(1000); // 等待系统稳定

  // 首次启动：尝试初始化AHT20
  if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
    printf("I2C Task: Initializing AHT20...\r\n");
    osMutexRelease(printfMutexHandle);
  }

  // 初始化AHT20传感器
  aht20_init_ok = (AHT20_Init(&hi2c1) == 0) ? 1 : 0;

  if (aht20_init_ok) {
    if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
      printf("I2C Task: AHT20 Init OK!\r\n");
      osMutexRelease(printfMutexHandle);
    }
  } else {
    if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
      printf("I2C Task: AHT20 Init Failed!\r\n");
      osMutexRelease(printfMutexHandle);
    }
  }

  for(;;) {
    if (aht20_init_ok) {
      // 读取温湿度数据
      if (AHT20_ReadData(&hi2c1, &aht20_data) == 0) {
        // 更新全局温湿度值
        if(osMutexAcquire(sensorDataMutexHandle, osWaitForever) == osOK) {
          g_aht20_temperature = aht20_data.temperature;
          g_aht20_humidity = aht20_data.humidity;
          osMutexRelease(sensorDataMutexHandle);
        }

        if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
          printf("Temp: %.1f C, Humidity: %.1f %%\r\n", aht20_data.temperature, aht20_data.humidity);
          osMutexRelease(printfMutexHandle);
        }
      } else {
        if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
          printf("I2C Task: Read Failed!\r\n");
          osMutexRelease(printfMutexHandle);
        }
      }
    }
    osDelay(2000); // 2秒采样一次温湿度
  }
}

/**
* @brief  CAN发送任务函数
* @param  argument: Not used
* @retval None
*/
void StartCanTxTask(void *argument) {
  CAN_TxHeaderTypeDef TxHeader;
  uint8_t TxData[8];
  uint32_t TxMailbox;
  uint32_t throttle_send_tick = osKernelGetTickCount();
  uint32_t aht20_send_tick = osKernelGetTickCount();

  // 开启CAN外设
  if(HAL_CAN_Start(&hcan1) != HAL_OK) {
    if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
      printf("CAN Start Failed!\r\n");
      osMutexRelease(printfMutexHandle);
    }
  }

  // 配置CAN发送帧头通用参数
  TxHeader.ExtId = 0x00;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.TransmitGlobalTime = DISABLE;

  for(;;) {
    /* 油门数据发送：200ms周期 */
    if(osKernelGetTickCount() - throttle_send_tick >= 200) {
      throttle_send_tick = osKernelGetTickCount();

      // 读取全局ADC值
      uint32_t adc_val;
      if(osMutexAcquire(sensorDataMutexHandle, osWaitForever) == osOK) {
        adc_val = g_adc_throttle_value;
        osMutexRelease(sensorDataMutexHandle);
      }

      // 封装CAN帧：ID=0x100，长度4字节，存储ADC值（大端模式，和F103解析匹配）
      TxHeader.StdId = CAN_ID_THROTTLE;
      TxHeader.DLC = 4;
      TxData[0] = (uint8_t)(adc_val >> 24);
      TxData[1] = (uint8_t)(adc_val >> 16);
      TxData[2] = (uint8_t)(adc_val >> 8);
      TxData[3] = (uint8_t)(adc_val & 0xFF);

      // 发送CAN帧
      if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
        if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
          printf("CAN Throttle Send Failed! Error Code: 0x%X\r\n", hcan1.Instance->ESR);
          osMutexRelease(printfMutexHandle);
        }
      }
    }

    /* 温湿度数据发送：2s周期 */
    if(osKernelGetTickCount() - aht20_send_tick >= 2000) {
      aht20_send_tick = osKernelGetTickCount();

      // 读取全局温湿度值
      float temp, humi;
      if(osMutexAcquire(sensorDataMutexHandle, osWaitForever) == osOK) {
        temp = g_aht20_temperature;
        humi = g_aht20_humidity;
        osMutexRelease(sensorDataMutexHandle);
      }

      // 封装CAN帧：ID=0x101，长度8字节，前4字节温度，后4字节湿度
      TxHeader.StdId = CAN_ID_TEMP_HUM;
      TxHeader.DLC = 8;
      // 温度放大10倍转int16传输，和F103解析逻辑匹配
      int16_t temp_int = (int16_t)(temp * 10.0f + 0.5f);
      uint16_t humi_int = (uint16_t)(humi * 10.0f + 0.5f);
      TxData[0] = (uint8_t)(temp_int >> 8);
      TxData[1] = (uint8_t)(temp_int & 0xFF);
      TxData[2] = (uint8_t)(humi_int >> 8);
      TxData[3] = (uint8_t)(humi_int & 0xFF);
      // 保留后4字节可扩展使用
      TxData[4] = 0x00;
      TxData[5] = 0x00;
      TxData[6] = 0x00;
      TxData[7] = 0x00;

      // 发送CAN帧
      if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
        if(osMutexAcquire(printfMutexHandle, osWaitForever) == osOK) {
          printf("CAN AHT20 Send Failed! Error Code: 0x%X\r\n", hcan1.Instance->ESR);
          osMutexRelease(printfMutexHandle);
        }
      }
    }

    osDelay(10); // 10ms调度一次，降低CPU占用
  }
}
/* USER CODE END Application */

