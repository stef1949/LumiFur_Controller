# LumiFur Controller
A program for controlling an LED matrix display for a Protogen mask, featuring various facial expressions and Bluetooth LE control with an ESP32.

## Features
Multiple facial expressions (idle, happy, angry, playful, silly, kinky, and more)
Smooth blinking animations
Bluetooth Low Energy (BLE) connectivity to switch expressions remotely
Boot-up animation with scrolling text
Hardware Requirements
ESP32 development board
14 x MAX7219 8x8 LED matrix modules
Connecting wires and power supply
Software Requirements
Arduino IDE
MD_MAX72XX library
NimBLE-Arduino library
Installation
Clone the repository:

## Install the required libraries in Arduino IDE:

### MD_MAX72XX Library:

Go to Sketch > Include Library > Manage Libraries...
Search for MD_MAX72XX and install it.
NimBLE-Arduino Library:

Follow the installation instructions here.
Open the project:

Open main.cpp in the Arduino IDE.
Configure hardware settings if necessary:

Check the pin definitions (CLK_PIN, DATA_PIN, CS_PIN) and adjust them to match your setup.
Usage
Upload the code to the ESP32:

Connect your ESP32 to the computer via USB.
Select the correct board and port in the Arduino IDE.
Click Upload to flash the code.
Assemble the hardware:

Connect the LED matrix modules to the ESP32 according to your wiring configuration.
Operate the Protogen mask:

Power on the device.
The mask will display the default idle face with animations.
Use a BLE-compatible app to connect to LumiFur_BLE.
Send values between 1 and 8 to change facial expressions.
Facial Expressions
1 - Idle Face
2 - Happy Face
3 - Angry Face
4 - Playful Face
5 - Silly Face
6 - Kinky Face
7 - Death Face
8 - Edgy Face

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your improvements.

## License
This project is licensed under the MIT License - see the LICENSE file for details.
