#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SHT31.h>
#include <PZEM004Tv30.h>
#include <Wifi.h>
// #include <FirebaseESP32.h>
#include <PubSubClient.h>
#include <FirebaseESP32.h>

#include <Fuzzy.h>
//#include <DHT.h>

#define API_KEY "AIzaSyAbBIkZKdUUZr-Nm5I1UAM991fiZOm37eY"
#define DATABASE_URL "https://project-st-iot-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

float voltage1, current1, power1, energy1, temp_energy1,frequency1, pf1, va1, VAR1;
float temp, hum, outfan, outfan2, dutyCycle;
int pwmValue;

const char *ssid = "panturas";
const char *pass = "skinhead";

TaskHandle_t shtTaskHandle;
TaskHandle_t pzemTaskHandle;
int lastRequest = 0;

void setupWifi();
void shtTask(void *parameter);
void pzemTask(void *parameter);


Adafruit_SHT31 sht31 = Adafruit_SHT31();

#if defined(ESP32)

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
// #define DHTPIN 23
// #define DHTTYPE DHT11
// DHT dht(DHTPIN, DHTTYPE);
#endif

 #define PZEM_SERIAL Serial2
#define Serial Serial
PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);


#elif defined(ESP8266)
/*************************
 *  ESP8266 initialization
 * ---------------------
 * 
 * esp8266 can connect with PZEM only via Serial0
 * For console output we use Serial1, which is gpio2 by default
 */
#define PZEM_SERIAL Serial
#define Serial Serial1
PZEM004Tv30 pzem(PZEM_SERIAL);
#else
/*************************
 *  Arduino initialization
 * ---------------------
 * 
 * Not all Arduino boards come with multiple HW Serial ports.
 * Serial2 is for example available on the Arduino MEGA 2560 but not Arduino Uno!
 * The ESP32 HW Serial interface can be routed to any GPIO pin 
 * Here we initialize the PZEM on Serial2 with default pins
 */
#define PZEM_SERIAL Serial2
#define Serial Serial
PZEM004Tv30 pzem(PZEM_SERIAL);
#endif

#define PIN_IN1  2 // ESP32 pin GPIO19 connected to the IN1 pin L298N
#define PIN_IN2  4 // ESP32 pin GPIO18 connected to the IN2 pin L298N
#define PIN_ENA  15 // ESP32 pin GPIO17 connected to the EN1 pin L298N
#define PIN_IN3  5 // ESP32 pin GPIO19 connected to the IN3 pin L298N
#define PIN_IN4  18 // ESP32 pin GPIO18 connected to the IN4 pin L298N
#define PIN_ENB  19 // ESP32 pin GPIO17 connected to the ENB pin L298N


Fuzzy *fuzzy = new Fuzzy();


  // Set up fuzzy variables
  // FuzzySet *temperatureDingin = new FuzzySet(0, 0, 20, 22);
  // FuzzySet *temperatureNormal = new FuzzySet(20, 22, 25, 26);
  // FuzzySet *temperaturePanas = new FuzzySet(25, 26, 30, 30);

  //   FuzzySet *temperatureDingin = new FuzzySet(0, 0, 25, 27);
  // FuzzySet *temperatureNormal = new FuzzySet(25, 27, 27, 30);
  // FuzzySet *temperaturePanas = new FuzzySet(27, 30, 40, 40);

    // Set up fuzzy variables
  FuzzySet *temperatureDingin = new FuzzySet(0, 0, 19, 21);
  FuzzySet *temperatureNormal = new FuzzySet(19, 21, 23, 25);
  FuzzySet *temperaturePanas = new FuzzySet(23, 25, 30, 30);

  FuzzySet *humidityKering = new FuzzySet(0, 0, 40, 45);
  FuzzySet *humidityLembab = new FuzzySet(40, 45, 60, 65);
  FuzzySet *humidityBasah = new FuzzySet(60, 65, 80, 80);

  // FuzzySet *pwmFanSlow = new FuzzySet(0, 0, 54, 106);
  // FuzzySet *pwmFanMedium = new FuzzySet(54, 106, 106, 212);
  // FuzzySet *pwmFanFast = new FuzzySet(106, 212, 255, 255);
  
  // FuzzySet *pwmFanSlow = new FuzzySet(0, 0, 30, 50);
  // FuzzySet *pwmFanMedium = new FuzzySet(30, 50, 127, 200);
  // FuzzySet *pwmFanFast = new FuzzySet(127, 255, 255, 255);


  // FuzzySet *pwmFanSlow = new FuzzySet(0, 0, 1024, 2048);
  // FuzzySet *pwmFanMedium = new FuzzySet(1024, 2048, 3072, 3584);
  // FuzzySet *pwmFanFast = new FuzzySet(3072, 3584, 4095, 4095);

  FuzzySet *pwmFanSlow = new FuzzySet(0, 0, 256, 384);
FuzzySet *pwmFanMedium = new FuzzySet(256, 384, 512, 768);
FuzzySet *pwmFanFast = new FuzzySet(512, 768, 1023, 1023);



  // FuzzySet *pwmFanSlow = new FuzzySet(0, 0, 320, 625);
  // FuzzySet *pwmFanMedium = new FuzzySet(320, 625, 625, 1250);
  // FuzzySet *pwmFanFast = new FuzzySet(625, 1250, 1250, 1250);

 




void setup() {
  Serial.begin(9600);
  sht31.begin();
  //dht.begin();
  //pinMode(relay, OUTPUT);
  //digitalWrite(relay, LOW);
  vTaskDelay(3000);

    setupWifi();


    ESP_LOGI("SETUP", "Create freertos task!");
    // Create RTOS task
    xTaskCreate(shtTask, "SHT30 Task", 20000, NULL, 1, &shtTaskHandle);
    xTaskCreate(pzemTask, "PZEM004T Task", 20000, NULL, 2, &pzemTaskHandle);



  // Uncomment in order to reset the internal energy counter
    // pzem.resetEnergy()
      // Setup PZEM-004T
  if (pzem.resetEnergy()){
    ESP_LOGI("SETUP PZEM-004T", "Energy Reset...");
    pzem.setPowerAlarm(2200);
  }
  vTaskDelay(3000);

    //kipas 1
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  //kipas 2
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);

  


  //WiFi Setup
WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    // Serial.print(".");
    //digitalWrite(32, HIGH);
    // delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wifi Connected");
    //digitalWrite(32, LOW);
  }

 config.api_key = API_KEY;
 config.database_url = DATABASE_URL;
 auth.user.email = "nazar@upi.edu";
 auth.user.password = "nazar123";
 Firebase.reconnectNetwork(true);
 fbdo.setBSSLBufferSize(4096, 1024);
 Firebase.begin(&config, &auth);
 Firebase.reconnectWiFi(true);
 Firebase.setDoubleDigits(5);

  FuzzySet ();

    // Fuzzy Input sht dan kelembaban
  FuzzyInput *temperature = new FuzzyInput(1);


  temperature->addFuzzySet(temperatureDingin);
  temperature->addFuzzySet(temperatureNormal);
  temperature->addFuzzySet(temperaturePanas);
  fuzzy->addFuzzyInput(temperature);


  //Fuzzy Input sht dan kelembaban
  FuzzyInput *humidity = new FuzzyInput(2);


  humidity->addFuzzySet(humidityKering);
  humidity->addFuzzySet(humidityLembab);
  humidity->addFuzzySet(humidityBasah);
  fuzzy->addFuzzyInput(humidity);

    //FuzzyOutput
  FuzzyOutput *fan = new FuzzyOutput(3);


  //fan->addFuzzySet(pwmFanOff);
  fan->addFuzzySet(pwmFanSlow);
  fan->addFuzzySet(pwmFanMedium);
  fan->addFuzzySet(pwmFanFast);
  fuzzy->addFuzzyOutput(fan);


  // Rule 1: Temperature Dingin AND Humidity Kering -> Fan Slow
    FuzzyRuleAntecedent *dinginkering = new FuzzyRuleAntecedent();
    dinginkering->joinWithAND(temperatureDingin, humidityKering);

    FuzzyRuleConsequent *kipas1 = new FuzzyRuleConsequent();
    kipas1->addOutput(pwmFanSlow);
    
    FuzzyRule *fuzzyRule01 = new FuzzyRule(1, dinginkering, kipas1);
    fuzzy->addFuzzyRule(fuzzyRule01);

    // Rule 2: Temperature Dingin AND Humidity Normal -> Fan Medium
    FuzzyRuleAntecedent *dinginlembab = new FuzzyRuleAntecedent();
    dinginlembab->joinWithAND(temperatureDingin, humidityLembab);
    FuzzyRuleConsequent *kipas2 = new FuzzyRuleConsequent();
    kipas2->addOutput(pwmFanMedium);
    FuzzyRule *fuzzyRule02 = new FuzzyRule(2, dinginlembab, kipas2);
    fuzzy->addFuzzyRule(fuzzyRule02);

    // Rule 3: Temperature Dingin AND Humidity basah -> Fan slow
    FuzzyRuleAntecedent *dinginbasah = new FuzzyRuleAntecedent();
    dinginbasah->joinWithAND(temperatureDingin, humidityBasah);
    FuzzyRuleConsequent *kipas3 = new FuzzyRuleConsequent();
    kipas3->addOutput(pwmFanSlow);
    FuzzyRule *fuzzyRule03 = new FuzzyRule(3, dinginbasah, kipas3);
    fuzzy->addFuzzyRule(fuzzyRule03);

    // Rule 4: Temperature Normal AND Humidity Kering -> Fan slow
    FuzzyRuleAntecedent *normalkering = new FuzzyRuleAntecedent();
    normalkering->joinWithAND(temperatureNormal, humidityKering);
    FuzzyRuleConsequent *kipas4 = new FuzzyRuleConsequent();
    kipas4->addOutput(pwmFanSlow);
    FuzzyRule *fuzzyRule04 = new FuzzyRule(4, normalkering, kipas4);
    fuzzy->addFuzzyRule(fuzzyRule04);

    // Rule 5: Temperature Normal AND Humidity lembab -> Fan Medium
    FuzzyRuleAntecedent *normallembab = new FuzzyRuleAntecedent();
    normallembab->joinWithAND(temperatureNormal, humidityLembab);
    FuzzyRuleConsequent *kipas5 = new FuzzyRuleConsequent();
    kipas5->addOutput(pwmFanMedium);
    FuzzyRule *fuzzyRule05 = new FuzzyRule(5, normallembab, kipas5);
    fuzzy->addFuzzyRule(fuzzyRule05);

    // Rule 6: Temperature Normal AND Humidity basah -> Fan slow
    FuzzyRuleAntecedent *normalbasah = new FuzzyRuleAntecedent();
    normalbasah->joinWithAND(temperatureNormal, humidityBasah);
    FuzzyRuleConsequent *kipas6 = new FuzzyRuleConsequent();
    kipas6->addOutput(pwmFanSlow);
    FuzzyRule *fuzzyRule06 = new FuzzyRule(6, normalbasah, kipas6);
    fuzzy->addFuzzyRule(fuzzyRule06);

    // Rule 7: Temperature Panas AND Humidity Kering -> Fan Fast
    FuzzyRuleAntecedent *panaskering = new FuzzyRuleAntecedent();
    panaskering->joinWithAND(temperaturePanas, humidityKering);
    FuzzyRuleConsequent *kipas7 = new FuzzyRuleConsequent();
    kipas7->addOutput(pwmFanFast);
    FuzzyRule *fuzzyRule07 = new FuzzyRule(7, panaskering, kipas7);
    fuzzy->addFuzzyRule(fuzzyRule07);

    // Rule 8: Temperature Panas AND Humidity lembab -> Fan Fast
    FuzzyRuleAntecedent *panaslembab = new FuzzyRuleAntecedent();
    panaslembab->joinWithAND(temperaturePanas, humidityLembab);
    FuzzyRuleConsequent *kipas8 = new FuzzyRuleConsequent();
    kipas8->addOutput(pwmFanFast);
    FuzzyRule *fuzzyRule08 = new FuzzyRule(8, panaslembab, kipas8);
    fuzzy->addFuzzyRule(fuzzyRule08);

    // Rule 9: Temperature Panas AND Humidity basah -> Fan Fast
    FuzzyRuleAntecedent *panasBasah = new FuzzyRuleAntecedent();
    panasBasah->joinWithAND(temperaturePanas, humidityBasah);
    FuzzyRuleConsequent *kipas9 = new FuzzyRuleConsequent();
    kipas9->addOutput(pwmFanFast);
    FuzzyRule *fuzzyRule09 = new FuzzyRule(9, panasBasah, kipas9);
    fuzzy->addFuzzyRule(fuzzyRule09);


    
  
}

void loop() {
  

 if (millis() - lastRequest > 10000) {
      if (WiFi.status() != WL_CONNECTED) {
          setupWifi();
      } else {
          ESP_LOGI("WIFI", "WiFi is already connected...");
      }
      lastRequest = millis();
  }

  digitalWrite(PIN_IN1, HIGH); // control the motor's direction in clockwise
  digitalWrite(PIN_IN2, LOW);  // control the motor's direction in clockwise
  digitalWrite(PIN_IN3, HIGH); // control the motor's direction in clockwise
  digitalWrite(PIN_IN4, LOW);  // control the motor's direction in clockwise
  

  //   float h = dht.readHumidity();
  // float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit
      // Read the data from the sensor
  void shtTask();
  void pzemTask();

  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();

  //fuzifikasi sht dan kelembaban
  fuzzy->setInput(1, temp);
  fuzzy->setInput(2, hum);
  fuzzy->fuzzify();

  float outfan = fuzzy->defuzzify(3);
  float outfan2 = fuzzy->defuzzify(3);

  analogWrite(PIN_ENA, outfan);
  analogWrite(PIN_ENB, outfan2);

  //   if (isnan(hum) || isnan(temp)) {
  //   Serial.println("Gagal membaca data suhu sht30!");
  //   return;
  // }

  Serial.print("Calculate Fan Speed PWM : ");
  Serial.println(outfan);
  Serial.print("Calculate Fan Speed PWM 2 : ");
  Serial.println(outfan2);



  // // Hitung Duty Cycle PWM
  // float dutyCycle = ((float)outfan / 1250) * 100.0;

  // // Konversi Duty Cycle ke nilai PWM (0-255)
  // int pwmValue = (dutyCycle / 100.0) * 255;

  // Serial.print("Hasil PWM = ");
  // Serial.print(pwmValue);
  // Serial.println(",");



      if(Firebase.ready()){
      FirebaseJson json;

      json.set("/temp", isnan(temp) ? 0 : temp);
      json.set("/hum", isnan(hum) ? 0 : hum);
      json.set("/outfan", isnan(outfan) ? 0 : outfan);
      json.set("/outfan2", isnan(outfan2) ? 0 : outfan2);
      json.set("/pwmValue", isnan(pwmValue) ? 0 : pwmValue);
      json.set("/voltage", isnan(voltage1) ? 0 : voltage1);
      json.set("/current", isnan(current1) ? 0 : current1);
      json.set("/power", isnan(power1) ? 0 : power1);
      json.set("/energy", isnan(energy1) ? 0 : energy1);
      json.set("/frequency",isnan(frequency1) ? 0 : frequency1);
      json.set("/pf", isnan(pf1) ? 0 : pf1);

      if (Firebase.updateNode(fbdo, "/devices/808184700/value", json))
      {
        Serial.println("Update-Succesfull");
      }else {
        Serial.println("Update-Failed");
        Serial.println(fbdo.errorReason());
      }
      
    }

    //Serial.println("===================END====================");
    Serial.println();

  delay(3000);
}




void shtTask(void *parameter) {
for (;;) {
    // Membaca suhu dan kelembapan
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();


    // Memeriksa apakah pembacaan berhasil
    if (!isnan(temp) && !isnan(hum)) {
      Serial.print("Data Suhu: ");
      Serial.print(temp);
      Serial.print(" °C");
      Serial.print(",");
      Serial.print("   ||    ");
      

      Serial.print("Data Kelembaban: ");
      Serial.print(hum);
      Serial.print(",");
      Serial.println(" %");
    } else {
      Serial.println(" Gagal membaca data dari sensor SHT30.");
    }

    // Tunda 3 detik sebelum pembacaan berikutnya
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
  
}

float zeroIfNan(float v) {
  if (isnan(v)) {
    v = 0;
  }
  return v;
}

void pzemTask(void *parameter){
  for (;;) {
    voltage1 = pzem.voltage();
    voltage1 = zeroIfNan(voltage1); // Voltage
    current1 = pzem.current();
    current1 = zeroIfNan(current1); // Current
    power1 = pzem.power();
    power1 = zeroIfNan(power1); // Power Active
    energy1 = pzem.energy();
    energy1 = temp_energy1+zeroIfNan(energy1); // Energy
    frequency1 = pzem.frequency();
    frequency1 = zeroIfNan(frequency1); // Frequency
    pf1 = pzem.pf();
    pf1 = zeroIfNan(pf1); // Cosine Phi

    if (pf1 == 0) {
      va1 = 0;
    } else {
      va1 = power1 / pf1;
    }
    if (pf1 == 0) {
      VAR1 = 0;
    } else {
      VAR1 = power1 / pf1 * sqrt(1 - sq(pf1));
    }
    
    ESP_LOGI("PZEM-004T", "Energy :  %.3f kWh", power1);

    // // Check if the data is valid
    // if(isnan(voltage1)){
    //     Serial.println("Error reading voltage");
    // } else if (isnan(current1)) {
    //     Serial.println("Error reading current");
    // } else if (isnan(power1)) {
    //     Serial.println("Error reading power");
    // } else if (isnan(energy1)) {
    //     Serial.println("Error reading energy");
    // } else if (isnan(frequency1)) {
    //     Serial.println("Error reading frequency");
    // } else if (isnan(pf1)) {
    //     Serial.println("Error reading power factor");
    // } else {

        // Print the values to the Serial console
        Serial.print("Voltage: ");      Serial.print(voltage1);      Serial.println(" V");
        Serial.print("Current: ");      Serial.print(current1);      Serial.println(" A");
        Serial.print("Power: ");        Serial.print(power1);        Serial.println(" W");
        Serial.print("Energy: ");       Serial.print(energy1,3);     Serial.println(" kWh");
        Serial.print("Frequency: ");    Serial.print(frequency1, 1); Serial.println(" Hz");
        Serial.print("PF: ");           Serial.println(pf1);

        // Serial.println(voltage1);
        // Serial.println(current1);
        // Serial.println(power1);
        // Serial.println(energy1,3);
        // Serial.println(frequency1,1);
        // Serial.println(pf1);



    //     //



    // }
    vTaskDelay(3000);
  }
  
}



void setupWifi() {
    vTaskDelay(10);
    // We start by connecting to a WiFi network
    ESP_LOGI("WIFI", "Connecting to %s", ssid);

    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500);
        ESP_LOGI("WIFI", ".");
    }
    ESP_LOGI("WIFI", "WiFi is connected!");
}
