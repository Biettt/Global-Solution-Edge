// Bibliotecas utilizadas
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Configurações de WiFi - Padrão Wokwi
const char *SSID = "Wokwi-GUEST";
const char *PASSWORD = "";  

// Configurações de MQTT - HiveMQ
const char *BROKER_MQTT = "broker.hivemq.com";
const int BROKER_PORT = 1883;
const char *ID_MQTT = "esp32_mqtt_gabs";
const char *TOPIC_PUBLISH_CARDIO = "gabs/iot/dados";

WiFiClient espClient;
PubSubClient MQTT(espClient);

const int potPinCardio = 25;
const int potPinGlicemia = 26;
// Parâmetros para detecção de queda - MPU6050
const float ACCELERATION_THRESHOLD = 2.0;
const int FALL_DURATION = 500;
const int FALL_RESET_DELAY = 2000;

const int cardioMin = 50;
const int cardioMax = 100;

Adafruit_MPU6050 mpu;
MPU6050 mpu6050;

bool isFallDetected = false;
unsigned long lastFallTime = 0;

// Conectando ao Wifi
void connectWiFi() {
  Serial.print("Conectando com a rede: ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso: ");
  Serial.println(SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Conectando ao MQTT
void connectMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);

  while (!MQTT.connected()) {
    Serial.print("Tentando conectar com o Broker MQTT: ");
    Serial.println(BROKER_MQTT);

    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao broker MQTT!");
    } else {
      Serial.println("Falha na conexão com MQTT. Tentando novamente em 2 segundos.");
      delay(2000);
    }
  }
}

// Definindo o setup
void setup() {
  Serial.begin(115200); 

  Wire.begin();

  connectWiFi();
  connectMQTT();
  mpu6050.initialize();

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050"); //Mensagem de erro para localizar o MPU6050
    while (1);
  }

  pinMode(potPinCardio, INPUT);
  pinMode(potPinGlicemia, INPUT);
}

void loop() {
  // Mudando os valores limites dos "sensores"
  int potValueCardio = int(potPinCardio); // Sensor de Batimentos Cardíacos
  int mappedValueCardio = map(potValueCardio, 0, 4095, 0, 200); 

  int potValueGlicemia = int(potPinGlicemia); // Sensor de Nível de Glicose no sangue
  int mappedValueGlicemia = map(potValueGlicemia, 0, 4095, 0, 500);

  // Leitura dos valores do acelerômetro
  int16_t ax, ay, az;
  mpu6050.getAcceleration(&ax, &ay, &az);

  // Cálculo do vetor de aceleração total
  float totalAcceleration = sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));

    // Verificar se a queda foi detectada
  if (totalAcceleration < ACCELERATION_THRESHOLD) {
    if (!isFallDetected) {
      isFallDetected = true;
      lastFallTime = millis();
    } else {
      unsigned long elapsedTime = millis() - lastFallTime;
      if (elapsedTime >= FALL_DURATION) {
        // Queda acidental detectada!
        Serial.println("Queda acidental detectada!");
        // Realizar ações necessárias
        delay(FALL_RESET_DELAY);
        isFallDetected = false;

        // Publicar mensagem de queda no MQTT Broker
        if (MQTT.connected()) {
          MQTT.publish(TOPIC_PUBLISH_CARDIO, "Queda detectada");
          Serial.println("Queda detectada. Mensagem publicada no tópico MQTT: " + String(TOPIC_PUBLISH_CARDIO));
        }
      }
    }
  } else {
    isFallDetected = false;
  }


  // Configurando a exibição dos valores na Porta Serial
  if (mappedValueCardio < cardioMin) {
    Serial.print("Risco de Bradicardia - ");
    Serial.print(mappedValueCardio);
    Serial.println(" bpm");
  } else if (mappedValueCardio > cardioMax) {
    Serial.print("Risco de Taquicardia - ");
    Serial.print(mappedValueCardio);
    Serial.println(" bpm");
  } else {
    Serial.print("Batimentos normais - ");
    Serial.print(mappedValueCardio);
    Serial.println(" bpm");
  }

  if (mappedValueGlicemia < 70) {
    Serial.print("Paciente Hipoglicemico - ");
    Serial.print(mappedValueGlicemia);
    Serial.println(" mg/dL");
  } else if (mappedValueGlicemia >= 71 && mappedValueGlicemia <= 99) {
    Serial.print("Glicemia normal - ");
    Serial.print(mappedValueGlicemia);
    Serial.println(" mg/dL");
  } else if (mappedValueGlicemia >= 100 && mappedValueGlicemia <= 125) {
    Serial.print("Paciente Pré-Diabético - ");
    Serial.print(mappedValueGlicemia);
    Serial.println(" mg/dL");
  } else {
    Serial.print("Paciente Diabético - ");
    Serial.print(mappedValueGlicemia);
    Serial.println(" mg/dL");
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  /* Print out the values */
  Serial.print("Aceleração: ");
  Serial.print(a.acceleration.x);
  Serial.print(",");
  Serial.print(a.acceleration.y);
  Serial.print(",");
  Serial.print(a.acceleration.z);
  Serial.print(", ");
  Serial.print("Giroscópio: ");
  Serial.print(g.gyro.x);
  Serial.print(",");
  Serial.print(g.gyro.y);
  Serial.print(",");
  Serial.print(g.gyro.z);
  Serial.println("");
  Serial.print("Temperatura: ");
  if (temp.temperature < 35.99){
    Serial.print("Hipotermina - ");
    Serial.print(temp.temperature);
    Serial.println("°C");
  } else if (temp.temperature > 36 || temp.temperature < 37.50){
    Serial.print("Temperatura Normal - ");
    Serial.print(temp.temperature);
    Serial.println("°C");
  } else if (temp.temperature > 37.51 || temp.temperature < 39.50){
    Serial.print("Febre - ");
    Serial.print(temp.temperature);
    Serial.println("°C");
  } else if (temp.temperature > 39.51 || temp.temperature < 41){
    Serial.print("Febre Alta - ");
    Serial.print(temp.temperature);
    Serial.println("°C");
  } else if(temp.temperature > 41.01){
    Serial.print("Hipertermia - ");
    Serial.print(temp.temperature);
    Serial.println("°C");
  } else {
    Serial.print("Temperatura não detectada");
  }
  Serial.println("---------------------------");
  
  // Publicando no Tópico do MQTT Broker
  if (MQTT.connected()) {
    String data = "{\"cardio\": " + String(mappedValueCardio) +
                  ", \"glicemia\": " + String(mappedValueGlicemia) +
                  ", \"acceleration\": " + String(totalAcceleration) +
                  ", \"temperature\": " + String(temp.temperature) + "}";
    MQTT.publish(TOPIC_PUBLISH_CARDIO, data.c_str());
    Serial.println("Data published to MQTT topic: " + String(TOPIC_PUBLISH_CARDIO));
  }

  delay(800); // Definindo intervalo entre escaneamento e publicação (Ciclo)
}