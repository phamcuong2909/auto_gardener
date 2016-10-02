
#include <ESP8266WiFi.h> // ESP8266WiFi.h library
#include "BasicStepperDriver.h"
#include <string.h>
#include <Wire.h>
#include <DHT.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>

#define DHTTYPE  DHT21
#define DHTPIN       D8

// Analog multiplexer
#define AMUXSEL0 D5     // AMUX Selector 0
#define AMUXSEL1 D6     // AMUX Selector 1
#define AMUXSEL2 D7     // AMUX Selector 2

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200

// All the wires needed for full functionality
#define DIR D2
#define STEP D1
#define ENBL D0

// Since microstepping is set externally, make sure this matches the selected mode
// 1=full step, 2=half step etc.
#define MICROSTEPS 1

// 2-wire basic config, microstepping is hardwired on the driver
BasicStepperDriver stepper(MOTOR_STEPS, DIR, STEP, ENBL);

const char* ssid     = "Sandiego";
const char* password = "0988807067";
const char* host = "api.thingspeak.com";
const char* writeAPIKey = "A1R9H3D7OZ125ZRI";


// Cấu hình cho giao thức MQTT
const char* clientId = "Autogardener1";
//const char* mqttServer = "broker.hivemq.com";
const char* mqttServer = "192.168.1.110";
const int mqttPort = 1883;
// Username và password để kết nối đến MQTT server nếu có 
// bật chế độ xác thực trên MQTT server
// Nếu không dùng thì cứ để vậy
const char* mqttUsername = "<MQTT_BROKER_USERNAME>";
const char* mqttPassword = "<MQTT_BROKER_PASSWORD>";

// Tên MQTT topic để gửi thông tin về nhiệt độ
const char* tempTopic = "/eHome/Garden/Temperature";
// Tên MQTT topic để gửi thông tin về độ ẩm
const char* humidityTopic = "/eHome/Garden/Humidity";
// Tên MQTT topic để gửi thông tin về độ ẩm
String moistureTopic = "/eHome/Garden/Moisture";

uint8_t           lastHumidity = 255;
uint8_t           lastTemperature = 255;

// Khởi tạo cảm biến độ ẩm nhiệt độ
DHT dht(DHTPIN, DHTTYPE, 15);

// Khởi tạo thư viện để kết nối wifi và MQTT server
WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(115200);
  delay(10);

  // Khởi tạo thư viện cho cảm biến DHT
  Wire.begin();
  dht.begin();
  
  // Set AMUX Selector pins as outputs
  pinMode(AMUXSEL0 , OUTPUT);
  pinMode(AMUXSEL1 , OUTPUT);
  pinMode(AMUXSEL2 , OUTPUT);

  /*
   * Set target motor RPM.
   * These motors can do up to about 200rpm.
   * Too high will result in a high pitched whine and the motor does not move.
   */
  stepper.setRPM(50);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to wifi");

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  // Kết nối tới MQTT server
  client.setServer(mqttServer, mqttPort);
  // Đăng ký hàm sẽ xử lý khi có dữ liệu từ MQTT server gửi về
  client.setCallback(onMQTTMessageReceived);

  launchWeb(0);
}

// the loop routine runs over and over again forever:
void loop() {
  /*
  if (!client.connected()) {
    if (client.connect(clientId, mqttUsername, mqttPassword)) {
      Serial.println("MQTT server reconnected");
      client.loop();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(60000);
      return;
    }
  } else {
    client.loop();  
  }

  updateTempAndHumid();

  updateMoistureSensor();
  */

  /*
  // read the input on analog pin 0:
  for(int i=0; i<8; i++) {
    int sensorValue = readAnalogIn(i);
    Serial.print("Value of channel "); Serial.print(i); Serial.print(" is: ");
    Serial.println(sensorValue);
    delay(1000);
  }  
  */
  //delay(900000);        // delay in between reads for stability
  delay(5000);        // delay in between reads for stability

  // control motor
  controlMotor();
  
}

void sendToThinkSpeak(int field, float value) {
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    return;
  }

  String url = "/update?key=";
  url+=writeAPIKey;
  url+="&field1=";
  url+=String(value);
  url+="\r\n";
  
  // Request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
}


int readAnalogIn(int chan) {
  yield();
  switch(chan) {
    case 0:
      //Set 8-1 amux to position 0
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
     break;
    case 1:
      //Set 8-1 amux to position 1
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 2:
      //Set 8-1 amux to position 2
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 3:
      //Set 8-1 amux to position 3
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 4:
      //Set 8-1 amux to position 4
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 5:
      //Set 8-1 amux to position 5
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 6:
      //Set 8-1 amux to position 6
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 7:
      //Set 8-1 amux to position 7
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    default:
      break;
  }
  return analogRead(A0); //Read ESP8266 analog input
}

void updateTempAndHumid() { 
  char buf[10];
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();  
  Serial.print("Do am (%) hien tai: "); Serial.println(humidity);
  Serial.print("Nhiet do ('C) hien tai: "); Serial.println(temperature);

  if (!isnan(temperature) && !isnan(humidity))
  {
    // Update sensor information to MQTT server
    dtostrf(humidity, 4, 1, buf);
    client.publish(humidityTopic, buf);
    delay(200);
    dtostrf(temperature, 4, 1, buf);
    client.publish(tempTopic, buf);
    delay(200);
    Serial.println("Gui thong tin nhiet do va do am ve cho server");
  }
}

void updateMoistureSensor() {
  char buf[10];
  char topic[100];

  for(int i=0; i<8; i++) {
    int moistureValue = readAnalogIn(i);
    Serial.print("Do am cua A"); Serial.print(i+1); Serial.print(" la: ");
    Serial.println(moistureValue);    
    dtostrf(moistureValue, 4, 1, buf);
    String topicStr = moistureTopic + String(i+1);
    topicStr.toCharArray(topic, 100);
    client.publish(topic, buf);
    delay(1000);
  }
}

void controlMotor() {
  // energize coils - the motor will hold position
  stepper.enable();

  /*
   * Tell the driver the microstep level we selected.
   * If mismatched, the motor will move at a different RPM than chosen.
   */
  stepper.setMicrostep(MICROSTEPS);

  /*
   * Moving motor one full revolution using the degree notation
   */
  stepper.rotate(180);
  delay(2000);

  stepper.rotate(180);
  delay(2000);

  stepper.rotate(-180);
  delay(2000);

  stepper.rotate(-180);
  delay(2000);

  /*
   * Moving motor to original position using steps
   */
  //stepper.move(-200*MICROSTEPS);

  // pause and allow the motor to be moved by hand
  stepper.disable();
}


void launchWeb(int webtype) {
  /*
  if (webtype == 0) {
    server.on("/", []() {
      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      server.send(200, "application/json", "{\"IP\":\"" + ipStr + "\"}");
    });
    server.on("/cleareeprom", []() {
      String content;
      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>Clearing the EEPROM</p></html>";
      server.send(200, "text/html", content);
      Serial.println("clearing eeprom");
      //for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
      //EEPROM.commit();
    });
  }
  // Start the server
  server.begin();
  Serial.println("Server started"); 
  */
}

void onMQTTMessageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nhan duoc du lieu tu MQTT server [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
