#include <ESP32Lib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

//Название Wi-Fi точки доступа
const char* ssid = "SSID";
//Пароль от Wi-Fi точки доступа
const char* password = "PASSWORD";
//IP-Адрес mqtt-сервера
const char* mqtt_server = "*..*.*.*";
//Уникальное название устройство в рамках сети
const char* hostname = "room0_Led";
const char* aliveTopic = "/device/room0/led/available";

const uint8_t countTopic = 5;
//топики, на которые будет реагировать устройство
const char* topics[] = {
  "/room0/led0/set",
  "/room0/led0/get",
  "/room0/t/get",
  "/room0/h/get",
  "/device/room0/led0/reset"
};

// DHT Sensor
const uint8_t DHTPin = 15;
//Пин, к которому подключена лента
const int ledPin = 22;
ESP32Lib esp;
PWM ch0(0, ledPin);

// инициализация dht11
DHT dht(DHTPin, DHT11);

//целевое значение освещенности
uint16_t brightness = 0;
//предыдущее значение освещенности
uint16_t lastBrightness = 0;

//Инициализация Wi-Fi
void initWiFi() {
  //Удаляем предыдущее настройки Wi-Fi
  WiFi.disconnect(true);
  //Режим клиента
  WiFi.mode(WIFI_STA);
  //Подключаемся к точке
  WiFi.begin(ssid, password);
  //Задержка на подключение
  delay(1000);
  //Подписываемся на события Wi-Fi для возможности переподключения
  WiFi.onEvent(WiFiEvent);
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event)
  {
    //Успешно подключились, выводим IP
    case SYSTEM_EVENT_STA_CONNECTED:
      //ждем для получения IP-адреса
      delay(1000);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    //Отключились от точки доступа, переподключаемся
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection. Reconnect...");
      initWiFi();
      break;
  }
}
//фактически устанавливаем значение
void ledWorker() {
  ch0.analogWrite(constrain(brightness, 0, 8192));
}
//Обработчик топиков
void callback(char* topic, byte* message, unsigned int length) {
  String s = String(topic);
  //пришла команда перезагрузки
  if (s == String("/device/room0/led0/reset")) {
    ESP.restart();
  }
  //устанавливаем значение освещения
  else if (s == String("/room0/led0/set")) {
    String val;
    //считываем полностью сообщение и преобразуем к числу
    for (int i = 0; i < length; i++) {
      val += (char)message[i];
    }
    brightness = val.toInt();
    //Публикуем ответное сообщение об "установленном" значении
    esp.MQTTPublish("/room0/led0", constrain(brightness, 0, 8192));
  }
  //Пришел запрос на текущее значение освещенности
  else if (s == String("/room0/led0/get")) {
    esp.MQTTPublish("/room0/led0", constrain(brightness, 0, 8192));
  }
  else if (s == String( "/room0/t/get")) {
    esp.MQTTPublish("/room0/t", (int)(dht.readTemperature() * 100)); //Передаем температуру в целочисленном виде
  }
  else if (s == String("/room0/h/get")) {
    esp.MQTTPublish("/room0/h", (int)(dht.readHumidity() * 100)); //Передаем влажность в целочесленном виде
  }
}

//Подписываемся на топики
void subTopics() {
  for (int i = 0; i < countTopic; i++) {
    esp.MQTTSubscribe(topics[i]);
  }
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  esp.initOTA(hostname);
  esp.initMQTT(hostname, mqtt_server, callback);
  delay(5000);
  subTopics();
  dht.begin();
}

unsigned long lastLed = 0;
void loop() {
  //Если mqtt не подключен, переподключаемся и переподписываемя на топики
  if (!esp.MQTTConnected()) {
    esp.MQTTreconnect();
    subTopics();
  }
  //обработчик библиотеки
  esp.handle();
  //Раз в 50 мс обновляем значение выводимое на светодиодную ленту
  if (abs(millis() - lastLed) > 50) {
    lastLed = millis();
    ledWorker();
  }
}
