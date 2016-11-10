
#include <ESP8266WiFi.h>
#include <string.h>
#include <PubSubClient.h>
#include <Servo.h> 

// Analog multiplexer
#define AMUXSEL0 D5     // AMUX Selector 0
#define AMUXSEL1 D6     // AMUX Selector 1
#define AMUXSEL2 D7     // AMUX Selector 2

// Khai báo các chân nối với động cơ servo, relay và nút nhấn
#define SERVO_PIN D1
#define RELAY_PIN D2
#define BUTTON_PIN D8

// Cấu hình Wifi, sửa lại theo đúng mạng Wifi của bạn
const char* WIFI_SSID = "Sandiego";
const char* WIFI_PWD = "0988807067";


// Cấu hình cho giao thức MQTT
const char* clientId = "Autogardener1";
const char* mqttServer = "192.168.1.110";
const int mqttPort = 1883;
// Username và password để kết nối đến MQTT server nếu server có
// bật chế độ xác thực trên MQTT server
// Nếu không dùng thì cứ để vậy
const char* mqttUsername = "<MQTT_BROKER_USERNAME>";
const char* mqttPassword = "<MQTT_BROKER_PASSWORD>";

// Tên MQTT topic để gửi thông tin về độ ẩm
String moistureTopic = "/easytech.vn/Garden/Moisture";
// Tên MQTT topic để gửi thông tin về relay
const char* relayStatusTopic = "/easytech.vn/Garden/Relay/Status";
const char* relayCmdTopic = "/easytech.vn/Garden/Relay/Command";

// Khai báo tần suất cập nhật dữ liệu là 10 phút 1 lần
const int UPDATE_INTERVAL = 20000; //1 * 60 * 1000;
unsigned long lastSentToServer = 0;

// Khai báo thời gian bật relay tưới nước
const int WATER_INTERVAL = 20000;
unsigned long wateringStartTime = 0;
int relayStatus = false;

// Khởi tạo module điều khiển động cơ servo
Servo servo;

// Khởi tạo thư viện để kết nối wifi và MQTT server
WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);
 
  // Thiết lập chế độ hoạt động các chân
  
  // Các chân nối điều khiển Analog Multiplexer và relay sẽ là output
  pinMode(AMUXSEL0, OUTPUT);
  pinMode(AMUXSEL1, OUTPUT);
  pinMode(AMUXSEL2, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);

  // Chân nhận tín hiệu từ nú nhấn sẽ là Input
  pinMode(BUTTON_PIN, INPUT);

  // Mặc định tắt relay khi khởi động
  digitalWrite(RELAY_PIN, LOW);

  // Khởi tạo động cơ servo và trả về vị trí ban đầu
  servo.attach(SERVO_PIN);
  servo.write(0);

  // Kết nối Wifi và chờ đến khi kết nối thành công
  Serial.print("Dang ket noi wifi");
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    count++;

    if (count > 20) {
      ESP.restart();
    }
  }

  Serial.println("thanh cong");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Kết nối tới MQTT server
  client.setServer(mqttServer, mqttPort);
  // Đăng ký hàm sẽ xử lý khi có dữ liệu từ MQTT server gửi về
  client.setCallback(onMQTTMessageReceived);
}

void loop() {
  // Kiểm tra kết nối tới MQTT server
  if (!client.connected()) {
    if (client.connect(clientId, mqttUsername, mqttPassword)) {
      Serial.println("Ket noi toi MQTT server thanh cong");
      // Đăng ký nhận lệnh bật tắt relay từ server qua MQTT topic
      client.subscribe(relayCmdTopic);
      client.loop();
    } else {
      Serial.print("Khong the ket noi toi MQTT server, error code =");
      Serial.print(client.state());
      delay(5000);
      return;
    }
  } else {
    client.loop();  
  }

  // Kiểm tra xem đến lúc gửi dữ liệu từ sensor về server hay chưa
  unsigned long currentMillis = millis();

  if (lastSentToServer == 0 || currentMillis - lastSentToServer >= UPDATE_INTERVAL) {
    updateMoistureSensor();
    lastSentToServer = currentMillis;
  }

  // Kiểm tra xem đến lúc tắt tưới nước chưa nếu đang tưới nước
  if (relayStatus) {
    unsigned long currentMillis = millis();

    if (currentMillis - wateringStartTime >= WATER_INTERVAL) {
      stopWatering();      
    }  
  }
  

  // Kiểm tra xem nút nhấn có được nhấn hay không
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == HIGH) {
    // Nếu nút được nhấn thì bật tưới nước
    startWatering();
    delay(500);
  }
  
  delay(50);
}

void updateMoistureSensor() {
  char buf[10];
  char topic[100];

  for(int i=0; i<4; i++) {
    float moistureValue = readAnalogInput(i)*100/1023;
    Serial.print("Do am cua A"); Serial.print(i+1); Serial.print(" la: ");
    Serial.println(moistureValue);    
    dtostrf(moistureValue, 4, 1, buf);
    String topicStr = moistureTopic + String(i+1);
    Serial.println(topicStr);
    Serial.println(buf);
    topicStr.toCharArray(topic, 100);
    client.publish(topic, buf);
    delay(50);
  }
}

/*
 * Hàm đọc giá trị analog từ analog multiplexer
 * 
 */
int readAnalogInput(int chan) {
  yield();
  // Thiết lập 3 chân điều khiển multiplexer tương ứng với analog channel cần đọc
  switch(chan) {
    case 0:
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
     break;
    case 1:
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 2:
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 3:
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 0);
      delay(100);
      break;
    case 4:
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 5:
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 0);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 6:
      digitalWrite(AMUXSEL0, 0);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    case 7:
      digitalWrite(AMUXSEL0, 1);
      digitalWrite(AMUXSEL1, 1);
      digitalWrite(AMUXSEL2, 1);
      delay(100);
      break;
    default:
      break;
  }
  
  return analogRead(A0);
}

void startWatering() {
  digitalWrite(RELAY_PIN, HIGH);
  client.publish(relayStatusTopic, "1");
  relayStatus = true;
  wateringStartTime = millis();
  /*
  for(int i=0; i<=180; i++) {
    servo.write(i); 
    i += 15;
    delay(2000);  
  }
  */ 
  return;
}

void stopWatering() {
  digitalWrite(RELAY_PIN, LOW);
  servo.write(0);
  client.publish(relayStatusTopic, "0");   
  wateringStartTime = 0;
  relayStatus = false;
  return;
}

void onMQTTMessageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Nhan duoc du lieu tu MQTT server [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, relayCmdTopic) == 0) {
    if ((char)payload[0] == '1') {
      Serial.println("Nhan duoc lenh bat relay");
      startWatering();
    } else if ((char)payload[0] == '0') {
      Serial.println("Nhan duoc lenh tat relay");
      stopWatering();
    } else {
      Serial.print("Lenh nhan duoc khong hop le: "); Serial.println(payload[0]);
    }
  }
}
