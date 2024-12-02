#if 1
/*
 * app.c
 *
 *  Created on: Nov 25, 2024
 *      Author: blueb
 */
#include "app.h"
#include "uart.h"
#include "delay.h"
#include <stdio.h>

// 장치 재선언
extern TIM_HandleTypeDef htim3;		// echo 파형의 길이 측정
extern TIM_HandleTypeDef htim11;	// us 지연함수
extern UART_HandleTypeDef huart2;
extern int ledTimer10s;
extern int gt5;
extern int lt5;
// 전역 변수
uint32_t inputCapture_Val1 	= 0; 	// Rising Edge  위치 저장
uint32_t inputCapture_Val2 	= 0;	// Falling Edge 위치 저장
uint32_t difference 				= 0;	// Rising Edge - Falling Edge
uint8_t  is_first_captured 	= 0; 	// 현재 캡처 단계
uint8_t distance 						= 0;	// 계산된 거리 (cm), (센서의 성능상 200cm 이상은 나오지도 않아 1byte로 선택했다)

// 인터럽트 서비스 루틴(ISR)
// Global Interrup Enable 하면 여기에 대응하는 함수를 만들어줘야 한다 (오타가 있어서는 안된다 : stm32f4xx_it.h)
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
	// 인터럽트가 발생된 타이머의 채널 확인
	if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2){
		// 현재 캡처 상승 단계
		if(is_first_captured == 0){ // Rising Edge
			// 타이머에서 Rising Edge 에서 캡처된 카운트 값을 저장해준다
			inputCapture_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
			// 다음 인터럽트 발생을 위한 설정
			is_first_captured = 1;
			// 타이머가 기본적으로 Rising Edge 에서동작하도록 되어 있기 때문에 Falling Edge 에서 인터럽트가 발생하도록 타이머 설정을 변경한다
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);

		}
		else if(is_first_captured == 1) {// Falling Edge
			// 타이머에서 Falling Edge 에서 캡처된 카운트 값을 저장해준다
			inputCapture_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

			/*
			 * 공기중 소리의 속도 = 340m/s = 0.034cm/us
			 * t(시간) = s(거리) / v(속도) = 10 / 0.034 = 294 us
			 * s(거리) = t(시간) * 0.034 / 2 (물체까지 가는 시간(1) + 돌아오는 시간(1) = 2, 고정된 위치를 가정한다)
			 */
			//거리 계산 (소요 시간)
			if(inputCapture_Val2 > inputCapture_Val1) {
				difference = inputCapture_Val2 - inputCapture_Val1;
			}
			else {
				difference = 65535 - inputCapture_Val1 + inputCapture_Val2;
			}
			distance = difference * 0.034 / 2;

			if(distance >= 5){
				gt5 = 1;
				lt5 = 0;
				if(ledTimer10s>=10000){
					HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_RESET);
					ledTimer10s = 10000;
				}
			}
			else{
				gt5 = 0;
				lt5 = 1;
				HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_SET);
			}

			// 다음 인터럽트 발생을 위한 설정
			is_first_captured = 0;
			// 타이머가 기본적으로 Falling Edge 에서동작하도록 되어 있기 때문에 Rising Edge 에서 인터럽트가 발생하도록 타이머 설정을 변경한다
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
			htim->Instance->CNT = 0;
			__HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC2);
		}
	}
}

/*
 * 100ms 마다 Trigger Pin을 쓰기 핀으로 변경해주고
 */
uint32_t getDistance(){

//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); 아래와 동일
	HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_SET);
	delayUS(10);
	HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_RESET);

	// 인터럽트 설정
	__HAL_TIM_ENABLE_IT(&htim3,TIM_IT_CC2);

	return distance;
}

/*
 * @brief generation pulse (100m period 10us duty)
 */
void setTrigger(){
	printf("distance = %ld\n", getDistance());
	HAL_Delay(100);
}

void app(){

	// Uart 장치 초기화
	initUart(&huart2);

	// Timer3 start (Input Capture)
	// @brief  Starts the TIM Input Capture measurement in interrupt mode.
	HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);

	// Timer11 start (Counter)
	// @brief  Starts the TIM Base generation.
	HAL_TIM_Base_Start(&htim11);

	while(1){

		setTrigger();

	}

}

#else

/*
 * app.c
 *
 *  Created on: Nov 25, 2024
 *      Author: iotpc
 */

#include "app.h"
#include "uart.h"
#include <stdio.h>

// 장치 재선언
extern TIM_HandleTypeDef htim3;		// echo 파형의 길이 측정
extern TIM_HandleTypeDef htim11;	// us지연함수
extern UART_HandleTypeDef huart2;

// 전역변수
uint32_t	ic_val1 = 0;		// 상승 위치 저장
uint32_t 	ic_val2 = 0;		// 하강 위치 저장
uint32_t	difference = 0;	// 하강 - 상승
uint8_t		is_first_captured = 0;	// 현재 캡춰 단계
uint8_t		distance = 0;		// 계산된 거리(cm)

// us delay
void delayUs(uint16_t t) {
	htim11.Instance->CNT = 0;					// 카운터 초기화
	while(htim11.Instance->CNT < t);	// up카운터의 값이 t와 같으면 종료
}

// 인터럽트 서비스 루틴(isr)
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	// 인터럽트가 발생된 타이머의 채널 확인
	if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
		// 현재 캡춰 상승 단계
		if(is_first_captured == 0) {
			// 현재 카운터의 값을 저장
			ic_val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
			// 다음 인터럽트 발생을 위한 설정
			is_first_captured = 1;
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
		}
		// 현재 캡춰 하강 단계
		else if(is_first_captured == 1) {
			// 현재 카운터의 값을 저장
			ic_val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

			// 거리 계산
			if(ic_val2 > ic_val1) {
				difference = ic_val2 - ic_val1;
			}
			else {
				difference = 65535 - ic_val1 + ic_val2;
			}
			distance = difference * 0.034 / 2;

			// 다음 인터럽트 발생을 위한 설정
			is_first_captured = 0;
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
			htim->Instance->CNT = 0;
			// 인터럽트 중지
			__HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC2);
		}
	}
}

// 거리를 측정하여 반환
uint32_t	getDistance() {
	// 트리거 핀 동작
	HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_SET);
	delayUs(10);
	HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_RESET);
	// 인터럽트 설정
	__HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC2);
	return distance;
}

void app() {
	// uart 장치 초기화
	initUart(&huart2);
	// 타이머 시작
	HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
	HAL_TIM_Base_Start(&htim11);
	while(1) {
		printf("distance = %ld\n", getDistance());
		HAL_Delay(100);
	}
}



#endif
