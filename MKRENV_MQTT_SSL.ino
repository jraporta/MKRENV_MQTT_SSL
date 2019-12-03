//MKRENV_MQTT_TLS v0.2.0

#include <Arduino_MKRENV.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include "RoundRobinbyJR.h"

#include "arduino_secrets.h"   //please enter your sensitive data in the Secret tab/device101_secrets.h
char ssid[] = SECRET_SSID;     // your network SSID (name)
char pass[] = SECRET_PASS;     // your network password

void getENVValues(int marca = 1);

int status = WL_IDLE_STATUS;
IPAddress server(34, 255, 208, 144); //MQTT Broker ip
int port = 1883;
//int port = 8883; //SSL/TLS
//WiFiClient client;
WiFiSSLClient client;
PubSubClient mqttClient(server, port, client);

File myFile;
char fileName[20] = "reg24h.txt";
const int trigger = 12 * 25; //25 hours
const int logsToRemove = 12; //1 hour
unsigned long timeSD = 5 * 60 * 1000; //every 5 minutes
unsigned long timeMQTT = 5000;
unsigned long prevTime1 = 0 , prevTime2 = 0;
byte insertedSD = 0;
char buffer [20];

void setup() {
  delay(5000);
  Serial.begin(9600);
  Serial.println("MKRENV_and_MQTT v0.1.0");

  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);      // Connect to WPA/WPA2 network
    delay(5000);
  }
  printWiFiStatus();
  IPAddress ip = WiFi.localIP();
  snprintf(buffer, 20, "%d.%ld.%ld.%ld" , ip[0], ip[1], ip[2], ip[3]);

  while (!mqttClient.connect("device101Client", SECRET_USERNAME, SECRET_PASSWORD, "homie/device101/$state", 2, 1, "lost", true)) {
    Serial.println("Retrying to connect to MQTT broker... ");
    delay(5000);
  }
  Serial.println("Connected to MQTT broker!! ");

  mqttClient.publish("homie/device101/$state", "init", true);

  Serial.println("Initializing MKR ENV shield...");
  if (!ENV.begin()) {
    Serial.println("Failed to initialize MKR ENV shield!");
    mqttClient.publish("homie/device101/$state", "alert", true);
    while (1);
  }

  mqttClient.publish("homie/device101/$homie", "4.0.0", true);
  mqttClient.publish("homie/device101/$name", "device101 mkr1000", true);
  mqttClient.publish("homie/device101/$nodes", "meteo001,sd", true);
  mqttClient.publish("homie/device101/$extensions", "", true);
  mqttClient.publish("homie/device101/$ip", buffer, true);
  mqttClient.publish("homie/device101/meteo001/$name", "mkr env shield", true);
  mqttClient.publish("homie/device101/meteo001/$type", "", true);
  mqttClient.publish("homie/device101/meteo001/$properties", "temperature,humidity,pressure,uva,uvb,uvindex,sd", true);
  mqttClient.publish("homie/device101/meteo001/temperature/$name", "Temperature", true);
  mqttClient.publish("homie/device101/meteo001/temperature/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/temperature/$unit", "ºC", true);
  mqttClient.publish("homie/device101/meteo001/humidity/$name", "Humidity", true);
  mqttClient.publish("homie/device101/meteo001/humidity/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/humidity/$unit", "%", true);
  mqttClient.publish("homie/device101/meteo001/pressure/$name", "Pressure", true);
  mqttClient.publish("homie/device101/meteo001/pressure/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/pressure/$unit", "kPa", true);
  mqttClient.publish("homie/device101/meteo001/uva/$name", "UVA", true);
  mqttClient.publish("homie/device101/meteo001/uva/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/uva/$unit", "", true);
  mqttClient.publish("homie/device101/meteo001/uvb/$name", "UVB", true);
  mqttClient.publish("homie/device101/meteo001/uvb/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/uvb/$unit", "", true);
  mqttClient.publish("homie/device101/meteo001/uvindex/$name", "UV Index", true);
  mqttClient.publish("homie/device101/meteo001/uvindex/$datatype", "float", true);
  mqttClient.publish("homie/device101/meteo001/uvindex/$unit", "", true);
  mqttClient.publish("homie/device101/sd/$name", "micro-SD", true);
  mqttClient.publish("homie/device101/sd/$type", "8GB", true);
  mqttClient.publish("homie/device101/sd/$properties", "state", true);
  mqttClient.publish("homie/device101/sd/state/$name", "state", true);
  mqttClient.publish("homie/device101/sd/state/$datatype", "enum", true);
  mqttClient.publish("homie/device101/sd/state/$format", "ready,missing,error,writing", true);



  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    mqttClient.publish("homie/device101/sd/state", "missing", true);
  } else {
    mqttClient.publish("homie/device101/sd/state", "ready", true);
    insertedSD = 1;
    Serial.println("initialization done.");
  }
  mqttClient.publish("homie/device101/$state", "ready", true);
}

void loop() {
  if (insertedSD && ((prevTime1 + timeSD) < millis())) {
    getENVValues();
    Serial.println("Published sensor data.");
    prevTime1 = millis();
    if (NumberOfLogs(fileName) >= trigger) {
      Serial.print("Deleting oldest logs.....");
      mqttClient.publish("homie/device101/sd/state", "writing", true);
      RemoveOldLogs(fileName, trigger, logsToRemove);
      mqttClient.publish("homie/device101/sd/state", "ready", true);
      Serial.println("done.");
    }
  }

  if ((prevTime2 + timeMQTT) < millis()) {
    getENVValues(2);
    prevTime2 = millis();
  }

  mqttClient.loop();
}

void getENVValues(int marca) {
  // read all the sensor values
  float temperature = ENV.readTemperature();
  float humidity    = ENV.readHumidity();
  float pressure    = ENV.readPressure();
  //  float illuminance = ENV.readIlluminance();
  float uva         = ENV.readUVA();
  float uvb         = ENV.readUVB();
  float uvIndex     = ENV.readUVIndex();
  if (marca == 1) {
    if (myFile = SD.open(fileName, FILE_WRITE)) {
      Serial.print("Writing to ");
      Serial.print(fileName);
      Serial.print(" .....");
      unsigned int prueba = millis();
      myFile.print(int(millis() / 1000.0));
      myFile.print(" s ");
      myFile.print(temperature);
      myFile.print(" °C ");
      myFile.print(humidity);
      myFile.print(" % ");
      myFile.print(pressure);
      myFile.print(" kPa ");
      myFile.print(uva);
      myFile.print(" UVA ");
      myFile.print(uvb);
      myFile.print(" UVB ");
      myFile.print(uvIndex);
      myFile.println(" UVIndex");
      myFile.close();
      Serial.println("done.");
      mqttClient.publish("homie/device101/sd/state", "ready", true);
    } else {
      Serial.println("error opening test.txt");
      mqttClient.publish("homie/device101/sd/state", "error", true);
    }
  }
  if (marca == 2) {
    snprintf(buffer, 20, "%f", temperature);
    mqttClient.publish("homie/device101/meteo001/temperature", buffer, true);
    snprintf(buffer, 20, "%f", humidity);
    mqttClient.publish("homie/device101/meteo001/humidity", buffer, true);
    snprintf(buffer, 20, "%f", pressure);
    mqttClient.publish("homie/device101/meteo001/pressure", buffer, true);
    snprintf(buffer, 20, "%f", uva);
    mqttClient.publish("homie/device101/meteo001/uva", buffer, true);
    snprintf(buffer, 20, "%f", uvb);
    mqttClient.publish("homie/device101/meteo001/uvb", buffer, true);
    snprintf(buffer, 20, "%f", uvIndex);
    mqttClient.publish("homie/device101/meteo001/uvindex", buffer, true);
  }
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP: ");
  Serial.println(ip);

  Serial.print("signal strength (RSSI):");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}
