# 1 "C:\\Users\\admin\\AppData\\Local\\Temp\\tmpy4pwaq8q"
#include <Arduino.h>
# 1 "C:/Users/admin/Documents/Projects/SOARS-1/src/main.ino"




#include <SPL06-007.h>
#include <Adafruit_ADXL375.h>
#include "Adafruit_SGP30.h"
#include "Adafruit_SHT4x.h"
#include <Adafruit_Sensor.h>
#include "esp_camera.h"
#include "Arduino.h"
#include <Wire.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

String header = "Time ,Temperature ,Humidity ,Raw H2 ,Raw Ethanol ,CO2 ,VOC,X ,Y ,Z ,Magnitude ,Pressure ,Altitude ,Relative Noise";
String defaultDataFileName = "data";
String defaultImageDiretoryName = "/images";
double localPressure = 1015.3;
String dataPath = "/" + defaultDataFileName + ".csv";
bool firstBoot = true;
int frameCounter = 0;
int frameRate = 150;
const byte noiseSensorAddress = 0x38;
uint16_t ADC_VALUE = 0;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_ADXL375 accel = Adafruit_ADXL375(12345);
Adafruit_SGP30 sgp3;
SPIClass spi = SPIClass(HSPI);
void log(String message);
uint16_t getNoise();
void initSensors();
void initCamera();
void write(String imagePath, camera_fb_t * image, String data);
String takeReadings();
void setup();
void loop();
#line 32 "C:/Users/admin/Documents/Projects/SOARS-1/src/main.ino"
void log(String message) {
    Serial.println(String(millis()) + " " + message);
}

uint16_t getNoise() {
    Wire.beginTransmission(noiseSensorAddress);
    Wire.write(0x05);
    Wire.endTransmission();
    Wire.requestFrom(noiseSensorAddress, 2);
    while (Wire.available()) {
        uint8_t ADC_VALUE_L = Wire.read();
        uint8_t ADC_VALUE_H = Wire.read();
        ADC_VALUE=ADC_VALUE_H;
        ADC_VALUE<<=8;
        ADC_VALUE|=ADC_VALUE_L;
        return(ADC_VALUE);
    }
    uint16_t x=Wire.read();
}

void initSensors() {
    if (! sgp3.begin()) {
        Serial.println("SGP Sensor not found :(");
        while(1);
    }

    if (! sht4.begin()) {
        Serial.println("SHT Sensor not found :(");
        while(1);
    }

    if(! accel.begin()) {
        Serial.println("No ADXL375 detected :(");
        while(1);
    }

    SPL_init(0x77);
    if (get_spl_id() != 16) {
        log("SPL06-007 not found :(");
        while(1);
    }

    Wire.beginTransmission(noiseSensorAddress);
    if (Wire.endTransmission() != 0) {
        Serial.println("No noise sensor found :(");
        while (1);
    }
}

void initCamera() {

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    if(psramFound()){
      config.frame_size = FRAMESIZE_UXGA;
      config.jpeg_quality = 20;
      config.fb_count = 2;
      log("PSRAM found");
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
      log("PSRAM not found");
    }

    config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void write(String imagePath, camera_fb_t * image, String data) {

    spi.begin(14, 12, 13, 5);
    if (!SD.begin(5, spi)) { return; } else { }
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){ return; } else { }


    if (!SD.exists(defaultImageDiretoryName)) { SD.mkdir(defaultImageDiretoryName); } else { }
    if (SD.exists(imagePath)) { SD.remove(imagePath); } else { }
    File file = SD.open(imagePath, FILE_WRITE);
    if (!file) { return; } else { }


    log("Writing Frame Buffer to file");
    file.write(image->buf, image->len);
    file.close();
    log("Frame buffer written");

    if (!SD.exists("/data")) { SD.mkdir("/data"); } else { }
    if(SD.exists(dataPath) and (firstBoot == true)) {
        firstBoot = false;
        log("Data file already exists, creating a new one");
        int i = 1;
        while (SD.exists("/data/" + defaultDataFileName + String(i) + ".csv")) {
          i++;
        }
        dataPath = "/data/" + defaultDataFileName + String(i) + ".csv";
        File dataFile = SD.open(dataPath, FILE_WRITE);
        if (!dataFile) { return; } else { }
        dataFile.println(header);
        dataFile.close();
    }


    File dataFile = SD.open(dataPath, FILE_APPEND);
    if (!dataFile) { return; } else { }

    log("Writing sensor data to file");
    dataFile.println(data);
    dataFile.close();
    log("Sensor data written");


    SD.end();
    spi.end();


    pinMode(12, INPUT);
    pinMode(13, INPUT);
    pinMode(14, INPUT);
    pinMode(5, HIGH);
    log("SD Card pins reset");

    log("Done writing to SD Card");
}

String takeReadings() {



        sensors_event_t humidity, temp, event;

        sgp3.IAQmeasureRaw();
        sgp3.IAQmeasure();
        sht4.getEvent(&humidity, &temp);
        accel.getEvent(&event);
        float x = event.acceleration.x;
        float y = event.acceleration.y;
        float z = event.acceleration.z;
        float magnitude = sqrt(x*x + y*y + z*z);

        String data =
            String(millis()) + "," +
            String(temp.temperature) + "," +
            String(humidity.relative_humidity) + "," +
            String(sgp3.rawH2) + "," +
            String(sgp3.rawEthanol) + "," +
            String(sgp3.eCO2) + "," +
            String(sgp3.TVOC) + "," +
            String(x) + "," +
            String(y) + "," +
            String(z) + "," +
            String(magnitude) + "," +
            String(get_pressure()) + "," +
            String(get_altitude(get_pressure(),localPressure)) + "," +
            String(getNoise());

        return data;
}

void setup() {
    delay(5000);
    Serial.begin(115200);
    Wire.setPins(26, 27);
    Wire.begin();
    log("Starting...");

    initSensors();
    initCamera();

    log("Initilization Complete");
}

void loop() {
    if (millis() % frameRate == 0) {

        String data = takeReadings();

        String imagePath = defaultImageDiretoryName + "/frame" + String(frameCounter) + ".jpg";
        frameCounter++;

        camera_fb_t * fb = NULL;
        fb = esp_camera_fb_get();
        if (!fb) {
            log("Camera capture failed");
            return;
        } else {
            log("Camera capture successful");
        }

        write(imagePath, fb, data);

        esp_camera_fb_return(fb);
    }

    if (millis() % 10000 == 0) {
        uint16_t TVOC_base, eCO2_base;
        if (! sgp3.getIAQBaseline(&eCO2_base, &TVOC_base)) {
            log("Failed to get baseline readings");
            return;
        } else {
            log("Baseline readings retrieved");
        }
    }
}