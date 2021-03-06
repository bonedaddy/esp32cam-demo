 /*******************************************************************************************************************
 *            
 *                                 ESP32Cam development board demo sketch using Arduino IDE
 *                                    Github: https://github.com/alanesq/ESP32Cam-demo
 *     
 *     Starting point sketch for projects using the esp32cam development board with the following features
 *        web server with live video streaming
 *        sd card support (using 1bit mode to free some io pins)
 *        io pins available for use are 13, 12(must be low at boot)
 *        flash led is still available for use on pin 4
 * 
 *     
 *     - created using the Arduino IDE with ESP32 module installed   (https://dl.espressif.com/dl/package_esp32_index.json)
 *       No additional libraries required
 * 
 * 
 * 
 *     Info on the esp32cam board:  https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/
 *            
 *            
 * 
 *******************************************************************************************************************/


#include "esp_camera.h"       // https://github.com/espressif/esp32-camera
#include <WiFi.h>
#include <WebServer.h>


// ---------------------------------------------------------------
//                          -SETTINGS
// ---------------------------------------------------------------

  // Wifi settings (enter your wifi network details)
    const char* ssid     = "<your wifi network name here>";
    const char* password = "<your wifi password here>";

  const String stitle = "ESP32Cam-demo";                 // title of this sketch
  const String sversion = "26Sep20";                     // Sketch version

  const bool debugInfo = 1;                              // show additional debug info. on serial port (1=enabled)

  // Camera related
  const bool flashRequired = 1;                          // If flash to be used when capturing image (1 = yes)
  const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA;    // Image resolution:   
                                                         //               default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_XGA"
                                                         //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 
                                                         //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 
                                                         //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)

  const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

  const int indicatorLED = 33;                           // onboard status LED pin (33)

  const int brightLED = 4;                               // onboard flash LED pin (4)


  
// ******************************************************************************************************************


WebServer server(80);                       // serve web pages on port 80

#include "soc/soc.h"                        // Used to disable brownout detection on 5v power feed
#include "soc/rtc_cntl_reg.h"      

#include "SD_MMC.h"                         // sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
#include <SPI.h>                       
#include <FS.h>                             // gives file access 
#define SD_CS 5                             // sd chip select pin = 5
  
// Define global variables:
  uint32_t lastStatus = millis();           // last time status light changed status (to flash all ok led)
  bool sdcardPresent;                       // flag if an sd card is detected
  int imageCounter;                         // image file name counter

// camera type settings (CAMERA_MODEL_AI_THINKER)
  #define CAMERA_MODEL_AI_THINKER
  #define PWDN_GPIO_NUM     32      // power to camera enable
  #define RESET_GPIO_NUM    -1      // -1 = not used
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26      // i2c sda
  #define SIOC_GPIO_NUM     27      // i2c scl
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25      // vsync_pin
  #define HREF_GPIO_NUM     23      // href_pin
  #define PCLK_GPIO_NUM     22      // pixel_clock_pin

  camera_config_t config;
  
  
  

// ******************************************************************************************************************


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {
  
  // Serial communication 
    Serial.begin(115200);
  
    Serial.println(("\n\n\n---------------------------------------"));
    Serial.println("Starting - " + stitle + " - " + sversion);
    Serial.println(("---------------------------------------"));

  // Turn-off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Connect to wifi
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();

  // define web pages (call procedures when url requested)
    server.on("/", handleRoot);               // root page
    server.on("/stream", handleStream);       // stream live video
    server.on("/photo", handlePhoto);         // capture image and save to sd card
    server.onNotFound(handleNotFound);        // invalid url requested

  // Define io pins
    pinMode(indicatorLED, OUTPUT);
    digitalWrite(indicatorLED,HIGH);
    pinMode(brightLED, OUTPUT);
    digitalWrite(brightLED,LOW);

  // set up camera
      Serial.print(("Initialising camera: "));
      if (setupCameraHardware()) Serial.println("OK");
      else {
        Serial.println("Error!");
        showError(2);       // critical error so stop and flash led
      }

  // Configure sd card
      if (!SD_MMC.begin("/sdcard", true)) {         // if loading sd card fails     
        // note: ("/sdcard", true) = 1 wire - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
        Serial.println("No SD Card detected"); 
        sdcardPresent = 0;                    // flag no sd card available
      } else {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE) {          // if invalid card found
            Serial.println("SD Card type detect failed"); 
            sdcardPresent = 0;                // flag no sd card available
        } else {
          sdcardPresent = 1;                  // valid sd card found
          uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
          Serial.println("SD Card found, free space = " + String(SDfreeSpace) + "MB");  
          sdcardPresent = 1;                  // flag sd card available
        }
      }
      fs::FS &fs = SD_MMC;                    // sd card file system

  // discover number of image files stored /img of sd card and set image file counter accordingly
    if (sdcardPresent) {
      int tq=fs.mkdir("/img");              // create the "/img" folder on sd card if not already there
      if (!tq) Serial.println("Unable to create IMG folder on sd card");
      
      File root = fs.open("/img");
      while (true)
      {
        File entry =  root.openNextFile();
        if (! entry) break;
        imageCounter ++;    // increment image counter
        entry.close();
      }
      root.close();
      Serial.println("Image file count = " + String(imageCounter));
    }
   
  // re-define io pins as sd card config. seems to reset them
    pinMode(brightLED, OUTPUT);
    pinMode(indicatorLED, OUTPUT);

  if (!psramFound()) {
    Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
    framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_CIF;
  }

  Serial.println("Started...");
  
}  // setup



// ******************************************************************************************************************


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------


void loop() {

  server.handleClient();          // handle any web page requests


  
  
    
  // If not in triggered state flash status light to show sketch is running ok
    if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) { 
      lastStatus = millis();                                               // reset timer
      digitalWrite(indicatorLED,!digitalRead(indicatorLED));               // flip indicator led status
    }
    
}  // loop



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                       Flash indicator LED
// ----------------------------------------------------------------


void flashLED(int reps) {
  for(int x=0; x < reps; x++) {
    digitalWrite(indicatorLED,LOW);
    delay(1000);
    digitalWrite(indicatorLED,HIGH);
    delay(500);
  }
}


// critical error - stop sketch and continually flash error status 
void showError(int errorNo) {
  while(1) {
    flashLED(errorNo);
    delay(4000);
  }
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                save live camera image to sd card
//                   iTitle = file name to use
// ----------------------------------------------------------------


void storeImage(String iTitle) {

  if (debugInfo) Serial.println("Storing image #" + String(imageCounter) + " to sd card");

  fs::FS &fs = SD_MMC;                              // sd card file system

  // capture live image from camera
    // cameraImageSettings();                         // apply camera sensor settings (in camera.h)
  if (flashRequired) digitalWrite(brightLED,HIGH);  // turn flash on
  camera_fb_t *fb = esp_camera_fb_get();            // capture image frame from camera
  digitalWrite(brightLED,LOW);                      // turn flash off
  if (!fb) {
    Serial.println("Error: Camera capture failed");
    flashLED(3);
  }
  
  // save the image to sd card
    imageCounter ++;                                                              // increment image counter
    String SDfilename = "/img/" + iTitle + ".jpg";                                // build the image file name
    File file = fs.open(SDfilename, FILE_WRITE);                                  // create file on sd card
    if (!file) {
      Serial.println("Error: Failed to create file on sd-card: " + SDfilename);
      flashLED(4);
    } else {
      if (file.write(fb->buf, fb->len)) {                                         // File created ok so save image to it
        if (debugInfo) Serial.println("Image saved to sd card"); 
      } else {
        Serial.println("Error: failed to save image to sd card");
        flashLED(4);
      }
      file.close();                // close image file on sd card
    }
    esp_camera_fb_return(fb);        // return frame so memory can be released

} // storeImage



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                       Set up the camera
// ----------------------------------------------------------------


bool setupCameraHardware() {
  
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;               // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    config.pixel_format = PIXFORMAT_JPEG;         // PIXFORMAT_ + YUV422, GRAYSCALE, RGB565, JPEG, RGB888?
    config.frame_size = FRAME_SIZE_IMAGE;         // Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 
                                                  //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
    config.jpeg_quality = 5;                      // 0-63 lower number means higher quality
    config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG
    
    esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
    if (camerr != ESP_OK) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);

    // cameraImageSettings();                 // apply camera sensor settings
    
    return (camerr == ESP_OK);             // return boolean result of camera initilisation
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------
// Info on using html see https://www.arduino.cc/en/Reference/Ethernet 

void handleRoot() {

  WiFiClient client = server.client();                                                        // open link with client
  String tstr;                                                                                // temp store for building line of html

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Root page requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // html header
    client.write("<!DOCTYPE html> <html> <body>\n");

  // html body
    client.write("<p>Hello from esp32cam</p>\n");

  // end html
    client.write("</body></htlm>\n");
    delay(3);
    client.stop();

}  // handleRoot


// ******************************************************************************************************************


// ----------------------------------------------------------------
//       -photo save to sd card    i.e. http://x.x.x.x/photo
// ----------------------------------------------------------------

void handlePhoto() {

  WiFiClient client = server.client();                                                        // open link with client
  String tstr;                                                                                // temp store for building line of html

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Photo to sd card requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // save an image to sd card
    storeImage(String(imageCounter));

  // html header
    client.write("<!DOCTYPE html> <html> <body>\n");

  // html body
    // note: if the line of html is not just plain text it has to be first put in to 'tstr' then sent in this way
    //       I find it easier to use strings then convert but this is probably frowned upon ;-)
    tstr = "<p>Image saved to sd card as image number " + String(imageCounter) + "</p>\n";
    client.write(tstr.c_str());          
    
  // end html
    client.write("</body></htlm>\n");
    delay(3);
    client.stop();

}  // handlePhoto


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------

void handleNotFound() {
  
  if (debugInfo) Serial.println("Invalid web page requested");      
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  message = "";      // clear variable
  
}  // handleNotFound


// ******************************************************************************************************************


// ----------------------------------------------------------------
//      -stream requested     i.e. http://x.x.x.x/stream
// ----------------------------------------------------------------
// Sends cam stream - thanks to Uwe Gerlach for the code showing how to do this

void handleStream(){

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      if (debugInfo) Serial.println("Video stream requested from: " + String(cip[0]) +"." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // HTML used in the web page
  const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                        "Access-Control-Allow-Origin: *\r\n" \
                        "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
  const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";           // marks end of each image frame
  const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";       // marks start of image data
  const int hdrLen = strlen(HEADER);         // length of the stored text, used when sending to web page
  const int bdrLen = strlen(BOUNDARY);
  const int cntLen = strlen(CTNTTYPE);

  // temp stores
    char buf[32];  
    int s;
    camera_fb_t * fb = NULL;

  // send html header 
    client.write(HEADER, hdrLen);
    client.write(BOUNDARY, bdrLen);

  // send live images until client disconnects
  while (true)
  {
    if (!client.connected()) break;
      fb = esp_camera_fb_get();                   // capture live image frame
      s = fb->len;                                // store size of image (i.e. buffer length)
      client.write(CTNTTYPE, cntLen);             // send content type html (i.e. jpg image)
      sprintf( buf, "%d\r\n\r\n", s );            // format the image's size as html 
      client.write(buf, strlen(buf));             // send image size 
      client.write((char *)fb->buf, s);           // send the image data
      client.write(BOUNDARY, bdrLen);             // send html boundary      see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
      esp_camera_fb_return(fb);                   // return frame so memory can be released
  }
  
  if (debugInfo) Serial.println("Video stream stopped");
  delay(3);
  client.stop();

  
}  // handleStream


// ******************************************************************************************************************
// end
