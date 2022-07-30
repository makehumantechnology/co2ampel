/*
read ppm from scd30 and display on neopixel strip (stick/ring)
version: standalone (data via bluetooth and serial)
*/
#include <Sensirion_GadgetBle_Lib.h>

#include <Wire.h>

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#include "SparkFun_SCD30_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_SCD30
SCD30 airSensor;

#define LED_PIN 15 // strip is connected to pin
#define LED_COUNT 12 // number of pixels

#define SEALEVELPRESSURE_HPA (1013.25)

#define HIGH_CO2_BOUNDARY 1200
#define LOW_CO2_BOUNDARY 800

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static int64_t lastMmntTime = 0;
static int startCheckingAfterUs = 1900000;

GadgetBle gadgetBle = GadgetBle(GadgetBle::DataType::T_RH_CO2);

int ppm;
int hum;
int temp;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

Button button1 = {0, 0, false};

void IRAM_ATTR isr() {
  button1.numberKeyPresses += 1;
  button1.pressed = true;
}

void setup()
{
  Serial.begin(115200);
 
  Wire.begin();
  delay(1000); // give sensors some time to power-up

  attachInterrupt(button1.PIN, isr, FALLING);	
	
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }  

  // Initialize the GadgetBle Library
  gadgetBle.begin();
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(gadgetBle.getDeviceIdString());

	
  // set temperature offset for co2-sensor
  airSensor.setTemperatureOffset(4);	
	
  strip.begin();
  strip.setBrightness(64); // set brightness
  strip.show(); // Initialize all pixels to 'off'
}

void loop()
{
  
  if (airSensor.dataAvailable())
  {
    ppm=airSensor.getCO2();
    hum=airSensor.getHumidity();
    temp=airSensor.getTemperature();

    gadgetBle.writeCO2(ppm);
    gadgetBle.writeTemperature(temp);
    gadgetBle.writeHumidity(hum);

    gadgetBle.commit();

    StaticJsonBuffer<200> jsonBuffer;

    JsonObject& root = jsonBuffer.createObject();
    root["sensor"] = "S00001";
    root["co2"] = airSensor.getCO2();
    root["temp"] = airSensor.getTemperature();
    root["humidity"] = airSensor.getHumidity();

    root.printTo(Serial);
    Serial.println("\n");
  }
  
  gadgetBle.handleEvents();
	
  if (button1.pressed) {
    Serial.printf("Button 1 has been pressed %u times\n", button1.numberKeyPresses);
    button1.pressed = false;

    // calibration
    airSensor.setAltitudeCompensation(0); // Altitude in m ü NN
    airSensor.setForcedRecalibrationFactor(400); // fresh air
    Serial.print("SCD30 calibration, please wait 30 s ...");
      
    strip.clear();
    for ( int i = 0; i < LED_COUNT; i++) { // blue for calibration
      strip.setPixelColor(i, 0, 0, 255);
    }
    strip.show();
      
    delay(30000);
      
    Serial.println("Calibration done");
    Serial.println("Rebooting device");
    ESP.restart();
      
    delay(5000);
  }
  
    int c;
    
    if (ppm>HIGH_CO2_BOUNDARY) { c=strip.Color(255,0,0); }
    if (ppm>LOW_CO2_BOUNDARY && ppm <= HIGH_CO2_BOUNDARY) { c=strip.Color(255,165,0); }
    if (ppm<=LOW_CO2_BOUNDARY) { c=strip.Color(0,128,0); }    

    int ct=map(ppm, 350, HIGH_CO2_BOUNDARY, 1, 12);
  
    for(uint16_t i=0; i<strip.numPixels(); i++) {
      if (i<=ct) {
        strip.setPixelColor(i, c);
      } else {
        strip.setPixelColor(i, strip.Color(0,0,0));
      }
    }
    strip.show();    
    
    delay(2000);
}
