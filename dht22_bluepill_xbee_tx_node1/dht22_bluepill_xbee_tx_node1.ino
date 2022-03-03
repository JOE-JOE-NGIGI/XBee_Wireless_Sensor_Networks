#include <binary.h>

/*Code to read Temperature and humidity
data from a DHT22 AM2302 sensor.
The sensor readings are displayed on
a 1602 LCD module that is interfaced
using an i2c connector. The sensor readings
are also relayed on the serial uart interface
to be sent by XBEE radio module wirelessly to a cordinator.

Code version 1.0

Author: Joseph Ngigi

Date 15/01/2022

*/
#include <XBee.h>




#include <String.h>


//#include <SoftwareSerial.h>




//SoftwareSerial xbee(rx, tx); //RX, TX


//these two libraries enable the i2c
#include <SoftWire.h>
#include <Wire.h>

#include <LiquidCrystal_I2C.h> //i2c lcd library

//DHT library
#include <DHT.h>


#define DEVICE_ID "NODE_2"

//create a LiquidCrystal object called lcd
LiquidCrystal_I2C lcd(0x27, 16, 2); //pass address 0x27, columns and rows


//define DHT data pin
#define DHTPIN PA8


#define DHTTYPE DHT22 //use DHT22 (AM2302) sensor type

#define statusLED PB14 //for status 

#define errorLED PB13  //for indicating transmission error

//initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

XBee xbee = XBee();


uint8_t payload[8] = {0,0,0,0,0,0,0,0}; //initialize data array with 0

//use a union to convert string to a byte string
union u_tag {
    uint8_t b[4];
    float fval;
} u;

// SH + SL Address of receiving XBee
XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x41B8E515);
ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload)); //create a Tx request
ZBTxStatusResponse txStatus = ZBTxStatusResponse();


//char t, h;


//XBee xbee;

//XBee *xbee = new XBee();


//define variables
float humidity; //stores humidity value of type float
float temperature; //stores temperature value of type float

//function to flash led


void flashLED(char pin, int times, int wait) {
 
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(wait);
    digitalWrite(pin, LOW);
 
    if (i + 1 < times) {
      delay(wait);
    }
  }
}

void setup() {
  // initialize serial
  Serial2.begin(9600);

  pinMode(statusLED, OUTPUT);

  pinMode(errorLED, OUTPUT);

  xbee.setSerial(Serial2); //tell XBee to use Hardware Serial

  //xbee.begin(9600);

  //Serial.println("Ready");

  //initialize display
  lcd.init();

  //enable backlight
  lcd.backlight();

  //set cursor position to column 0 row 0
  lcd.setCursor(0,0);

  //print initial message
  lcd.print("Sensor Node 1 ...");
  delay(1000); //wait 1 second
  
  lcd.clear(); //clear the display

  //initialize dht sensor
  dht.begin();

  //Serial.println("Ready!");

  flashLED(statusLED, 3,50);

}


void loop() {
  delay(2000);
  
  //read temperature and humidity values and store in variables temperature and humidity
  temperature = dht.readTemperature();
  humidity =    dht.readHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    // convert humidity into a byte array and copy it into the payload array
    u.fval = humidity;
    for (int i=0;i<4;i++){
      payload[i]=u.b[i];
    }

    // same for the temperature
    u.fval = temperature;
    for (int i=0;i<4;i++){
      payload[i+4]=u.b[i];
    }

    xbee.send(zbTx);

    // flash TX indicator
    flashLED(statusLED, 1, 100);


    // after sending a tx request, we expect a status response
    // wait up to half second for the status response
    if (xbee.readPacket(500)) {
      // got a response!
 
      // should be a znet tx status             
      if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
        xbee.getResponse().getZBTxStatusResponse(txStatus);
 
        // get the delivery status, the fifth byte
        if (txStatus.getDeliveryStatus() == SUCCESS) {
          // success.  time to celebrate
          flashLED(statusLED,5, 50);
        } else {
          // the remote XBee did not receive our packet. is it powered on?
          flashLED(errorLED, 3, 500);
        }
      }
    } else if (xbee.getResponse().isError()) {
      //nss.print("Error reading packet.  Error code: ");  
      //nss.println(xbee.getResponse().getErrorCode());
    } else {
      // local XBee did not provide a timely TX Status Response -- should not happen
      flashLED(errorLED, 2, 50);
    }
  }
  
  /*String frame = String(DEVICE_ID) + ":" + String(temperature) + ":" + String(humidity);*/
 // Serial.println("check");
  
/*
  //print the readings on serial monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("\xc2\xb0");
  Serial.print("C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
*/
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0,1);
  lcd.print("Humi: ");
  lcd.print(humidity);
  lcd.print(" %");

  delay(2000);

}
