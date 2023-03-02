// NOTE: If you are trying to compile this code there will be a type conflict error 
// with the Adafruit sensor library and the ESP32 Camera Library. 
// You must change the type "sensor_t" in all related adafruit libraries to "adafruit_sensor_t" in order to compile.
/* --- Headers --- */
#include <string>
#include <Wire.h>
#include <Adafruit_ADXL375.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_SGP30.h"
#include "Adafruit_SHT4x.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

bool firstBoot = true;
int frameCounter = 0;
String header = "Time ,Temperature ,Humidity ,Raw H2 ,Raw Ethanol ,CO2 ,VOC, X, Y, Z, Magnitude";
String defaultDataFileName = "data";
String dataPath = "/" + defaultDataFileName + ".csv";
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_ADXL375 accel = Adafruit_ADXL375(12345);
Adafruit_SGP30 sgp3;
SPIClass spi = SPIClass(HSPI);

void log(String message) { //Profiling and debugging function
    Serial.println(String(millis()) + " " + message);
}

void initSensors() {
    if (! sgp3.begin()) { // initialize SGP30
        Serial.println("SGP Sensor not found :(");
        while(1);
    }

    if (! sht4.begin()) { // initialize SHT4x
        Serial.println("SHT Sensor not found :(");
        while(1);
    }

    if(! accel.begin()) {
        Serial.println("Ooops, no ADXL375 detected ... Check your wiring!");
        while(1);
    }
}

void initCamera() {
    //Initialize Camera Pins (AI-Thinker ESP32-CAM Pinout)
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
      config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
      config.jpeg_quality = 20;
      config.fb_count = 2;
      log("PSRAM found");
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
      log("PSRAM not found");
    }
    //config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config); //Initialize the camera & store the returned code
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void write(String imagePath, camera_fb_t * image, String data) { 
    //Because the SD Card is on the same SPI bus as the camera, we need to reinitialize the SPI bus
    spi.begin(14, 12, 13, 5); //SCK, MISO, MOSI, SS on CWV module
    if (!SD.begin(5, spi)) { return; } else { } //Initialize SD Card on SPI bus
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){ return; } else { } //Check if SD Card is present

    //Open the image file
    if (SD.exists(imagePath)) { SD.remove(imagePath); } else { } //If the file already exists, delete it
    File file = SD.open(imagePath, FILE_WRITE);
    if (!file) { return; } else { } //If the file doesn't open, return

    //Write the image to the file
    log("Writing Frame Buffer to file");
    file.write(image->buf, image->len);
    file.close();
    log("Frame buffer written");

    if(SD.exists(dataPath) and (firstBoot == true)) { //Check if the data file already exists and create a new one if it does
        firstBoot = false;
        log("Data file already exists, creating a new one");
        int i = 1;
        while (SD.exists("/" + defaultDataFileName + String(i) + ".csv")) {
          i++;
        }
        dataPath = "/" + defaultDataFileName + String(i) + ".csv";
        File dataFile = SD.open(dataPath, FILE_WRITE);
        if (!dataFile) { return; } else { } //If the file doesn't open, return
        dataFile.println(header);
        dataFile.close();
    }

    File dataFile = SD.open(dataPath, FILE_APPEND);
    if (!dataFile) { return; } else { } //If the file doesn't open, return
    //Write the data to the file
    log("Writing sensor data to file");
    dataFile.println(data);
    dataFile.close();
    log("Sensor data written");

    //End the SPI and SD card buses
    SD.end();
    spi.end();

    //Reset the SD Card pins for the Camera to use.
    pinMode(12, INPUT);
    pinMode(13, INPUT);
    pinMode(14, INPUT);
    pinMode(5, HIGH); //Set the SD Card CS pin to high to prevent it from interfering with the camera
    log("SD Card pins reset");

    log("Done writing to SD Card");
}

String takeReadings() {
        
        //Measure Sensors
        
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
            String(magnitude);

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
    if (millis() % 150 == 0) {

        String data = takeReadings();

        String imagePath = "/frame" + String(frameCounter) + ".jpg"; //Create the path for the image
        frameCounter++; //Increment the frame counter
        
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