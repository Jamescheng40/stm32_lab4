/**

******************************************************************************
  * @file    GPIO/GPIO_EXTI/Src/main.c
  * @author  MCD Application Team
  * @version V1.8.0
  * @date    21-April-2017
  * @brief   This example describes how to configure and use GPIOs through
  *          the STM32L4xx HAL API.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************


*/




/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32L4xx_HAL_Examples
  * @{
  */

/** @addtogroup GPIO_EXTI
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO HAL_StatusTypeDef Hal_status;  //HAL_ERROR, HAL_TIMEOUT, HAL_OK, of HAL_BUSY

ADC_HandleTypeDef    Adc1_Handle;
ADC_ChannelConfTypeDef sConfig;

TIM_HandleTypeDef    Tim3_Handle, Tim4_Handle;
TIM_OC_InitTypeDef Tim3_OCInitStructure, Tim4_OCInitStructure;




uint16_t TIM3_Prescaler;
__IO uint16_t TIM3_CCR4_Val;   //make it interrupt every 500 ms, halfsecond.

uint16_t TIM4Prescaler;    // will set it to make the timer's counter run at 10Khz. ---50 ticks is 1ms.
uint16_t TIM4Period;       //will make a period to be 20ms. ----1000 ticks
__IO uint16_t TIM4_CCR1_Val=0;  // for pulse width!!! can vary it from 1 to 1000 to vary the pulse width.
// the CCR value must be less than period. otherwise, the interrupt will never happen.
//duty cycle need to >25% to make fan run (CCR1_VaL need to >250).

__IO uint16_t ADC1ConvertedValue;   //if declare it as 16t, it will not work.


volatile double  setPoint=23.5;    //default set point

double measuredTemp;

//the buttons used to change the set point temperature
__IO uint8_t halfSecond;
__IO uint8_t up_pressed;
__IO uint8_t down_pressed;
__IO uint8_t sel_pressed;


typedef enum state
{
	temperature_state, 	//SHOW TEMPERATURE AND SETPOINT.
	setting_state,  //TEST
} FanState;

FanState current_state = temperature_state;


char lcd_buffer[6];    // LCD display buffer


/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);

void ADC_DMA_Config(void); // the GPIO pin configuration is in msp.c
void TIM4_PWM_Config(void);  //set fan voltage
void Show_Temperature(void);
void Show_Setpoint(void);


//static void EXTILine14_Config(void); // configure the exti line4, for exterrnal button, WHICH BUTTON?


/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
	/* STM32L4xx HAL library initialization:
	     - Configure the Flash prefetch
	     - Systick timer is configured by default as source of time base, but user
	       can eventually implement his proper time base source (a general purpose
	       timer for example or other time source), keeping in mind that Time base
	       duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
	       handled in milliseconds basis.
	     - Set NVIC Group Priority to 4
	     - Low Level Initialization
	   */

	HAL_Init();
	//HAL_NVIC_SetPriority(SysTick_IRQn,  0 ,0); //if set here immediately after HAL_Init. it will not take effect!!!



	SystemClock_Config();   //sysclock is 80Hz. HClkm apb1 an apb2 are all 80Mhz.
	//RTC use LSE.

	//HAL_NVIC_SetPriority(SysTick_IRQn, (uint32_t) 0 ,0);
	HAL_InitTick(0x0000); // set systick's priority to the highest.


	BSP_LED_Init(LED4);
	BSP_LED_Init(LED5);


	BSP_LCD_GLASS_Init();

	BSP_JOY_Init(JOY_MODE_EXTI);

	// ===========Config the GPIO for external interupt==============
	//EXTILine14_Config();

//	TIM3_IT_CC4_Config();

	//configure PWM output
	TIM4_PWM_Config();

	ADC_DMA_Config();


	while (1)
	{
		
		if(0.02442*ADC1ConvertedValue >= setPoint) // the temperature is higher than set point
		{
			TIM4_CCR1_Val = (0.02442*ADC1ConvertedValue - setPoint)/0.05 + 250;  
			/**
			 * enable fan rotating by set the temperature
			 * particularly, the voltage is propotional to the temperature gap
			 * between current temperature and set temperature so it will rotate 
			 * faster when current temperature is higher 
			 */
		}
		else
		{
			TIM4_CCR1_Val =  0; // dose not rotate (no voltage when current temperature is lower than set temperature

		}
		if(current_state == temperature_state) //the show current temperature mode
		{
			Show_Temperature(); //current temperature

			__HAL_TIM_SET_COMPARE(&Tim4_Handle, TIM_CHANNEL_1,TIM4_CCR1_Val); //set PWM compare



		}
		else  //go to set temperature mode
		{
			Show_Setpoint(); // set temperature
			
			__HAL_TIM_SET_COMPARE(&Tim4_Handle, TIM_CHANNEL_1,TIM4_CCR1_Val); //set PWM compare
		}
	} //end of while 1

}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follows :
  *            System Clock source            = MSI
  *            SYSCLK(Hz)                     = 4000000
  *            HCLK(Hz)                       = 4000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            MSI Frequency(Hz)              = 4000000
  *            Flash Latency(WS)              = 0
  * @param  None
  * @retval None
  */



void SystemClock_Config(void)
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};

	//RTC requires to use HSE (or LSE or LSI, suspect these two are not available)
	//reading from RTC requires the APB clock is 7 times faster than HSE clock,
	//so turn PLL on and use PLL as clock source to sysclk (so to APB)
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; // RCC_MSIRANGE_6 is for 4Mhz. _7 is for 8 Mhz, _9 is for 16..., _10 is for 24 Mhz, _11 for 48Hhz
	RCC_OscInitStruct.MSICalibrationValue= RCC_MSICALIBRATION_DEFAULT;

	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;   //PLL source: either MSI, or HSI or HSE, but can not make HSE work.
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 40;
	RCC_OscInitStruct.PLL.PLLR = 2;  //2,4,6 or 8
	RCC_OscInitStruct.PLL.PLLP = 7;   // or 17.
	RCC_OscInitStruct.PLL.PLLQ = 4;   //2, 4,6, 0r 8
	//the PLL will be MSI (4Mhz)*N /M/R =

	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		// Initialization Error
		while(1);
	}

	// configure the HCLK, PCLK1 and PCLK2 clocks dividers
	// Set 0 Wait State flash latency for 4Mhz
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; //the freq of pllclk is MSI (4Mhz)*N /M/R = 80Mhz
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;


	if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)   //???
	{
		// Initialization Error
		while(1);
	}

	// The voltage scaling allows optimizing the power consumption when the device is
	//   clocked below the maximum system frequency, to update the voltage scaling value
	//   regarding system frequency refer to product datasheet.

	// Enable Power Control clock
	__HAL_RCC_PWR_CLK_ENABLE();

	if(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2) != HAL_OK)
	{
		// Initialization Error
		while(1);
	}

	// Disable Power Control clock   //why disable it?
	__HAL_RCC_PWR_CLK_DISABLE();
}
//after RCC configuration, for timmer 2---7, which are one APB1, the TIMxCLK from RCC is 4MHz??

/**
  * @brief  Configures EXTI line 0 (connected to PA.0 pin) in interrupt mode
  * @param  None
  * @retval None
  */

//  the joystick use exti0, exti1, exti2, exti3 and exti5(exti9_5). Initiating joystick will config exti0.
/*
static void EXTILine14_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  // Enable GPIOA clock
  __HAL_RCC_GPIOE_CLK_ENABLE();

	GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_14;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2); //is defined the same as the clear_flag()
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_14);

  // Enable and set EXTI Line0 Interrupt to the lowest priority
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}
*/

void  TIM4_PWM_Config(void)   // timer 4, channel is is mapped to AF2 of PB6 of the STM32L476G_disco board.
{
//TIM3 TIM4 are 16 bits, TIM2 and TIM5 are 32 bits
//with the settings in the sysclock_config(), the sysclk is 80Mhz, so is the AHB, APB1 and APB2.
//since the the prescaler of the APB is 1, the timeer's frequncy is 80Mhz.

	TIM4Prescaler=(uint16_t) (SystemCoreClock/ 50000) - 1;    //set the frequency as 50 Khz (50 ticks is 1ms). the prescaler is less than 65535, is OK.
	TIM4Period=1000;   //this makes the period as 20ms

	Tim4_Handle.Instance = TIM4; //TIM3 is defined in stm32f429xx.h

	Tim4_Handle.Init.Period = TIM4Period; //pwm frequency?
	Tim4_Handle.Init.Prescaler = TIM4Prescaler;
	Tim4_Handle.Init.ClockDivision = 0;
	Tim4_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;


	if(HAL_TIM_PWM_Init(&Tim4_Handle) != HAL_OK)
	{
		Error_Handler();
	}

	//configure the PWM channel
	Tim4_OCInitStructure.OCMode=  TIM_OCMODE_PWM1; //TIM_OCMODE_TIMING;
	Tim4_OCInitStructure.OCFastMode=TIM_OCFAST_DISABLE;
	Tim4_OCInitStructure.OCPolarity=TIM_OCPOLARITY_HIGH;

	Tim4_OCInitStructure.Pulse = TIM4_CCR1_Val;

	if(HAL_TIM_PWM_ConfigChannel(&Tim4_Handle, &Tim4_OCInitStructure, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	if(HAL_TIM_PWM_Start(&Tim4_Handle, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

}





void ADC_DMA_Config(void)     //use ADC1, channel 6, which connects to DMA1/channel1 or DMA2/channel3
{


	/******************

	1. Initialize the ADC low level resources by implementing the HAL_ADC_MspInit():
			a. Enable the ADC interface clock using __HAL_RCC_ADC_CLK_ENABLE()
			b. ADC pins configuration
					? Enable the clock for the ADC GPIOs using the following function:
					__HAL_RCC_GPIOx_CLK_ENABLE()
					? Configure these ADC pins in analog mode using HAL_GPIO_Init()
			c. In case of using interrupts (e.g. HAL_ADC_Start_IT())
					? Configure the ADC interrupt priority using HAL_NVIC_SetPriority()
					? Enable the ADC IRQ handler using HAL_NVIC_EnableIRQ()
					? In ADC IRQ handler, call HAL_ADC_IRQHandler()
			d. In case of using DMA to control data transfer (e.g. HAL_ADC_Start_DMA())
					? Enable the DMAx interface clock using
					__HAL_RCC_DMAx_CLK_ENABLE()
					? Configure and enable two DMA streams stream for managing data transfer
					from peripheral to memory (output stream)
					? Associate the initialized DMA handle to the CRYP DMA handle using
					__HAL_LINKDMA()
					? Configure the priority and enable the NVIC for the transfer complete
					interrupt on the two DMA Streams. The output stream should have higher
					priority than the input stream.


	************************/
	Adc1_Handle.Instance          = ADC1;

	if (HAL_ADC_DeInit(&Adc1_Handle) != HAL_OK)
	{
		Error_Handler();
	}

	Adc1_Handle.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;         // MUST USE THIS VALUE! OTHERWISE adc MAY STOP WORKING. //Asynchronous clock mode, input ADC clock not divided
	//ADC_CLOCKPRESCALER_PCLK_DIV2;

	Adc1_Handle.Init.Resolution = ADC_RESOLUTION_12B; //can be 6b, 8b, 10b, or 12b. for 12b, it taks 12 clock cycle to convert
	Adc1_Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;

	Adc1_Handle.Init.ScanConvMode = DISABLE;
	Adc1_Handle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;//DISABLE;  //the HAL manual show the possible values are: ADC_EOC_SEQ_CONV, or ADC_EOC_SINGLE_CONV.
	Adc1_Handle.Init.LowPowerAutoWait      = DISABLE;


	Adc1_Handle.Init.ContinuousConvMode = ENABLE;
	Adc1_Handle.Init.NbrOfConversion = 1; //this parameter' value ranges from 0000 to 1111,(this value will be written to ADC_SQR1 regitster)
	//0000--for 1 conversion, 0001---for 2 conversion, ......1111--for 16 conversion
	//here set as 1, is it correct?
	Adc1_Handle.Init.DiscontinuousConvMode = DISABLE;
	Adc1_Handle.Init.NbrOfDiscConversion = 1;

	//Adc1_Handle.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_CC4; //ADC control register 2 (ADC_CR2), defines certain external events to trigger
	Adc1_Handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;

	Adc1_Handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	//Adc1_Handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;

	Adc1_Handle.Init.DMAContinuousRequests = ENABLE;

	Adc1_Handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
	Adc1_Handle.Init.OversamplingMode      = DISABLE;



	if(HAL_ADC_Init(&Adc1_Handle) != HAL_OK)
	{
		Error_Handler();
	}


	if (HAL_ADCEx_Calibration_Start(&Adc1_Handle, ADC_SINGLE_ENDED) !=  HAL_OK)  //necessary!!! otherwise, reading (DR) will be different.
	{
		Error_Handler();
	}



	//##-2- Configure ADC regular channel ######################################
	sConfig.Channel = ADC_CHANNEL_7;     //ADC1, IN7 is on PA2
	sConfig.Rank = ADC_REGULAR_RANK_1;  //1;
	sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;   //has to be 640 to make it work
	sConfig.SingleDiff   = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;

	sConfig.Offset = 0;

	if(HAL_ADC_ConfigChannel(&Adc1_Handle, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}

	if(HAL_ADC_Start_DMA(&Adc1_Handle, (uint32_t*)&ADC1ConvertedValue, 1) != HAL_OK)   //second parameter would change according to time
	{
		Error_Handler();
	}


}

void Show_Temperature(void) //display current temperature
{
	/*
	VDD voltage is 3V mapped to 12 bits ADC 12 bits = 4095 locations
	so the voltage resolution is 3/4095
	since the temperature is amplified 3 times 
	the actual voltage ADC_convertedvalue* (3/4095)/3.
	temperature sensor sensitity is 10mV/C == 0.01V/C
	temperature is: ADC_convertedvalue* (3/4095) /3 /0.01 = ADC_convertedvalue * 0.02442
	*/

	measuredTemp=0.02442*ADC1ConvertedValue;

	sprintf(lcd_buffer,"T%5.2f",measuredTemp);
	BSP_LCD_GLASS_DisplayString((uint8_t *)lcd_buffer);
}

void Show_Setpoint(void) //display set temperature
{
	sprintf(lcd_buffer,"T%5.2f",setPoint);
	BSP_LCD_GLASS_DisplayString((uint8_t *)lcd_buffer);
}




/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
	case GPIO_PIN_0: 		               //SELECT button
		BSP_LCD_GLASS_Clear();
		BSP_LCD_GLASS_DisplayString((uint8_t*)"Set pt");

		if(current_state == temperature_state)
		{
			current_state = setting_state;
		}
		else
		{
			current_state = temperature_state;
		}

		break;
	case GPIO_PIN_1:     //left button
		//BSP_LCD_GLASS_Clear();
		//BSP_LCD_GLASS_DisplayString((uint8_t*)"left");   //can not be used, since ADC use PIN PA1.
		break;
	case GPIO_PIN_2:    //right button						  to play again.
		//BSP_LCD_GLASS_Clear();
		//BSP_LCD_GLASS_DisplayString((uint8_t*)"right");
		break;
	case GPIO_PIN_3:    //up button

		if(current_state == setting_state)
		{
			setPoint+= 0.5; //change set temperature
		}
		BSP_LCD_GLASS_Clear();
		BSP_LCD_GLASS_DisplayString((uint8_t*)"up");

		break;

	case GPIO_PIN_5:    //down button

		if(current_state == setting_state)
		{
			setPoint-= 0.5;
		}
		BSP_LCD_GLASS_Clear();
		BSP_LCD_GLASS_DisplayString((uint8_t*)"down");
		break;
	case GPIO_PIN_14:    //down button
		BSP_LCD_GLASS_Clear();
		BSP_LCD_GLASS_DisplayString((uint8_t*)"PE14");
		break;
	default://
		//default
		break;
	}
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32XXX_hal_tim.c for different callback function names.
{
	//for timer4
	if ((*htim).Instance==TIM3)
	{
//					sprintf(lcd_buffer,"b%5d",ADC1ConvertedValue);
//					BSP_LCD_GLASS_DisplayString((uint8_t *)lcd_buffer);
		//HAL_Delay(300);   //seems that with this set of Hal library, the HAL_Delay can not be used in any ISR.
		//clear the timer counter!  in stm32f4xx_hal_tim.c, the counter is not cleared after  OC interrupt
		__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h
	}
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef * htim)   //this is for TIM4_pwm
{

	__HAL_TIM_SET_COUNTER(htim, 0x0000);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
{
	/* Get the converted value of regular channel */
	//BSP_LED_Toggle(LED4);
//		ADC1ConvertedValue = HAL_ADC_GetValue(AdcHandle);
//		sprintf(lcd_buffer,"b%5d",ADC1ConvertedValue);
//		BSP_LCD_GLASS_DisplayString((uint8_t *)lcd_buffer);
	//HAL_Delay(300);
}



static void Error_Handler(void)
{
	/* Turn LED4 on */
	BSP_LED_On(LED4);
	while(1)
	{
	}
}





#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
	{
	}
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
