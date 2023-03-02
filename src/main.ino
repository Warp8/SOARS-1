// NOTE: If you are trying to compile this code there will be a type conflict error 
// with the Adafruit sensor library and the ESP32 Camera Library. 
// You must change the type "sensor_t" in all related adafruit libraries to "adafruit_sensor_t" in order to compile.
// (Use the find and replace function in VScode)
/* --- Headers --- */
#include <SPL06-007.h> // Altitude (you may have to modify the I2C address in the library to 0x77)
#include <Adafruit_ADXL375.h> // IMU 
#include "Adafruit_SGP30.h" // Gas
#include "Adafruit_SHT4x.h" // Temp & Humidity
#include <Adafruit_Sensor.h> // Needed for Adafruit sensors
#include "esp_camera.h"
#include "Arduino.h"
#include <Wire.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

String header = "Time ,Temperature ,Humidity ,Raw H2 ,Raw Ethanol ,CO2 ,VOC,X ,Y ,Z ,Magnitude ,Pressure ,Altitude ,Relative Noise";
String defaultDataFileName = "data";
String defaultImageDiretoryName = "/images";
double localPressure = 1015.3; // Local pressure in hPa
String dataPath = "/" + defaultDataFileName + ".csv";
bool firstBoot = true;
int frameCounter = 0;
int frameRate = 150; // (Target) milliseconds between frames
const byte noiseSensorAddress = 0x38; //Zio Qwiic Noise Sensor Address
uint16_t ADC_VALUE = 0; //Noise Sensor Value
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_ADXL375 accel = Adafruit_ADXL375(12345);
Adafruit_SGP30 sgp3;
SPIClass spi = SPIClass(HSPI);

void log(String message) { //Profiling and debugging function
    Serial.println(String(millis()) + " " + message);
}

uint16_t getNoise() { //tbh I just copied this whole function from the example code
    Wire.beginTransmission(noiseSensorAddress);
    Wire.write(0x05); // Command for status
    Wire.endTransmission(); 
    Wire.requestFrom(noiseSensorAddress, 2); // Request 1 bytes from slave device noiseSensorAddress    
    while (Wire.available()) { // slave may send less than requested
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
    if (! sgp3.begin()) { // initialize SGP30
        Serial.println("SGP Sensor not found :(");
        while(1);
    }

    if (! sht4.begin()) { // initialize SHT4x
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
    if (!SD.exists(defaultImageDiretoryName)) { SD.mkdir(defaultImageDiretoryName); } else { }
    if (SD.exists(imagePath)) { SD.remove(imagePath); } else { }
    File file = SD.open(imagePath, FILE_WRITE);
    if (!file) { return; } else { } //If the file doesn't open, return

    //Write the image to the file
    log("Writing Frame Buffer to file");
    file.write(image->buf, image->len);
    file.close();
    log("Frame buffer written");

    if (!SD.exists("/data")) { SD.mkdir("/data"); } else { } //Create the data directory if it doesn't exist
    if(SD.exists(dataPath) and (firstBoot == true)) { //Check if the data file already exists and create a new one if it does
        firstBoot = false;
        log("Data file already exists, creating a new one");
        int i = 1;
        while (SD.exists("/data/" + defaultDataFileName + String(i) + ".csv")) {
          i++;
        }
        dataPath = "/data/" + defaultDataFileName + String(i) + ".csv";
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
        float magnitude = sqrt(x*x + y*y + z*z); //Absolute acceleration of the sensor

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

        String imagePath = defaultImageDiretoryName + "/frame" + String(frameCounter) + ".jpg"; //Create the path for the image
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