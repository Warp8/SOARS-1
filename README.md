# SOARS-1
SOARS - Stockbridge Observatory and Research Satellite

Team: Jack Hammerberg & Shay Handshoe

This code is developed for our satellite and is designed to work in combination with the MaxIQ kit. 
If you want to replicate our hardware configuration you will need:

- A Fat32 format SD card
- An OV2640 Camera sensor
- Adafruit SGP30, SHT40, and ADXL375 sensors
- The SPL-06-007 Altimeter
- And the Zio Qwiic connect loudness sensor.

This is meant to study the flight of the rocket, gas in the upper atmosphere and record video of the event.

You'll also have to solder and wire the MaxIQ GPIO expansion chip.

This is what your hardware configuration shoud look like.

![IMG_20230309_074607](https://user-images.githubusercontent.com/22381811/224048804-50c15c5f-3fe9-4aca-bea1-427a61e08cc5.jpg)
![IMG_20230309_074651](https://user-images.githubusercontent.com/22381811/224048817-7842ffaf-bc43-4368-856f-4d46da78c1e9.jpg)
![IMG_20230302_092925](https://user-images.githubusercontent.com/22381811/224048824-212e9c99-7d33-4bfe-89b3-bfb9acb57bb7.jpg)

Here's the STL to the expansion board thing I mounted the sensors and camera to. I printed it on my Ender 3 and sliced it with inside tolerences.
https://www.dropbox.com/s/zui7gxph093zsvh/Expansion%20Board.stl?dl=0

If you're having trouble compiling, look at the comments at the top of the code.
