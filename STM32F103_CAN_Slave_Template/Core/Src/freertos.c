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
#include "can.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// 报文ID定义
#define CAN_ID_THROTTLE  0x100  // 油门ADC数据
#define CAN_ID_TEMP_HUM  0x101  // 温湿度数据

// CAN接收数据结构体
typedef struct {
  uint16_t throttle_adc;   // ADC油门值 0~4095
  int16_t  temperature;    // 温度值，单位0.1℃（例：256表示25.6℃）
  uint16_t humidity;       // 湿度值，单位0.1%RH（例：605表示60.5%RH）
  uint32_t rx_timestamp;   // 接收时间戳
} CanRxData_t;

CanRxData_t can_rx_data; // 全局接收数据缓存
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* Definitions for LedTask */
osThreadId_t LedTaskHandle;
const osThreadAttr_t LedTask_attributes = {
  .name = "LedTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE END Variables */
/* Definitions for CanRxTask */
osThreadId_t CanRxTaskHandle;
const osThreadAttr_t CanRxTask_attributes = {
  .name = "CanRxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartLedTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartCanRxTask(void *argument);

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
  /* add mutexes, ... */
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
  /* creation of CanRxTask */
  CanRxTaskHandle = osThreadNew(StartCanRxTask, NULL, &CanRxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* 创建LED心跳任务 */
  LedTaskHandle = osThreadNew(StartLedTask, NULL, &LedTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartCanRxTask */
/**
  * @brief  Function implementing the CanRxTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartCanRxTask */
void StartCanRxTask(void *argument)
{
  /* USER CODE BEGIN StartCanRxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCanRxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  LED心跳任务函数
  * @param  argument: Not used
  * @retval None
  */
void StartLedTask(void *argument)
{
  for(;;)
  {
    // 假设LED接在PC13引脚，根据实际硬件修改引脚定义
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    osDelay(500); // 500ms闪烁周期
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK)
  {
    /* 收到CAN报文就翻转LED，证明中断触发 */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    can_rx_data.rx_timestamp = HAL_GetTick();

    switch(rx_header.StdId)
    {
      case CAN_ID_THROTTLE:
        if(rx_header.DLC >= 4)
        {
          // F407发送：字节2=ADC高8位，字节3=ADC低8位
          can_rx_data.throttle_adc = (uint16_t)(rx_data[2] << 8 | rx_data[3]);
          printf("[%lu] Throttle ADC: %d (0x%04X)\r\n",
                 can_rx_data.rx_timestamp,
                 can_rx_data.throttle_adc,
                 can_rx_data.throttle_adc);
        }
        break;

      case CAN_ID_TEMP_HUM:
        if(rx_header.DLC >= 4)
        {
          // 匹配F407的大端格式：温度高字节在字节0，低字节在字节1
          // 湿度高字节在字节2，低字节在字节3
          can_rx_data.temperature = (int16_t)(rx_data[0] << 8 | rx_data[1]);
          can_rx_data.humidity = (uint16_t)(rx_data[2] << 8 | rx_data[3]);
          printf("[%lu] Temp: %.1fC, Humidity: %.1f%%RH\r\n",
                 can_rx_data.rx_timestamp,
                 can_rx_data.temperature / 10.0f,
                 can_rx_data.humidity / 10.0f);
        }
        break;

      default:
        printf("Received reserved ID: 0x%X, Length: %d\r\n", rx_header.StdId, rx_header.DLC);
        break;
    }
  }
}

/* USER CODE END Application */

