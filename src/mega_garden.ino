#include "secrets.h"
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiEspAT.h>
#include "Adafruit_MQTT_Client.h"
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

/************************* DS18B20  Sensor  ***************************/
#define ONE_WIRE_BUS 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float waterTempC;
float waterTempF;

/************************* DHT11 Sensor  ***************************/
#define Type DHT11
int sensePin=2;
DHT HT(sensePin,Type);
float humidity;
float tempC;
float tempF;
int setTime=500;
int dt=1000;

/*************************  Relays  ***************************/
// Light01
const int relayPinLight01 = 3;
const int dawnEEPROMLight01 = 1;
const int duskEEPROMLight01 = 2;
String valStateOverideLight01 = "OFF";
String valStateLight01= "OFF";
time_t valDawnLight01_tm;
char *valDawnLight01_str;
time_t valDuskLight01_tm;
char *valDuskLight01_str;

// Heater01
const int relayPinHeater01 = 4;
const int thermostatEEPROMHeater01 = 0;
String valStateOverideHeater01 = "OFF";
String valStateHeater01= "OFF";
float valThermostatHeater01;

// Pump01
const int relayPinPump01 = 6;
String valStateOveridePump01 = "OFF";
String valStatePump01= "OFF";

//Pump02
const int relayPinPump02 = 7;
String valStateOveridePump02 = "OFF";
String valStatePump02= "OFF";

/************************* TDS Sensor v1.0  ***************************/
#define TdsSensorPin A0
#define VREF 5.0 // analog reference voltage(Volt) of the ADC
#define SCOUNT 10 // sum of sample point

int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0,copyIndex = 0;

float averageVoltage = 0,tdsValue = 0,temperature = 25;

/************************* ESP-01(ESP8266) ***************************
// Using Arduino Mega2650 Serial 2 for serial monitor 
// Serial1 on pins 19 (RX) and 18 (TX)
// Serial2 on pins 17 (RX) and 16 (TX) 
// Serial3 on pins 15 (RX) and 14 (TX)*/
#define EspSerial              Serial2
#define ESP8266_BAUD           115200 //9600, 57600, 115200

/************************* WiFi Access Point *********************************/
int status = WL_IDLE_STATUS;   // the Wifi radio's status

/************************* Time and NTP Config *********************************/
//Week Days
//#define UNIX_OFFSET            946684800;
time_t epochTime;
struct tm; 
int monthDay;
int currentMonth;
String currentMonthName;
int currentYear;
String formattedTime;
int currentHour;
int currentMinute;
int currentSecond;
String weekDay;
String currentDate;
String currentDateLong;
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const char* ntpServer = "pool.ntp.org";
const bool daylightOffset = true;
// Set offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 1
// GMT +5(EST) = 5
// GMT -1 = -1
// GMT 0 = 0
const int  gmtOffset = 5;

/***************************** Global State **********************************/

// Initialize the WiFi UDP object and NTP time Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Initialize the WiFi client object
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT);

/****************************** Publishing Feeds ******************************/

// Setup a feed called 'ultrasonic' for publishing.
// Notice MQTT paths for MQTT follow the form: <username>/feeds/<feedname>

Adafruit_MQTT_Publish tdsPub        = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/TDS");
Adafruit_MQTT_Publish humidityPub   = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Humidity");
Adafruit_MQTT_Publish airTempCPub   = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/AirTemp");
Adafruit_MQTT_Publish waterTempCPub = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/WaterTemp");

Adafruit_MQTT_Publish stateLight01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Light01/state");
Adafruit_MQTT_Publish stateHeater01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Heater01/state");
Adafruit_MQTT_Publish statePump01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Pump01/state");
Adafruit_MQTT_Publish statePump02  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Pump02/state");

Adafruit_MQTT_Publish stateOverideLight01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Light01/override/state");
Adafruit_MQTT_Publish stateOverideHeater01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Heater01/override/state");
Adafruit_MQTT_Publish stateOveridePump01  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Pump01/override/state");
Adafruit_MQTT_Publish stateOveridePump02  = Adafruit_MQTT_Publish(&mqtt, HOSTNAME "/Pump02/override/state");



/****************************** Subscribed Feeds ******************************/

Adafruit_MQTT_Subscribe soilsub01 = Adafruit_MQTT_Subscribe(&mqtt, "Soilsensor01/WaterDetected");
Adafruit_MQTT_Subscribe soilsub02 = Adafruit_MQTT_Subscribe(&mqtt, "Soilsensor02/WaterDetected");


Adafruit_MQTT_Subscribe thermostatHeater01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Heater01/thermostat");
Adafruit_MQTT_Subscribe dawnLight01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Light01/dawn");
Adafruit_MQTT_Subscribe duskLight01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Light01/dusk");

Adafruit_MQTT_Subscribe overideLight01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Light01/override/");
Adafruit_MQTT_Subscribe overideHeater01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Heater01/override/");
Adafruit_MQTT_Subscribe overidePump01 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Pump01/override/");
Adafruit_MQTT_Subscribe overidePump02 = Adafruit_MQTT_Subscribe(&mqtt,  HOSTNAME "/Pump02/override/");


void MQTT_connect();

/****************************** Functions ************************************/


void printSerial()
{ 

  Serial.print("Time: ");
  Serial.println(formattedTime);


  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  
  Serial.print("Air Temp: ");
  Serial.print(tempC);
  Serial.print("\xC2\xB0");
  Serial.print("C | ");
  Serial.print(tempF);
  Serial.print("\xC2\xB0");
  Serial.println("F");

  Serial.print("Water Temp: ");
  Serial.print(waterTempC);
  Serial.print("\xC2\xB0");
  Serial.print("C | ");
  Serial.print(waterTempF);
  Serial.print("\xC2\xB0");
  Serial.println("F");
  
  Serial.print("TDS: ");
  Serial.print(tdsValue);
  Serial.println("ppm");

  Serial.print("Light 1 Status: ");
  Serial.println(valStateLight01);
  Serial.print("Light 1 Override Status: ");
  Serial.println(valStateOverideLight01);
  Serial.print("Light 1 Dawn Epoch: ");
  Serial.println(valDawnLight01_tm);
  valDawnLight01_str = ctime(&valDawnLight01_tm);
  Serial.print("Light 1 Dawn: ");
  Serial.println(valDawnLight01_str);


  Serial.print("Light 1 Dusk: "); 
  Serial.println(valDuskLight01_tm);
  valDuskLight01_str = ctime(&valDuskLight01_tm);
  Serial.print("Light 1 Dusk: ");
  Serial.println(valDuskLight01_str);

  Serial.print("Heater 1 Status: "); 
  Serial.println(valStateHeater01); 
  Serial.print("Heater 1 Override Status: "); 
  Serial.println(valStateOverideHeater01);
  Serial.print("Heater 1 Thermostat: "); 
  Serial.println(valThermostatHeater01);

  Serial.print("Pump 1 Status: "); 
  Serial.println(valStatePump01); 
  Serial.print("Pump 1 Override Status: "); 
  Serial.println(valStateOveridePump01); 

  Serial.print("Pump 2 Status: "); 
  Serial.println(valStatePump01); 
  Serial.print("Pump 2 Override Status: "); 
  Serial.println(valStateOveridePump02); 
  Serial.println();
  Serial.println();

}

/******************************/
void publishMQTT()
{ 
  Serial.print(F("Send to MQTT: "));
  if (tdsPub.publish(tdsValue) \
    && humidityPub.publish(humidity) \
    && airTempCPub.publish(tempC) \
    && waterTempCPub.publish(waterTempC) \
    
    && stateOverideLight01.publish((char*) valStateOverideLight01.c_str()) \
    && stateOverideHeater01.publish((char*) valStateOverideHeater01.c_str()) \
    && stateOveridePump01.publish((char*) valStateOveridePump01.c_str()) \
    && stateOveridePump02.publish((char*) valStateOveridePump02.c_str()) \
    
    && stateLight01.publish((char*) valStateLight01.c_str()) \
    && stateHeater01.publish((char*) valStateHeater01.c_str()) \
    && statePump01.publish((char*) valStatePump01.c_str()) \
    && statePump02.publish((char*) valStatePump02.c_str())) {
    Serial.println(F("Successful"));
  } else {
    Serial.println(F("Failed"));
  }
  Serial.println();

}
/******************************/

//float convertToString(char* a) {
//    //int i= sizeof(a);
//    String s = a;
//    //for (i = 0; i < size; i++) {
//    //    s = s + a[i];
//    //}
//    return s.toFloat();
//}

//constexpr float ToFloatAtCompileTime(uint8_t u8) {
//    return u8; // implicit conversion to return type
//}

time_t tmConvert_t(char *timestring) { 

  int position = 0;
  int time_ints[3];      
  char separator[] = ":";
  char *token;
  String tokenString;
  token = strtok((char *)timestring, separator);
  tokenString = token;
  time_ints[position] = tokenString.toInt();
  while(token != NULL) {
    token = strtok(NULL, separator);
    position++;
    tokenString = token;
    time_ints[position] = tokenString.toInt();
  }
  struct tm time_tm = {0};

  Serial.print("Hour: ");
  Serial.println(time_ints[0]);
  Serial.print("Min: ");
  Serial.println(time_ints[1]);
  Serial.print("Sec: ");
  Serial.println(time_ints[2]);



  time_tm.tm_mday = monthDay;
  time_tm.tm_mon = currentMonth-1;
  time_tm.tm_year = currentYear-1900;
  time_tm.tm_hour = time_ints[0]; // hours
  time_tm.tm_min = time_ints[1]; // minutes
  time_tm.tm_sec = time_ints[2]; // seconds
  time_t time_epoch = mktime(&time_tm);
  Serial.println("Seconds since 1900: ");
  Serial.println(time_epoch);


  char *str = ctime(&time_epoch);
  Serial.println(str);

  delay(50000);
  return time_epoch; 

}


//Epoch Time: 714878577
//Time: 01:22:57
//Long Date: Saturday, August 27, 2022
//
//
//Light 1 Dawn Epoch: 3894119552
//
//Light 1 Dawn: Wed May 26 19:52:32 2123
//
//Light 1 Dusk: 3894162812
//
//Light 1 Dusk: Thu May 27 07:53:32 2123
//

void readMQTT() // float tdsValue
{
  //MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  bool readcheck=false;
  
  while ((subscription = mqtt.readSubscription(500))) {
    readcheck = true;

    Serial.print((char *)subscription->topic);
    Serial.print(" = ");
    Serial.println((char *)subscription->lastread);

    if (subscription == &dawnLight01) {
      valDawnLight01_tm = tmConvert_t((char *)dawnLight01.lastread);
      EEPROM.update(dawnEEPROMLight01, valDawnLight01_tm);
    }

    if (subscription == &duskLight01) {
      valDuskLight01_tm = tmConvert_t((char *)duskLight01.lastread);
      EEPROM.update(duskEEPROMLight01, valDuskLight01_tm);
    }



    if (subscription == &thermostatHeater01) {
      valThermostatHeater01 = atoi((char *)thermostatHeater01.lastread);
      EEPROM.update(thermostatEEPROMHeater01, valThermostatHeater01);
    }



    if (subscription == &overideLight01) {
      if (strcmp((char *)overideLight01.lastread, "ON") == 0) {
        valStateOverideLight01 = "ON";
      }
      if (strcmp((char *)overideLight01.lastread, "OFF") == 0) {
        valStateOverideLight01 = "OFF";
      }
    }

    if (subscription == &overideHeater01) {
      if (strcmp((char *)overideHeater01.lastread, "ON") == 0) {
        valStateOverideHeater01 = "ON";
      }
      if (strcmp((char *)overideHeater01.lastread, "OFF") == 0) {
        valStateOverideHeater01 = "OFF";
      }
    }


    if (subscription == &overidePump01) {
      if (strcmp((char *)overidePump01.lastread, "ON") == 0) {
        valStateOveridePump01 = "ON";
      }
      if (strcmp((char *)overidePump01.lastread, "OFF") == 0) {
        valStateOveridePump01 = "OFF";
      }
    }
    
    if (subscription == &overidePump02) {
      if (strcmp((char *)overidePump02.lastread, "ON") == 0) {
        valStateOveridePump02 = "ON";
      }
      if (strcmp((char *)overidePump02.lastread, "OFF") == 0) {
        valStateOveridePump02 = "OFF";
      }
    }

  }
  Serial.print(F("Read from MQTT: "));
  if (readcheck){
    Serial.println("Successful");
    delay(5000);
  } else {
    Serial.println("Unchanged");
  }
  Serial.println();
}
/******************************/

void MQTT_connect() {
  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 10;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 3 seconds...");
       mqtt.disconnect();
       delay(3000);  // wait 3 seconds
       retries--;
       if (retries == 0) {
         Serial.println("System Halt");
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
  Serial.println();  
}

/******************************/

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

/*****************************/

void setup(){
  //serial 0 is to usb
  Serial.begin(115200);
  while(!Serial); 
  Serial.println("Garden01 on Mega2560"); 
  
  //serial 2 is to esp8266 
  Serial.println("Start Init serial to ESP-01");
  EspSerial.begin(ESP8266_BAUD);
  while(!EspSerial);
  
  //start WiFi
  WiFi.init(&EspSerial);

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }
  //Set new hostname
  // WiFi.mode(WIFI_STA); // (no AT command available)
  // WiFi.hostname(HOSTNAME); // (no AT command available)

  // Connect to WiFi access point.
  Serial.print("Connecting to "); Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  // Serial.printf("Hostname: %s\n", WiFi.hostname().c_str());  // (no AT command available)
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  // Init and get the time
  // calculate the local time from NTP
  int daylightOffset_sec;
  if (daylightOffset) {
    daylightOffset_sec = 3600;
  } else {
    daylightOffset_sec = 0;
  }
  int timeOffset_sec = -(gmtOffset*3600) + daylightOffset_sec;
  timeClient.begin();
  timeClient.setTimeOffset(timeOffset_sec);
  timeClient.update();
  getTimeValues();
  Serial.print("Epoch Time: ");
  Serial.println(epochTime);

  Serial.print("Time: ");
  Serial.println(timeClient.getFormattedTime());

  Serial.print("Long Date: ");
  Serial.println(currentDateLong);

  valThermostatHeater01 = EEPROM.read(thermostatEEPROMHeater01);
  Serial.print("EEPROM Thermostat Temp: ");
  Serial.println(valThermostatHeater01);

  //start 1-Wire sensors
  sensors.begin();
  
  //start DHT11 sensor
  HT.begin();
  
  //set relay output pins
  pinMode(relayPinLight01, OUTPUT);
  digitalWrite(relayPinLight01, HIGH);  
  
  pinMode(relayPinHeater01, OUTPUT);
  digitalWrite(relayPinHeater01, HIGH);  
  
  pinMode(relayPinPump01, OUTPUT);
  digitalWrite(relayPinPump01, HIGH);
  
  pinMode(relayPinPump02, OUTPUT);
  digitalWrite(relayPinPump02, HIGH);

  //Subscribe to feeds
  mqtt.subscribe(&soilsub01);
  mqtt.subscribe(&soilsub02);
  mqtt.subscribe(&overideLight01);
  mqtt.subscribe(&overideHeater01);
  mqtt.subscribe(&overidePump01);
  mqtt.subscribe(&overidePump02);
  mqtt.subscribe(&thermostatHeater01);
  mqtt.subscribe(&dawnLight01);
  mqtt.subscribe(&duskLight01);
  Serial.println();
  

}

void getSensorValues() {
  //Get DHT11 sensor values
  humidity = HT.readHumidity();
  tempC = HT.readTemperature();
  tempF = HT.readTemperature(true);

  //Get 1-Wire sensor values
  sensors.requestTemperatures();
  waterTempC = sensors.getTempCByIndex(0); // the index 0 refers to the first device
  waterTempF = sensors.getTempFByIndex(0);

  static unsigned long analogSampleTimepoint = millis();
  if(millis()-analogSampleTimepoint > 10U){     //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT){ 
      analogBufferIndex = 0;
    }
  }   
  
  static unsigned long printTimepoint = millis();
  if(millis()-printTimepoint > 80U){
    printTimepoint = millis();

    for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
      
      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;
      
      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
      float compensationCoefficient = 1.0+0.02*(temperature-25.0);
      //temperature compensation
      float compensationVoltage=averageVoltage/compensationCoefficient;
      
      //convert voltage value to tds value
      tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;  
    }
    
  }
}


void getTimeValues() {
  //Get the date and time relative to which a computer's clock
  epochTime = timeClient.getEpochTime() - UNIX_OFFSET;
  //Set a time structure
  tm *ptm = gmtime ((time_t *)&epochTime); 
  monthDay = ptm->tm_mday;
  currentMonth = ptm->tm_mon+1;
  currentMonthName = months[currentMonth-1];
  currentYear = ptm->tm_year+1900;

  //unsure if these values in this library's functions are off due to the Unix time offest (UNIX_OFFSET = 946684800) of the AVR library
  formattedTime = timeClient.getFormattedTime();
  currentHour = timeClient.getHours();
  currentMinute = timeClient.getMinutes();
  currentSecond = timeClient.getSeconds();
  weekDay = weekDays[timeClient.getDay()];

  currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  currentDateLong = String(weekDay) + ", " + String(currentMonthName) + " " + String(monthDay) + ", " + String(currentYear);

}

void setRelayValues() {
  valStateLight01 = "OFF";
  if (((valThermostatHeater01 >= waterTempF) \
    && (digitalRead(relayPinLight01) == HIGH)) \
      || ((valThermostatHeater01 + 0.5 >= waterTempF) \
    && (digitalRead(relayPinLight01) == LOW))) {
  



    digitalWrite(relayPinLight01, LOW);
  } else if (valStateOverideLight01 == "ON") {
    digitalWrite(relayPinLight01, LOW);
  } else {
    digitalWrite(relayPinLight01, HIGH);
  }
  if (digitalRead(relayPinLight01) == LOW) {
    valStateLight01 = "ON";
  } 







  valStateHeater01 = "OFF";
  if (((valThermostatHeater01 >= waterTempF) \
    && (digitalRead(relayPinHeater01) == HIGH)) \
      || ((valThermostatHeater01 + 0.5 >= waterTempF) \
    && (digitalRead(relayPinHeater01) == LOW))) {
    digitalWrite(relayPinHeater01, LOW);
  } else if (valStateOverideHeater01 == "ON") {
    digitalWrite(relayPinHeater01, LOW);
  } else {
    digitalWrite(relayPinHeater01, HIGH);
  }
  if (digitalRead(relayPinHeater01) == LOW) {
    valStateHeater01 = "ON";
  } 

  valStatePump01 = "OFF";
  if (valStateOveridePump01 == "ON") {
    digitalWrite(relayPinPump01, LOW);
  } else {
    digitalWrite(relayPinPump01, HIGH);
  }
  if (digitalRead(relayPinPump01) == LOW) {
    valStatePump01 = "ON";
  } 

  valStatePump02 = "OFF";
  if (valStateOveridePump02 == "ON") {
    digitalWrite(relayPinPump02, LOW);
  } else {
    digitalWrite(relayPinPump02, HIGH);
  }
  if (digitalRead(relayPinPump02)== LOW) {
    valStatePump02 = "ON";
  }
}


void loop() {
  getTimeValues();
  getSensorValues();
  // yield();
  MQTT_connect();
  readMQTT();

  setRelayValues();
  publishMQTT();
  printSerial();

}