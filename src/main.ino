// NOTE: If you are trying to compile this code there will be a type conflict error 
// with the Adafruit sensor library and the ESP32 Camera Library. 
// You must change the type "sensor_t" in all related adafruit libraries to "adafruit_sensor_t" in order to compile.
/* --- Headers --- */
#include <string>
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include "Adafruit_SHT4x.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

/* --- SD Card/SPI pins --- */
#define SPI_MISO 12
#define SPI_MOSI 13
#define SPI_SCK 14
#define SD_CS 5

/* --- I2C Lines for CWV Module --- */
#define I2C_SDA 26
#define I2C_SCL 27

bool firstBoot = true;
int frame = 0;
String header = "Time ,Temperature ,Humidity ,Raw H2 ,Raw Ethanol ,CO2 ,VOC";
String defaultDataFileName = "data";
String dataPath = "/" + defaultDataFileName + ".csv";
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SGP30 sgp3;
SPIClass spi = SPIClass(HSPI); //It's important this is outside of the setup function for some reason

void log(String message) { //Profiling and debugging function
    Serial.println(String(millis()) + " " + message);
}

void initSensors() {
    if (! sgp3.begin()) { // initialize SGP30
        Serial.println("SGP Sensor not found :(");
        while (1);
    }

    if (! sht4.begin()) { // initialize SHT4x
        Serial.println("SHT Sensor not found :(");
        while (1);
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
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config); //Initialize the camera & store the returned code
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void write(String imagePath, camera_fb_t * image, String data) { 
    //Because the SD Card is on the same SPI bus as the camera, we need to reinitialize the SPI bus
    spi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS); 
    if (!SD.begin(SD_CS, spi)) { return; } else { } //Initialize SD Card on SPI bus
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
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_MOSI, INPUT);
    pinMode(SPI_SCK, INPUT);
    pinMode(SD_CS, HIGH); //Set the SD Card CS pin to high to prevent it from interfering with the camera
    log("SD Card pins reset");

    log("Done writing to SD Card");
}

void setup() {
    delay(5000);
    Serial.begin(115200);
    log("Starting...");
    Wire.setPins(I2C_SDA, I2C_SCL);
    Wire.begin();

    initSensors();
    initCamera();

    log("Initilization Complete");
}

void loop() {
    if (millis() % 1000 == 0) {
        
        //Capture Frame
        
        camera_fb_t * fb = NULL;
        fb = esp_camera_fb_get();
        if (!fb) {
            log("Camera capture failed");
            return;
        } else {
            log("Camera capture successful");
        }

        String imagePath = "/frame" + String(frame) + ".jpg"; //Create the path for the image
        frame++; //Increment the frame counter
    
        //Measure Sensors
        
        sensors_event_t humidity, temp;
        
        sgp3.IAQmeasureRaw();
        sgp3.IAQmeasure();
        sht4.getEvent(&humidity, &temp);

        String data = "";
        data +=
            String(millis()) + "," +
            String(temp.temperature) + "," +
            String(humidity.relative_humidity) + "," +
            String(sgp3.rawH2) + "," +
            String(sgp3.rawEthanol) + "," +
            String(sgp3.eCO2) + "," +
            String(sgp3.TVOC);

        String dataPath = "/data.csv"; //Create the path for the data

        write(imagePath, fb, data); //.c_str() converts the String to a char array
        
        esp_camera_fb_return(fb); //Return the frame buffer to the camera
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