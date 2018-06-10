/*	Bea Esquivel!
 *	Temperature and Relative Humidity Monitor (gizDuino+ 644)
 *	
 *
 *
 *	ver: b0.07
 *	Changelog:
 *	b0.01 :	added sensor, push button, LCD functions
 *	b0.02 :	added watchdog timer, RTC
 *	b0.03 :	added SD
 *	b0.04 :	added settings
 *	b0.05 :	bools are weird, watch out! started with GSM settings
 *	b0.06 : added power failure detect, fixed weird memory error due to 
 *		  :	Strings? avoid String in the future ._. removed some String 
 *		  :	declarations in some functions
 *	b0.07 :	merged SMStest into main program. added GSM
 */
 
#include <LiquidCrystal.h>
#include "DHT22.h"	// DHT22 sensor library - Developed by Ben Adams - 2011
#include <avr/wdt.h>
#include <GPRS_Shield_Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "RTClib.h"
#include <SD.h>

#define LCD_LED_PIN A4
#define DHTpin 26
#define pushPin 0
#define anaPushPin A0

#define powerDetPin 7

typedef struct {
	
	int errorNum;
	double temp;
	double rh;
	
} dhtdata;

typedef struct { 

	char profileName [15];
	char phoneNum [14];
	
} profile;

dhtdata sensor [6];
profile contact[1];

RTC_DS1307 rtc;

const int chipSelect = 4;

// Initialize DHT sensors at pins DHTpin to DHTpin+3
DHT22 dht_0 (DHTpin+0);
DHT22 dht_1 (DHTpin+1);
DHT22 dht_2 (DHTpin+2);
DHT22 dht_3 (DHTpin+3);
DHT22 dht_4 (DHTpin+4);
DHT22 dht_5 (DHTpin+5);

// Initialize LCD interface pins
LiquidCrystal lcd (23, 22, 7, 6, 3, 2);

// Initialize GSM module
GPRS Gsm(16, 15, 9600);

unsigned long dataInterval = 20000;	// Time interval (in ms) for data collection
unsigned long prevDataTime, curTime = 0;	// For data collection timer
unsigned long powerInterval = 10000, powerAlertInterval = 300000;	// Time interval (ms) for power failure detection
unsigned long prevPowerTime;	// Power fluctuation timer
unsigned long startTime, forcedResetInterval = 10800000;//86400000//300000;	//3hrs
unsigned long scrnChangeTime, scrnTimeout = 300000;
unsigned long prevReportCheckTime, prevTextCheckTime; 
unsigned long reportCheckInterval = 50000;
unsigned long startAlertTime = 0, powerChangeTime = 0;

float aveTemp = 0.0, aveRHum = 0.0;	// Average temperature and RH

float curVolt = 0.0;
const float vPow = 4.7;
const float r2 = 9770;//4940;
const float r1 = 9770;//849;

String strDate, strTime;
char chrDate[] = "YYYY/MM/DD", chrTime[] = "HH:mm:ss";	//grr String!
char curDate[] = "abcd/ef/gh";

char filename[] = "data/YYYY/MMDD.txt"; 

// range == 24,48,72 hrs; period == 60,90,120 mins
unsigned int setting_GsmReportRange = 24, setting_GsmReportPeriod = 60;
char setting_startReportTime[] = "12:00", setting_nextReportDate[] = "2016/12/12";

boolean powerOn = false, startPowerTimer = false, powerLow = false, startPowerAlertTimer = false, powerInit = false;

// LCDBacklight doesn't work when setting_lcdLED initialized as true. ???
boolean setting_SD = false, setting_lcdLED = false, setting_GSM = false, setting_powerAlert = false, setting_sendReport = false;

byte degree [8] = { // Degree character for LCD printing
	B11100,
	B10100,
	B11100,
	B00000,
	B00000,
	B00000,
	B00000,
};

byte check [8] = { // Check character for LCD printing
	B00001,
	B00001,
	B00010,
	B00010,
	B10100,
	B10100,
	B01000,
};

char textTime[] = "hh:mm", textPhoneNum[16], textDateTime[24], textMessage[160]; 
int textRange, textPeriod, textIndex = 0;

int mainscreen = 0, errorsRead = 0;

void setup () {
	
	wdt_disable();
	
	// Set push buttons' analog input pin
	pinMode(anaPushPin, INPUT_PULLUP);	// Internal pull-up needed to avoid board explosion
	
	// LCD setup
	lcd.begin(16, 2);
	lcd.createChar(0, degree);	// Make degree character for printing
	lcd.createChar(1, check);
	pinMode(LCD_LED_PIN, OUTPUT);
	//setting_lcdLED = ~setting_lcdLED;
	if (setting_lcdLED) 
		digitalWrite(LCD_LED_PIN, HIGH);
	else if (!setting_lcdLED)
		digitalWrite(LCD_LED_PIN, LOW);
	
	// Print setup sequence on LCD
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Temp & RH Reader");
	lcd.setCursor(0,1);
	lcd.print("b0.07");
	delay(2000);
	
	// Serial comm setup
	Serial.begin(9600); 
	Serial.println(F("Starting..."));
	
	strDate.reserve(12);
	strTime.reserve(10);
	
	// RTC setup
	Wire.begin();
    rtc.begin();
	if (! rtc.isrunning()) {
		Serial.println(F("RTC is NOT running!"));
		lcd.setCursor(0,1);
		lcd.print("RTC not running");
		rtc.adjust(DateTime(__DATE__, __TIME__));
		//rtc.adjust(DateTime(2014, 1, 1, 3, 0, 0));
	}
	
	// SD card setup
	/* pinMode(10, OUTPUT);
	if (!SD.begin(4)) {
		Serial.println(F("Card failed, or not present"));
		lcd.setCursor(0,1);
		lcd.print("SDcard not found");
		setting_SD = false;
	} else {
		Serial.println(F("card initialized."));
		lcd.setCursor(0,1);
		lcd.print("SDcard found    ");
		setting_SD = true;
	} */
	// SD card setup
	pinMode(10, OUTPUT);
	if (!SD.begin(chipSelect)) {
		Serial.println(F("Card failed, or not present"));
	} else {
		Serial.println(F("card initialized."));
		setting_SD = true;
	}
	
	if (SD.exists("data/")) {
		Serial.println("ok");
	}
	/* if (strcmp(curDate, chrDate) != 0) {
		getTimeDate();
		Serial.println(chrDate);
		getFilename(chrDate);
		strcpy(curDate, chrDate);
		Serial.println(filename);
		
	} 
	
	File testfile = SD.open(filename, FILE_WRITE);
	if (testfile) {
		Serial.println("okok");
	}
	testfile.close(); */
	curTime = millis();
	// GSM setup
	setting_GSM = true;
	while (!Gsm.init()) {
		//Serial.println("wait for gsm");
		if (millis() - curTime >= 20000) {
			setting_GSM = false;
			lcd.print("GSM not found   ");
			break;
		}
	}
	if (setting_GSM != false) {
		Serial.println(F("GSM initialized."));
		setting_GSM = true;
		lcd.print("GSM initialized");
	}
	
	// The DHT22 requires a minimum 2s warm-up.
	delay (2000);
	
	writeSDSettings();
	readSDSettings(); 
	
	lcd.setCursor(0,1);
	lcd.print("Wait for reading");
	
	wdt_enable(WDTO_4S);
	wdt_reset();
	
	Serial.println(F("Setup finished"));
	// Print header line
	printToSerial(1);
	
	
	//Gsm.sendSMS(contact[0].phoneNum, "Hi");
	/* 
	powerOn = true;
	powerInit = true;
 */	
	setting_sendReport = true;
	setting_powerAlert = true;
	
	prevDataTime = millis();
	prevReportCheckTime = millis();
	prevTextCheckTime = millis();
	startTime = millis();
}

void loop () {

	wdt_reset();
	
	int button = 0;
	
	// Check if a button was pressed
	// Change main LCD screen or open controller settings menu
	button = readPushButton();
	
	switch (button) {
		
		case 1:	// Change main LCD screen left
			if (mainscreen == 0) 
				mainscreen = 6;
			else
				mainscreen--;
			
			printToLCD();
			scrnChangeTime = millis();
			break;
		
		case 2:	// Change main LCD screen right
			if (mainscreen == 6) 
				mainscreen = 0;
			else
				mainscreen++;
			
			printToLCD();
			scrnChangeTime = millis();
			break;
			
		case 4:	// Open settings menu
			wdt_reset();
			wdt_disable();	// disable muna si wdt, tsaka na idagdag sa settings
			lcd.begin(16, 2);
			changeSettings();
			lcd.begin(16, 2);
			printToLCD();
			wdt_enable(WDTO_4S);
			break;
	}
	
	// Get current time
	curTime = millis();
	
	if (curTime - prevDataTime >= dataInterval) {
	
		// Save current time as latest time of measurement
		prevDataTime = curTime;
		
		// Get time & date from RTC
		getTimeDate();
		
		// Get sensor readings
		getDHTReadings();
		
		// Show measurements/errors on serial monitor
		printToSerial(0);
		
		// Show average measurements/error warning on LCD
		printToLCD();
		//Serial.println("lcd ok");
		if (SD.exists("DATA/")) {
		Serial.println("ok");
	}
		// Save data to SD
		if (setting_SD == true){
			if (strcmp(curDate, chrDate) != 0) {
				/* // Convert to char
				strDate.toCharArray(chrDate, strDate.length()+1);
				strTime.toCharArray(chrTime, strTime.length()+1); */
				Serial.print("ac");
				getTimeDate();
				getFilename(chrDate);
				strcpy(curDate, chrDate);
				
			}
			
			printToSD();
		
		}
		
	}
	
	// Check if power is on
	curVolt = voltmeter();
	
	// If curVolt is HIGH
	if (curVolt > 5.0 && curVolt < 5.5) {	// DC adapter gives 5.26-5.35 V
		// if powerLow is still true, set to false b/c power is HIGH and start "debounce" (fluctuation timer)
		if (powerLow == true) { 
			powerLow = false;
			prevPowerTime = curTime;
			startPowerTimer = true;
			printToLCD();
		} else {	// else if powerLow is false (power is HIGH)
			if ((startPowerTimer == true) && (curTime - prevPowerTime >= powerInterval)) { 	// if timer is on && timer has passed
			// debounce period has passed, so power prob. stopped fluctuation. set powerOn as true and turn off timer
				powerOn = true;
				startPowerTimer = false;
				powerChangeTime = curTime;
				if (startPowerAlertTimer == false) {
					startPowerAlertTimer = true;
					startAlertTime = curTime;
					powerInit = true;
					Serial.print("start timer: ");
					getTimeDate();
					Serial.println(chrTime);
				}
			}
		}
	// Else if curVolt is LOW	
	} else if (curVolt < 0.50) { 
		// if powerLow is still false, set to true b/c power is now LOW and start fluctuation timer
		if (powerLow == false) { 
			powerLow = true;
			prevPowerTime = curTime;
			startPowerTimer = true;
			printToLCD();
		} else {	// else if powerLow is true 
			if ((startPowerTimer == true) && (curTime - prevPowerTime >= powerInterval)) { 	// if timer is on && timer has passed 
			// set powerOn as false and turn off timer
				powerOn = false;
				startPowerTimer = false;
				powerChangeTime = curTime;
				if (startPowerAlertTimer == false) {
					startPowerAlertTimer = true;
					startAlertTime = curTime;
					powerInit = false;
					Serial.print("start timer: ");
					getTimeDate();
					Serial.println(chrTime);
				}
			}
		}
	}
	
	if ((startPowerAlertTimer == true) && (curTime - startAlertTime >= powerAlertInterval)) {
		// Stop alert timer
		startPowerAlertTimer = false;
		Serial.println(curTime);
		Serial.println(startAlertTime);
		Serial.println(powerAlertInterval);
		
		Serial.print("stop timer: ");
		getTimeDate();
		Serial.println(chrTime);
		
		if (setting_powerAlert == true) {
			wdt_disable();
			// if power was lost and is still off
			if (powerInit == false && powerOn == false) {
				sendPowerAlert(0);
			} else if (powerInit == true && powerOn == true) {
			// if power was restored and still on
				sendPowerAlert(1);
			}
			wdt_enable(WDTO_4S);
		}
	}
	
	if (curTime - startTime >= forcedResetInterval) {
			Serial.println(F("Forcing reset of box"));
			lcd.clear();
			lcd.setCursor(0,0);
			lcd.print("Resetting box");
			while (1) {
				// use the watchdog timer to reset the board
			}
		}
	
	if (setting_GSM == false) {
		setting_powerAlert = false;
		setting_sendReport = false;
	}
	/*	
	if (curTime - prevReportCheckTime >= reportCheckInterval) {
		if (checkReportTimeDate() == true) {
			if (setting_sendReport == true)
				sendReportString(0);
		}
		prevReportCheckTime = curTime;
	}
	
	// Check every 20 b/c checking takes some time to finish, hangs up button pressing
	if (curTime - prevTextCheckTime >= 20000) {
		prevTextCheckTime = curTime;
		textIndex = Gsm.isSMSunread();
		if (textIndex > 0) {
			
			Gsm.readSMS(textIndex, textMessage, 160, textPhoneNum, textDateTime);
			Gsm.deleteSMS(textIndex);
			Serial.println(textMessage);
			parseText();
			//Serial.println("sent msg");
		}
		//Serial.println("check msg");
	}
	
	// LCD screen timeout: if screen stays at sensors for more than 5 mins, return to main screen
	if ((millis() - scrnChangeTime >= scrnTimeout) && (mainscreen != 0)) {
		mainscreen = 0;
		printToLCD();
	} */
}

void printToLCD () { 
// Display measurements, date, time, actions on LCD screen
	
	lcd.begin(16, 2);

	// Change screen accdg to buttons
	switch (mainscreen) {
		
		case 0: // Main screen: time, date, actions, average measurements
			LCD_main();
			break;
		
		case 1: // Sensor 1
			LCD_sensor(0);
			break;
		
		case 2: // Sensor 2
			LCD_sensor(1);
			break;
		
		case 3: // Sensor 3
			LCD_sensor(2);
			break;
		
		case 4: // Sensor 4
			LCD_sensor(3);
			break;
			
		case 5: // Sensor 5
			LCD_sensor(4);
			break;
		
		case 6: // Sensor 6
			LCD_sensor(5);
			break;	
	}
}

void LCD_main () { 
// Main LCD screen with the date and time and averages
	
	/*	MM-DD hh:mm CHIE
		Temp:##.##°RH:##
	*/
	
	// Convert floats to string for proper decimal point placement
	char ATemp[] = "00.0", ARH[] = "00";
	if (abs(aveTemp) >= 100) {
		dtostrf(aveTemp, 4, 0, ATemp);
	}
	else if (abs(aveTemp) < 10) {
		dtostrf(aveTemp, 4, 2, ATemp);
	}
	else {
		dtostrf(aveTemp, 4, 1, ATemp);
	}
	
	if (aveRHum < 99.5)
		dtostrf(aveRHum, 2, 0, ARH);
	
	// First row
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(strDate.substring(5, 10));
	lcd.print(" ");
	lcd.print(strTime.substring(0, 5));
	lcd.print(" PWR");
	
	if (startPowerTimer == true) {
		lcd.print("?");
	} else {
		if (powerOn == true)
			lcd.write(byte(1));
		else if (powerOn == false)
			lcd.print("x");
	}
	
	// Print second row
	lcd.setCursor(0,1);
	if (errorsRead == 6) {	// All sensors returned an error
		lcd.print("All snsrs errord");
				
	} else {	// At least one sensor did not error
		lcd.print("Temp:");
		lcd.print(ATemp);
		lcd.write(byte(0));
		lcd.print("RH:");
		if (aveRHum >= 99.5) {
			lcd.print("100");
		}
		else {
			lcd.print(ARH);
			lcd.print("%");
		}
	}
}

void LCD_sensor (int sensorNum) { 
// Sensor LCD screen: shows temp and RH OR error msg per sensor
	
	lcd.clear();
	lcd.setCursor(0,0);
	
	char strTemp[] = "00.0", strRH[] = "00";
	
	if (sensor[sensorNum].errorNum == 0) {	// No error
		/*	< Sensor #     >
			Temp:##.##°RH:##
		*/
		
		// Convert floats to string for proper decimal point placement
		if (abs(sensor[sensorNum].temp) >= 100) {
			dtostrf(sensor[sensorNum].temp, 4, 0, strTemp);	
		}
		else if (abs(sensor[sensorNum].temp) < 10) {
			dtostrf(sensor[sensorNum].temp, 4, 2, strTemp);	
		}
		else {
			dtostrf(sensor[sensorNum].temp, 4, 1, strTemp);	
		} 
		
		if (sensor[sensorNum].rh < 99.5)
			dtostrf(sensor[sensorNum].rh, 2, 0, strRH);
		
		lcd.print("< Sensor ");
		lcd.print(sensorNum+1);
		lcd.print("     >");
		lcd.setCursor(0,1);
		lcd.print("Temp:");
		lcd.print(strTemp);
		/* Serial.println(sensor[sensorNum].temp);
		Serial.println(strTemp); */
		lcd.write(byte(0));
		lcd.print("RH:");
		if (sensor[sensorNum].rh >= 99.5) {
			lcd.print("100");
		}
		else {
			lcd.print(strRH);
			lcd.print("%");
		}
		
	
	} else if (sensor[sensorNum].errorNum == 1) {	// Checksum error
		/*	< Snsr # chksm >
			Temp:##.##°RH:##
		*/
		
		// Convert floats to string for proper decimal point placement
		if (abs(sensor[sensorNum].temp) >= 100) {
			dtostrf(sensor[sensorNum].temp, 4, 0, strTemp);	
		}
		else if (abs(sensor[sensorNum].temp) < 10) {
			dtostrf(sensor[sensorNum].temp, 4, 2, strTemp);	
		}
		else {
			dtostrf(sensor[sensorNum].temp, 4, 1, strTemp);	
		}
			
		if (sensor[sensorNum].rh < 99.5)	
			dtostrf(sensor[sensorNum].rh, 2, 0, strRH);
		
		lcd.print("< Snsr ");
		lcd.print(sensorNum+1);
		lcd.print(" chksm >");
		lcd.setCursor(0,1);
		lcd.print("Temp:");
		lcd.print(strTemp);
		lcd.write(byte(0));
		lcd.print("RH:");
		if (sensor[sensorNum].rh >= 99.5) {	//add rounding fn; lcd.print rounds rh
			lcd.print("100");
		}
		else {
			lcd.print(strRH);
			lcd.print("%");
		}
	
	} else {	// Error num: 2/3/4/5/6/7
		/*	< Snsr # error >
			error message :)
		*/
		lcd.print("< Snsr ");
		lcd.print(sensorNum+1);
		lcd.print(" error >");
		lcd.setCursor(0,1);
		
		// Print error message
		switch (sensor[sensorNum].errorNum) {
			
			case 2:
				lcd.print("Bus Hung");
				break;
			
			case 3:
				lcd.print("Not Present");
				break;
			
			case 4:
				lcd.print("ACK Time Out");
				break;
			
			case 5:
				lcd.print("Sync Timeout");
				break;
			
			case 6:
				lcd.print("Data Timeout");
				break;
			
			case 7:
				lcd.print("Polled too quick");
				break;
		
		}
	}
}

void printToSerial (int opt) { 
// Print measurements, average measurements, and actions to serial monitor
	//wdt_disable();
	// For printing header line; used in setup() only
	if (opt == 1) {
		Serial.println(F("Date\t\tTime\t\tTemp1\tRH1\tError1\tTemp2\tRH2\tError2\tTemp3\tRH3\tError3\tTemp4\tRH4\tError4\tTemp5\tRH5\tError5\tTemp6\tRH6\tError6\tAvgTemp\tAvgRH\tPower"));
		return;
	}
	char chrData[180];
	char cTemp[] = "00.00", cRh[] = "00.00";
	//Serial.print("ba");
	//strData = strDate + "\t" + strTime + "\t";
	//Serial.println(strData);
	
	strcpy(chrData, chrDate);
	strcat(chrData, "\t");
	strcat(chrData, chrTime);
	strcat(chrData, "\t");
	
	for (int i = 0; i < 6; i++) {
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
		// If no error or checksum error
			if (abs(sensor[i].temp) >= 100) {
				dtostrf(sensor[i].temp, 5, 1, cTemp);	
			}
			else {
				dtostrf(sensor[i].temp, 5, 2, cTemp);	
			}
			if (sensor[i].rh == 100)
				dtostrf(sensor[i].rh, 5, 1, cRh);
			else
				dtostrf(sensor[i].rh, 5, 2, cRh);
			
			/* strData += cTemp;
			strData += "\t";
			strData += cRh;
			strData += "\t";
			 */
			
			strcat(chrData, cTemp);
			strcat(chrData, "\t");
			strcat(chrData, cRh);
			strcat(chrData, "\t");
			
		} else {
		// If error
			strcat(chrData, "-----\t-----\t");		
		}
		
		//strData = strData + sensor[i].errorNum + " ";
		char cError[2];
		
		strcat(chrData, itoa(sensor[i].errorNum, cError, 10));
		strcat(chrData, " ");
		
		switch (sensor[i].errorNum) {
			case 0:
				//strData = strData + "none ";
				strcat(chrData, "none ");
				break;
			case 1:
				//strData = strData + "chksm";
				strcat(chrData, "chksm");
				break;
			case 2:
				//strData = strData + "bus  ";
				strcat(chrData, "bus  ");
				break;
			case 3:
				//strData = strData + "n.p. ";
				strcat(chrData, "n.p. ");
				break;
			case 4:
				//strData = strData + "ackTO";
				strcat(chrData, "ackTO");
				break;
			case 5:
				//strData = strData + "syncT";
				strcat(chrData, "syncT");
				break;
			case 6:
				//strData = strData + "dataT";
				strcat(chrData, "dataT");
				break;
			case 7:
				//strData = strData + "quick";
				strcat(chrData, "quick");
				break;
		}
		//Serial.print(i);
		//strData = strData + "\t";	
		strcat(chrData, "\t");
	}
	//Serial.print("bc");
	//Serial.println(chrData);
	if (errorsRead == 6) {
		//strData += "-----\t-----\t";	
		strcat(chrData, "-----\t-----\t");
	} else {
		if (abs(aveTemp) >= 100) {
				dtostrf(aveTemp, 5, 0, cTemp);	
		}
		else {
			dtostrf(aveTemp, 5, 2, cTemp);	
		}
		if (aveRHum == 100)
			dtostrf(aveRHum, 5, 1, cRh);
		else
			dtostrf(aveRHum, 5, 2, cRh);
		
		strcat(chrData, cTemp);
		strcat(chrData, "\t");
		strcat(chrData, cRh);
		strcat(chrData, "\t");
		
		/* strData += cTemp;
		strData += "\t";
		strData += cRh;
		strData += "\t";
		 */	
	} 
	
	if (powerOn == true) {
		strcat(chrData, "ON\n");
		
		//strData += "ON\n";
	}
	else if (powerOn == false) {
		strcat(chrData, "OFF\n");
		
		//strData += "OFF\n";
	} 
	//String temp = makeDataString();
	Serial.println(chrData);
	delay(200);	// DO NOT REMOVE MAGIC DELAY
 	//wdt_enable(WDTO_4S);
}

void printToSD () { 
	Serial.print("aa");
	wdt_reset();
	
	
	//Serial.print("ab");
	 
	Serial.print(F("Opening "));
	delay(200);
	Serial.println(filename);
	
	delay(200);
	//wdt_disable();
	File datafile = SD.open(filename, FILE_WRITE);
	delay(200);
	Serial.print("ac");
	// Check if SD is available
	/* if (datafile) { // if available,
			
		// Write data string to SD
		//datafile.print(makeDataString());
			
		char chrData[180];
		char cTemp[] = "00.00", cRh[] = "00.00";
		//Serial.print("ba");
		//strData = strDate + "\t" + strTime + "\t";
		//Serial.println(strData);
		
		strcpy(chrData, chrDate);
		strcat(chrData, "\t");
		strcat(chrData, chrTime);
		strcat(chrData, "\t");
		
		for (int i = 0; i < 6; i++) {
			if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			// If no error or checksum error
				if (abs(sensor[i].temp) >= 100) {
					dtostrf(sensor[i].temp, 5, 1, cTemp);	
				}
				else {
					dtostrf(sensor[i].temp, 5, 2, cTemp);	
				}
				if (sensor[i].rh == 100)
					dtostrf(sensor[i].rh, 5, 1, cRh);
				else
					dtostrf(sensor[i].rh, 5, 2, cRh);
			
				
				strcat(chrData, cTemp);
				strcat(chrData, "\t");
				strcat(chrData, cRh);
				strcat(chrData, "\t");
				
			} else {
			// If error
				strcat(chrData, "-----\t-----\t");		
			}
			
			//strData = strData + sensor[i].errorNum + " ";
			char cError[2];
			
			strcat(chrData, itoa(sensor[i].errorNum, cError, 10));
			strcat(chrData, " ");
			
			switch (sensor[i].errorNum) {
				case 0:
					//strData = strData + "none ";
					strcat(chrData, "none ");
					break;
				case 1:
					//strData = strData + "chksm";
					strcat(chrData, "chksm");
					break;
				case 2:
					//strData = strData + "bus  ";
					strcat(chrData, "bus  ");
					break;
				case 3:
					//strData = strData + "n.p. ";
					strcat(chrData, "n.p. ");
					break;
				case 4:
					//strData = strData + "ackTO";
					strcat(chrData, "ackTO");
					break;
				case 5:
					//strData = strData + "syncT";
					strcat(chrData, "syncT");
					break;
				case 6:
					//strData = strData + "dataT";
					strcat(chrData, "dataT");
					break;
				case 7:
					//strData = strData + "quick";
					strcat(chrData, "quick");
					break;
			}
			//Serial.print(i);
			//strData = strData + "\t";	
			strcat(chrData, "\t");
		}
		//Serial.print("bc");
		Serial.println(chrData);
		if (errorsRead == 6) {
			//strData += "-----\t-----\t";	
			strcat(chrData, "-----\t-----\t");
		} else {
			if (abs(aveTemp) >= 100) {
					dtostrf(aveTemp, 5, 0, cTemp);	
			}
			else {
				dtostrf(aveTemp, 5, 2, cTemp);	
			}
			if (aveRHum == 100)
				dtostrf(aveRHum, 5, 1, cRh);
			else
				dtostrf(aveRHum, 5, 2, cRh);
			
			strcat(chrData, cTemp);
			strcat(chrData, "\t");
			strcat(chrData, cRh);
			strcat(chrData, "\t");
			
		} 
		
		if (powerOn == true) {
			strcat(chrData, "ON\n");
			
			//strData += "ON\n";
		}
		else if (powerOn == false) {
			strcat(chrData, "OFF\n");
			
			//strData += "OFF\n";
		} 
		//String temp = makeDataString();
		Serial.println(chrData);
		delay(200);	// DO NOT REMOVE MAGIC DELAY
			datafile.print(chrData);
			datafile.close();
			
			Serial.println(F("Saved to SD card"));
	}
	else { 
		Serial.print(F("Error opening "));
		Serial.println(filename);
	}  */
	datafile.close();
	//wdt_enable(WDTO_4S);

}

/* String makeDataString () { 
	
	char chrData[180];
	char cTemp[] = "00.00", cRh[] = "00.00";
	//Serial.print("ba");
	//strData = strDate + "\t" + strTime + "\t";
	//Serial.println(strData);
	
	strcpy(chrData, chrDate);
	strcat(chrData, "\t");
	strcat(chrData, chrTime);
	strcat(chrData, "\t");
	
	for (int i = 0; i < 6; i++) {
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
		// If no error or checksum error
			if (abs(sensor[i].temp) >= 100) {
				dtostrf(sensor[i].temp, 5, 1, cTemp);	
			}
			else {
				dtostrf(sensor[i].temp, 5, 2, cTemp);	
			}
			if (sensor[i].rh == 100)
				dtostrf(sensor[i].rh, 5, 1, cRh);
			else
				dtostrf(sensor[i].rh, 5, 2, cRh);
			
			
			strcat(chrData, cTemp);
			strcat(chrData, "\t");
			strcat(chrData, cRh);
			strcat(chrData, "\t");
			
		} else {
		// If error
			strcat(chrData, "-----\t-----\t");		
		}
		
		//strData = strData + sensor[i].errorNum + " ";
		char cError[2];
		
		strcat(chrData, itoa(sensor[i].errorNum, cError, 10));
		strcat(chrData, " ");
		
		switch (sensor[i].errorNum) {
			case 0:
				//strData = strData + "none ";
				strcat(chrData, "none ");
				break;
			case 1:
				//strData = strData + "chksm";
				strcat(chrData, "chksm");
				break;
			case 2:
				//strData = strData + "bus  ";
				strcat(chrData, "bus  ");
				break;
			case 3:
				//strData = strData + "n.p. ";
				strcat(chrData, "n.p. ");
				break;
			case 4:
				//strData = strData + "ackTO";
				strcat(chrData, "ackTO");
				break;
			case 5:
				//strData = strData + "syncT";
				strcat(chrData, "syncT");
				break;
			case 6:
				//strData = strData + "dataT";
				strcat(chrData, "dataT");
				break;
			case 7:
				//strData = strData + "quick";
				strcat(chrData, "quick");
				break;
		}
		//Serial.print(i);
		//strData = strData + "\t";	
		strcat(chrData, "\t");
	}
	//Serial.print("bc");
	if (errorsRead == 6) {
		//strData += "-----\t-----\t";	
		strcat(chrData, "-----\t-----\t");
	} else {
		if (abs(aveTemp) >= 100) {
				dtostrf(aveTemp, 5, 0, cTemp);	
		}
		else {
			dtostrf(aveTemp, 5, 2, cTemp);	
		}
		if (aveRHum == 100)
			dtostrf(aveRHum, 5, 1, cRh);
		else
			dtostrf(aveRHum, 5, 2, cRh);
		
		strcat(chrData, cTemp);
		strcat(chrData, "\t");
		strcat(chrData, cRh);
		strcat(chrData, "\t");
		
		
	} 
//Serial.print("bd");
	if (powerOn == true) {
		strcat(chrData, "ON\n\0");
		
		//strData += "ON\n";
	}
	else if (powerOn == false) {
		strcat(chrData, "OFF\n\0");
		
		//strData += "OFF\n";
	}
	
	return String(chrData);
} */

void getFilename (char *date) { 
// Gets new filename & directory for data	
// New file per week; new directory per year
// data/YYYY/MMDD.txt
	
	wdt_reset();
	
	char chrDay[] = "DD";
	for (int i = 0; i < 2; i++)
		chrDay[i] = date[i+8];
	int intDay = atoi(chrDay);
	
	char chrMonth[] = "MM";
	for (int i = 0; i < 2; i++) 
		chrMonth[i] = date[i+5];
	int intMonth = atoi(chrMonth);
	
	char chrYear[] = "YYYY";
	for (int i = 0; i < 4; i++)
		chrYear[i] = date[i];
	int intYear = atoi(chrYear);
	Serial.println("sd");
	delay(1000);
	if (SD.exists("DATA/")) {
		Serial.println("ok");
	}
	Serial.println("sd");
	// Look for existing file to write to
	// Check files in data folder
	if (SD.exists("data/")) {
		Serial.println("sd");
		// Look for same year as today
		char fyear[] = "data/YYYY";
		for (int i = 0; i < 4; i++) 
			fyear[i+5] = chrYear[i];
		
		if (SD.exists(fyear)) {
			// Search for file to use
			char ftemp[] = "data/YYYY/MMDD.txt", chrTDay[] = "DD";
			Serial.println("sd");
			for (int i = 0; i < 4; i++) 
				ftemp[i+5] = date[i];
			for (int i = 0; i < 2; i++)
				ftemp[i+10] = date[i+5];
			for (int i = 0; i < 2; i++)
				ftemp[i+12] = date[i+8];
			
			
			// If today is <=06, check in month before
			if (intDay <= 6) {
				
				char chrTMonth[] = "MM", chrTYear[] = "YYYY";
				bool isFileFound = false;

				if (intMonth == 1) {
					// Check in previous year
					sprintf(chrTYear, "%04d", intYear-1);
					sprintf(chrTMonth, "12");
				}
				else {
					strcpy(chrTYear, chrYear);
					sprintf(chrTMonth, "%02d", intMonth-1);
				}
				
				int intTDay;
				
				if (intMonth == 1) 
					intTDay = daysInMonth(12, intYear-1) - (6 - intDay);
				else
					intTDay = daysInMonth(intMonth-1, intYear) - (6 - intDay);
				
				for (int i = 0; i < ((6-intDay)+1); i++) {
					
					sprintf(chrTDay, "%02d", intTDay+i);
					
					for (int i = 0; i < 4; i++) 
						ftemp[i+5] = chrTYear[i];
					for (int i = 0; i < 2; i++)
						ftemp[i+10] = chrTMonth[i];
					for (int i = 0; i < 2; i++)
						ftemp[i+12] = chrTDay[i];
					//Serial.println(ftemp);
					if (SD.exists(ftemp)) {
						// save to ftemp
						strcpy(filename, ftemp);
						isFileFound = true;
						break;
					} 
					
				}
				
				if (isFileFound == false) {
					if (SD.exists(ftemp)) {
						// save to ftemp
						strcpy(filename, ftemp);
					} 
					else {
						for (int i = 1; i < (intDay + 1); i++) {
							
							sprintf(chrTDay, "%02d", i);
							
							for (int i = 0; i < 4; i++) 
								ftemp[i+5] = date[i];
							for (int i = 0; i < 2; i++)
								ftemp[i+10] = date[i+5];
							for (int i = 0; i < 2; i++)
								ftemp[i+12] = chrTDay[i];
							//Serial.println(ftemp);
							if (SD.exists(ftemp)) {
								// save to ftemp
								strcpy(filename, ftemp);
								break;
							} 
							else {
								if (i == intDay) { 
									makeTxt();
								} 
							}
						}
					}
				}
				
			} 
			else {
				// Look for today in (today-6, today)
				int intTDay = intDay - 6;
				
				for (int i = 0; i < 7; i++) {
					
					sprintf(chrTDay, "%02d", intTDay+i);
					
					for (int i = 0; i < 4; i++) 
						ftemp[i+5] = date[i];
					for (int i = 0; i < 2; i++)
						ftemp[i+10] = date[i+5];
					for (int i = 0; i < 2; i++)
						ftemp[i+12] = chrTDay[i];
					//Serial.println(ftemp);
					if (SD.exists(ftemp)) {
						// save to ftemp
						strcpy(filename, ftemp);
						break;
					} 
					else {
						if (SD.exists(ftemp)) {
							// save to ftemp
							strcpy(filename, ftemp);
						} 
						else {
							if (i == 6) { 
								makeTxt();
							}
						} 
					}
					
				}
			}
			
		} else {
			// Make year directory and file
			Serial.println(F("Year directory does not exist"));
			
			char dir[] = "data/YYYY";
			for (int i = 0; i < 4; i++) 
				dir[i+5] = date[i];
			Serial.print(F("making "));
			Serial.println(dir);
			SD.mkdir(dir);

			if(!SD.exists(dir))
				Serial.println(F("error creating directory"));
			else
				Serial.println(F("directory making ok"));

			makeTxt();
		}
		
	} else {
		Serial.println(F("data/ does not exist"));
		
		char dir[] = "data/YYYY";
		for (int i = 0; i < 4; i++) 
			dir[i+5] = date[i];
		Serial.print(F("making "));
		Serial.println(dir);
		SD.mkdir(dir);
		
		if(!SD.exists(dir))
			Serial.println(F("error creating directory"));
		else
			Serial.println(F("directory making ok"));
		
		makeTxt();
	}
	//Serial.println("sd");
	// Save current date for checking if new date
	
}

void makeTxt () { 
// Make directory & file
	for (int i = 0; i < 4; i++) 
		filename[i+5] = chrDate[i];
	for (int i = 0; i < 2; i++)
		filename[i+10] = chrDate[i+5];
	for (int i = 0; i < 2; i++)
		filename[i+12] = chrDate[i+8];
	
	Serial.print(F("file does not exist, making "));
	Serial.println(filename);
	
	File dataFile = SD.open(filename, FILE_WRITE);
	if (dataFile) {
		Serial.println(F("file making ok"));
		dataFile.println(F("Date\t\tTime\t\tTemp1 \tRH1\t\tError1\tTemp2 \tRH2\t\tError2\tTemp3 \tRH3\t\tError3\tTemp4 \tRH4\t\tError4\tTemp5 \tRH5\t\tError5\tTemp6 \tRH6\t\tError6\tAvgTemp\tAvgRH\tPower"));
		
	}
	
	dataFile.close();
	
	if(!SD.exists(filename))
		Serial.println(F("error creating file"));
	else
		Serial.println(F("file making ok"));
}

void getDHTReadings () { 
// Get readings from DHT sensors; save to sensor[]
	
	float totalTemp = 0, totalRh = 0;
	
	errorsRead = 0;
	
	// Declare error codes for DHT sensors
	DHT22_ERROR_t errorCode0;
	DHT22_ERROR_t errorCode1;
	DHT22_ERROR_t errorCode2;
	DHT22_ERROR_t errorCode3;
	DHT22_ERROR_t errorCode4;
	DHT22_ERROR_t errorCode5;
	
	// Check sensors for errors
	errorCode0 = dht_0.readData();
	errorCode1 = dht_1.readData();
	errorCode2 = dht_2.readData();
	errorCode3 = dht_3.readData();
	errorCode4 = dht_4.readData();
	errorCode5 = dht_5.readData();

	switch(errorCode0) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[0].temp = dht_0.getTemperatureC();
			sensor[0].rh = dht_0.getHumidity();	
			sensor[0].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[0].temp = dht_0.getTemperatureC();
			sensor[0].rh = dht_0.getHumidity();	
			sensor[0].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[0].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[0].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[0].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[0].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[0].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[0].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	switch(errorCode1) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[1].temp = dht_1.getTemperatureC();
			sensor[1].rh = dht_1.getHumidity();	
			sensor[1].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[1].temp = dht_1.getTemperatureC();
			sensor[1].rh = dht_1.getHumidity();	
			sensor[1].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[1].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[1].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[1].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[1].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[1].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[1].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	switch(errorCode2) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[2].temp = dht_2.getTemperatureC();
			sensor[2].rh = dht_2.getHumidity();	
			sensor[2].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[2].temp = dht_2.getTemperatureC();
			sensor[2].rh = dht_2.getHumidity();	
			sensor[2].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[2].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[2].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[2].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[2].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[2].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[2].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	switch(errorCode3) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[3].temp = dht_3.getTemperatureC();
			sensor[3].rh = dht_3.getHumidity();	
			sensor[3].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[3].temp = dht_3.getTemperatureC();
			sensor[3].rh = dht_3.getHumidity();	
			sensor[3].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[3].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[3].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[3].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[3].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[3].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[3].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	switch(errorCode4) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[4].temp = dht_4.getTemperatureC();
			sensor[4].rh = dht_4.getHumidity();	
			sensor[4].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[4].temp = dht_4.getTemperatureC();
			sensor[4].rh = dht_4.getHumidity();	
			sensor[4].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[4].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[4].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[4].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[4].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[4].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[4].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	switch(errorCode5) {
	  
		case DHT_ERROR_NONE:	// No error detected
			// Get measurements
			sensor[5].temp = dht_5.getTemperatureC();
			sensor[5].rh = dht_5.getHumidity();	
			sensor[5].errorNum = 0;
			break;	
		case DHT_ERROR_CHECKSUM:
			//Serial.print("check sum error");
			// Get measurements
			sensor[5].temp = dht_5.getTemperatureC();
			sensor[5].rh = dht_5.getHumidity();	
			sensor[5].errorNum = 1;	
			break;
		case DHT_BUS_HUNG:
			//Serial.println("BUS Hung ");
			sensor[5].errorNum = 2;
			errorsRead++;
			break;
		case DHT_ERROR_NOT_PRESENT:
			//Serial.println("Not Present ");
			sensor[5].errorNum = 3;
			errorsRead++;
			break;
		case DHT_ERROR_ACK_TOO_LONG:
			//Serial.println("ACK time out ");
			sensor[5].errorNum = 4;
			errorsRead++;
			break;
		case DHT_ERROR_SYNC_TIMEOUT:
			//Serial.println("Sync Timeout ");
			sensor[5].errorNum = 5;
			errorsRead++;
			break;
		case DHT_ERROR_DATA_TIMEOUT:
			//Serial.println("Data Timeout ");
			sensor[5].errorNum = 6;
			errorsRead++;
			break;
		case DHT_ERROR_TOOQUICK:
			//Serial.println("Polled to quick ");
			sensor[5].errorNum = 7;
			errorsRead++;
			break;		
	}
	
	for (int i = 0; i < 6; i++) {
		
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			totalTemp += sensor[i].temp;
			totalRh += sensor[i].rh;
		} 

	}
	
	aveTemp = totalTemp / (6 - errorsRead);
	aveRHum = totalRh / (6 - errorsRead);
	
	/* sensor[0].temp = 33.9;
	sensor[0].rh = 71.8;	
	sensor[0].errorNum = 0;
	sensor[1].temp = 32.9;
	sensor[1].rh = 95.7;	
	sensor[1].errorNum = 0;
	sensor[2].temp = 28.9;
	sensor[2].rh = 70.3;	
	sensor[2].errorNum = 0;
	sensor[3].temp = 30.9;
	sensor[3].rh = 81.0;	
	sensor[3].errorNum = 0; */
	
	/* for (int i = 0; i < 4; i++) {
		
		if (sensor[i].errorNum == 0 || sensor[i].errorNum == 1) {
			Serial.print(sensor[i].cTemp);
			Serial.print("\t");
			Serial.print(sensor[i].cRh);
			Serial.print("\t");
		} else {
			Serial.print("-\t-\t");		
		}

	}
	
	Serial.print(errorsRead);
	Serial.print("\t");
	if (errorsRead == 4) {
		Serial.println("-\t-");	
	}
	else {
		Serial.print(aveTemp);
		Serial.print("\t");
		Serial.println(aveRHum);
	} */
}

int getAnalogValue (int value) { 
// Determine button pressed
	
	// Return an int depending on value read due to different resistances
	if (value > 900) {	// No button pressed
		return 0;
	} else if (value > 420 && value < 470) {	//450
		return 1;
	} else if (value > 325 && value < 375) {	//355
		return 2;
	} else if (value > 195 && value < 245) {	//218
		return 3;
	} else if (value < 20) {	//11
		return 4;
	}
}

int readPushButton () { 
// Returns button pressed; if none pressed, returns 0; w/ debounce
	
	int value = 0;
	long dbPeriod = 150;	// debounce period in ms
	long lastDbTime = 0; 
	int lastValue, justpressed = 0;
	
	while (1) {
		
		wdt_reset();
		
		// Get button pressed
		value = getAnalogValue(analogRead(pushPin));
	
		// If different from last value, start debounce timer
		if (value != lastValue) {
			justpressed = 1;	// Button was pressed
			lastDbTime = millis();
		}
		
		// If debounce period has passed and button value is stable
		if (((millis() - lastDbTime) > dbPeriod) && (value == lastValue)) {
			
			// Return value of button just once
			if (justpressed == 1) {
				justpressed = 0;
				return value;
			} else {

				return 0;
			}

		}
		
		// Save read button value for loop
		lastValue = value;
	}

}


void getTimeDate () {
		
	DateTime now = rtc.now();
	
	// Make date string
	strDate = String(now.year(), DEC);
	strDate += "/";
	
	if (now.month() < 10) { 
		strDate += "0";
		strDate += String(now.month(), DEC);
	} else
		strDate += String(now.month(), DEC);
	
	strDate += "/";
	
	if (now.day() < 10) { 
		strDate += "0";
		strDate += String(now.day(), DEC);
	} else
		strDate += String(now.day(), DEC);

	// Make time string
	if (now.hour() < 10) { 
		strTime = "0";
		strTime += String(now.hour(), DEC);
	} else
		strTime = String(now.hour(), DEC);
	
	strTime += ":";
	
	if (now.minute() < 10) { 
		strTime += "0";
		strTime += String(now.minute(), DEC);
	} else
		strTime += String(now.minute(), DEC);
	
	strTime += ":";
	
	if (now.second() < 10) { 
		strTime += "0";
		strTime += String(now.second(), DEC);
	} else
		strTime += String(now.second(), DEC);
	
	/*  strTime = "12:00:33";
	strDate = "2016/12/07";  */
	// Convert to char
	strDate.toCharArray(chrDate, strDate.length()+1);
	strTime.toCharArray(chrTime, strTime.length()+1);
	
}

int daysInMonth (int month, int year) { 
	
	switch (month) {
		case 1: return 31;
		case 2: 
			if (year % 4 == 0)
				return 29;
			else 
				return 28;
		case 3: return 31;
		case 4: return 30;
		case 5: return 31;
		case 6: return 30;
		case 7: return 31;
		case 8: return 31;
		case 9: return 30;
		case 10: return 31;
		case 11: return 30;
		case 12: return 31;
	}
}

void setRTC (int type, int fnl1, int fnl2, int fnl3) { 
// type == 0 for date, 1 for time	
	
	int fnlA, fnlB, fnlC;
	String A_s = "", B_s = "", C_s = "";
	
	if (type == 0) { 
		// get current time
		
		//strTime = hh:mm:ss
		A_s = strTime.substring(0, 2);
		B_s = strTime.substring(3, 5);
		C_s = strTime.substring(6, 8);
		
		fnlA = A_s.toInt();
		fnlB = B_s.toInt();
		fnlC = C_s.toInt();
		
		// YYYY, MM, DD, hh, mm, ss
		rtc.adjust(DateTime(fnl1, fnl2, fnl3, fnlA, fnlB, fnlC));
		
	} else if (type == 1) { 
		// get current date
		
		//strDate = YYYY/MM/DD
		A_s = strDate.substring(0, 4);
		B_s = strDate.substring(5, 7);
		C_s = strDate.substring(8, 10);
		
		fnlA = A_s.toInt();
		fnlB = B_s.toInt();
		fnlC = C_s.toInt();
		
		// YYYY, MM, DD, hh, mm, ss
		rtc.adjust(DateTime(fnlA, fnlB, fnlC, fnl1, fnl2, fnl3));
	}
}

void changeSettings () { 
// Change controller settings
// LCD updates only when buttons are pressed due to screen flickering (...which means that time setting screen only updates when buttons are pressed ): )

	int menu = 0, button = 5;	// button is initially zero so menu switch-case will run once
	
	LCD_settings(menu, 0);
	
	while (1) {
		wdt_reset();
		if (button != 0) {
			
			// Press menu button to exit menu
			if (button == 4) {
				
				return;
			}
			
			// Left and right screens
			if (button == 1) {
				if (menu == 0) {
					if (setting_GSM) 
						menu = 12;
					else if (!setting_GSM)
						menu = 5;
				}
				else
					menu--;
			} 
			else if (button == 2) {
				if (setting_GSM && menu == 12) { 
					Serial.println("menu == 12");
					menu = 0;
				} 
				else if (!setting_GSM && menu == 5) { 
					Serial.println("menu == 5");
					menu = 0;
				}
				else
					menu++;
			}
		
			if (menu == 5 || menu == 6)
				getTimeDate();
			
			Serial.print("menu: ");
			Serial.print(menu);
			LCD_settings(menu, 0);
		}
		
		// Change screen
		switch (menu) {
			case 0:	// LCD LED On/Off
			
				if (button == 3) {
					setting_lcdLED = ~setting_lcdLED;
					
					if (setting_lcdLED) 
						digitalWrite(LCD_LED_PIN, HIGH);
					else if (!setting_lcdLED) {
						digitalWrite(LCD_LED_PIN, LOW);
						return;
					}
					
					LCD_settings(menu, 0);
				} 
				break;
			
			
			case 1:	// Save to SD Card On/Off
				
				if (button == 3) {
					setting_SD = ~setting_SD;
					LCD_settings(menu, 0);
				}
				break;
			
			
			case 2:  // Date
			{
				
				if (button == 3) {
				// Start changing date	
					wdt_reset();
					getTimeDate();	//get latest date
					LCD_settings(menu, 0);
					
					// Get current date, convert to ints
					int iniM, iniD, iniY, fnlM, fnlD, fnlY;
					String M_s = "", D_s = "", Y_s = "";
					
					//strDate = YYYY/MM/DD
					Y_s = strDate.substring(0, 4);
					M_s = strDate.substring(5, 7);
					D_s = strDate.substring(8, 10);
					
					iniY = Y_s.toInt();
					iniM = M_s.toInt();
					iniD = D_s.toInt();
					
					// Choose new date
					fnlY = changeTD('Y', menu, 1, iniY, 1900, 2100);
					fnlM = changeTD('M', menu, 2, iniM, 1, 12);
					// Check if current day is valid for month
					if (iniD > daysInMonth(fnlM, fnlY)) 
						iniD = daysInMonth(fnlM, fnlY);
					fnlD = changeTD('D', menu, 3, iniD, 1, daysInMonth(fnlM, fnlY));
					
					// Set new clock
					setRTC(0, fnlY, fnlM, fnlD);
					
					// Show new date
					getTimeDate();	//get latest date
					LCD_settings(menu, 0);
				}
				
				//Serial.println("menu 5");
				break;
			}
			
			case 3: // Time
			{

				if (button == 3) {
				// Start changing time	
					wdt_reset();
					getTimeDate();	//get latest time
					LCD_settings(menu, 0);
					
					// Get current time, convert to ints
					int inih, inim, inis, fnlh, fnlm, fnls;
					String h_s = "", m_s = "", s_s = "";
					
					//strTime = hh:mm:ss
					h_s = strTime.substring(0, 2);
					m_s = strTime.substring(3, 5);
					s_s = strTime.substring(6, 8);
					
					inih = h_s.toInt();
					inim = m_s.toInt();
					inis = s_s.toInt();
					
					// Choose new time
					fnlh = changeTD('h', menu, 1, inih, 0, 23);
					fnlm = changeTD('m', menu, 2, inim, 0, 59);
					fnls = changeTD('s', menu, 3, inis, 0, 59);
					
					// Set new clock
					setRTC(1, fnlh, fnlm, fnls);
					
					// Show new time
					getTimeDate();	//get latest time
					LCD_settings(menu, 0);
				}
				
				//Serial.println("menu 6");
				break;
			}
		
			case 4:	// Time interval
			{
				if (button == 3) {
					
					button = 0;
					LCD_settings(menu, 1);
					
					while (button != 3) {
						wdt_reset();
						if (button != 0) {
							
							switch (button) {
								case 1:
									if (dataInterval <= 30000) 
										dataInterval = 30000;
									 else 
										dataInterval -= 30000;
									break;
								
								case 2: 
									if (dataInterval >= 300000)
										dataInterval = 300000;
									else 
										dataInterval += 30000;
									break;
							}
							
							LCD_settings(menu, 1);
						}
						
						button = readPushButton();
						
						if (button == 3) {
							Serial.print("Changed interval to ");
							Serial.println(dataInterval);
						}
					}
					
					LCD_settings(menu, 0);
					
				}
			}
			
			case 5:	// SMS Comm On/Off
				if (button == 3) {
					setting_GSM = ~setting_GSM;
					
					LCD_settings(menu, 0);
				}
				break;
			
			case 6:	// Edit Profile
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 7:	// Add Profile
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 8:	// Delete Profile
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 9:	// Check Load
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 10:// Monitor Report Interval
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 11:// Power Failure Timer
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			case 12:// Power Fluctuation Timer
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
			default:// default
				if (button == 3) {
										
					LCD_settings(menu, 0);
				} 
				break;
			
		}
		
		// Check for new input
		button = readPushButton();
	
	}
	
}


int changeTD (char type, int menu, int asteriskPos, int iniValue, int lowerLimit, int upperLimit) { 
// Up/down function in time/date settings	
	
	int tempValue = iniValue, button = 0;
	
	LCD_settings(menu, asteriskPos);
	
	while (1) {
		wdt_reset();
		if (button != 0) {
			switch (button) {
				
				case 1:	// decrease value
				{
					if (tempValue <= lowerLimit) //iikot ang value
						tempValue = upperLimit;
					else
						tempValue--;
					break;
				}	
				case 2: // increase value
				{
					if (tempValue >= upperLimit) //iikot ang value
						tempValue = lowerLimit;
					else 
						tempValue++;
					break;
				}
				case 3: // accept value
				{	
					
					Serial.print("new strDate: ");
					Serial.println(strDate);
					Serial.print("new strTime: ");
					Serial.println(strTime);
					
					return tempValue;
					break;
				}
			}
			/*
			Serial.print("tempValue == ");
			Serial.println(tempValue);
			Serial.print("old strTime: ");
			Serial.println(strTime);
			*/
			
			// Save to strings
			switch (type) {
				//strDate = YYYY/MM/DD
				case 'Y':
				{
					strDate = tempValue + strDate.substring(4, 10);
					break;
				}
				case 'M':
				{ 
					// add zero fill
					if (tempValue < 10) 
						strDate = strDate.substring(0, 5) + '0' + tempValue + strDate.substring(7, 10);
					else
						strDate = strDate.substring(0, 5) +  tempValue + strDate.substring(7, 10);
					break;
				}
				case 'D':
				{ 
					if (tempValue < 10) 
						strDate = strDate.substring(0, 8) + '0' + tempValue;
					else
						strDate = strDate.substring(0, 8) + tempValue;
					break;
				}
				//strTime = hh:mm:ss
				case 'h': 
				{
					if (tempValue < 10)  {
						String tempStr = "0";	//can't start String addition w/ a char
						strTime = tempStr + tempValue + strTime.substring(2, 8);
					}	
					else
						strTime = tempValue + strTime.substring(2, 8);
					break;
				}
				case 'm': 
				{
					if (tempValue < 10) 
						strTime = strTime.substring(0, 3) + '0' + tempValue + strTime.substring(5, 8);
					else
						strTime = strTime.substring(0, 3) + tempValue + strTime.substring(5, 8);
					break;
				}
				case 's': 
				{
					if (tempValue < 10) 
						strTime = strTime.substring(0, 6) + '0' + tempValue;
					else
						strTime = strTime.substring(0, 6) + tempValue;
					break;
				}
			}
			
			// Refresh LCD with new value
			LCD_settings(menu, asteriskPos);
		}
		
		button = readPushButton();
	}
}

void LCD_settings (int screenNum, int option) { 
// Show settings menu on LCD
	
	lcd.clear();
	
	switch (screenNum) {
		
		case 0: // LCD LED:
			lcd.setCursor(0,0);
			lcd.print("< LCDBacklight >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off       ");
			
			if (setting_lcdLED) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (!setting_lcdLED){
				lcd.setCursor(5,1);
				lcd.print("*");
			}
			break;
		
		case 1: // Save to SDC: 
			lcd.setCursor(0,0);
			lcd.print("< Save to SDC: >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off       ");
			
			if (setting_SD == true) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else {
				lcd.setCursor(5,1);
				lcd.print("*");
			}
			break;
		
		case 2: // Date: YYYY / MM / DD
			lcd.setCursor(0,0);
			lcd.print("< Date:        >");
			lcd.setCursor(1,1);
			lcd.print(strDate.substring(0, 4));
			lcd.print(" / ");
			lcd.print(strDate.substring(5, 7));
			lcd.print(" / ");
			lcd.print(strDate.substring(8, 10));
			
			if (option == 1) {
				lcd.setCursor(0,1);
				lcd.print("*");
			} else if (option == 2) {
				lcd.setCursor(7,1);
				lcd.print("*");
			} else if (option == 3) {
				lcd.setCursor(12,1);
				lcd.print("*");
			}
			break;
		
		case 3: // Time: hh : mm : ss
			lcd.setCursor(0,0);
			lcd.print("< Time:        >");
			lcd.setCursor(2,1);
			lcd.print(strTime.substring(0, 2));
			lcd.print(" : ");
			lcd.print(strTime.substring(3, 5));
			lcd.print(" : ");
			lcd.print(strTime.substring(6, 8));
			
			if (option == 1) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (option == 2) {
				lcd.setCursor(6,1);
				lcd.print("*");
			} else if (option == 3) {
				lcd.setCursor(11,1);
				lcd.print("*");
			}
			break;
		
		case 4: // Time interval
		/*	< Interval:    >
			   00 m  30 s
		*/
		{
			lcd.setCursor(2,0);
			lcd.print("Interval:");
			lcd.setCursor(3,1);
			
			// Convert interval to mins & secs 
			int tempMin = dataInterval/60000, tempSec = (dataInterval%60000)/1000;
			
			if (tempMin < 10)
				lcd.print("0");
			lcd.print(tempMin);
			lcd.print(" m  ");
			if (tempSec < 10)
				lcd.print("0");
			lcd.print(tempSec);
			lcd.print(" s");
			
			if (option == 0) {
				lcd.setCursor(0,0);
				lcd.print("<");
				lcd.setCursor(15,0);
				lcd.print(">");
			} else if (option == 1) {
				if (dataInterval > 30000) {
					lcd.setCursor(0,1);
					lcd.print("<");
				}
				if (dataInterval < 300000) {
					lcd.setCursor(15,1);
					lcd.print(">");
				}
			}
			
			break;
		}
		case 5: // SMS Comm: 
			lcd.setCursor(0,0);
			lcd.print("< SMS Comm:    >");
			lcd.setCursor(0,1);
			lcd.print("  On  Off       ");
			
			if (setting_GSM) {
				lcd.setCursor(1,1);
				lcd.print("*");
			} else if (!setting_GSM) {
				lcd.setCursor(5,1);
				lcd.print("*");
			}
			break;
		
		case 6: // Edit Profile
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 7: // Add Profile
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 8: // Delete Profile
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 9: // Check Load
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 10:// Monitor Report Interval
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 11:// Power Failure Timer
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		case 12:// Power Fluctuation Timer
			lcd.setCursor(0,0);
			lcd.print("< Change SMS   >");
			lcd.setCursor(0,1);
			lcd.print("  Settings      ");
			break;
		
		default: // default
			lcd.setCursor(0,0);
			lcd.print("< why r u here >");
			lcd.setCursor(0,1);
			lcd.print("  menu = ");
			lcd.print(screenNum);
			break;
		
	}
	
}

float voltmeter () {
	
	float v = (analogRead(powerDetPin) * vPow) / 1024.0;
	float v2 = v / (r2/(r1 + r2));
	
	return v2;
}

void sendPowerAlert (int type) {
// Send power message; type: 0 = power outage. 1 = power on
	//wdt_disable();
	char msg[160];
	
	// Get current time
	getTimeDate();
	
	switch (type) { 
	
		case 0:
			// power outage
			
			strcpy(msg, "Power outage occurred at ");
			strcat(msg, chrTime);
			strcat(msg, " on ");
			strcat(msg, chrDate);
			strcat(msg, ".");
			
			Gsm.sendSMS(contact[0].phoneNum, msg);
			break;
			
		case 1:
			// power is back on
			
			strcpy(msg, "Power returned at ");
			strcat(msg, chrTime);
			strcat(msg, " on ");
			strcat(msg, chrDate);
			strcat(msg, ".");
			
			Gsm.sendSMS(contact[0].phoneNum, msg);			
			break;
			
		default:
			//??? do nothing
			break;
	}
	//wdt_enable(WDTO_4S);
}

void readSDSettings () {
	
	char character;
	char settingName[21], settingValue[21];

	strcpy(settingName, "");
	strcpy(settingValue, "");
	
	File myFile = SD.open("settings.txt");
	
	if (myFile) {
		while (myFile.available()) {
			character = myFile.read();
			while ((myFile.available()) && (character != '[')) {
				character = myFile.read();
			}
			character = myFile.read();
			while ((myFile.available()) && (character != '=')) {
				strncat(settingName, &character, 1);
				character = myFile.read();
			}
			strcat(settingName, "\0");
			character = myFile.read();
			while ((myFile.available()) && (character != ']')) {
				strncat(settingValue, &character, 1);
				character = myFile.read();
			}
			strcat(settingValue, "\0");
			if (character == ']') {
				applySetting(settingName, settingValue);
				
				strcpy(settingName, "");
				strcpy(settingValue, "");
			}
		}
		myFile.close();
	} else {
		Serial.println(F("error opening settings.txt"));
		myFile.close();
	}
}

void applySetting (char *settingName, char *settingValue) { 
	
	Serial.println(settingName);
	Serial.println(settingValue);
	
	if (strcmp(settingName, "LCD backlight") == 0) {
		Serial.println(setting_lcdLED);
		setting_lcdLED = toBoolean(settingValue);
		Serial.println(setting_lcdLED);
	}
	if (strcmp(settingName, "SD card saving") == 0) {
		Serial.println(setting_SD);
		setting_SD = toBoolean(settingValue);
		Serial.println(setting_SD);
	}
	if (strcmp(settingName, "Data Interval") == 0) {
		Serial.println(dataInterval);
		dataInterval = atol(settingValue);
		Serial.println(dataInterval);
	}
	
	if (strcmp(settingName, "SMS comm") == 0) {
		Serial.println(setting_GSM);
		setting_GSM = toBoolean(settingValue);
		Serial.println(setting_GSM);
	}
	if (strcmp(settingName, "Name") == 0) {
		Serial.println(contact[0].profileName);
		strcpy(contact[0].profileName, settingValue);
		Serial.println(contact[0].profileName);
	}
	if (strcmp(settingName, "Number") == 0) {
		Serial.println(contact[0].phoneNum);
		strcpy(contact[0].phoneNum, settingValue);
		Serial.println(contact[0].phoneNum);
	}
	
	if (strcmp(settingName, "Report sending") == 0) {
		Serial.println(setting_sendReport);
		setting_sendReport = toBoolean(settingValue);
		Serial.println(setting_sendReport);
	}
	if (strcmp(settingName, "Report range") == 0) {
		Serial.println(setting_GsmReportRange);
		setting_GsmReportRange = atoi(settingValue);
		Serial.println(setting_GsmReportRange);
	}
	if (strcmp(settingName, "Report period") == 0) {
		Serial.println(setting_GsmReportPeriod);
		setting_GsmReportPeriod = atoi(settingValue);
		Serial.println(setting_GsmReportPeriod);
	}
	if (strcmp(settingName, "Start report time") == 0) {
		Serial.println(setting_startReportTime);
		strcpy(setting_startReportTime, settingValue);
		Serial.println(setting_startReportTime);
	}
	if (strcmp(settingName, "Next report date") == 0) {
		Serial.println(setting_nextReportDate);
		strcpy(setting_nextReportDate, settingValue);
		Serial.println(setting_nextReportDate);
	}
	
	if (strcmp(settingName, "Power alert") == 0) {
		Serial.println(setting_powerAlert);
		setting_powerAlert = toBoolean(settingValue);
		Serial.println(setting_powerAlert);
	}
	if (strcmp(settingName, "Power alert delay") == 0) {
		Serial.println(powerAlertInterval);
		powerAlertInterval = atol(settingValue);
		Serial.println(powerAlertInterval);
	}
	
}

boolean toBoolean (char *settingValue) {
	
	if (atoi(settingValue) == 1) {
		return true;
	} else {
		return false;
	}
	
}

void writeSDSettings () {
	
	SD.remove("settings.txt");
	Serial.println("removed");
	File myFile = SD.open("settings.txt", FILE_WRITE);
	
	myFile.print("[LCD backlight=");
	myFile.print(setting_lcdLED);
	myFile.print("]\n[SD card saving=");
	myFile.print(setting_SD);
	myFile.print("]\n[Data Interval=");
	myFile.print(dataInterval);
	
	myFile.print("]\n\n[SMS comm=");
	myFile.print(setting_GSM);
	myFile.print("]\n[Name=");
	myFile.print(contact[0].profileName);
	myFile.print("]\n[Number=");
	myFile.print(contact[0].phoneNum);
	
	myFile.print("]\n\n[Report sending=");
	myFile.print(setting_sendReport);
	myFile.print("]\n[Report range=");
	myFile.print(setting_GsmReportRange);
	myFile.print("]\n[Report period=");
	myFile.print(setting_GsmReportPeriod);
	myFile.print("]\n[Start report time=");
	myFile.print(setting_startReportTime);
	myFile.print("]\n[Next report date=");
	myFile.print(setting_nextReportDate);
	
	myFile.print("]\n\n[Power alert=");
	myFile.print(setting_powerAlert);
	myFile.print("]\n[Power alert delay=");
	myFile.print(powerAlertInterval);
	myFile.print("]");
	
	myFile.close();
	Serial.println("file done");
}
 
void sendReportString (int opt) { 
// opt: 0-timed report, 1-text report

	wdt_disable(); // watchdog doesn't like waiting for texting to finish! bad dog!
	
	char chrReport [160], tempchr[25];
	//String tempString;
	int counter = 1;

	strcpy(chrReport, "Set 1\nTime  ATemp AveRH\n"); 
	// Get first date for interval settings
	getTimeDate();
	
	char reportDate[] = "YYYY/MM/DD", reportTime[] = "hh:mm";
	char searchDate[] = "YYYY/MM/DD", searchTime[] = "hh:mm";
	char searchDateTime[] = "YYYY/MM/DD\thh:mm";
	
	if (opt == 0) {
		strcpy(reportDate, setting_nextReportDate);
		strcpy(reportTime, setting_startReportTime);
	} else if (opt == 1) {
		getTimeDate();
		strcpy(reportDate, chrDate);
		strcpy(reportTime, textTime);
	}
	
	char chrDay[] = "DD";
	for (int i = 0; i < 2; i++)
		chrDay[i] = reportDate[i+8];
	int intDay = atoi(chrDay);
	
	char chrMonth[] = "MM";
	for (int i = 0; i < 2; i++) 
		chrMonth[i] = reportDate[i+5];
	int intMonth = atoi(chrMonth);
	
	char chrYear[] = "YYYY";
	for (int i = 0; i < 4; i++)
		chrYear[i] = reportDate[i];
	int intYear = atoi(chrYear);
	//Serial.println(intDay);
	// If start date is in prev month
	if (intDay - (setting_GsmReportRange / 24) < 1) {
		
		if (intMonth == 1) {
			intMonth = 12;
			intYear--;
		}
		else 
			intMonth--;
		
		intDay = daysInMonth(intMonth, intYear) + (intDay - (setting_GsmReportRange / 24));
	} else {
	// Else if start date is in same month	
		intDay = intDay - (setting_GsmReportRange / 24);
		//Serial.println(intDay);
	}
	
	// Get start time in int
	char chrHour[] = "hh";
	for (int i = 0; i < 2; i++)
		chrHour[i] = reportTime[i];
	int intHour = atoi(chrHour);
	
	char chrMinute[] = "mm";
	for (int i = 0; i < 2; i++) 
		chrMinute[i] = reportTime[i+3];
	int intMinute = atoi(chrMinute);
	
	// Get number of times for interval
	int numLines = (setting_GsmReportRange*60) / setting_GsmReportPeriod;
	int hourPeriod = setting_GsmReportPeriod / 60;
	int minutePeriod = setting_GsmReportPeriod % 60;
	
	int intSearchHour, intSearchMinute, prevHour = intHour, nextHour;
	
	// Convert int date to char
	sprintf(chrYear, "%04d", intYear);
	sprintf(chrMonth, "%02d", intMonth);
	sprintf(chrDay, "%02d", intDay);
	
	for (int j = 0; j < 4; j++) 
		searchDate[j] = chrYear[j];
	for (int j = 0; j < 2; j++)
		searchDate[j+5] = chrMonth[j];
	for (int j = 0; j < 2; j++)
		searchDate[j+8] = chrDay[j];
/* 
	strReport += searchDate;
	strReport += '\n'; */
	strcat(chrReport, searchDate);
	strcat(chrReport, "\n");
	// Loop using time+period to get averages
	for (int i = 0; i < numLines; i++) {
		
		if (strlen(chrReport) >= 160-21) {
			Serial.println("----TEXT----");
			Serial.println(chrReport);
			Serial.println("----TEXT----");
			Gsm.sendSMS(contact[0].phoneNum, chrReport);
			counter++;
			strcpy(chrReport, "Set ");
			strcat(chrReport, itoa(counter, tempchr, 10));
			strcat(chrReport, "\n");
			
		}
		
		// Get time to search for
		intSearchHour = (intHour + (hourPeriod * i) + (intMinute + (minutePeriod * i)) / 60) % 24;
		intSearchMinute = (intMinute + (minutePeriod * i)) % 60;
		
		// If time passes 00:00 mn, use date+1
		nextHour = intSearchHour;
		if (nextHour < prevHour) { 
			
			// If last day of month
			if (intDay + 1 > daysInMonth(intMonth, intYear)) { 
				intDay = 1;
				
				if (intMonth == 12) {
					intMonth = 1;
					intYear++;
				} else { 
					intMonth++;
				}
			} else {
				intDay++;
			}
			
			// Convert int date to char
			sprintf(chrYear, "%04d", intYear);
			sprintf(chrMonth, "%02d", intMonth);
			sprintf(chrDay, "%02d", intDay);
			
			for (int j = 0; j < 4; j++) 
				searchDate[j] = chrYear[j];
			for (int j = 0; j < 2; j++)
				searchDate[j+5] = chrMonth[j];
			for (int j = 0; j < 2; j++)
				searchDate[j+8] = chrDay[j];
			
			/* strReport += searchDate;
			strReport += '\n'; */
			strcat(chrReport, searchDate);
			strcat(chrReport, "\n");
		}
		
		// Convert int time to char
		sprintf(chrHour, "%02d", intSearchHour);
		sprintf(chrMinute, "%02d", intSearchMinute);
		
		for (int j = 0; j < 2; j++) {
			searchTime[j] = chrHour[j];
			searchTime[j+3] = chrMinute[j];
		}
		
		Serial.println(searchDate);
		Serial.println(searchTime); 
		strcpy(searchDateTime, searchDate);
		strcat(searchDateTime, "\t");
		strcat(searchDateTime, searchTime);
		
		/* strReport += findAveString(searchDate, searchTime);
		strReport += '\n';
		Serial.print(strReport); */
		/* tempString = "";
		tempString = findAveString(searchDate, searchTime);
		tempString.toCharArray(tempchr, tempString.length()+1); */
		
		char tempstr[25];
		
		// Open file for reading
		getFilename(searchDate);
		
		Serial.print(F("Opening "));
		Serial.println(filename);
		File datafile = SD.open(filename, FILE_READ); 
		
		if (datafile) { 
			
			// Search for date
			if (datafile.find(searchDateTime)) {
				// If time found
				strcpy(tempstr, searchTime);
				strcat(tempstr, " ");
				// Get aveTemp and aveRh of line
				//datafile.seek(datafile.position() + 124);
				
				char a = 'q';
				int i = 0;
				
				while (i < 19) {
					a = datafile.read();
					//Serial.print(a);
					if (a == '\t')
						i++;
				}
				
				i = 0;
				while (i < 2) {
					a = datafile.read();
					//Serial.print(a);
					if (a == '\t') {
						i++;
						//tempString += ' ';
						strcat(tempstr, " ");
					}	
					else if (i < 2) { 
						//tempString += a;	
						strncat(tempstr, &a, 1);
					}
				}
				
				strcat(tempstr, "\0");
				
				datafile.close();
				
			} else {
				// If date not found, return error msg (date not found
				datafile.close();
				strcpy(tempstr, "data not found"); 
				//return
			}
		}
		
		Serial.println(tempstr);
		
		 
		strcat(chrReport, tempstr);
		strcat(chrReport, "\n");
		//Serial.println(chrReport);
	
		
		prevHour = intSearchHour;
	}	
	
	Serial.println("----TEXT----");
	Serial.println(chrReport);
	Serial.println("----TEXT----");
	
	Gsm.sendSMS(contact[0].phoneNum, chrReport);
	
	if (opt == 0) {
		// Set next report date
		int addDay = setting_GsmReportRange / 24;
		
		for (int i = 0; i < 2; i++)
			chrDay[i] = setting_nextReportDate[i+8];
		intDay = atoi(chrDay);
		
		for (int i = 0; i < 2; i++) 
			chrMonth[i] = setting_nextReportDate[i+5];
		intMonth = atoi(chrMonth);
		
		for (int i = 0; i < 4; i++)
			chrYear[i] = setting_nextReportDate[i];
		intYear = atoi(chrYear);
		
		// Check if next date is in next month
		if (intDay + addDay > daysInMonth(intMonth, intYear)) {
			
			intDay = (intDay + addDay) % daysInMonth(intMonth, intYear);
			
			if (intMonth == 12) {
				intMonth = 1;
				intYear++;
			}
			else
				intMonth++;
		
		} else {
			intDay = intDay + addDay;
		}
		
		// Make setting_nextReportDate
		sprintf(chrDay, "%02d", intDay);
		sprintf(chrMonth, "%02d", intMonth);
		sprintf(chrYear, "%04d", intYear);
		
		for (int i = 0; i < 2; i++) {
			setting_nextReportDate[i+8] = chrDay[i];
			setting_nextReportDate[i+5] = chrMonth[i];
		}
		
		for (int i = 0; i < 4; i++)
			setting_nextReportDate[i] = chrYear[i];
	}
	/* Serial.println(setting_nextReportDate);
	strcpy(setting_nextReportDate, "2016/12/07"); */
	wdt_enable(WDTO_4S); // ok na dog
}

boolean checkReportTimeDate () { 
// Check if current time is same as report time
	
	// Get current time
	getTimeDate();
	
	// Compare current time/date to report time/date & return value
	if (strncmp(setting_startReportTime, chrTime, 5) == 0) {
		if (strcmp(setting_nextReportDate, chrDate) == 0) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
	
}

void parseText () {
// Read text
	
	if (strcmp(textMessage, "LOAD") == 0) {
	// Load inquiry	
		wdt_disable();
		
		// Balance inquiry
		Gsm.sendSMS("214", "?1515");
		while (1) {
			textIndex = Gsm.isSMSunread();
			if (textIndex > 0) {
				
				Gsm.readSMS(textIndex, textMessage, 160, textPhoneNum, textDateTime);
				Gsm.deleteSMS(textIndex);
				
				Serial.println(textMessage);
				
				// Forward msg received 
				Gsm.sendSMS(contact[0].phoneNum, textMessage);
				//Serial.println("sent msg");
				
				break;
			}
		}
	
		wdt_enable(WDTO_4S);
	}
	
	else if (strcmp(textMessage, "REPORT ON") == 0) { 
	// Switch report on
		setting_sendReport = true;
		Serial.print(F("sendReport: "));
		Serial.println(setting_sendReport);
	}
	
	else if (strcmp(textMessage, "REPORT OFF") == 0) { 
	// Switch report on
		setting_sendReport = false;
		Serial.print(F("sendReport: "));
		Serial.println(setting_sendReport);
	}
	
	else if (strcmp(textMessage, "ALERT ON") == 0) { 
	// Switch report on
		setting_powerAlert = true;
		Serial.print(F("powerAlert: "));		
		Serial.println(setting_powerAlert);
	}
	
	else if (strcmp(textMessage, "ALERT OFF") == 0) { 
	// Switch report on
		setting_powerAlert = false;
		Serial.print(F("powerAlert: "));		
		Serial.println(setting_powerAlert);
	}
	
	else {
	// Deafult	
	}
	
}

void loadCheck () {
	
	wdt_disable();
		
	// Balance inquiry
	Gsm.sendSMS("214", "?1515");
	while (1) {
		textIndex = Gsm.isSMSunread();
		if (textIndex > 0) {
			
			Gsm.readSMS(textIndex, textMessage, 160, textPhoneNum, textDateTime);
			Gsm.deleteSMS(textIndex);
			
			break;
		}
	}
	
	
	
	wdt_enable(WDTO_4S);
	
}
