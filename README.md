# esp32cam-demo
esp32cam module demo / project starting point sketch using Arduino ide


This is a starting point sketch for projects using the esp32cam development board with the following features
 *        web server with live video streaming
 *        sd card support (using 1bit mode to free some io pins)
 *        io pins available for use are 13, 12(must be low at boot)
 *        flash led is still available for use on pin 4 when using sd card
 
 I found it quiet confusing trying to do much with this module and so have tried to compile everything I have learned so far in to 
 a easy to follow sketch to encourage others to have a try with these powerful and VERY affordable modules...

created using the Arduino IDE with ESP32 module installed   (https://dl.espressif.com/dl/package_esp32_index.json)
No additional libraries required

----------------

How to use:

Enter your network details (ssid and password in to the sketch) and upload it to your esp32cam module
If you monitor the serial port (speed 15200) it will tell you what ip address it has been given.
If you now enter this ip address in to your browser you should be greated with the message "Hello from esp32cam"

If you now put /stream at the end of the url      i.e.   http://x.x.x.x/stream
It will live stream video from the camera

If you have an sd card inserted then accessing    http://x/x/x/x/photo
Will capture an image and save it to the sd card


----------------
