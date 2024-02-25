/* 
  Water Level
  sensorPinWL 27
  sensorPower 26
  Ultrasonic
  trig 12
  echo 13
  pHmeter
  potPin 34
  Water Temp
  Sensor pin  25
  Servo
  signal 14
  FlowMeter
  interrupt 33
  Sensor pin 33
  
 */

#define BUILTIN_LED 2
#include <WiFi.h>
#include <PubSubClient.h>

#include <HCSR04.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define sensorPinWL 27
#define sensorPower 26

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
const char* ssid = "Fik";
const char* password = "23456789";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Water Level
int val = 0;

// Ultrasonic
HCSR04 hc(12, 13); //initialisation class HCSR04 (trig pin , echo pin)

// Servo
Servo myservo;  // create servo object to control a servo

// pHmeter
const int potPin=34;
float ph;
float Value=0;

// Water Temperature
const int SENSOR_PIN = 25; 
OneWire oneWire(SENSOR_PIN);         
DallasTemperature tempSensor(&oneWire); 
float tempCelsius;    

// FlowMeter 
byte sensorInterrupt = 33;  // 0 = digital pin 2
byte sensorPin       = 33;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 4.5;

volatile byte pulseCount;  

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

 for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("aley", "hello world");
      // ... and resubscribe
      client.subscribe("fishfeeder");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
   // Initialize the BUILTIN_LED pin as an output
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    delay(1000);
    
    // Water Level
    pinMode(sensorPower, OUTPUT);
    digitalWrite(sensorPower, LOW);
  
    //Servo
    myservo.attach(14);  // attaches the servo on pin 9 to the servo object
    
    // pHmeter
    pinMode(potPin,INPUT);
    delay(1000);

    // Water Temp
    tempSensor.begin();

    //FlowMeter
    pinMode(sensorPin, INPUT);
    digitalWrite(sensorPin, HIGH);
  
    pulseCount        = 0;
    flowRate          = 0.0;
    flowMilliLitres   = 0;
    totalMilliLitres  = 0;
    oldTime           = 0;
  
    // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
    // Configured to trigger on a FALLING state change (transition from HIGH
    // state to LOW state)
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

void loop()
{
    Water_Level();
    Ultrasonic();
    pHmeter();
    WaterTemp();
    FlowMeter();

    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  
    unsigned long now = millis();
    if (now - lastMsg > 2000) {
      lastMsg = now;
    }
      //pH meter
      if (ph < 5.5) {
        client.publish("phmeter","pH air rendah");
      }
      else if (ph > 8.5) {
        client.publish("phmeter","pH air tinggi");
      }
      else {
        client.publish("phmeter","pH normal");
      }
      
      //water level
      int level = readSensor();
      if (level < 500){

       client.publish("waterlevel","Tambah Air");
        }
      else {
        client.publish("waterlevel","Air cukup");
      }
  
      //flowmeter
      if (int(flowRate)==0 ) {
        client.publish("flowmeter","Pompa tersumbat");
      }
      else{
        client.publish("flowmeter", "Pompa lancar");
      }
  
      //ultrasonic distance sensor
      if (hc.dist() >=6) {
        client.publish("ultrasonic","Makanan habis");
      }
      else {
        client.publish("ultrasonic","Makanan Cukup");
      }
      
      char msg_out[20];
      sprintf(msg_out, "%f",tempCelsius);
      client.publish("watertemperature",msg_out);  
        
    if (bootCount%3 == 0) {
    // run code if test expression is true
      servo();
    }
    deepsleep();
}

void Water_Level()
{
    int level = readSensor();
    Serial.print("Water level: ");
    Serial.println(level);
    if (level < 500){
      Serial.print("Kering");
      }
    else {
      Serial.print("Basah");
      }

      delay(1000);
}

void Ultrasonic() 
{
    Serial.print("jarak makanan");
    Serial.println(hc.dist()); 
    delay(60);
}


void pHmeter()
{
    Value= analogRead(potPin);
    Serial.print(Value);
    Serial.print(" | ");
    float voltage=Value*(3.3/4095.0);
    ph=(3.3*voltage);
    Serial.println(ph);
    delay(500);
}

void WaterTemp()
{
  tempSensor.requestTemperatures();             // send the command to get temperatures
  tempCelsius = tempSensor.getTempCByIndex(0);  // read temperature in Celsius
  Serial.print("Temperature: ");
  Serial.print(tempCelsius);    // print the temperature in Celsius
  Serial.print("°C");
  Serial.print("  ~  ");        // separator between Celsius and Fahrenheit

  delay(500);
}


void FlowMeter()
{
   if((millis() - oldTime) > 1000)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorInterrupt);
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
 // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;
    
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t");       // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");        
    Serial.print(totalMilliLitres);
    Serial.println("mL"); 
    Serial.print("\t");       // Print tab space
  Serial.println();
    

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
  // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }
}

void servo(){
  myservo.write(180);
  delay(1000);
  myservo.write(0);
  delay(100);
  myservo.write(180);
  delay(100);
  myservo.write(0);
  delay(1000);
  myservo.write(180);
  delay(1000);
  Serial.println("servo work");

}

void deepsleep(){
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
//Serial.println("Configured all RTC Peripherals to be powered down in sleep");

  /*
  Now that we have setup a wake cause and if needed setup the
  peripherals state in deep sleep, we can now start going to
  deep sleep.
  In the case that no wake up sources were provided but deep
  sleep was started, it will sleep forever unless hardware
  reset occurs.
  */
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}


// ADITIONAL CODE

// FlowMeter
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}


// Water Level
int readSensor() {
  digitalWrite(sensorPower, HIGH);  
  delay(10);                        
  val = analogRead(sensorPinWL);      
  digitalWrite(sensorPower, LOW);  
  return val;                      
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;

   default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
};

