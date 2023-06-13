// Autor: Lucas Fonseca e Gabriel Fonseca
// Titulo: Sit arduino
//.........................................................................................................................

#include <DHT.h>
#include "integration.h"

// pinos
#define DHTPIN 21
#define ANEMOMETER_PIN 18
#define PLV_PIN 5
#define PIN_VANE 12
#define DHTTYPE DHT11  // Define o tipo de sensor (DHT11 ou DHT22)
// constants
#define INTERVAL 5000  // Intervalo de Tempo entre medições (ms)
#define DEBOUNCE_DELAY 25

// Anemometro (Velocidade do vento)
#define ANEMOMETER_CIRC (2 * 3.14159265 * 0.145)  // Circunferência anemometro (m)
unsigned long lastVVTImpulseTime = 0;
unsigned int anemometerCounter = 0;

// Pluviometro
#define VOLUME_PLUVIOMETRO 0.33  // Volume do pluviometro em ml
unsigned long lastPVLImpulseTime = 0;
unsigned rainCounter = 0;

// Biruta (Direção do vento)
#define NUMDIRS 8
unsigned long adc[NUMDIRS] = {26, 45, 77, 118, 161, 196, 220, 256};
char *strVals[NUMDIRS] = {"W", "NW", "N", "SW", "NE", "S", "SE", "E"};
unsigned int vane_dir = 0;
byte dirOffset = 0;

// Temperatura, umidade
DHT dht(DHTPIN, DHTTYPE);

// dto
struct
{
  float wind_speed = 0;
  float rain_acc = 0;
  float humidity = 0;
  float temperature = 0;
  float pressure = 0;
  int wind_dir = -1;
} Data;

const char* DataToJson(long startTime){
  const char* csv_header ="{ timestamp: %d, wind_speed: %.2f, rain_cc: %.2f, humidity: %.2f, temperature: %.2f }";
  char json_output[160];
  sprintf(json_output, csv_header, startTime + INTERVAL, Data.wind_speed, Data.rain_acc, Data.humidity,Data.temperature);
  return json_output;
}

const char* DataToCsv(long startTime){
  const char* csv_header ="timestamp,wind_speed,rain_cc,humidity,temperature\n%d,%.2f,%.2f,%.2f,%.2f";
  char csv_output[160];
  sprintf(csv_output, csv_header, startTime + INTERVAL, Data.wind_speed, Data.rain_acc, Data.humidity,Data.temperature);
  return csv_output;
}
//

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n Sistema Integrado de meteorologia \n");
  connectWifi();
  connectMqtt();
  // pinos
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  pinMode(PLV_PIN, INPUT_PULLUP);

  // initial setup
  int now = millis();
  lastVVTImpulseTime = now;
  lastPVLImpulseTime = now;
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerChange, FALLING);
  attachInterrupt(digitalPinToInterrupt(PLV_PIN), pluviometerChange, FALLING);
  dht.begin();
}


void loop() {
  Serial.println("\n");

  anemometerCounter = 0;
  rainCounter = 0;

  long startTime = millis();
  while (millis() < startTime + INTERVAL) {}

  // calc
  Data.wind_dir = getWindDir();
  Data.wind_speed = (ANEMOMETER_CIRC * anemometerCounter) / (INTERVAL / 1000.0f);  // em segundos
  Data.rain_acc = rainCounter * VOLUME_PLUVIOMETRO;
  DHTRead();

  // presentation
  Serial.print("....................\n");
  Serial.print("Velocidade do vento:  ");
  Serial.println(Data.wind_speed);
  Serial.print("Chuva acumulada....:  ");
  Serial.println(Data.rain_acc);
  Serial.print("Umidade............:  ");
  Serial.println(Data.humidity);
  Serial.print("Temperatura........:  ");
  Serial.print(Data.temperature);
  Serial.println("°C");
  Serial.print("Direção do vento...:  ");
  Serial.println(Data.wind_dir);

  const char* csv_output = DataToJson(startTime);
  // publish
  Serial.print("Enviando...........:  ");
  sendMeasurementToMqtt(csv_output);
  Serial.println("ok");
}

int getWindDir() {
  long long val, x, reading;
  val = analogRead(PIN_VANE);
  val >>= 2;                        // Shift to 255 range
  Serial.println(val);
  return 0;
  reading = val;
  for (x = 0; x < NUMDIRS; x++) {
    if (adc[x] >= reading)
      break;
  }

  return (x + dirOffset) % NUMDIRS;   // Adjust for orientation
}

void anemometerChange() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastVVTImpulseTime >= DEBOUNCE_DELAY) {
    anemometerCounter++;
    lastVVTImpulseTime = currentMillis;
  }
}

void pluviometerChange() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastPVLImpulseTime >= DEBOUNCE_DELAY) {
    rainCounter++;
    lastPVLImpulseTime = currentMillis;
  }
}

void DHTRead() {

  float humidity = dht.readHumidity();        // umidade relativa
  float temperature = dht.readTemperature();  //  temperatura em graus Celsius

  // Verifica se alguma leitura falhou
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Falha ao ler o sensor DHT!");
    return;
  }

  Data.humidity = humidity;
  Data.temperature = temperature;
}