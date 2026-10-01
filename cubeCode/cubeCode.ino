#include <BlinkyPicoW.h>
#include "one_wire.h"    // from https://github.com/adamboardman/pico-onewire

// --- Configuration Constants ---
constexpr int BLINKY_DIAG    = 0;
constexpr int CUBE_DIAG      = 0;
constexpr int COMM_LED_PIN   = 2;
constexpr int RST_BUTTON_PIN = 3;
constexpr int NUMCHAN        = 3;
constexpr float TMIN        = -50.0;
constexpr float TMAX        =  80.0;

constexpr int SIGNAL_PIN[NUMCHAN] = {12, 15, 17};
constexpr int POWER_PIN[NUMCHAN]  = {11, 14, 16};


struct CubeSetting
{
  uint32_t publishInterval;
};

struct CubeReading
{
  float temp[NUMCHAN];
};

struct CubeArm 
{
  bool temp[NUMCHAN];
};

// --- Global Variables ---
CubeSetting setting;
CubeReading reading;
CubeReading readingLow;
CubeReading readingHigh;
CubeArm readingArm;

unsigned long lastPublishTime;

One_wire tempOneWire[] = 
{
  One_wire(SIGNAL_PIN[0]),
  One_wire(SIGNAL_PIN[1]),
  One_wire(SIGNAL_PIN[2])
};

rom_address_t tempAddress[3]{}; 

// --- Helper Functions ---
template <typename T>
inline bool outsideLimits(T current, T low, T high) 
{
  return (current < low) || (current > high);
}

void setupBlinky()
{
  if (BLINKY_DIAG > 0) Serial.begin(9600);

  BlinkyPicoW.setMqttKeepAlive(15);
  BlinkyPicoW.setMqttSocketTimeout(4);
  BlinkyPicoW.setMqttPort(1883);
  BlinkyPicoW.setMqttLedFlashMs(100);
  BlinkyPicoW.setHdwrWatchdogMs(8000);

  BlinkyPicoW.begin(BLINKY_DIAG, COMM_LED_PIN, RST_BUTTON_PIN, true, sizeof(setting), sizeof(reading));
}

void setupCube()
{
  if (BLINKY_DIAG < 1 && CUBE_DIAG > 0) {Serial.begin(9600);}
  setting.publishInterval = 4000;

  for (int i = 0; i < NUMCHAN; ++i) 
  {
    pinMode(SIGNAL_PIN[i], INPUT_PULLUP);
    pinMode(POWER_PIN[i],  OUTPUT);
    digitalWrite(POWER_PIN[i], HIGH);
  }
  
  delay(1000);

  for (int i = 0; i < NUMCHAN; ++i) 
  {
    tempOneWire[i].init();
    tempOneWire[i].single_device_read_rom(tempAddress[i]);
    reading.temp[i] = -100.0;
    readingArm.temp[i] = true;
  }

  lastPublishTime = millis(); 
}

void loopCube()
{
  unsigned long now = millis();
  if ((now - lastPublishTime) > setting.publishInterval)
  {
    lastPublishTime = now;
    if (BlinkyPicoW.publishCubeData(reinterpret_cast<uint8_t*>(&setting), reinterpret_cast<uint8_t*>(&reading), false)) 
    {
      for (int i = 0; i < NUMCHAN; ++i) 
      {
        if (!outsideLimits(reading.temp[i], readingLow.temp[i], readingHigh.temp[i])) 
        {
          readingArm.temp[i] = true;
        }
      }
    }  
  }
  for (int i = 0; i < NUMCHAN; ++i) 
  {
    tempOneWire[i].convert_temperature(tempAddress[i], true, false);
    float testTemp = tempOneWire[i].temperature(tempAddress[i]);
    if ((TMIN < testTemp) && (testTemp < TMAX))
    {
      reading.temp[i] = testTemp;
      if (BlinkyPicoW.isInitialized())
      {  
        if (outsideLimits(reading.temp[i], readingLow.temp[i], readingHigh.temp[i])) 
        {
          if (readingArm.temp[i]) 
          {
            const bool published = BlinkyPicoW.publishCubeData(
              reinterpret_cast<uint8_t*>(&setting), 
              reinterpret_cast<uint8_t*>(&reading), 
              true
            );
            readingArm.temp[i] = !published;
            if (published) 
            {
              lastPublishTime = now;
            }
          }
        }
      }
    }
  }
  // Check for New MQTT Settings
  const bool newSettings = BlinkyPicoW.retrieveCubeSetting(
    reinterpret_cast<uint8_t*>(&setting), 
    reinterpret_cast<uint8_t*>(&readingLow), 
    reinterpret_cast<uint8_t*>(&readingHigh)
  );
  if (newSettings) 
  {
    if (setting.publishInterval < 4000) setting.publishInterval = 4000;
  }
}
