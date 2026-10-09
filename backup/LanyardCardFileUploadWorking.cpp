// GxEPD2_HelloWorld.ino by Jean-Marc Zingg
//
// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

// #include "Brown12pt7b.h"
// #include "FreeMonoBold9pt7b.h"
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include "CONSOLA5pt7b.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>

void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

String readFile(fs::FS& fs, const char* path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

// Setup XIAO ESP32C3 With Waveshare 1.54 inch E-paper Display
#define MAX_DISPLAY_BUFFER_SIZE 800
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
#define GxEPD2_DISPLAY_CLASS GxEPD2_4C
#define GxEPD2_DRIVER_CLASS GxEPD2_150_BN  // DEPG0150BN 200x200, SSD1681, (FPC8101), TTGO T5 V2.4.1
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_GDEY0154D67  // GDEY0154D67 200x200, SSD1681, (FPC-B001 20.05.21)
//GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ 10, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7)); // Duemilamove
//GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/2, /*DC=*/3, /*RST=*/4, /*BUSY=*/5));  // XIAO ESP32C3
GxEPD2_4C<GxEPD2_154c_GDEM0154F51H, GxEPD2_154c_GDEM0154F51H::HEIGHT> display(GxEPD2_154c_GDEM0154F51H(/*CS=*/2, /*DC=*/3, /*RST=*/4, /*BUSY=*/5)); // 4-colour screen

#define switchPin 21

#define rotatePin 6

const char* PARAM_INPUT_2 = "input2";

String inputMessage = "Booting...";
String inputMessageOld = inputMessage;

String altMessage = "If you can read this, something has gone wrong. Please tell me, so I can fix it.";
String altMessageOld = altMessage;

int settingUp = 1;

int rotation = 0;

int sleepiness = 1;

String displaytext = inputMessage;


String indexOrig = "An error has occured. Plese fix it.";


// You can customize the SSID name and change the password
String ssid = "Ultracooldood Lanyard Card";
String password = "12345678";
String mdnsName = "lanyard";

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_C = "inputC";
const char* PARAM_INPUT_S = "inputS";
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

Preferences lanyardPrefs;


void helloWorld() {
  display.init(115200, true, 2, false);  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse - wake the board
  display.setRotation(rotation);
  display.setFont(&CONSOLA5pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(displaytext, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = 0;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(displaytext);
  } while (display.nextPage());
  display.hibernate(); 
}

void changeMessage() {
  if (displaytext == inputMessage) {
    displaytext = altMessage;
    helloWorld();
  } else {
    displaytext = inputMessage;
    helloWorld();
  }
}

void setup() {
  settingUp = 1;

  gpio_wakeup_enable(GPIO_NUM_6, GPIO_INTR_LOW_LEVEL);  // Sleep wakeup pins
  gpio_wakeup_enable(GPIO_NUM_21, GPIO_INTR_LOW_LEVEL);   
  esp_sleep_enable_gpio_wakeup();

  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  //display.init(115200, true, 2, false);  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  /* //Displays Ultracooldood Logo on startup
  display.setRotation(4);
  display.setFont(&Brown12pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds("@ultracooldood", 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print("@ultracooldood");
  } while (display.nextPage());
  delay(50);
  */

  pinMode(switchPin, INPUT_PULLUP);
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  lanyardPrefs.begin("prefs", false);

  indexOrig = readFile(LittleFS, "/indexOrig.html");

  indexOrig.replace("It doesn't work. Try fixing it.", readFile(LittleFS, "/timetable.txt"));
  indexOrig.replace("This should be displaying the previous message. If it isn't, it needs to be fixed", readFile(LittleFS, "/message1.txt"));
  indexOrig.replace("You haven't implemented the code for your other message to display correctly. Maybe fix it?", readFile(LittleFS, "/altMessage.txt"));
  indexOrig.replace("If the text box above works but this didn't, I'm slightly concerned.", readFile(LittleFS, "/altMessageOld.txt"));
  writeFile(LittleFS, "/index.html", indexOrig.c_str());
  indexOrig = readFile(LittleFS, "/indexOrig.html");


  altMessage = readFile(LittleFS, "/altMessage.txt");
  altMessageOld = altMessage;

  inputMessage = readFile(LittleFS, "/timetable.txt");
  inputMessageOld = inputMessage;

  // Start Wi-Fi
  WiFi.softAP(ssid, password);

  if (!MDNS.begin(mdnsName)) {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }

  MDNS.addService("_http", "_tcp", 80);
  Serial.println("mDNS responder started. Access your ESP32 at http://" + String(mdnsName) + ".local");

  // Print ESP32 Local IP Address
  Serial.println(WiFi.softAPIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/style.css", "text/css");
  });

  server.on("/textBoxIcon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/textBoxIcon.png", "image/png");
  });

  server.on("/moonStars.png", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/moonStars.png", "image/png");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest* request) {
    String inputParam;
    if (request->hasParam(PARAM_INPUT_1)) {
      // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      Serial.println(inputMessage);
      request->redirect("/");
      /*indexOrig.replace("You haven't implemented the code for your other message to display correctly. Maybe fix it?", altMessage);
      indexOrig.replace("If the text box above works but this didn't, I'm slightly concerned.", altMessageOld);
      indexOrig.replace("It doesn't work. Try fixing it.", inputMessage);
      indexOrig.replace("This should be displaying the previous message. If it isn't, it needs to be fixed", inputMessageOld);
      writeFile(LittleFS, "/index.html", indexOrig.c_str());
      indexOrig = readFile(LittleFS, "/indexOrig.html");
      writeFile(LittleFS, "/altMessage.txt", altMessage.c_str());
      writeFile(LittleFS, "/altMessageOld.txt", altMessageOld.c_str());
      if (settingUp == 0){
        displaytext = altMessage;
        //helloWorld();
        altMessageOld = altMessage;
        writeFile(LittleFS, "/lastMessage.txt", "1");
      }*/
    }
    if (request->hasParam(PARAM_INPUT_2)) {
      // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
      altMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      Serial.println(altMessage);
      request->redirect("/");
      /*indexOrig.replace("It doesn't work. Try fixing it.", inputMessage);
      indexOrig.replace("This should be displaying the previous message. If it isn't, it needs to be fixed", inputMessageOld);
      indexOrig.replace("You haven't implemented the code for your other message to display correctly. Maybe fix it?", altMessage);
      indexOrig.replace("If the text box above works but this didn't, I'm slightly concerned.", altMessageOld);
      writeFile(LittleFS, "/index.html", indexOrig.c_str());
      indexOrig = readFile(LittleFS, "/indexOrig.html");
      writeFile(LittleFS, "/timetable.txt", inputMessage.c_str());
      writeFile(LittleFS, "/message1.txt", inputMessageOld.c_str());
      if (settingUp == 0){
        displaytext = inputMessage;
        //helloWorld();
        inputMessageOld = inputMessage;
        writeFile(LittleFS, "/lastMessage.txt", "0");
      }*/
    }
    if (request->hasParam(PARAM_INPUT_C)) {
      changeMessage();
      request->redirect("/");
    }
    if (request->hasParam(PARAM_INPUT_S)) {
      /*rotation += 1;
      if (rotation > 4) {
        rotation = 1;
        
      }
      */
      request->redirect("/");
      //sleepiness = 1;
      esp_light_sleep_start();
    }
  });



  // Start server
  server.begin();

  if (readFile(LittleFS, "/lastMessage.txt") == "0") {
    displaytext = readFile(LittleFS, "/timetable.txt");
  } else if (readFile(LittleFS, "/lastMessage.txt") == "1") {
    displaytext = readFile(LittleFS, "/altMessage.txt");
  }

  //helloWorld();

  //if (sleepiness == 1) {
 //   esp_light_sleep_start();
 // }

  settingUp = 0;
}

void loop() {

  if (settingUp == 0) {
    delay(10);
    if (altMessage != altMessageOld) {
      indexOrig.replace("You haven't implemented the code for your other message to display correctly. Maybe fix it?", altMessage);
      indexOrig.replace("If the text box above works but this didn't, I'm slightly concerned.", altMessageOld);
      indexOrig.replace("It doesn't work. Try fixing it.", inputMessage);
      indexOrig.replace("This should be displaying the previous message. If it isn't, it needs to be fixed", inputMessageOld);
      writeFile(LittleFS, "/index.html", indexOrig.c_str());
      indexOrig = readFile(LittleFS, "/indexOrig.html");
      writeFile(LittleFS, "/altMessage.txt", altMessage.c_str());
      writeFile(LittleFS, "/altMessageOld.txt", altMessageOld.c_str());
      if (settingUp == 0) {
        displaytext = altMessage;
        altMessageOld = altMessage;
        writeFile(LittleFS, "/lastMessage.txt", "1");
      }
      helloWorld();
    } else if (inputMessage != inputMessageOld) {
      indexOrig.replace("It doesn't work. Try fixing it.", inputMessage);
      indexOrig.replace("This should be displaying the previous message. If it isn't, it needs to be fixed", inputMessageOld);
      indexOrig.replace("You haven't implemented the code for your other message to display correctly. Maybe fix it?", altMessage);
      indexOrig.replace("If the text box above works but this didn't, I'm slightly concerned.", altMessageOld);
      writeFile(LittleFS, "/index.html", indexOrig.c_str());
      indexOrig = readFile(LittleFS, "/indexOrig.html");
      writeFile(LittleFS, "/timetable.txt", inputMessage.c_str());
      writeFile(LittleFS, "/message1.txt", inputMessageOld.c_str());
      if (settingUp == 0) {
        displaytext = inputMessage;
        inputMessageOld = inputMessage;
        writeFile(LittleFS, "/lastMessage.txt", "0");
        lanyardPrefs.putInt("lastMessage", 0);
      }
      helloWorld();
    }
    if (digitalRead(switchPin) == LOW) {
      changeMessage();
      // esp_light_sleep_start();
    }
    if (digitalRead(rotatePin) == LOW) {
      /* rotation += 1;
      if (rotation > 4) {
        rotation = 1;
      }   */   
      helloWorld();
      //sleepiness = 0;
    }
  }
}
