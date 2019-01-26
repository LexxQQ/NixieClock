/*
  Скетч к проекту "Часы на ГРИ"
  Страница проекта (схемы, описания): https://alexgyver.ru/nixieclock/
  Исходники на GitHub: https://github.com/AlexGyver/nixieclock/
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2018
  https://AlexGyver.ru/
*/
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
#define INDICATOR_QTY 7	// количество индикаторов (H)(H) (M)(M) (S)(S) (Dots)
#define BRIGHT 100	// яркость цифр дневная, %
#define BRIGHT_NIGHT 30	// яркость ночная, % // 20
#define NIGHT_START 20	// час перехода на ночную подсветку (BRIGHT_NIGHT)
#define NIGHT_END 8	// час перехода на дневную подсветку (BRIGHT) // 7

#define CLOCK_TIME_s 10	// время (с), которое отображаются часы
#define TEMPERATURE_TIME_s 5	// время (с), которое отображается температура и влажность
#define ALARM_TIMEOUT_s 30	// таймаут будильника
#define ALARM_FREQ 900	// частота писка будильника

/* !!!!!!!!!!!! ДЛЯ РАЗРАБОТЧИКОВ !!!!!!!!!!!! */
#define BURN_TIME_us 500000	// период обхода в режиме очистки (мкс) // 200

#define REDRAW_TIME_us 3000	// время цикла одной цифры (мкс) // 3000
#define ON_TIME_us 2500	// время включенности одной цифры (мкс) (при 100% яркости) // 2200

// пины
#define PIEZO_PORT 3
#define DHT22_PIN 2
#define DS18B20_PIN 11
#define ALARM_PIN 12 // 11

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

//enum Mode {
//	// часы
//	Clock,
//
//	// температура
//	Temperature,
//
//	// настройка будильника
//	AlarmSet,
//
//	// настройка часов
//	ClockSet,
//
//	// будильник
//	Alarm
//};

#include "DHT.h"
DHT dht22Sensor(DHT22_PIN, DHT22);

#include "GyverTimer.h"
GTimer_us tmrRedraw(REDRAW_TIME_us); // таймер отрисовки одной цифры
GTimer_ms tmrMode(CLOCK_VISIBLE_ms); // таймер длительности режима
GTimer_ms tmrDots(500); // таймер мигания точек
GTimer_ms tmrBlink(BLINK_ON_ms); // таймер мигания цифры в нестройках
GTimer_ms tmrAlarm(ALARM_TIMEOUT_ms);

#include "GyverButton.h"
GButton btnSet(3, LOW_PULL, NORM_OPEN);
GButton btnUp(3, LOW_PULL, NORM_OPEN);
GButton btnDown(3, LOW_PULL, NORM_OPEN);

#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 ds3231Rtc;

#include "EEPROMex.h"

#define IS_HEAT_INDEX_ENABLED	false	// отображать ли температуру "как чувствуется человеком"

#define BL_ENABLED	true	// включена ли подсветка на WS2812B
#if BL_ENABLED
#include <FastLED.h>
#define BL_PORT	1	// порт подсветки
#define BL_LEDS_QTY	INDICATOR_QTY	//	количество светодиодов
#define BL_BRIGHTNESS	70	// яркость подсветки
#define BL_LED_TYPE	WS2812B	// тип светодиодов
#define BL_COLOR_ORDER GRB
CRGB backLightLeds[BL_LEDS_QTY];
CRGB humidityColors[] = { CRGB::Green, CRGB::Yellow, CRGB::Yellow, CRGB::Red, CRGB::Red };
static uint8_t blGradientStartIndex = 0;
#endif

int indicators[] = { INDICATOR_0_PORT, INDICATOR_1_PORT, INDICATOR_2_PORT, INDICATOR_3_PORT, INDICATOR_4_PORT, INDICATOR_5_PORT, INDICATOR_6_PORT };
byte curentIndicator;
byte digitsDraw[INDICATOR_QTY]; //
bool isDot;
byte hour = 10;
byte minute = 10;
byte second = 0;
byte alarmHour = 10;
byte alarmMinute = 10;
bool isIndicatorOn;
byte mode = Clock;	// 0 - часы, 1 - температура, 2 - настройка будильника, 3 - настройка часов, 4 - аларм
bool isChange;
bool isBlink;
unsigned int onTime_us = ON_TIME_us;
bool isAlarm;

// установка яркости от времени суток
void changeBright() {
	if ((hour >= NIGHT_START && hour <= 23) || (hour >= 0 && hour <= NIGHT_END))
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
	digitsDraw[1] = (byte)(hh / 10);
	digitsDraw[2] = (byte)(hh % 10);

	digitsDraw[3] = (byte)(mm / 10);
	digitsDraw[4] = (byte)(mm % 10);

	digitsDraw[5] = (byte)(ss / 10);
	digitsDraw[6] = (byte)(ss % 10);
}

// отправить температуру и влажность в массив отображения
void sendTemperature(byte tt, byte hh, byte heatIndex) {
	digitsDraw[1] = (byte)(tt / 10);
	digitsDraw[2] = (byte)(tt % 10);

	if (IS_HEAT_INDEX_ENABLED) {
		digitsDraw[3] = (byte)(heatIndex / 10);
		digitsDraw[4] = (byte)(heatIndex % 10);
	}
	else {
		digitsDraw[3] = 10;
		digitsDraw[4] = 10;
	}

	digitsDraw[5] = (byte)(hh / 10);
	digitsDraw[6] = (byte)(hh % 10);
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
				digitsDraw[1] = 10;
				digitsDraw[2] = 10;
			}
			else {
				digitsDraw[3] = 10;
				digitsDraw[4] = 10;
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
		changeBright();
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
		tmrRedraw.setInterval(onTime_us);	// переставляем таймер (столько индикатор будет светить)
	}
	else {
		setIndicatorState(curentIndicator, false);		// выключаем текущий индикатор	
		int offTime_us = REDRAW_TIME_us - onTime_us;
		tmrRedraw.setInterval(offTime_us);	// переставляем таймер (столько индикаторы будут выключены)
	}
	isIndicatorOn = !isIndicatorOn;
}

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
					DateTime now = ds3231Rtc.now(); // синхронизация с RTC
					second = now.second();
					minute = now.minute();
					hour = now.hour();
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
				changeBright();
			}

			if (mode == Clock)
			{
				sendTime(hour, minute, second);
#if BL_ENABLED
				FillLEDsFromPaletteColors(blGradientStartIndex++);
#endif
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
		int humidity = digitsDraw[5] * 10 + digitsDraw[6];
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
		if (mode == Clock) {
			indicatorsOff(true);
			isDot = false;
			float temp = dht22Sensor.readTemperature();
			float hum = dht22Sensor.readHumidity();
			float heatIndex = dht22Sensor.computeHeatIndex(temp, hum, false);
			sendTemperature(temp, hum, heatIndex);
			setMode(Temperature);
		}
		else
		{
			if (mode == Temperature) {
				indicatorsOff();
				setMode(Clock);
			}
		}
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
		backLightLeds[i] = ColorFromPalette(RainbowColors_p, colorIndex, brightness, LINEARBLEND);
		colorIndex += 3;
	}
	FastLED.show();
}
#endif

void ConfigurePwm() {
	//// задаем частоту ШИМ на 9 выводе 30кГц
	// TCCR1B = TCCR1B & 0b11111000 | 0x01;
	// analogWrite(DS18B20_PIN, 130);

	TCCR2A = TCCR2A & 0b11111000 | 0x01;
	analogWrite(DS18B20_PIN, 130); // Функция переводит вывод в режим ШИМ и задает для него коэффициент заполнения (ШИМ=50.9% (значение 0 до 255))
}

// 
void setup() {
	ConfigurePwm();

#if !BL_ENABLED
	Serial.begin(9600);
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

	sendTime(hour, minute, second);
	changeBright();

#if BL_ENABLED
	// delay(3000); // power-up safety delay
	FastLED.addLeds<BL_LED_TYPE, BL_PORT, BL_COLOR_ORDER>(backLightLeds, BL_LEDS_QTY).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(BL_BRIGHTNESS);
#endif
}

// главный цикл
void loop() {
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

	buttonsTick();
}
