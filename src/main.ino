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

int frame = 0;
String header = "Time ,Temperature ,Humidity ,Raw H2 ,Raw Ethanol ,CO2 ,VOC";

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SGP30 sgp3;
SPIClass spi = SPIClass(HSPI); //It's important this is outside of the setup function for some reason

void writeHeader(String path, String header) {
    //Because the SD Card is on the same SPI bus as the camera, we need to reinitialize the SPI bus
    //Start the SD Card
    spi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS); 
    if (!SD.begin(SD_CS, spi)) { return; } else { } //Initialize SD Card on SPI bus
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){ return; } else { } //Check if SD Card is present

    //Open the image file and write the header if it doesn't exist
    File file = SD.open(path, FILE_WRITE);
    file.println(header);
    file.close();

    //Reset the SD Card pins for the Camera to use.
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_MOSI, INPUT);
    pinMode(SPI_SCK, INPUT);
    pinMode(SD_CS, INPUT);
    Serial.println("SD Card pins reset");

    //End the SPI and SD card buses
    SD.end();
    spi.end();
}

void write(String imagePath, camera_fb_t * image, String dataPath, const char * data) {
    int time = millis();
    //Because the SD Card is on the same SPI bus as the camera, we need to reinitialize the SPI bus
    //Start the SD Card
    spi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS); 
    if (!SD.begin(SD_CS, spi)) { return; } else { } //Initialize SD Card on SPI bus
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){ return; } else { } //Check if SD Card is present

    //Open the image file
    if (SD.exists(imagePath)) { SD.remove(imagePath); } else { } //If the file already exists, delete it
    File file = SD.open(imagePath, FILE_WRITE);
    if (!file) { return; } else { } //If the file doesn't open, return

    //Write the image to the file
    Serial.println("Writing Frame Buffer to file");
    file.write(image->buf, image->len);
    file.close();
    Serial.println("File written");

    //Open the data file
    File file2 = SD.open(dataPath, FILE_APPEND);
    if (!file2) { return; } else { } //If the file doesn't open, return

    //Write the data to the file
    Serial.println("Writing Data to file");
    file2.print(data);
    file2.close();
    Serial.println("File written");

    //Reset the SD Card pins for the Camera to use.
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_MOSI, INPUT);
    pinMode(SPI_SCK, INPUT);
    pinMode(SD_CS, INPUT);
    Serial.println("SD Card pins reset");

    //End the SPI and SD card buses
    SD.end();
    spi.end();
    Serial.print("Time Taken: "); Serial.println(millis()-time);
}

void setup() {
    Serial.begin(115200);
    Wire.setPins(I2C_SDA, I2C_SCL);
    Wire.begin();
    while (!Serial) { delay(1); } //waits for serial monitor to be opened, remove on final build
    delay(5000);
    Serial.println("Starting...");

    if (! sgp3.begin()) { // initialize SGP30
        Serial.println("SGP Sensor not found :(");
        while (1);
    }

    if (! sht4.begin()) { // initialize SHT4x
        Serial.println("SHT Sensor not found :(");
        while (1);
    }
    
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

    writeHeader("/data.csv", header); //Write the header to the data file

    Serial.println("Initilization Complete");
}

void loop() {
    if (millis() % 2000 == 0) {
        
        //Capture Frame
        
        camera_fb_t * fb = NULL;
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            return;
        }
        Serial.println("Camera capture successful");
        
        String imagePath = "/frame" + String(frame) + ".jpg"; //Create the path for the image
        frame++; //Increment the frame counter
    
        //Measure Sensors
        
        sensors_event_t humidity, temp;
        
        sgp3.IAQmeasureRaw();
        sgp3.IAQmeasure();
        sht4.getEvent(&humidity, &temp);

        String data = "";
        data +=
            String(millis()/1000) + "," +
            String(temp.temperature) + "," +
            String(humidity.relative_humidity) + "," +
            String(sgp3.rawH2) + "," +
            String(sgp3.rawEthanol) + "," +
            String(sgp3.eCO2) + "," +
            String(sgp3.TVOC) + "," + "\n";

        String dataPath = "/data.csv"; //Create the path for the data

        write(imagePath, fb, dataPath, data.c_str()); //.c_str() converts the String to a char array
        
        esp_camera_fb_return(fb); //Return the frame buffer to the camera
    }

    if (millis() % 10000 == 0) {
        uint16_t TVOC_base, eCO2_base;
        if (! sgp3.getIAQBaseline(&eCO2_base, &TVOC_base)) {
            Serial.println("Failed to get baseline readings");
            return;
        }
        Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
        Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
    }
}