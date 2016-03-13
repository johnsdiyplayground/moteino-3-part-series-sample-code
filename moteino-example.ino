/* ***************************************************************************************************
// Example sketch from John's DIY Playground
// to demonstrate the Moteino IoT device using an RFM69 transceiver attached to it.
// Visit http://www.lowpowerlab.com/ to purchase a Moteino of your own
//
// This sketch will demonstrate a few basic features of the Moteino:
//   1. It will flash the onboard LED located at pin D9
//   2. It will report sensor light levels from a photoresistor on pin A5 to the Home Automation gateway
//   3. It will allow us to control an LED connected to pin D3 remotely
//
// Demonstration of this code can be found on my YouTube channel, called John's DIY Playground
// The channel's URL is http://www.youtube.com/c/johnsdiyplayground
// Software code examples can be found on my GitHub page at
// https://github.com/johnsdiyplayground
// 
// Hardware Required:
// 1. Moteino or Moteino USB with RFM69 transceiver (http://www.lowpowerlab.com/)
// 2. LED
// 3. 2 quantity of 220 ohm resistor for the external LED and photoresistor circuits
//     Put the resistors in series; the LED to ground and the other from photoresistor A5 to ground
// 4. Photoresistor
// 5. USB to micro USB cable for powering the Moteino
// 6. Another USB Moteino with the Pi Gateway code loaded on it and connected to a Raspberry Pi
//      NOTE: this second Moteino must have the same frequency RFM69 transceiver attached to it
// 7. FTDI USB to Moteino programming interface (Not needed if you are using Moteino USB devices)
******************************************************************************************************
*/
#include <RFM69.h>         //get it here: http://github.com/lowpowerlab/rfm69
#include <SPIFlash.h>      //get it here: http://github.com/lowpowerlab/spiflash
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming
#include <SPI.h>           //comes with Arduino IDE (www.arduino.cc)

#define SERIAL_EN          //comment out if you don't want any serial output

//*****************************************************************************************************************************
// ADJUST THE SETTINGS BELOW DEPENDING ON YOUR HARDWARE/TRANSCEIVER SETTINGS/REQUIREMENTS
//*****************************************************************************************************************************
#define GATEWAYID   1    // this is the node ID of your gateway (which is probably tied to a Raspberry Pi)
#define NODEID      121   // must be unique to each sensor node
#define NETWORKID   100  // every node must match the same network ID to hear messages from each other and take actions directly
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY       RF69_915MHZ //Match this with the version of your Moteino's transceiver! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY      "0000000000000000" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW      //uncomment only for RFM69HW! Leave out if you have RFM69W!
//*****************************************************************************************************************************

#ifdef SERIAL_EN
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
  #define SERIALFLUSH();
#endif

// Define our pins
#define onboardLED 		9   // Moteino onboard LED is on digital pin 9
#define photoResistor 	A5
#define photoResistorPower  A2   // The photoresistor will be hooked to pins A2 and A5
#define externalLED 	3	// careful about which pins are already in use by Moteino's RFM69 transceiver! 
							// For example, on Moteino you CANNOT use D2, D8, D10, D11, D12, D13!!

// Now we will define how often to flash our onboard LED 
const long blinkLEDtime = 1000;  // 1000 is milliseconds so this means blink on/off in 1 second intervals
// and we need a way to record the last time we turned the LED on or off
unsigned long lastBlinkTime;

// The next variable defines how often to check the photoresistor's light level
const long photoResistorCheckTime = 30000;  // 10000 milliseconds is 10 seconds
// and we also record the time we report the photoresistor status with this
unsigned long lastPhotoResistorReport;

// We don't need to define how often to check commands to turn on/off the external LED.  That's because
// our Moteino will respond to commands to request it come on or off from the Raspberry Pi / Moteino gateway device.
// The only thing we do track is the external LED's current status as on (1) or off (0)
int ledStatus;  

char payload[] = "123 ABCDEFGHIJKLMNOPQRSTUVWXYZ";   //this is for transmitting to the Pi Gateway
byte STATUS;
RFM69 radio;
SPIFlash flash(8, 0xEF30); //WINDBOND 4MBIT flash chip on CS pin D8 (default for Moteino)

void setup() {
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
  #endif

  // Tell Moteino if our pins are inputs or outputs
  pinMode(onboardLED, OUTPUT);
  pinMode(photoResistor, INPUT);
  pinMode(photoResistorPower, OUTPUT);
  pinMode(externalLED, OUTPUT);

  // Initialize our onboard and external LEDs to start up turned off (set to low)
  digitalWrite(onboardLED, LOW);
  digitalWrite(externalLED, LOW);
  
  // Turn on power to the photoresistor
  digitalWrite(photoResistorPower, HIGH);
  
  // Initialize our timers for the onboard LED and photoresistor using millis which is the Photon's internal clock
  lastBlinkTime = millis();
  lastPhotoResistorReport = millis();
  
  char buff[20];  // create a variable called buff (short for "buffer") that can hold up to 20 characters
  
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
     radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  
  if (flash.initialize()){
    DEBUGln("EEPROM flash chip found, and is OK ...");
  }
  else 
    DEBUGln("EEPROM flash chip failed to initialize! (is chip present on the Moteino?)");
  
  
  sprintf(payload, "Moteino Example : %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);  // sprintf command creates a string to put in the buff variable
  DEBUGln(payload);  // check what we are transmitting to the gateway receiver
  delay(5000); 
}

void loop() {
   // wireless programming token check
   // DO NOT REMOVE, or this sensor node will not be wirelessly programmable any more!
   CheckForWirelessHEX(radio, flash, true);

  // check for messages from the home automation gateway or other Moteino nodes  
    if (radio.receiveDone()){
      DEBUG("Msg received from sender ID ");DEBUG('[');DEBUG(radio.SENDERID);DEBUG("] ");
      for (byte i = 0; i < radio.DATALEN; i++)
          DEBUG((char)radio.DATA[i]);
      DEBUGln();
      //first send any ACK to request
      DEBUG("   [RX_RSSI:");DEBUG(radio.RSSI);DEBUGln("]");
      DEBUG("   Payload length = ");
      DEBUGln(radio.DATALEN);  
    
      if (radio.DATALEN==3){
          //check for a web page request from Pi Gateway to turn on or off the external LED
          //we will receive from the Pi Gateway either a message of "LOF" or LON"
          if (radio.DATA[0]=='L' && radio.DATA[1]=='O' && radio.DATA[2]=='F'){    // "LOF" is short for "LED off", keep radio messages short for best results
            digitalWrite(externalLED, LOW);	// Tell our external LED to turn OFF
            ledStatus = 0;					// this sets our LED status variable 
            delay(50);
            transmitStatus(9900, ledStatus);   // now we transmit LED status back to the Pi Gateway that we turned the LED off using transmitStatus subroutine
            									// We are passing a unique 4-digit number to identify a unique data type for our Pi Gateway, since it listens
            									// to a lot of sensor nodes in our house.  Each sensor node gets its own set of 4-digit codes.
            									// Any time we transmit an LED status, we will always identify it with sensor code "9900"
          }
          else if (radio.DATA[0]=='L' && radio.DATA[1]=='O' && radio.DATA[2]=='N'){  // "LON" is short for "LED on"
       	    digitalWrite(externalLED, HIGH);	// Tell our external LED to turn ON
            ledStatus = 1;					// this sets our LED status variable 
            delay(50);
            transmitStatus(9900, ledStatus);   // now we transmit LED status back to the Pi Gateway that we turned the LED off
          }
      }
    }

    if (radio.ACKRequested())
    {
      radio.sendACK();
      DEBUGln("ACK sent.");
    }

  // check if the onboard LED should be turned on or off
  if ((millis() - lastBlinkTime) >= blinkLEDtime) {
      lastBlinkTime = millis();  // update current time for the next time through the loop
      digitalWrite(onboardLED, !digitalRead(onboardLED));  // this says in one simple line to flip-flop the status of the pin
  }

  // check if it is time to report the photoresistor's value to the Moteino USB gateway (receives data for the Raspberry Pi) 
  if ((millis() - lastPhotoResistorReport) >= photoResistorCheckTime) {
      lastPhotoResistorReport = millis();   // update current time for the next time through the loop
      int lightLevel = analogRead(photoResistor);  // we read in the photoResistor level here (value can be 0 to 4095)
      
      // publish the actual value of the light level.  We will call this sensor ID data code "9912"
      transmitStatus(9905, lightLevel);
  }
}  // this is the end of the loop


// The function below will transmit the LED status to our Moteino USB / Pi Gateway
void transmitStatus(int item, int status){  
    sprintf(payload, "%d:%d", item, status);
    byte buffLen=strlen(payload);
    radio.sendWithRetry(GATEWAYID, payload, buffLen);
    DEBUG("Transmitted payload: ");
    DEBUGln(payload);
    delay(10);
}
