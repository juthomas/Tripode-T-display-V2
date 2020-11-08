#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include <Button2.h>
#include "esp_adc_cal.h"
#include <WiFiUdp.h>

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL              4   // Display backlight control pin
#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34
#define BUTTON_1            35
#define BUTTON_2            0

#define MOTOR_1				25
#define MOTOR_2				26
#define MOTOR_3				27

const int motorFreq = 5000;
const int motorResolution = 8;
const int motorChannel1 = 0;
const int motorChannel2 = 1;
const int motorChannel3 = 2;

typedef	struct 	s_data_task
{
	int duration;
	int pwm;
	int motor_id;
	TaskHandle_t thisTaskHandler;
}				t_data_task;

bool isTaskActives[3] = {false, false, false};

void task1( void * parameter);
void task2( void * parameter);
void task3( void * parameter);

typedef void(*t_task_func)(void *param);

#define TASK_NUMBER 3
static const t_task_func	g_task_func[TASK_NUMBER] = {
	(t_task_func)task1,
	(t_task_func)task2,
	(t_task_func)task3,
};

t_data_task g_data_task[3];

int vref = 1100;

#define BLACK 0x0000
#define WHITE 0xFFFF
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);


// const char* ssid = "Matrice";
const char* ssid = "Freebox-0E3EAE";
// const char* password =  "QGmatrice";
const char* password =  "taigaest1chien";

WiFiUDP Udp;
unsigned int localUdpPort = 49141;
char incomingPacket[255];
String convertedPacket;
char  replyPacket[] = "Message received";


const char* wl_status_to_string(int ah) {
	switch (ah) {
		case WL_NO_SHIELD: return "WL_NO_SHIELD";
		case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
		case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
		case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
		case WL_CONNECTED: return "WL_CONNECTED";
		case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
		case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
		case WL_DISCONNECTED: return "WL_DISCONNECTED";
		default: return "ERROR NOT VALID WL";
	}
}

const char* eTaskGetState_to_string(int ah) {
	switch (ah) {
		case eRunning: return "eRunning";
		case eReady: return "eReady";
		case eBlocked: return "eBlocked";
		case eSuspended: return "eSuspended";
		case eDeleted: return "eDeleted";
		default: return "ERROR NOT STATE";
	}
}




void showVoltage()
{
	static uint64_t timeStamp = 0;
	if (millis() - timeStamp > 1000) {
		timeStamp = millis();
		uint16_t v = analogRead(ADC_PIN);
		float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
		String voltage = "Voltage :" + String(battery_voltage) + "V";
		Serial.println(voltage);
		tft.fillScreen(TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.drawString(voltage,  tft.width() / 2, tft.height() / 2 );
	}
}

void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	WiFi.begin(ssid, password);


	ledcSetup(motorChannel1, motorFreq, motorResolution);
	ledcSetup(motorChannel2, motorFreq, motorResolution);
	ledcSetup(motorChannel3, motorFreq, motorResolution);
	ledcAttachPin(MOTOR_1, motorChannel1);
	ledcAttachPin(MOTOR_2, motorChannel2);
	ledcAttachPin(MOTOR_3, motorChannel3);



	pinMode(ADC_EN, OUTPUT);
	digitalWrite(ADC_EN, HIGH);
	tft.init();
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);



	if (TFT_BL > 0) {                           // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
			pinMode(TFT_BL, OUTPUT);                // Set backlight pin to output mode
			digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
	}

    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);

	tft.drawString("Connecting", tft.width() / 2, tft.height() / 2);
	uint64_t timeStamp = millis();
	
	Serial.println("Connecting");
	while (WiFi.status() != WL_CONNECTED)
	{
		if(millis() - timeStamp > 10000)
		{
			ESP.restart();
			tft.fillScreen(TFT_BLACK);
			tft.drawString("Restarting", tft.width() / 2, tft.height() / 2);
			
		}
		delay(500);
		Serial.println(wl_status_to_string(WiFi.status()));
	tft.fillScreen(TFT_BLACK);
		
		tft.drawString(wl_status_to_string(WiFi.status()), tft.width() / 2, tft.height() / 2);
	}
	Serial.print("Connected, IP address: ");
	Serial.println(WiFi.localIP());
	Udp.begin(localUdpPort);
	Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}




void drawMotorsActivity()
{
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(0, 0);
	tft.printf("Ssid : %s\n\nIp : %s\n\nUdp port : %d\n\n", ssid,WiFi.localIP().toString().c_str(), localUdpPort);
	uint16_t v = analogRead(ADC_PIN);
	float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
	tft.printf("Voltage : %.2fv\n", battery_voltage);

	
	//uint32_t color1 = TFT_BLUE;
	uint32_t color2 = TFT_WHITE;
	tft.drawCircle(67, 120 , 26, color2);
	tft.drawCircle(27, 190 , 26, color2);
	tft.drawCircle(108, 190 , 26, color2);
	tft.drawLine(15, 167, 40, 150, color2);
	tft.drawLine(40, 150, 40, 120, color2);
	tft.drawLine(93, 120, 93, 150, color2);
	tft.drawLine(93, 150, 120, 167, color2);
	tft.drawLine(100, 215, 67, 195, color2);
	tft.drawLine(67, 195, 35, 215, color2);


	for (int i = 0; i < TASK_NUMBER; i++)
	{
		if (	isTaskActives[i] == true)
		{
			//tft.drawCircle(TFT_WIDTH / 2, TFT_HEIGHT/4 * i + TFT_HEIGHT/4 , 20, TFT_BLUE);
			if (i == 0)
			{
				tft.fillCircle(67, 120 ,  g_data_task[i].pwm / 11, TFT_BLUE);
			}
			else if (i == 1)
			{
				tft.fillCircle(27, 190 ,  g_data_task[i].pwm / 11, TFT_BLUE);
			}
			else if (i == 2)
			{
				tft.fillCircle(108, 190 ,  g_data_task[i].pwm / 11, TFT_BLUE);
			}

			
		}
	}
}

void loop() {
	int packetSize = Udp.parsePacket();
	if (packetSize)
	{
		int len = Udp.read(incomingPacket, 255);
		if (len > 0)
		{
			incomingPacket[len] = 0;
		}
		Serial.printf("UDP packet contents: %s\n", incomingPacket);
		convertedPacket = String(incomingPacket);
		Serial.println(convertedPacket);
		Serial.printf("Debug indexof = P:%d, D:%d, I:%d\n",convertedPacket.indexOf('P'), convertedPacket.indexOf('D'), convertedPacket.indexOf("I"));

		
		
	}
	drawMotorsActivity();
	//Serial.println(".");
	delay(100);
	// put your main code here, to run repeatedly:
}