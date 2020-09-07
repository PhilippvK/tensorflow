/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/examples/micro_speech/audio_provider.h"

#include "stm32f413h_discovery_audio.h"
#include "stm32f413h_discovery_psram.h"
//#include "stm32f4xx_hal_rcc.h"

//#include "SDRAM_DISCO_F746NG.h"
#include "mbed.h"  // NOLINT
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"

namespace {

bool g_is_audio_initialized = false;
constexpr int kAudioCaptureBufferSize = kAudioSampleFrequency * 0.5;
int16_t g_audio_capture_buffer[kAudioCaptureBufferSize];
int16_t g_audio_output_buffer[kMaxAudioSampleSize];
int32_t g_latest_audio_timestamp = 0;

// For a full example of how to access audio on the STM32F746NG board, see
// https://os.mbed.com/teams/ST/code/DISCO-F746NG_AUDIO_demo/
//AUDIO_DISCO_F746NG g_audio_device;
//SDRAM_DISCO_F746NG g_sdram_device;

typedef enum {
  BUFFER_OFFSET_NONE = 0,
  BUFFER_OFFSET_HALF = 1,
  BUFFER_OFFSET_FULL = 2,
} BUFFER_StateTypeDef;

#define AUDIO_BLOCK_SIZE ((uint32_t)2048)
//#define AUDIO_BLOCK_SIZE ((uint32_t)2048*4)
//#define PSRAM_DEVICE_ADDR 
#define AUDIO_BUFFER_IN PSRAM_DEVICE_ADDR /* In PSRAM */
#define AUDIO_BUFFER_OUT \
  (PSRAM_DEVICE_ADDR + (AUDIO_BLOCK_SIZE * 2)) /* In PSRAM */
__IO uint32_t g_audio_rec_buffer_state = BUFFER_OFFSET_NONE;

#define SCRATCH_BUFF_SIZE  512
//#define SCRATCH_BUFF_SIZE  128
//uint16_t  internal_buffer[AUDIO_BLOCK_SIZE] = {0};
//uint16_t  internal_buffer2[AUDIO_BLOCK_SIZE] = {0};

#if defined ( __CC_ARM )  /* !< ARM Compiler */
int32_t Scratch [SCRATCH_BUFF_SIZE] __attribute__((at(0x2000E000)));
#elif defined ( __ICCARM__ )  /* !< ICCARM Compiler */
#pragma location=0x2000E000
int32_t Scratch [SCRATCH_BUFF_SIZE];
#elif defined ( __GNUC__ )  /* !< GNU Compiler */
int32_t Scratch [SCRATCH_BUFF_SIZE] __attribute__((section(".scratch_section")));
#endif

//uint8_t SetSysClock_PLL_HSE_200MHz() {
//  RCC_ClkInitTypeDef RCC_ClkInitStruct;
//  RCC_OscInitTypeDef RCC_OscInitStruct;
//
//  // Enable power clock
//  __PWR_CLK_ENABLE();
//
//  // Enable HSE oscillator and activate PLL with HSE as source
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//  RCC_OscInitStruct.HSEState = RCC_HSE_ON; /* External xtal on OSC_IN/OSC_OUT */
//
//  // Warning: this configuration is for a 25 MHz xtal clock only
//  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
//  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
//  RCC_OscInitStruct.PLL.PLLM = 25;   // VCO input clock = 1 MHz (25 MHz / 25)
//  RCC_OscInitStruct.PLL.PLLN = 400;  // VCO output clock = 400 MHz (1 MHz * 400)
//  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;  // PLLCLK = 200 MHz (400 MHz / 2)
//  RCC_OscInitStruct.PLL.PLLQ = 8;  // USB clock = 50 MHz (400 MHz / 8)
//
//  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
//    return 0;  // FAIL
//  }
//
//  // Activate the OverDrive to reach the 216 MHz Frequency
//  if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
//    return 0;  // FAIL
//  }
//
//  // Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
//  // clocks dividers
//  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
//                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
//  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 200 MHz
//  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // 200 MHz
//  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;          //  50 MHz
//  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;          // 100 MHz
//
//  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) {
//    return 0;  // FAIL
//  }
//  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_4);
//  return 1;  // OK
//}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    //Error_Handler();
    while(1);
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    //Error_Handler();
    while(1);
  }
}

TfLiteStatus InitAudioRecording(tflite::ErrorReporter* error_reporter) {
  //HAL_Init();
  BSP_LED_Init((Led_TypeDef)0);
  BSP_LED_Init((Led_TypeDef)1);
  BSP_LED_On((Led_TypeDef)0);
  BSP_LED_On((Led_TypeDef)1);
  //SetSysClock_PLL_HSE_200MHz();
  uint32_t clock = 0;
  clock = HAL_RCC_GetSysClockFreq();
  TF_LITE_REPORT_ERROR(error_reporter, "Clock: %d", clock);
  //SystemClock_Config();
  clock = HAL_RCC_GetSysClockFreq();
  TF_LITE_REPORT_ERROR(error_reporter, "Clock: %d", clock);

  // Initialize SDRAM buffers. TODO: ?
    /*##-1- Configure the PSRAM device ##########################################*/
  /* PSRAM device configuration */
  if(BSP_PSRAM_Init() != PSRAM_OK)
  {
    while(1);
  }
  memset((uint16_t*)AUDIO_BUFFER_IN, 0, AUDIO_BLOCK_SIZE * 2);
  memset((uint16_t*)AUDIO_BUFFER_OUT, 0, AUDIO_BLOCK_SIZE * 2);
  /* Initialize Audio Recorder */
  BSP_AUDIO_IN_AllocScratch (Scratch, SCRATCH_BUFF_SIZE);
  if (BSP_AUDIO_IN_Init(I2S_AUDIOFREQ_16K, DEFAULT_AUDIO_IN_BIT_RESOLUTION, 2) != AUDIO_OK)
  {
    while(1);
  }

  g_audio_rec_buffer_state = BUFFER_OFFSET_NONE;

  // Start Recording.

  BSP_AUDIO_IN_Record((uint16_t*)AUDIO_BUFFER_IN, AUDIO_BLOCK_SIZE);
  //BSP_AUDIO_IN_Record((uint16_t*)internal_buffer, AUDIO_BLOCK_SIZE);
  //BSP_AUDIO_IN_RecordEx(&internal_buffer[0], AUDIO_BLOCK_SIZE);
  //BSP_AUDIO_IN_Record((uint16_t*)&internal_buffer[0], AUDIO_BLOCK_SIZE);

  // Also play results out to headphone jack.
  //BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, 50, I2S_AUDIOFREQ_16K);
  BSP_AUDIO_OUT_Play((uint16_t*)AUDIO_BUFFER_OUT, AUDIO_BLOCK_SIZE * 2);
  //BSP_AUDIO_OUT_Play(&internal_buffer2[0], AUDIO_BLOCK_SIZE * 2);
  clock = HAL_RCC_GetSysClockFreq();
  TF_LITE_REPORT_ERROR(error_reporter, "Clock: %d", clock);

  return kTfLiteOk;
}

void CaptureSamples(const int16_t* sample_data) {
  const int sample_size = AUDIO_BLOCK_SIZE / (sizeof(int16_t) * 2);
  const int32_t time_in_ms =
      g_latest_audio_timestamp + (sample_size / (kAudioSampleFrequency / 1000));

  const int32_t start_sample_offset =
      g_latest_audio_timestamp * (kAudioSampleFrequency / 1000);
  BSP_LED_Toggle((Led_TypeDef)0);
  for (int i = 0; i < sample_size; ++i) {
    const int capture_index =
        (start_sample_offset + i) % kAudioCaptureBufferSize;
    g_audio_capture_buffer[capture_index] =
        (sample_data[(i * 2) + 0] / 2) + (sample_data[(i * 2) + 1] / 2);
  }
  // This is how we let the outside world know that new audio data has arrived.
  g_latest_audio_timestamp = time_in_ms;
}

}  // namespace

// These callbacks need to be linkable symbols, because they override weak
// default versions.
void BSP_AUDIO_IN_TransferComplete_CallBack(void) {
  g_audio_rec_buffer_state = BUFFER_OFFSET_FULL;
  /* Copy recorded 1st half block */
  memcpy((uint16_t*)(AUDIO_BUFFER_OUT), (uint16_t*)(AUDIO_BUFFER_IN),
         AUDIO_BLOCK_SIZE);
  CaptureSamples(reinterpret_cast<int16_t*>(AUDIO_BUFFER_IN));
  //memcpy((uint16_t*)(&internal_buffer2[0]), (uint16_t*)(&internal_buffer[0]),
  //       AUDIO_BLOCK_SIZE);
  //CaptureSamples(reinterpret_cast<int16_t*>(&internal_buffer[0]));
  return;
}

// Another weak symbol override.
void BSP_AUDIO_IN_HalfTransfer_CallBack(void) {
  g_audio_rec_buffer_state = BUFFER_OFFSET_HALF;
  /* Copy recorded 2nd half block */
  memcpy((uint16_t*)(AUDIO_BUFFER_OUT + (AUDIO_BLOCK_SIZE)),
         (uint16_t*)(AUDIO_BUFFER_IN + (AUDIO_BLOCK_SIZE)), AUDIO_BLOCK_SIZE);
  CaptureSamples(
      reinterpret_cast<int16_t*>(AUDIO_BUFFER_IN + AUDIO_BLOCK_SIZE));
  //memcpy((uint16_t*)(&internal_buffer2[0] + (AUDIO_BLOCK_SIZE/2)),
  //       (uint16_t*)(&internal_buffer[0] + (AUDIO_BLOCK_SIZE/2)), AUDIO_BLOCK_SIZE);
  //CaptureSamples(
  //    reinterpret_cast<int16_t*>(&internal_buffer[0]+AUDIO_BLOCK_SIZE/2));
  return;
}

/**
  * @brief  Audio IN Error callback function.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_IN_Error_Callback(void)
{
  /* This function is called when an Interrupt due to transfer error on or peripheral
     error occurs. */
  /* Display message on the LCD screen */
  //BSP_LCD_SetBackColor(LCD_COLOR_RED);
  //BSP_LCD_DisplayStringAt(0, LINE(14), (uint8_t *)"       DMA  ERROR     ", CENTER_MODE);
  while(1);
  /* Stop the program with an infinite loop */
  //while (BSP_PB_GetState(BUTTON_WAKEUP) != RESET)
  //{
  //  return;
  //}
  /* could also generate a system reset to recover from the error */
  /* .... */
}

// Main entry point for getting audio data.
TfLiteStatus GetAudioSamples(tflite::ErrorReporter* error_reporter,
                             int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  if (!g_is_audio_initialized) {
    TfLiteStatus init_status = InitAudioRecording(error_reporter);
    if (init_status != kTfLiteOk) {
      return init_status;
    }
    g_is_audio_initialized = true;
  }
  // This should only be called when the main thread notices that the latest
  // audio sample data timestamp has changed, so that there's new data in the
  // capture ring buffer. The ring buffer will eventually wrap around and
  // overwrite the data, but the assumption is that the main thread is checking
  // often enough and the buffer is large enough that this call will be made
  // before that happens.
  const int start_offset = start_ms * (kAudioSampleFrequency / 1000);
  const int duration_sample_count =
      duration_ms * (kAudioSampleFrequency / 1000);
  BSP_LED_Toggle((Led_TypeDef)1);
  //TF_LITE_REPORT_ERROR(error_reporter, "OUTPUT");
  for (int i = 0; i < duration_sample_count; ++i) {
    const int capture_index = (start_offset + i) % kAudioCaptureBufferSize;
    g_audio_output_buffer[i] = g_audio_capture_buffer[capture_index];
  }

  *audio_samples_size = kMaxAudioSampleSize;
  *audio_samples = g_audio_output_buffer;
  return kTfLiteOk;
}

int32_t LatestAudioTimestamp() { return g_latest_audio_timestamp; }
