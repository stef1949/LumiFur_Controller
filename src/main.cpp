//General Libraries
#include <MD_MAX72XX.h>
#include <fastLED.h> //FastLED library for controlling built in LED for BLE connection status
#include <SPI.h>

//Bluetooth LE Libraries
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// BLE UUIDs
#define SERVICE_UUID        "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"  // Unique UUID for temperature data

// Built-in RGB LED pin
const int RGB_PIN = 48;                // Data pin for the built-in RGB LED (use GPIO 45 if 48 doesn’t work)
#define NUM_LEDS 1                     // Number of RGB LEDs (1 if it’s built-in)

CRGB leds[NUM_LEDS];                   // Array to control RGB LED

// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTD(x) Serial.println(x, DEC)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTD(x)
#endif

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW/*PAROLA_HW/FC16_HW*/
#define MAX_DEVICES  14

#define CLK_PIN   18  // or SCK
#define DATA_PIN  23  // or MOSI
#define CS_PIN    5  // or SS

// Ultrasonic Sensor Pins
#define TRIG_PIN 6  // ESP32 GPIO pin for HC-SR04 Trigger
#define ECHO_PIN 18  // ESP32 GPIO pin for HC-SR04 Echo
//#define READINGS_COUNT 3  // Number of readings to average

// Ultrasonic Sensor Variables
long duration; // Time taken for the ultrasonic wave to return
int distance; // Distance in centimeters
int currentDistance; // Distance in centimeters
const int READINGS_COUNT = 10; // Number of readings to average

int readings[READINGS_COUNT]; // the readings from the analog input
int readIndex = 0; // the index of the current reading
unsigned long lastDistanceCheck = 0; // Last time distance was checked

const unsigned long DISTANCE_CHECK_INTERVAL = 200; // Check distance every 100ms

// Ultrasonnic Sensor Function Declaration
void setupHCSR04() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    // Initialize the readings array
    for (int i = 0; i < READINGS_COUNT; i++) {
        readings[i] = 0;
    }
}

// Get a single distance reading
int getSingleReading() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2); // Delay to ensure a clean pulse

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10); // Generate a 10us pulse
    digitalWrite(TRIG_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        #if DEBUG
            Serial.println("No pulse received");
        #endif
        return 400;  // Return max range if no pulse received
    }

    distance = duration * 0.0344 / 2;  // Calculate distance in cm
    
    Serial.print("Calculated distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Limit the range to 2-400cm (HC-SR04 specs)
    if (distance > 400 || distance < 2) {
        #if DEBUG
            Serial.println("Distance out of range");
        #endif
        return 400;  // Return max range if out of bounds
    }

    return distance;
}

// Get averaged distance reading
int getDistance() {
    #if DEBUG
        Serial.println("Reading HC-SR04 sensor...");
    #endif

    readings[readIndex] = getSingleReading();
    readIndex = (readIndex + 1) % READINGS_COUNT;

    long sum = 0;
    for (int i = 0; i < READINGS_COUNT; i++) {
        sum += readings[i];
    }

    int avgDistance = (READINGS_COUNT > 0) ? (sum / READINGS_COUNT) : 0;

    #if DEBUG
        Serial.print("Average Distance: ");
        Serial.print(avgDistance);
        Serial.println(" cm");
    #endif

    return avgDistance;
}

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Specific SPI hardware interface
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, SPI1, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define  DELAYTIME  60

/*
/////////////// Uncomment if you want to use Button Pins ///////////////
#define PULS1 9
#define PULS2 8
#define PULS3 7
#define PULS4 6
#define PULS5 5
#define PULS6 4
#define PULS7 3
#define PULS8 2
*/

uint8_t face=1;    // Current face display state
short tempo;    //Time delay for blinking

bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE Server pointers
NimBLEServer* pServer = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLECharacteristic* pTemperatureCharacteristic = NULL;

// Class to handle BLE server callbacks
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected, LED solid blue.");
        leds[0] = CRGB::Blue;          // Set RGB LED to solid blue when connected
        FastLED.show();
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected, LED will blink blue.");
    }
};

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            uint8_t newFace = (uint8_t)value[0];
            if (newFace >= 1 && newFace <= 8 && newFace != face) {
                face = newFace;
                mx.clear(); // Clear display before changing face
            }
        }
    }
};

//Face function declarations
void setupHCSR04();
int getSingleReading();
int getDistance();
void fadeBlueLED();
void fadeInAndOutLED(CRGB color, uint8_t step, uint8_t delayTime);
void handleBLEConnection();
void OSBoot();
void idleFace();
void loopBlink(byte x);
void happyFace();
void angyFace();
void playfulFace();
void sillyFace();
void kinkyFace();
void deathFace();
void edgyEyes();
void normalNose();
void normalMouth();
void edgyEyesBlink();
void happyEyes();
void angyEyes();
void angyMouth();
void playfulEyes();
void owoEyes();
void wMouth();
void owoEyesBlink();
void uwuEyes();
void deathEyes();
void scrollText(const char *p);
void angyEyesBlink();

// Setup function
void setup() {

  mx.begin();

#if  DEBUG
  Serial.begin(115200);
#endif
  Serial.println("\n=== Booting LumiFur OS ===");

  OSBoot(); //Play boot animation on startup
    
    // Initialize HC-SR04
    setupHCSR04(); // Setup the ultrasonic sensor
    Serial.println("HC-SR04 initialized");

    //Test the ultrasonic sensor
    int initialDistance = getDistance();
    Serial.print("Initial distance: ");
    Serial.print(initialDistance);
    Serial.println(" cm");
  
    // Initialize NimBLE
    NimBLEDevice::init("LumiFur_BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Set maximum power level

    // Create the BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create the BLE Service and Characteristic
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // Face characteristic
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        NIMBLE_PROPERTY::READ |
                        NIMBLE_PROPERTY::WRITE |
                        NIMBLE_PROPERTY::NOTIFY);
    
    // Temperature Characteristic
    pTemperatureCharacteristic = pService->createCharacteristic(
                                    TEMPERATURE_CHARACTERISTIC_UUID,
                                    NIMBLE_PROPERTY::READ |
                                    NIMBLE_PROPERTY::NOTIFY);
    
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone connection optimization
    pAdvertising->setMaxPreferred(0x12);
    NimBLEDevice::startAdvertising();

    PRINTS("\nBLE Active, waiting for connections...");
    
     // Initialize the FastLED library
    FastLED.addLeds<NEOPIXEL, RGB_PIN>(leds, NUM_LEDS);
    FastLED.clear();  // Ensure LED is off at startup
    FastLED.show();
}

void handleBLEConnection() {
    // BLE connection status handling
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Give Bluetooth stack time to settle
        pServer->startAdvertising(); //start advertising to reconnect
        PRINTS("\nStarting advertising");
        oldDeviceConnected = deviceConnected; //Update the old connection status
    }

     // Fade blue LED in and out when waiting for a connection
    if (!deviceConnected) {
        fadeBlueLED(); // Indicate waiting for connection by fading blue LED
    }

    if (deviceConnected && !oldDeviceConnected) {
        Serial.println("Device connected");
        // Solid blue LED when connected
        leds[0] = CRGB::Green;
        FastLED.show();
        oldDeviceConnected = deviceConnected; // Update the old connection status
    }
}

// Handle distance measurements and face changes
void handleDistance() {
    if (millis() - lastDistanceCheck >= DISTANCE_CHECK_INTERVAL) {
        lastDistanceCheck = millis();
        currentDistance = getDistance();
        
        /*
        // Change face based on distance
        if (currentDistance < 10) {
            face = 6;  // Very close - show surprised/kinky face
        } else if (currentDistance < 30) {
            face = 2;  // Close - show happy face
        } else if (currentDistance < 50) {
            face = 4;  // Medium distance - show playful face
        } else if (currentDistance < 100) {
            face = 1;  // Far - show idle face
        } else {
            face = 1;  // Very far or no detection - show idle face
        }
        */

        // Send distance data over BLE if connected
        if (deviceConnected) {
            char distStr[8];
            snprintf(distStr, sizeof(distStr), "%d", currentDistance);
            pTemperatureCharacteristic->setValue(distStr);
            pTemperatureCharacteristic->notify();
        }

        // Print distance to Serial Monitor
        Serial.print("Distance: ");
        Serial.print(currentDistance);
        Serial.println(" cm");
        delay(100);  // Delay to prevent spamming the Serial Monitor
    }
    int READINGS_COUNT = 0;
}

void fadeBlueLED() { // Continue fading while no device is connected
    while (!deviceConnected) {
        fadeInAndOutLED(CRGB::Blue, 5, 5); // Fade the blue LED with specific step and delay
    }
}

void fadeInAndOutLED(CRGB color, uint8_t step, uint8_t delayTime) {
    // Fade in
    for (int brightness = 0; brightness <= 255; brightness += step) {
        leds[0] = color;
        leds[0].fadeLightBy(255 - brightness);   // Increase brightness
        FastLED.show();
        delay(delayTime);                        // Adjust delay for speed of fade
    }
    // Fade out
    for (int brightness = 255; brightness >= 0; brightness -= step) {
        leds[0] = color;
        leds[0].fadeLightBy(255 - brightness);   // Decrease brightness
        FastLED.show();
        delay(delayTime);                        // Adjust delay for speed of fade
    }
}

void loop() {
  // Handle BLE connection status & LED indicator
  handleBLEConnection();

  // Handle distance measurements and face changes
  handleDistance();

    // Check if a device is connected
if (deviceConnected) {
        // Read the ESP32's internal temperature
        float temperature = temperatureRead();  // Temperature in Celsius

        // Convert temperature to string and send over BLE
        char tempStr[8];
        dtostrf(temperature, 1, 2, tempStr);  // Convert float to string
        pTemperatureCharacteristic->setValue(tempStr);  // Update BLE characteristic
        pTemperatureCharacteristic->notify();  // Notify the connected device

        // Optional: Debug output to serial
        Serial.print("Internal Temperature: ");
        Serial.println(tempStr);

        delay(2000);  // Adjust the update frequency as needed
    }

// Original face control logic
  if (DEBUG) Serial.println(face); // Print the face value if debugging is enabled

  tempo=0; // Reset tempo for each loop iteration

  // Display face based on `face` value
  switch(face) {
    case 1: //The face assigned to the number
      idleFace();
      loopBlink(face);
      break;
    case 2: 
      happyFace();
      loopBlink(face);
      break;
    case 3:
      angyFace();
      loopBlink(face);
      break;
    case 4:
      playfulFace();
      loopBlink(face);
      break;
    case 5:
      sillyFace();
      loopBlink(face);
      break;
    case 6:
      kinkyFace();
      loopBlink(face);
      break;
    case 7:
      deathFace();
      loopBlink(face);
      break;
    case 8:
      idleFace();
      loopBlink(face);
      break;
  }
}

void printSprite(uint8_t sprite[], byte setPosition) {          //function for printing the sprites
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);              //auto update off
  mx.setBuffer(((setPosition+1)*COL_SIZE)-1, COL_SIZE, sprite); //set of the sprite
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);               //auto update on
}

void idleFace() { //idle face components
  edgyEyes();
  normalNose();
  normalMouth();
  edgyEyesBlink();
}

void happyFace() {  //happy face components
  happyEyes();
  normalNose();
  normalMouth();
}

void angyFace() { //hungry face components
  angyEyes();
  normalNose();
  angyMouth();
  angyEyesBlink();
}

void playfulFace() {  //playful face components
  playfulEyes();
  normalNose();
  normalMouth();
}

void sillyFace() {  //silly face components
  owoEyes();
  normalNose();
  wMouth();
  owoEyesBlink();
}

void kinkyFace() {  //kinky face components
  uwuEyes();
  normalNose();
  wMouth();
}

void deathFace() {  //death face components
  deathEyes();
  normalNose();
  wMouth();
}

void setOnLine(byte x, byte y, byte line) { //turn on the a line from an initial matrix coordinate to another
  for(byte i=x ; i<y ; i++) mx.setPoint(line, i, true);
}

void setOffLine(byte x, byte y, byte line) { //turn off the a line from an initial matrix coordinate to another
  for(byte i=x ; i<y ; i++) mx.setPoint(line, i, false);
}

void loopBlink(byte x) {
   tempo = 0;  // Reset tempo

  // 'Delay' for 7 seconds while periodically performing blink animation
  while (tempo <= 7000) {
    delay(1);

    // Check if face has changed via Bluetooth; if so, exit the loop
    if (face != x) {
      mx.clear();  // Clear the display when face changes
      break;       // Exit the loop if face has been changed via Bluetooth
    }

    tempo++;
  }
}

void OSBoot() { //asthetic funcions for the protogen booting animation
//mx.control(MD_MAX72XX::setFont());
  scrollText("LUMIFUR OS 1.0 BOOTING..");
  delay(500);//delay for reading
  mx.clear();//i clear the matrix for not exploding the previous pixels
  uint8_t sys1[COL_SIZE]{
    0b00000110,
    0b01001001,
    0b01001001,
    0b01001001,
    0b00110001,
    0b00000011,
    0b00000100,
    0b01111000
  };
  uint8_t sys2[COL_SIZE]{
    0b00000110,
    0b01001001,
    0b01001001,
    0b01001001,
    0b00110001,
    0b00000000,
    0b00000000,
    0b00000000
  };
  for(byte j=0 ; j<3 ; j++){
    for(byte i=0 ; i<10 ; i++){
      printSprite(sys1, 3);         //i print the first matrix onto the 3 connected matrix 
      printSprite(sys2, 2);         //print the second matrix onto the second connected matrix
      mx.setChar(17, '1'+j);        //increase every time the system to boot
      for(byte j=0 ; j<11 ; j++){
        mx.setChar(11, '0'+i);
        mx.setChar(5, '0'+j);
      delay(5);
      }
      mx.clear();
    }
    printSprite(sys1, 3);         //i print the first matrix onto the 3 connected matrix 
    printSprite(sys2, 2);         //print the second matrix onto the second connected matrix
    mx.setChar(17, '1'+j);        //i print another time the system to boot
    mx.setChar(10, 'O');          //"avvio" del sistema desiderato
    mx.setChar(5, 'N');
    delay(1500);
  }
  mx.clear();
}


void scrollText(const char *p){//function for the horizontal scrolling of the words
  uint8_t charWidth;
  uint8_t cBuf[8];  // this should be ok for all built-in fonts
  PRINTS("\nprotogen booting..");
  mx.clear();
  while (*p != '\0'){
    charWidth = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
    for (uint8_t i=0; i<=charWidth; i++)  {// allow space between characters
      mx.transform(MD_MAX72XX::TSL);
      if (i < charWidth) mx.setColumn(0, cBuf[i]);
      delay(DELAYTIME);
    }
  }
}

void edgyEyesBlink() {
  PRINTS("\nedgy eyes blinking..");
  byte line=0;
  setOffLine(32, 48, line);
  setOffLine(64, 80, line);
  line++;
  setOffLine(32, 48, line);
  setOffLine(64, 80, line);
  delay(20);
  line++;
  setOffLine(33, 48, line);
  setOffLine(64, 79, line);
  delay(20);
  line++;
  setOffLine(35, 47, line);
  setOffLine(65, 77, line);
  line++;
  setOffLine(35, 47, line);
  setOffLine(65, 77, line);
  line++;
  delay(10);
  line=7;
  setOffLine(39, 43, line);
  setOffLine(69, 73, line);
  delay(20);
  line--;
  setOffLine(37, 45, line);
  setOffLine(67, 75, line);
  delay(20);
  line--;
  setOffLine(36, 46, line);
  setOffLine(66, 76, line);
  delay(130);///////////////////////////////////////////////////////
  setOnLine(36, 46, line);
  setOnLine(66, 76, line);
  delay(20);
  line++;
  setOnLine(37, 45, line);
  setOnLine(67, 75, line);
  delay(20);
  line++;
  setOnLine(39, 43, line);
  setOnLine(69, 73, line);
  delay(20);
  line=5;
  line--;
  setOnLine(35, 47, line);
  setOnLine(65, 77, line);
  line--;
  setOnLine(35, 47, line);
  setOnLine(65, 77, line);
  delay(20);
  line--;
  setOnLine(33, 48, line);
  setOnLine(64, 79, line);
  delay(20);
  line--;
  setOnLine(32, 48, line);
  setOnLine(64, 80, line);
  line--;
  setOnLine(32, 48, line);
  setOnLine(64, 80, line);
}

void angyEyesBlink() {
  PRINTS("\nangy eyes blinking..");
  byte line=0;
  setOffLine(32, 35, line);
  setOffLine(77, 80, line);
  delay(30);
  line++;
  setOffLine(32, 38, line);
  setOffLine(74, 80, line);
  delay(30);
  line++;
  setOffLine(33, 41, line);
  setOffLine(71, 79, line);
  delay(30);
  line++;
  setOffLine(34, 44, line);
  setOffLine(68, 78, line);
  delay(30);
  line++;
  setOffLine(34, 46, line);
  setOffLine(66, 78, line);
  delay(30);
  line=7;
  setOffLine(37, 45, line);
  setOffLine(67, 75, line);
  delay(30);
  line--;
  setOffLine(36, 46, line);
  setOffLine(66, 76, line);
  delay(30);
  line--;
  setOffLine(35, 47, line);
  setOffLine(65, 77, line);
  delay(130);///////////////////////////////////////////////////////
  setOnLine(35, 47, line);
  setOnLine(65, 77, line);
  delay(30);
  line++;
  setOnLine(36, 46, line);
  setOnLine(66, 76, line);
  delay(30);
  line++;
  setOnLine(37, 45, line);
  setOnLine(67, 75, line);
  delay(30);
  line=4;
  setOnLine(34, 46, line);
  setOnLine(66, 78, line);
  delay(30);
  line--;
  setOnLine(34, 44, line);
  setOnLine(68, 78, line);
  delay(30);
  line--;
  setOnLine(33, 41, line);
  setOnLine(71, 79, line);
  delay(30);
  line--;
  setOnLine(32, 38, line);
  setOnLine(74, 80, line);
  delay(30);
  line--;
  setOnLine(32, 35, line);
  setOnLine(77, 80, line);
}

void owoEyesBlink() {
  PRINTS("\nowo eyes blinking..");
  byte i, line=0;
  setOffLine(38, 42, line);
  setOffLine(70, 74, line);
  delay(10);
  line++;
  for(i=37 ; i<43 ; i++){
    if((i>=37 && i<=38) || (i>=41 && i<=42)) mx.setPoint(line, i, false);
  }
  for(i=69 ; i<75 ; i++){
    if((i>=69 && i<=70) || (i>=73 && i<=74)) mx.setPoint(line, i, false);
  }
  delay(10);
  line++;
  for(i=36 ; i<44 ; i++){
    if((i>=36 && i<=37) || (i>=42 && i<=43)) mx.setPoint(line, i, false);
  }
  for(i=68 ; i<76 ; i++){
    if((i>=68 && i<=69) || (i>=74 && i<=75)) mx.setPoint(line, i, false);
  }
  delay(10);
  line++;
  mx.setPoint(line, 36, false);
  mx.setPoint(line, 43, false);
  mx.setPoint(line, 68, false);
  mx.setPoint(line, 75, false);
  delay(10);
  line++;
  mx.setPoint(line, 36, false);
  mx.setPoint(line, 43, false);
  mx.setPoint(line, 68, false);
  mx.setPoint(line, 75, false);
  delay(10);
  line=7;
  setOffLine(38, 42, line);
  setOffLine(70, 74, line);
  delay(10);
  line--;
  for(i=37 ; i<43 ; i++){
    if((i>=37 && i<=38) || (i>=41 && i<=42)) mx.setPoint(line, i, false);
  }
  for(i=69 ; i<75 ; i++){
    if((i>=69 && i<=70) || (i>=73 && i<=74)) mx.setPoint(line, i, false);
  }
  delay(10);
  line--;
  for(i=36 ; i<44 ; i++){
    if((i>=36 && i<=37) || (i>=42 && i<=43)) mx.setPoint(line, i, false);
  }
  for(i=68 ; i<76 ; i++){
    if((i>=68 && i<=69) || (i>=74 && i<=75)) mx.setPoint(line, i, false);
  }
  delay(130);///////////////////////////////////////////////////////
  for(i=36 ; i<44 ; i++){
    if((i>=36 && i<=37) || (i>=42 && i<=43)) mx.setPoint(line, i, true);
  }
  for(i=68 ; i<76 ; i++){
    if((i>=68 && i<=69) || (i>=74 && i<=75)) mx.setPoint(line, i, true);
  }
  delay(10);
  line++;
  for(i=37 ; i<43 ; i++){
    if((i>=37 && i<=38) || (i>=41 && i<=42)) mx.setPoint(line, i, true);
  }
  for(i=69 ; i<75 ; i++){
    if((i>=69 && i<=70) || (i>=73 && i<=74)) mx.setPoint(line, i, true);
  }
  delay(10);
  line++;
  setOnLine(38, 42, line);
  setOnLine(70, 74, line);
  delay(10);
  line=4;
  mx.setPoint(line, 36, true);
  mx.setPoint(line, 43, true);
  mx.setPoint(line, 68, true);
  mx.setPoint(line, 75, true);
  delay(10);
  line--;
  mx.setPoint(line, 36, true);
  mx.setPoint(line, 43, true);
  mx.setPoint(line, 68, true);
  mx.setPoint(line, 75, true);
  delay(10);
  line--;
  for(i=36 ; i<44 ; i++){
    if((i>=36 && i<=37) || (i>=42 && i<=43)) mx.setPoint(line, i, true);
  }
  for(i=68 ; i<76 ; i++){
    if((i>=68 && i<=69) || (i>=74 && i<=75)) mx.setPoint(line, i, true);
  }
  delay(10);
  line--;
  for(i=37 ; i<43 ; i++){
    if((i>=37 && i<=38) || (i>=41 && i<=42)) mx.setPoint(line, i, true);
  }
  for(i=69 ; i<75 ; i++){
    if((i>=69 && i<=70) || (i>=73 && i<=74)) mx.setPoint(line, i, true);
  }
  delay(10);
  line--;
  setOnLine(38, 42, line);
  setOnLine(70, 74, line);
}

void happyEyes() {
  PRINTS("\nhappy eyes blinking..");
  uint8_t happyEyesLeft1[COL_SIZE]{
    0b00001110,
    0b00001110,
    0b00001110,
    0b00001100,
    0b00001100,
    0b00011000,
    0b00011000,
    0b00110000,
  };
  uint8_t happyEyesleft2[COL_SIZE]{
    0b00011000,
    0b00111100,
    0b00111110,
    0b00011110,
    0b00001111,
    0b00001111,
    0b00001111,
    0b00001111,
  };
  uint8_t happyEyesright1[COL_SIZE]{
    0b00001111,
    0b00001111,
    0b00001111,
    0b00001111,
    0b00011110,
    0b00111110,
    0b00111100,
    0b00011000,
  };
  uint8_t happyEyesRight2[COL_SIZE]{
    0b00110000,
    0b00011000,
    0b00011000,
    0b00001100,
    0b00001100,
    0b00001110,
    0b00001110,
    0b00001110,
  };
  printSprite(happyEyesLeft1, 4);
  printSprite(happyEyesleft2, 5);
  printSprite(happyEyesright1, 8);
  printSprite(happyEyesRight2, 9);
  PRINTS("\nhappy eyes done");
}

void edgyEyes() {
  PRINTS("\nedgy eye booting..");
  uint8_t edgyEyesLeft1[COL_SIZE]{
    0b11111111,
    0b01111111,
    0b01111111,
    0b00111111,
    0b00011111,
    0b00000111,
    0b00000111,
    0b00000011
  };
  uint8_t edgyEyesleft2[COL_SIZE]{
    0b00000111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b01111111,
    0b11111111,
    0b11111111,
    0b11111111
  };
  uint8_t edgyEyesright1[COL_SIZE]{
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111111,
    0b01111111,
    0b00111111,
    0b00011111,
    0b00000111
  };
  uint8_t edgyEyesRight2[COL_SIZE]{
    0b00000011,
    0b00000111,
    0b00000111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b01111111,
    0b11111111
  };
  printSprite(edgyEyesLeft1, 4);
  printSprite(edgyEyesleft2, 5);
  printSprite(edgyEyesright1, 8);
  printSprite(edgyEyesRight2, 9);
  
  PRINTS("\nedgy eye done");
}

void angyEyes() {
  PRINTS("\nangy eyes booting..");
  uint8_t angyEyesLeft1[COL_SIZE]{
    0b11111100,
    0b11111100,
    0b11111110,
    0b01111110,
    0b00111110,
    0b00011111,
    0b00000111,
    0b00000011
  };
  uint8_t angyEyesLeft2[COL_SIZE]{
    0b00000000,
    0b00100000,
    0b01110000,
    0b11110000,
    0b11111000,
    0b11111000,
    0b11111000,
    0b11111100
  };
  uint8_t angyEyesRight1[COL_SIZE]{
    0b11111100,
    0b11111000,
    0b11111000,
    0b11111000,
    0b11110000,
    0b01110000,
    0b00100000,
    0b00000000
  };
  uint8_t angyEyesRight2[COL_SIZE]{
    0b00000011,
    0b00000111,
    0b00011111,
    0b00111110,
    0b01111110,
    0b11111110,
    0b11111100,
    0b11111100
  };
  printSprite(angyEyesLeft1, 4);
  printSprite(angyEyesLeft2, 6);
  printSprite(angyEyesRight1, 8);
  printSprite(angyEyesRight2, 9);
  PRINTS("\nangy eyes done");
}

void playfulEyes() {
  PRINTS("\nplayful eyes booting..");
  uint8_t playfulEyesLeft1[COL_SIZE]{
    0b11000110,
    0b10000010,
    0b10000010,
    0b10000011,
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000000
  };
  uint8_t playfulEyesLeft2[COL_SIZE]{
    0b00010000,
    0b00010000,
    0b00111000,
    0b00101000,
    0b00101000,
    0b01101100,
    0b01000100,
    0b01000100
  };
  uint8_t playfulEyesRight1[COL_SIZE]{
    0b01000100,
    0b01000100,
    0b01101100,
    0b00101000,
    0b00101000,
    0b00111000,
    0b00010000,
    0b00010000
  };
  uint8_t playfulEyesRight2[COL_SIZE]{
    0b00000001,
    0b00000001,
    0b00000001,
    0b10000011,
    0b10000010,
    0b10000010,
    0b10000010,
    0b11000110
  };
  printSprite(playfulEyesLeft1, 4);
  printSprite(playfulEyesLeft2, 5);
  printSprite(playfulEyesRight1, 8);
  printSprite(playfulEyesRight2, 9);
  PRINTS("\nplayful eyes done");
}
  
void owoEyes() {
  PRINTS("\nowo eyes booting..");
  uint8_t owoEyesLeftSide[COL_SIZE]{
    0b10000001,
    0b11000011,
    0b01100110,
    0b00111100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  };
  uint8_t owoEyesRightSide[COL_SIZE]{
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00111100,
    0b01100110,
    0b11000011,
    0b10000001
  };
  printSprite(owoEyesLeftSide, 4);
  printSprite(owoEyesRightSide, 5);
  printSprite(owoEyesLeftSide, 8);
  printSprite(owoEyesRightSide, 9);
  PRINTS("\nowo eyes done");
}

void uwuEyes() {
  PRINTS("\nuwu eyes booting..");
  uint8_t uwuEyesLeftSide[COL_SIZE]{
    0b10000000,
    0b11000000,
    0b01110000,
    0b00011111,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  };
  uint8_t uwuEyesRightSide[COL_SIZE]{
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011111,
    0b01110000,
    0b11000000,
    0b10000000
  };
  printSprite(uwuEyesLeftSide, 4);
  printSprite(uwuEyesRightSide, 5);
  printSprite(uwuEyesLeftSide, 8);
  printSprite(uwuEyesRightSide, 9);
  PRINTS("\nuwu eyes done");
}

void deathEyes() {
  PRINTS("\ndeath eyes booting..");
  uint8_t deathEyesLeftSide[COL_SIZE]{
    0b00011000,
    0b00111100,
    0b01111110,
    0b01100110,
    0b11100111,
    0b11000011,
    0b10000001,
    0b10000001
  };
  uint8_t deathEyesRightSide[COL_SIZE]{
    0b10000001,
    0b10000001,
    0b11000011,
    0b11100111,
    0b01100110,
    0b01111110,
    0b00111100,
    0b00011000
  };
  printSprite(deathEyesLeftSide, 4);
  printSprite(deathEyesRightSide, 5);
  printSprite(deathEyesLeftSide, 8);
  printSprite(deathEyesRightSide, 9);
  PRINTS("\ndeath eyes done");
}

void normalNose() {
  PRINTS("\nnormal nose booting..");
  //left e right rispetto alla faccia del protogen
  uint8_t leftboop[COL_SIZE]{
    0b00111110,
    0b00011111,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00000001
  };
  uint8_t rightboop[COL_SIZE]{
    0b00000001,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00000011,
    0b00011111,
    0b00111110
  };
  printSprite(leftboop, 6);
  printSprite(rightboop, 7);
  PRINTS("\nnormal nose done");
}

void normalMouth() {
  PRINTS("\nnormal mouth booting..");
  //left e right rispetto alla faccia del protogen
  uint8_t leftNormalMouth1[COL_SIZE]{
    0b00011000,
    0b00011100,
    0b00011100,
    0b00010110,
    0b00010111,
    0b00010010,
    0b00011110,
    0b00011100
  };
  uint8_t leftNormalMouth2[COL_SIZE]{
    0b10000000,
    0b11000000,
    0b11000000,
    0b01100000,
    0b01100000,
    0b00110000,
    0b00110000,
    0b00011000
  };
  uint8_t leftNormalMouth3[COL_SIZE]{
    0b00010000,
    0b00110000,
    0b00110000,
    0b01100000,
    0b01100000,
    0b11000000,
    0b11000000,
    0b10000000
  };
  uint8_t leftNormalMouth4[COL_SIZE]{
    0b11000000,
    0b01100000,
    0b01100000,
    0b00110000,
    0b00110000,
    0b00011000,
    0b00011000,
    0b00011000
  };
  uint8_t rightNormalMouth1[COL_SIZE]{
    0b00011000,
    0b00011000,
    0b00011000,
    0b00110000,
    0b00110000,
    0b01100000,
    0b01100000,
    0b11000000
  };
  uint8_t rightNormalMouth2[COL_SIZE]{
    0b10000000,
    0b11000000,
    0b11000000,
    0b01100000,
    0b01100000,
    0b00110000,
    0b00110000,
    0b00010000
  };
  uint8_t rightNormalMouth3[COL_SIZE]{
    0b00011000,
    0b00110000,
    0b00110000,
    0b01100000,
    0b01100000,
    0b11000000,
    0b11000000,
    0b10000000
  };
  uint8_t rightNormalMouth4[COL_SIZE]{
    0b00011100,
    0b00011110,
    0b00010010,
    0b00010111,
    0b00010110,
    0b00011100,
    0b00011100,
    0b00011000
  };
  
  printSprite(leftNormalMouth1, 0);
  printSprite(leftNormalMouth2, 1);
  printSprite(leftNormalMouth3, 2);
  printSprite(leftNormalMouth4, 3);
  printSprite(rightNormalMouth1, 10);
  printSprite(rightNormalMouth2, 11);
  printSprite(rightNormalMouth3, 12);
  printSprite(rightNormalMouth4, 13);
  PRINTS("\nnormal mouth done ");
}

void angyMouth() {
  PRINTS("\nangy  mouth booting..");
  //left e right in relation to the protogen face
  uint8_t leftangyMouth1[COL_SIZE]{
    0b01100000,
    0b01000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  };
  uint8_t leftangyMouth2[COL_SIZE]{
    0b00001110,
    0b00001100,
    0b00011100,
    0b00011100,
    0b00011000,
    0b00111000,
    0b00110000,
    0b01110000
  };
  uint8_t leftangyMouth3[COL_SIZE]{
    0b00001110,
    0b00001111,
    0b00001111,
    0b00000111,
    0b00000111,
    0b00000111,
    0b00000111,
    0b00001110
  };
  uint8_t leftangyMouth4[COL_SIZE]{
    0b01111100,
    0b01111100,
    0b01111100,
    0b00111110,
    0b00111110,
    0b00011110,
    0b00011110,
    0b00011110
  };
  uint8_t rightangyMouth1[COL_SIZE]{
    0b00011110,
    0b00011110,
    0b00011110,
    0b00111110,
    0b00111110,
    0b01111100,
    0b01111100,
    0b01111100
  };
  uint8_t rightangyMouth2[COL_SIZE]{
    0b00001110,
    0b00000111,
    0b00000111,
    0b00000111,
    0b00000111,
    0b00001111,
    0b00001111,
    0b00001110
  };
  uint8_t rightangyMouth3[COL_SIZE]{
    0b01110000,
    0b00110000,
    0b00111000,
    0b00011000,
    0b00011100,
    0b00011100,
    0b00001100,
    0b00001110
  };
  uint8_t rightangyMouth4[COL_SIZE]{
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01000000,
    0b01100000
  };
  printSprite(leftangyMouth1, 0);
  printSprite(leftangyMouth2, 1);
  printSprite(leftangyMouth3, 2);
  printSprite(leftangyMouth4, 3);
  printSprite(rightangyMouth1, 10);
  printSprite(rightangyMouth2, 11);
  printSprite(rightangyMouth3, 12);
  printSprite(rightangyMouth4, 13);
  PRINTS("\nangy mouth done ");
}

void wMouth() {
  PRINTS("\nw  mouth booting..");
  //left e right in relation to the protogen face
  uint8_t leftWMouth1[COL_SIZE]{
    0b00110000,
    0b00011000,
    0b00011100,
    0b00010110,
    0b00010111,
    0b00010010,
    0b00011110,
    0b00011100
  };
  uint8_t leftWMouth2[COL_SIZE]{
    0b00000110,
    0b00001100,
    0b00011000,
    0b00110000,
    0b01100000,
    0b11000000,
    0b11000000,
    0b01100000
  };
  uint8_t leftWMouth3[COL_SIZE]{
    0b01100000,
    0b11000000,
    0b11000000,
    0b01100000,
    0b00110000,
    0b00011000,
    0b00001100,
    0b00000110
  };
  uint8_t leftWMouth4[COL_SIZE]{
    0b00110000,
    0b00011000,
    0b00001100,
    0b00000110,
    0b00000110,
    0b00001100,
    0b00011000,
    0b00110000
  };
  uint8_t rightWMouth1[COL_SIZE]{
    0b00110000,
    0b00011000,
    0b00001100,
    0b00000110,
    0b00000110,
    0b00001100,
    0b00011000,
    0b00110000
  };
  uint8_t rightWMouth2[COL_SIZE]{
    0b00000110,
    0b00001100,
    0b00011000,
    0b00110000,
    0b01100000,
    0b11000000,
    0b11000000,
    0b01100000
  };
  uint8_t rightWMouth3[COL_SIZE]{
    0b01100000,
    0b11000000,
    0b11000000,
    0b01100000,
    0b00110000,
    0b00011000,
    0b00001100,
    0b00000110
  };
  uint8_t rightWMouth4[COL_SIZE]{
    0b00011100,
    0b00011110,
    0b00010010,
    0b00010111,
    0b00010110,
    0b00011100,
    0b00110000,
    0b00110000
  };
  printSprite(leftWMouth1, 0);
  printSprite(leftWMouth2, 1);
  printSprite(leftWMouth3, 2);
  printSprite(leftWMouth4, 3);
  printSprite(rightWMouth1, 10);
  printSprite(rightWMouth2, 11);
  printSprite(rightWMouth3, 12);
  printSprite(rightWMouth4, 13);
  PRINTS("\nw mouth done ");
}