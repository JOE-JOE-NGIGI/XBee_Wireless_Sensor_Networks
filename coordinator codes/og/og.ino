/*
Coordinator sensor Node for a Smart Laboratory Information Management System.
=============================================================================


NodeMCU 32S (ESP32) implementation of
an XBee cordinator Node, receiving temperature and humidity
data from two XBee router sensor nodes via Hardware Serial UART interface,
using ZigBee Protocol.

An RFID reader has been integrated for scanning tags when registering
Laboratory Samples.

All sensor Data is sent to ThingSpeak Cloud,
while the tag IDs are transmitted to a remote web-server via MQTT Protocol,
using Wi-Fi.

Authors: Edson Mwambe, Joseph Ngigi Wangere, Daudi Flavian

 */


#include <HTTPClient.h>

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <MFRC522.h>
#include <DHT.h>
#include <Printers.h>
#include <XBee.h>

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include "binary.h"
#include <HardwareSerial.h>

#include "PubSubClient.h"

XBeeWithCallbacks xbee;
HardwareSerial MySerial(1);


#define DebugSerial Serial
#define XBeeSerial MySerial

#define DHTPIN 21
#define DHTTYPE DHT22

#define SS_PIN 5  //--> SDA / SS is connected to pinout D2
#define RST_PIN 22  //--> RST is connected to pinout D1

#define I2C_SDA 4
#define I2C_SCL 0

#define buzzer 12
#define ON_Board_LED 2  //--> Defining an On Board LED, used for indicators when the process of connecting to a wifi router

#define TX_PIN 17 //Rx of xbee is connected to 17
#define RX_PIN 16 // TX of xbee is connected to 16


float t0, h0, t1, h1, t2, h2;

//mqtt credentials
const char mqttUserName[] = "EyApGS0TKTgvNAcJCzUlLDU";
const char clientID[] = "EyApGS0TKTgvNAcJCzUlLDU";
const char mqttPass[] = "JccRvq02/dm22nRJOwDOCtkc";

#define mqttPort 1883
#define channelID 1646176

const char* server = "mqtt3.thingspeak.com";
long lastPublishMillis = 0;
int connectionDelay = 1;
int updateInterval = 15;


WiFiClient client;
PubSubClient mqttClient(client);

MFRC522 mfrc522(SS_PIN, RST_PIN);  //--> Create MFRC522 instance.

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);


//----------------------------------------SSID and Password of your WiFi router-------------------------------------------------------------------------------------------------------------//
//const char* ssid = "C12";
//const char* password = "bereal12";
const char* ssid = "Coco";
const char* password = "usiingie9";
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

int readsuccess;
int count = 0;
byte readcard[4];
char str[32] = "";
String StrUID;

// Connect to WiFi.
void connectWifi()
{

  WiFi.begin( ssid, password );
  DebugSerial.print( F("Connecting to Wi-Fi..." ));
  lcd.clear();
  // Loop until WiFi connection is successful
  while ( WiFi.status() != WL_CONNECTED ) {
    lcd.setCursor(0, 0);
    lcd.print(F("no WiFi"));
    DebugSerial.println("not connected to WiFi");
    //----------------------------------------Make the On Board Flashing LED on the process of connecting to the wifi router.
    digitalWrite(ON_Board_LED, LOW);
    delay(250);
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
  }
  DebugSerial.println( F("Connected to Wi-Fi." ));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("WiFi connected"));
  digitalWrite(ON_Board_LED, HIGH);

  //----------------------------------------If successfully connected to the wifi router, the IP Address that will be visited is displayed in the serial monitor
  delay(1000);
//  lcd.clear();
  DebugSerial.println("");
  DebugSerial.print(F("Successfully connected to : "));
  DebugSerial.println(ssid);
  DebugSerial.print(F("IP address: "));
  DebugSerial.println(WiFi.localIP());
}


// Publish messages to a ThingSpeak channel.
void mqttPublish(long pubChannelID, String message) {
  String topicString = "channels/" + String( pubChannelID ) + "/publish";
  mqttClient.publish( topicString.c_str(), message.c_str() );
}


// Connect to MQTT server.
void mqttConnect() {
  // Loop until connected.
  while ( !mqttClient.connected() )
  {
    // Connect to the MQTT broker.
    if ( mqttClient.connect( clientID, mqttUserName, mqttPass ) ) {
      DebugSerial.print(F( "MQTT to " ));
      DebugSerial.print( server );
      DebugSerial.print (F(" at port "));
      DebugSerial.print( mqttPort );
      DebugSerial.println( F(" successful." ));
    } else {
      DebugSerial.print(F( "MQTT connection failed, rc = " ));
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      DebugSerial.print( mqttClient.state() );
      DebugSerial.println(F( " Will try again in a few seconds" ));

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT failed ");
      lcd.setCursor(0, 1);
      lcd.print("Restart device ");
      delay( connectionDelay * 1000 );
     
    }
  }
}

//display sensor data on lcd
void display_data() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp : ");
  lcd.print(t0, 1);
  lcd.print(char(223));
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(h0, 1);
  lcd.print("%");

}


//----------------------------------------Procedure for reading and obtaining a UID from a card or keychain---------------------------------------------------------------------------------//
int getid() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }

  DebugSerial.print("THE UID OF THE SCANNED CARD IS : ");

  for (int i = 0; i < 4; i++) {
    readcard[i] = mfrc522.uid.uidByte[i]; //storing the UID of the tag in readcard
    array_to_string(readcard, 4, str);
    StrUID = str;
  }
  mfrc522.PICC_HaltA();
  return 1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

//----------------------------------------Procedure to change the result of reading an array UID into a string------------------------------------------------------------------------------//
void array_to_string(byte array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}

// xbee function
void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());
  uint8_t type = b.remove<uint8_t>();

  if (type == 1 && b.len() == 8) {
    DebugSerial.print(F("DHT packet received from "));
    printHex(DebugSerial, rx.getRemoteAddress64());
    DebugSerial.println("");
    if (rx.getRemoteAddress64() == 5526146541606159) {
      DebugSerial.println("Node 2");
      t1 = b.remove<float>();
      h1 = b.remove<float>();
    }

    if (rx.getRemoteAddress64() == 5526146543843488) {
      DebugSerial.println("Node 1");
      t2 = b.remove<float>();
      h2 = b.remove<float>();
    }


  }

  DebugSerial.println(F("Unknown or invalid packet"));
  printResponse(rx, DebugSerial);
}



//-----------------------------------------------------------------------------------------------SETUP--------------------------------------------------------------------------------------//
void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Coordinator"));
  delay(1000);
  lcd.clear();

  SPI.begin();      //--> Init SPI bus
  mfrc522.PCD_Init(); //--> Init MFRC522 card

  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  pinMode(buzzer, OUTPUT);

  // Setup XBee serial communication
  XBeeSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  xbee.begin(XBeeSerial);
  delay(1);

  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
  xbee.onResponse(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
  xbee.onZBRxResponse(processRxPacket);

  dht.begin();

  pinMode(ON_Board_LED, OUTPUT);
  digitalWrite(ON_Board_LED, HIGH); //--> Turn off Led On Board

  //connect to WiFi
  connectWifi();

  mqttClient.setServer( server, mqttPort );

  DebugSerial.println("Please tag a card or keychain to see the UID !");
  DebugSerial.println("");


}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

//-----------------------------------------------------------------------------------------------LOOP---------------------------------------------------------------------------------------//
void loop() {
  // Reconnect to WiFi if it gets disconnected.
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  // Connect if MQTT client is not connected and resubscribe to channel updates.
  if (!mqttClient.connected()) {
    mqttConnect();
  }

  readsuccess = getid();
  xbee.loop();


  // Call the loop to maintain connection to the server.
  mqttClient.loop();

  if ( abs(millis() - lastPublishMillis) > updateInterval * 1000) {

    h0 = dht.readHumidity();
    t0 = dht.readTemperature();


    if (isnan(h0) || isnan(t0)) {
      DebugSerial.println(F("Failed to read from DHT22"));
      return;
    }

    display_data();

    String msg = "field1=";
    msg += t0;
    msg += "&field2=";
    msg += h0;
    msg += "&field3=";
    msg += t1;
    msg += "&field4=";
    msg += h1;
    msg += "&field5=";
    msg += t2;
    msg += "&field6=";
    msg += h2;
    mqttPublish( channelID, msg);
    lastPublishMillis = millis();

  }


  if (readsuccess) {

    digitalWrite(buzzer, HIGH);
    digitalWrite(ON_Board_LED, LOW);
    delay(1000);
    digitalWrite(buzzer, LOW);
    HTTPClient http;    //Declare object of class HTTPClient

    String UIDresultSend, postData;
    UIDresultSend = StrUID;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(UIDresultSend);
    

    //Post Data
    postData = "UIDresult=" + UIDresultSend;

        http.begin("http://192.168.98.116/LIMS/staff/getUID.php");  //Specify request destination(private)
//    http.begin("http://10.30.20.209/LIMS/staff/getUID.php");  //Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //Specify content-type header

    int httpCode = http.POST(postData);   //Send the request


    DebugSerial.println(UIDresultSend);
    if (httpCode != 200) {

      DebugSerial.print("Couldn't sent post request, httpCode: ");
    } else {

      DebugSerial.print("Post request is successful, httpCode: ");
    }
    DebugSerial.println(httpCode);   //Print HTTP return code
    String payload = http.getString();    //Get the response payload
    DebugSerial.println(payload);    //Print request response payload
    http.end();  //Close connection
//    delay(500);
    
    digitalWrite(ON_Board_LED, HIGH);
    delay(5000);
    display_data();
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
