/*
  Скетч к проекту "Часы на ГРИ"
  Страница проекта (схемы, описания): https://alexgyver.ru/nixieclock/
  Исходники на GitHub: https://github.com/AlexGyver/nixieclock/
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  https://AlexGyver.ru/
*/
/**********************************
**        LexxQQ Edition         **
**        LexxQQ Edition         **
**********************************/
/*
  SET
  - удержали в режиме часов - настройка БУДИЛЬНИКА
  - удержали в режиме настройки будильника - настройка ЧАСОВ
  - двойной клик в режиме будильника - вернуться к часам
  - удержали в настройке часов - возврат к часам с новым временем
  - клик во время настройки - смена настройки часов/минут
  ALARM_PIN - вкл/выкл будильник
*/
// ************************** НАСТРОЙКИ **************************
#define BULB_TYPE  1  // 1 = (ИН-14 + ИН-16)  // 2 = (ИН-12)
#if BULB_TYPE == 2
#define IN12_WITH_SECONDS true  // (ТОЛЬКО для ИН-12) true = ЕСТЬ секундные индикаторы. false = НЕТ секундных индикаторов. Если НЕТ - то влажность будет отображаться на "минутных" индикаторах, иначе - в секундных
#endif
#define IS_DS18B20_ENABLED  false // включён ли второй датчик температуры. Если включён, то его значение заменяет температуру "как чувствуется человеком"
#define IS_HEAT_INDEX_ENABLED  false // отображать ли температуру "как чувствуется человеком"
#define BL_ENABLED  false // включена ли подсветка на WS2812B
#define AUTO_NIGHT_ENABLED  false // автоматическая яркость, исходя из освещенности комнаты

#define INDICATOR_QTY 7	// количество индикаторов (Dots) (H)(H) (M)(M) (S)(S)
#define BRIGHT 100	// яркость цифр дневная, %
#define BRIGHT_NIGHT 30	// яркость ночная, % // 20
#define NIGHT_START 20	// час перехода на ночную подсветку (BRIGHT_NIGHT)
#define NIGHT_END 8	// час перехода на дневную подсветку (BRIGHT) // 7

#define CLOCK_TIME_s 10	// время (с), которое отображаются часы
#define TEMPERATURE_TIME_s 5	// время (с), которое отображается температура и влажность
#define ALARM_TIMEOUT_s 30	// таймаут будильника
#define ALARM_FREQ 900	// частота писка будильника

/* !!!!!!!!!!!! ДЛЯ РАЗРАБОТЧИКОВ !!!!!!!!!!!! */
#define BURN_TIME_us 200	// период обхода в режиме очистки (мкс) // 200

#define REDRAW_TIME_us 3000	// время цикла одной цифры (мкс) // 3000
#define ON_TIME_us 1000	// время включенности одной цифры (мкс) (при 100% яркости) // 2200

// пины
#define PIEZO_PORT 3
#define DHT22_PIN 2

#define ALARM_PIN 12 // у Гайвера стояло 11

#define DECODER_0_PORT A0
#define DECODER_1_PORT A1
#define DECODER_2_PORT A2
#define DECODER_3_PORT A3

#define INDICATOR_0_PORT 4    // точка // D4
#define INDICATOR_1_PORT 10   // часы // D10
#define INDICATOR_2_PORT 9    // часы // D9
#define INDICATOR_3_PORT 5    // минуты // D5
#define INDICATOR_4_PORT 6    // минуты // D6
#define INDICATOR_5_PORT 7    // секунды // D7
#define INDICATOR_6_PORT 8    // секунды // D8

#define CLOCK_VISIBLE_ms CLOCK_TIME_s * 1000	// время (мс), которое отображается температура и влажность
#define TEMPERATURE_VISIBLE_ms TEMPERATURE_TIME_s * 1000	// время (мс), которое отображается температура и влажность
#define ALARM_TIMEOUT_ms ALARM_TIMEOUT_s * 1000	// таймаут будильника

/*********************** MODES ***********************/
#define Clock 0
#define Temperature 1
#define AlarmSet 2
#define ClockSet 3
#define Alarm 4

#define BLINK_ON_ms 800
#define BLINK_OFF_ms 200

/*********************** DS18B20 ***********************/
#define DS18B20_PIN 11
#if IS_DS18B20_ENABLED
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(DS18B20_PIN);	// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature ds18b20(&oneWire);	// Pass our oneWire reference to Dallas Temperature
#endif

/*********************** DHT22 ***********************/
#include "DHT.h"
DHT dht22Sensor(DHT22_PIN, DHT22);

#include "GyverTimer.h"
GTimer_us tmrRedraw(REDRAW_TIME_us); // таймер отрисовки одной цифры
GTimer_ms tmrMode(CLOCK_VISIBLE_ms); // таймер длительности режима
GTimer_ms tmrDots(500); // таймер мигания точек
GTimer_ms tmrBlink(BLINK_ON_ms); // таймер мигания цифры в нестройках
GTimer_ms tmrAlarm(ALARM_TIMEOUT_ms);
GTimer_ms tmrFade(2);
GTimer_ms tmrTest(125);
GTimer_ms tmrScroll(200);

#include "GyverButton.h"
GButton btnSet(3, LOW_PULL, NORM_OPEN);
GButton btnUp(3, LOW_PULL, NORM_OPEN);
GButton btnDown(3, LOW_PULL, NORM_OPEN);

/*********************** DS3231 ***********************/
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 ds3231Rtc;

#include "EEPROMex.h"
#if BL_ENABLED
#include <FastLED.h>
#define BL_PORT	0	// порт подсветки
#define BL_LEDS_QTY	INDICATOR_QTY	//	количество светодиодов
#define BL_BRIGHTNESS	50	// яркость подсветки
#define BL_LED_TYPE	WS2812B	// тип светодиодов
#define BL_COLOR_ORDER GRB
CRGB backLightLeds[BL_LEDS_QTY];
CRGB humidityColors[] = { CRGB::Green, CRGB::Yellow, CRGB::Yellow, CRGB::Red, CRGB::Red };
static uint8_t blGradientStartIndex = 0;
#endif

#if AUTO_NIGHT_ENABLED
#define AUTO_NIGHT_SENSOR_PIN	1	// пин датчика освещённости
#endif

unsigned char onTimePercents[] = { 100, 100, 100, 100, 100, 100, 100 };

int indicators[] = { INDICATOR_0_PORT, INDICATOR_1_PORT, INDICATOR_2_PORT, INDICATOR_3_PORT, INDICATOR_4_PORT, INDICATOR_5_PORT, INDICATOR_6_PORT };
byte curentIndicator;
byte digitsDraw[INDICATOR_QTY]; //
bool isDot;
byte hour = 0;
byte minute = 0;
byte second = 0;
byte alarmHour = 12;
byte alarmMinute = 0;
bool isIndicatorOn;
byte mode = Clock;	// 0 - часы, 1 - температура, 2 - настройка будильника, 3 - настройка часов, 4 - аларм
bool isChange;
bool isBlink;
unsigned int onTime_us = ON_TIME_us;
bool isAlarm;

const byte dotsIndex = 0;
const byte h0Index = BULB_TYPE == 1 ? 1 : INDICATOR_QTY - 1 - 2;
const byte h1Index = BULB_TYPE == 1 ? 2 : INDICATOR_QTY - 2 - 2;
const byte m0Index = BULB_TYPE == 1 ? 3 : INDICATOR_QTY - 3 - 2;
const byte m1Index = BULB_TYPE == 1 ? 4 : INDICATOR_QTY - 4 - 2;
const byte s0Index = 5;
const byte s1Index = 6;

// установка яркости от времени суток
bool prevIsNight = false;
void changeBright() {
	bool isNight = (hour >= NIGHT_START && hour <= 23) || (hour >= 0 && hour <= NIGHT_END); // старый механизм

#if AUTO_NIGHT_ENABLED
	isNight = digitalRead(AUTO_NIGHT_SENSOR_PIN); // 1 = день, 0 - ночь
	digitalWrite(LED_BUILTIN, isNight);
	if (isNight != prevIsNight) {
		prevIsNight = isNight;
		tone(PIEZO_PORT, ALARM_FREQ, 50);
	}
#endif

	if (isNight)
	{
		onTime_us = (float)ON_TIME_us * BRIGHT_NIGHT / 100;
	}
	else
	{
		onTime_us = (float)ON_TIME_us * BRIGHT / 100;
	}
}

// отправить время в массив отображения
void sendTime(byte hh, byte mm, byte ss) {
	digitsDraw[h0Index] = (byte)(hh / 10);
	digitsDraw[h1Index] = (byte)(hh % 10);

	digitsDraw[m0Index] = (byte)(mm / 10);
	digitsDraw[m1Index] = (byte)(mm % 10);

	digitsDraw[s0Index] = (byte)(ss / 10);
	digitsDraw[s1Index] = (byte)(ss % 10);
}

// отправить температуру и влажность в массив отображения
void sendTemperature(byte tt, byte hh, byte heatIndex) {
	digitsDraw[h0Index] = (byte)(tt / 10);
	digitsDraw[h1Index] = (byte)(tt % 10);

	if (IS_DS18B20_ENABLED || IS_HEAT_INDEX_ENABLED) {
		digitsDraw[m0Index] = (byte)(heatIndex / 10);
		digitsDraw[m1Index] = (byte)(heatIndex % 10);
	}
	else {
		digitsDraw[m0Index] = 10;
		digitsDraw[m1Index] = 10;
	}

#if BULB_TYPE == 2 // && !IN12_WITH_SECONDS
	digitsDraw[m0Index] = (byte)(hh / 10);
	digitsDraw[m1Index] = (byte)(hh % 10);
#else
	digitsDraw[s0Index] = (byte)(hh / 10);
	digitsDraw[s1Index] = (byte)(hh % 10);
#endif
}

// включение режима и запуск таймера
void setMode(byte newMode) {
	mode = newMode;
	switch (mode)
	{
	case Clock:
		tmrMode.setInterval(CLOCK_VISIBLE_ms);
		break;

	case Temperature:
		tmrMode.setInterval(TEMPERATURE_VISIBLE_ms);
		break;
	}
}

//
void buttonsTick() {
	int analog = analogRead(7);

#if !BL_ENABLED
	//Serial.println(analog);
	//return;
#endif

	btnSet.tick(analog > 1000 && analog < 1024);
	btnUp.tick(analog > 750 && analog < 810);
	btnDown.tick(analog > 190 && analog < 240);
	// 1023 > 1000 < 1023 set
	// 785 > 690 <= 820 +
	// 216 > 120 <= 280 -

	if (mode == AlarmSet || mode == ClockSet) {
		if (btnUp.isClick()) {
			if (mode == AlarmSet) {
				if (!isChange) {
					alarmMinute++;
					if (alarmMinute > 59) {
						alarmMinute = 0;
						alarmHour++;
					}
					if (alarmHour > 23)
					{
						alarmHour = 0;
					}
				}
				else {
					alarmHour++;
					if (alarmHour > 23)
					{
						alarmHour = 0;
					}
				}
			}
			else {
				if (!isChange) {
					minute++;
					if (minute > 59) {
						minute = 0;
						hour++;
					}
					if (hour > 23)
					{
						hour = 0;
					}
				}
				else {
					hour++;
					if (hour > 23)
					{
						hour = 0;
					}
				}
			}
		}

		if (btnDown.isClick()) {
			if (mode == AlarmSet) {
				if (!isChange) {
					alarmMinute--;
					if (alarmMinute < 0) {
						alarmMinute = 59;
						alarmHour--;
					}
					if (alarmHour < 0)
					{
						alarmHour = 23;
					}
				}
				else {
					alarmHour--;
					if (alarmHour < 0)
					{
						alarmHour = 23;
					}
				}
			}
			else {
				if (!isChange) {
					minute--;
					if (minute < 0) {
						minute = 59;
						hour--;
					}
					if (hour < 0)
					{
						hour = 23;
					}
				}
				else {
					hour--;
					if (hour < 0)
					{
						hour = 23;
					}
				}
			}
		}

		if (tmrBlink.isReady()) {
			if (isBlink)
			{
				tmrBlink.setInterval(BLINK_ON_ms);
			}
			else
			{
				tmrBlink.setInterval(BLINK_OFF_ms);
			}
			isBlink = !isBlink;
		}

		if (mode == AlarmSet) {
			sendTime(alarmHour, alarmMinute, 0);
		}
		else {
			sendTime(hour, minute, 0);
		}

		if (isBlink) {      // горим
			if (isChange) {
				digitsDraw[h0Index] = 10;
				digitsDraw[h1Index] = 10;
			}
			else {
				digitsDraw[m0Index] = 10;
				digitsDraw[m1Index] = 10;
			}
		}
	}

	if (mode == Temperature && btnSet.isClick()) {
		setMode(Clock);
	}

	if (mode == Clock && btnSet.isHolded()) {
		setMode(AlarmSet);
	}

	if (mode == AlarmSet && btnSet.isHolded()) {
		setMode(ClockSet);
	}

	if (mode == AlarmSet && btnSet.isDouble()) {
		sendTime(hour, minute, second);
		EEPROM.updateByte(0, alarmHour);
		EEPROM.updateByte(1, alarmMinute);
		setMode(Clock);
	}

	if (mode == ClockSet && btnSet.isHolded()) {
		sendTime(hour, minute, second);
		second = 0;
		EEPROM.updateByte(0, alarmHour);
		EEPROM.updateByte(1, alarmMinute);
		ds3231Rtc.adjust(DateTime(2018, 1, 12, hour, minute, 0)); // дата первого запуска
		// changeBright();
		setMode(Clock);
	}

	if ((mode == AlarmSet || mode == ClockSet) && btnSet.isClick()) {
		isChange = !isChange;
	}
}

// включает или отключает индикатор
void setIndicatorState(byte indicatorNumber, bool isOn) {
	digitalWrite(indicators[indicatorNumber], isOn);	// включаем текущий индикатор
}

// потушить все индикаторы
void indicatorsOff(bool isImmediately = false) {
	for (byte i = 1; i < INDICATOR_QTY; i++)
	{
		digitsDraw[i] = 10;
		if (isImmediately) { // немедленное отключение индикатора (не ждать таймера отрисовки)
			setIndicatorState(indicators[i], false);
		}
	}
}

// функция настройки декодера
void setDecoder(byte dec0, byte dec1, byte dec2, byte dec3) {
	digitalWrite(DECODER_0_PORT, dec0);
	digitalWrite(DECODER_1_PORT, dec1);
	digitalWrite(DECODER_2_PORT, dec2);
	digitalWrite(DECODER_3_PORT, dec3);
}

// настраиваем декодер согласно отображаемой ЦИФРЕ
void setDigit(byte digit) {
	switch (digit) {
	case 0:
		setDecoder(0, 0, 0, 0);
		break;

	case 1:
		setDecoder(1, 0, 0, 0);
		break;

	case 2:
		setDecoder(0, 0, 1, 0);
		break;

	case 3:
		setDecoder(1, 0, 1, 0);
		break;

	case 4:
		setDecoder(0, 0, 0, 1);
		break;

	case 5:
		setDecoder(1, 0, 0, 1);
		break;

	case 6:
		setDecoder(0, 0, 1, 1);
		break;

	case 7:
		setDecoder(1, 0, 1, 1);
		break;

	case 8:
		setDecoder(0, 1, 0, 0);
		break;

	case 9:
		setDecoder(1, 1, 0, 0);
		break;

	case 10:
		setDecoder(0, 1, 1, 1);    // выключить цифру!
		break;
	}
}

// прожиг (антиотравление)
void burnIndicators() {
	indicatorsOff(true);

	// повключать все индикаторы
	for (byte i = 0; i < INDICATOR_QTY; i++) {
		setIndicatorState(indicators[i], true);

		// повключать все цифры
		for (byte j = 0; j < 10; j++) {
			setDigit(j);
			delayMicroseconds(BURN_TIME_us);
		}

		setIndicatorState(indicators[i], false);
	}
}

unsigned int curentOnTime_us = onTime_us;
//
void tmrRedraw_Event() {
	if (!isIndicatorOn) {
		curentIndicator++;					// счётчик бегает по индикаторам (0 - 6)
		if (curentIndicator > 6)
		{
			curentIndicator = 0;	// дошли
		}
		if (curentIndicator != 0) {		// если это не точка
			setIndicatorState(curentIndicator, true);	// включаем текущий индикатор
			setDigit(digitsDraw[curentIndicator]);	// отображаем ЦИФРУ
		}
		else {		// если это точка
			if (isDot)
			{
				if (mode != Temperature)
				{
					setIndicatorState(curentIndicator, true);	// включаем точку
				}
				else
				{
					setIndicatorState(curentIndicator, false);	// выключаем точку
				}
			}
		}
		if (curentIndicator < sizeof(onTimePercents)) {
			curentOnTime_us = (float)onTime_us * onTimePercents[curentIndicator - 1] / 100; // яркость
		}
		else {
			curentOnTime_us = onTime_us; // яркость
		}
		tmrRedraw.setInterval(curentOnTime_us);	// переставляем таймер (столько индикатор будет светить)
	}
	else {
		setIndicatorState(curentIndicator, false);		// выключаем текущий индикатор
		int offTime_us = REDRAW_TIME_us - curentOnTime_us;
		tmrRedraw.setInterval(offTime_us);	// переставляем таймер (столько индикаторы будут выключены)
	}
	isIndicatorOn = !isIndicatorOn;
}

bool tmrFadeInc = true;

//
void tmrDots_Event() {
	if (mode == Clock || mode == Temperature) {
		isDot = !isDot;
		if (isDot) {
			second++;
			if (second > 59) {
				second = 0;
				minute++;

				if (minute == 1 || minute == 30) { // каждые полчаса
					burnIndicators();  // чистим чистим!
					//DateTime now = ds3231Rtc.now(); // синхронизация с RTC
					//second = now.second();
					//minute = now.minute();
					//hour = now.hour();
				}

				if (!isAlarm && alarmMinute == minute && alarmHour == hour && !digitalRead(ALARM_PIN)) {
					setMode(Clock); // mode = 0;
					isAlarm = true;
					tmrAlarm.start();
					tmrAlarm.reset();
				}
			}

			if (minute > 59) {
				minute = 0;
				hour++;
				if (hour > 23) {
					hour = 0;
				}
				// changeBright();
			}

			if (mode == Clock)
			{
				sendTime(hour, minute, second);
#if BL_ENABLED
				FillLEDsFromPaletteColors(blGradientStartIndex++);
#endif
				//tmrFade.start();
			}
			else {
				onTimePercents[4] = onTimePercents[5] = 100;
				// tmrFade.stop();
			}

			if (isAlarm) {
				if (tmrAlarm.isReady() || digitalRead(ALARM_PIN)) {
					isAlarm = false;
					tmrAlarm.stop();
					noTone(PIEZO_PORT);
					setMode(Clock);
				}
			}
		}
		else {
			if (mode == Clock)
			{
				tmrFade.start();
			}
			else {
				tmrFade.stop();
			}
		}

		// мигать на будильнике
		if (isAlarm) {
			if (!isDot) {
				noTone(PIEZO_PORT);
				indicatorsOff();
			}
			else {
				tone(PIEZO_PORT, ALARM_FREQ);
				sendTime(hour, minute, second);
			}
		}
	}

#if BL_ENABLED
	setBackLight();
#endif
	changeBright();
}

#if BL_ENABLED
void setBackLight() {
	if (mode == Clock) {
		//for (byte i = 0; i < BL_LEDS_QTY; i++)
		//{
		//	backLightLeds[i] = CRGB::BlueViolet;
		//}
	}

	if (mode == Temperature) {
		// потушить все
		for (byte i = 0; i < BL_LEDS_QTY; i++)
		{
			backLightLeds[i] = CRGB::Black;
		}
		int humidity = digitsDraw[s0Index] * 10 + digitsDraw[s1Index];
		byte humidityColorIndex = 0;
		if (humidity >= 50) {
			humidityColorIndex = humidity - 50;
		}
		else {
			humidityColorIndex = 50 - (humidity + 1);
		}
		humidityColorIndex /= 10;

		//backLightLeds[5] = backLightLeds[6] = humidityColors[humidityColorIndex];
		for (byte i = 0; i < BL_LEDS_QTY; i++)
		{
			backLightLeds[i] = humidityColors[humidityColorIndex];
		}
	}

	if (mode == Alarm) {
		for (byte i = 0; i < BL_LEDS_QTY; i++)
		{
			backLightLeds[i] = CRGB::Red;
		}
	}

	FastLED.show();
}
#endif

//
void tmrMode_Event() {
	if (!isAlarm)
	{
		tmrDots.stop();
		tmrScroll.start();
	}
#if BL_ENABLED
	setBackLight();
#endif
}

#if BL_ENABLED
void FillLEDsFromPaletteColors(uint8_t colorIndex)
{
	uint8_t brightness = 255;
	for (int i = 0; i < BL_LEDS_QTY; i++) {
		backLightLeds[i] = ColorFromPalette(RainbowStripeColors_p, colorIndex, brightness, LINEARBLEND);
		colorIndex += 3;
	}
	FastLED.show();
}
#endif

#if !IS_DS18B20_ENABLED
void ConfigurePwm() {
	// PWM frequency for D3 & D11:
	TCCR2B = TCCR2B & B11111000 | B00000001; // for PWM frequency of 31372.55 Hz
	// TCCR2B = TCCR2B & B11111000 | B00000010; // for PWM frequency of 3921.16 Hz
	// TCCR2B = TCCR2B & B11111000 | B00000011; // for PWM frequency of 980.39 Hz
	// TCCR2B = TCCR2B & B11111000 | B00000100; // for PWM frequency of 490.20 Hz (The DEFAULT)
	// TCCR2B = TCCR2B & B11111000 | B00000101; // for PWM frequency of 245.10 Hz
	// TCCR2B = TCCR2B & B11111000 | B00000110; // for PWM frequency of 122.55 Hz
	// TCCR2B = TCCR2B & B11111000 | B00000111; // for PWM frequency of 30.64 Hz

	// PWM frequency for D5 & D6: (interactions with the millis() and delay() functions)
	// TCCR0B = TCCR0B & B11111000 | B00000001; // for PWM frequency of 62500.00 Hz
	// TCCR0B = TCCR0B & B11111000 | B00000010; // for PWM frequency of 7812.50 Hz
	// TCCR0B = TCCR0B & B11111000 | B00000011; // for PWM frequency of 976.56 Hz (The DEFAULT)
	// TCCR0B = TCCR0B & B11111000 | B00000100; // for PWM frequency of 244.14 Hz
	// TCCR0B = TCCR0B & B11111000 | B00000101; // for PWM frequency of 61.04 Hz

	// PWM frequency for D9 & D10:
	// TCCR1B = TCCR1B & B11111000 | B00000001; // set timer 1 divisor to 1 for PWM frequency of 31372.55 Hz
	// TCCR1B = TCCR1B & B11111000 | B00000010; // for PWM frequency of 3921.16 Hz
	// TCCR1B = TCCR1B & B11111000 | B00000011; // for PWM frequency of 490.20 Hz (The DEFAULT)
	// TCCR1B = TCCR1B & B11111000 | B00000100; // for PWM frequency of 122.55 Hz
	// TCCR1B = TCCR1B & B11111000 | B00000101; // for PWM frequency of 30.64 Hz

	// PWM values for D11:
	analogWrite(11, 160); // Функция переводит вывод в режим ШИМ и задает для него коэффициент заполнения (значение 0 до 255)
}
#endif

void tmrFade_Event() {
	if (mode != Clock)
	{
		//tmrFade.stop();
		// return;
	}

	if (tmrFadeInc) {
		onTimePercents[4] = onTimePercents[5]++;
	}
	else {
		onTimePercents[4] = onTimePercents[5]--;
	}

	if (onTimePercents[5] >= 100) {
		tmrFadeInc = false;
		tmrFade.stop();
	}
	if (onTimePercents[5] <= 0) {
		tmrFadeInc = true;
	}
}

byte testIndicator = 1;
byte testSign = 0;
void tmrTest_Event() {
	for (byte i = 0; i < INDICATOR_QTY; i++) {
		if (i == testIndicator) {
			digitsDraw[i] = testSign;
		}
		else {
			digitsDraw[i] = 10;
		}
	}

	testSign++;

	if (testSign > 9) {
		testIndicator++;
		testSign = 0;
	}

	if (testIndicator >= INDICATOR_QTY) {
		testSign = 0;
		testIndicator = 0;
		tmrTest.stop();
		tmrMode.start();
		tmrDots.start();
		tmrBlink.start();
		tmrAlarm.start();
		tmrFade.start();
	}
}

byte scrollPosition = 0;
void tmrScroll_Event() {
	// digitsDraw[scrollPosition] = digitsDraw[scrollPosition + 1];
	digitsDraw[h0Index] = digitsDraw[h1Index];
	digitsDraw[h1Index] = digitsDraw[m0Index];
	digitsDraw[m0Index] = digitsDraw[m1Index];
	digitsDraw[m1Index] = digitsDraw[s0Index];
	digitsDraw[s0Index] = digitsDraw[s1Index];
	digitsDraw[s1Index] = 10;

	if (scrollPosition++ >= INDICATOR_QTY) {
		scrollPosition = 0;
		tmrScroll.stop();
		tmrDots.start();
		if (mode == Clock) {
			indicatorsOff(true);
			isDot = false;
			float temp = dht22Sensor.readTemperature();
			float hum = dht22Sensor.readHumidity();
			float heatIndex = 0;
#if IS_HEAT_INDEX_ENABLED
			heatIndex = dht22Sensor.computeHeatIndex(temp, hum, false);
#endif
#if IS_DS18B20_ENABLED
			ds18b20.requestTemperatures(); // Send the command to get temperatures
			if (ds18b20.isConversionComplete()) {
				float ds18b20Temperature = ds18b20.getTempCByIndex(0);
				heatIndex = ds18b20Temperature; // вывод на минутные индикаторы
			}
#endif
			sendTemperature(temp, hum, heatIndex);
			setMode(Temperature);
		}
		else {
			DateTime now = ds3231Rtc.now(); // синхронизация с RTC
			second = now.second();
			minute = now.minute();
			hour = now.hour();
			indicatorsOff();
			setMode(Clock);
		}

	}
}

//
void setup() {
	pinMode(LED_BUILTIN, OUTPUT);

	tmrMode.stop();
	tmrDots.stop();
	tmrBlink.stop();
	tmrAlarm.stop();
	tmrFade.stop();
	tmrScroll.stop();
#if !IS_DS18B20_ENABLED
	ConfigurePwm();
#endif

#if !BL_ENABLED
	//Serial.begin(9600);
#endif

	tmrAlarm.stop();
	btnSet.setTimeout(400);
	btnSet.setDebounce(90);
	dht22Sensor.begin();
	ds3231Rtc.begin();
	if (ds3231Rtc.lostPower()) {
		ds3231Rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // following line sets the RTC to the date & time this sketch was compiled
	}
	DateTime now = ds3231Rtc.now();
	second = now.second();
	minute = now.minute();
	hour = now.hour();

	pinMode(DECODER_0_PORT, OUTPUT);
	pinMode(DECODER_1_PORT, OUTPUT);
	pinMode(DECODER_2_PORT, OUTPUT);
	pinMode(DECODER_3_PORT, OUTPUT);

	pinMode(INDICATOR_0_PORT, OUTPUT);
	pinMode(INDICATOR_1_PORT, OUTPUT);
	pinMode(INDICATOR_2_PORT, OUTPUT);
	pinMode(INDICATOR_3_PORT, OUTPUT);
	pinMode(INDICATOR_4_PORT, OUTPUT);
	pinMode(INDICATOR_5_PORT, OUTPUT);
	pinMode(INDICATOR_6_PORT, OUTPUT);

	pinMode(PIEZO_PORT, OUTPUT);
	pinMode(ALARM_PIN, INPUT_PULLUP);

	if (EEPROM.readByte(100) != 66) {   // проверка на первый запуск
		EEPROM.writeByte(100, 66);
		EEPROM.writeByte(0, 0);     // часы будильника
		EEPROM.writeByte(1, 0);     // минуты будильника
	}

	// EEPROM.writeByte(0, 7);     // часы будильника
	// EEPROM.writeByte(1, 10);     // минуты будильника

	alarmHour = EEPROM.readByte(0);
	alarmMinute = EEPROM.readByte(1);

#if BL_ENABLED
	// delay(3000); // power-up safety delay
	FastLED.addLeds<BL_LED_TYPE, BL_PORT, BL_COLOR_ORDER>(backLightLeds, BL_LEDS_QTY).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(BL_BRIGHTNESS);
#endif

#if AUTO_NIGHT_ENABLED
	pinMode(AUTO_NIGHT_SENSOR_PIN, INPUT);
#endif

#if IS_DS18B20_ENABLED
	ds18b20.begin();
#endif

	sendTime(hour, minute, second);
	changeBright();
}

// главный цикл
void loop() {
	if (tmrTest.isReady())
	{
		tmrTest_Event();
	}

	if (tmrRedraw.isReady())
	{
		tmrRedraw_Event();
	}

	if (tmrDots.isReady())
	{
		tmrDots_Event();
	}

	if (tmrMode.isReady())
	{
		tmrMode_Event();
	}

	if (tmrFade.isReady())
	{
		tmrFade_Event();
	}

	if (tmrScroll.isReady())
	{
		tmrScroll_Event();
	}

	buttonsTick();
}
