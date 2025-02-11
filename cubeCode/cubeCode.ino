#define BLINKY_DIAG        0
#define CUBE_DIAG          0
#define COMM_LED_PIN       2
#define RST_BUTTON_PIN     3
#include <BlinkyPicoW.h>
#include "one_wire.h" 
// from https://github.com/adamboardman/pico-onewire

struct CubeSetting
{
  uint16_t publishInterval;
};
CubeSetting setting;

struct CubeReading
{
  int16_t tempA;
  int16_t tempB;
  int16_t tempC;
  int16_t chipTemp;
};
CubeReading reading;

unsigned long lastPublishTime;
int signalPinA = 12;
int signalPinB = 15;
int signalPinC = 17;

int powerPinA = 11;
int powerPinB = 14;
int powerPinC = 16;

One_wire tempAOneWire(signalPinA);
One_wire tempBOneWire(signalPinB);
One_wire tempCOneWire(signalPinC);

rom_address_t tempAaddress{};
rom_address_t tempBaddress{};
rom_address_t tempCaddress{};

int g_tempCount = 0;

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
  if (CUBE_DIAG > 0) Serial.begin(9600);
  setting.publishInterval = 4000;
  pinMode(signalPinA, INPUT_PULLUP);  
  pinMode(signalPinB, INPUT_PULLUP);  
  pinMode(signalPinC, INPUT_PULLUP);  

  pinMode(powerPinA, OUTPUT);  
  pinMode(powerPinB, OUTPUT);  
  pinMode(powerPinC, OUTPUT);  

  digitalWrite(powerPinA, HIGH);
  digitalWrite(powerPinB, HIGH);
  digitalWrite(powerPinC, HIGH);
  
  delay(1000);
  
  tempAOneWire.init();
  tempBOneWire.init();
  tempCOneWire.init();
  
  tempAOneWire.single_device_read_rom(tempAaddress);
  tempBOneWire.single_device_read_rom(tempBaddress);
  tempCOneWire.single_device_read_rom(tempCaddress);
   
  reading.tempA = -100;
  reading.tempB = -100;
  reading.tempC = -100;
  g_tempCount = 0;

  lastPublishTime = millis(); 
}

void loopCube()
{
  unsigned long now = millis();
  int16_t temp;
  if ((now - lastPublishTime) > setting.publishInterval)
  {
    reading.chipTemp = (int16_t) (analogReadTemp() * 100.0);
    switch (g_tempCount) 
    {
      case 0:
        tempAOneWire.convert_temperature(tempAaddress, true, false);
        temp = (tempAOneWire.temperature(tempAaddress) * 100.0);
        if (temp > 30000)
        {
          tempAOneWire.convert_temperature(tempAaddress, true, false);
          temp = (tempAOneWire.temperature(tempAaddress) * 100.0);
        }
        if (temp < 30000) reading.tempA = temp;
        if (CUBE_DIAG > 0) Serial.print("Temp A: ");
        if (CUBE_DIAG > 0) Serial.println(reading.tempA);
        break;
      case 1:
        tempBOneWire.convert_temperature(tempBaddress, true, false);
        temp =  (tempBOneWire.temperature(tempBaddress) * 100.0);
        if (temp > 30000)
        {
          tempBOneWire.convert_temperature(tempBaddress, true, false);
          temp = (tempBOneWire.temperature(tempBaddress) * 100.0);
        }
        if (temp < 30000) reading.tempB = temp;
        if (CUBE_DIAG > 0) Serial.print("Temp B: ");
        if (CUBE_DIAG > 0) Serial.println(reading.tempB);
        break;
      case 2:
        tempCOneWire.convert_temperature(tempCaddress, true, false);
        temp = (tempCOneWire.temperature(tempCaddress) * 100.0);
        if (temp > 30000)
        {
          tempCOneWire.convert_temperature(tempCaddress, true, false);
          temp = (tempCOneWire.temperature(tempCaddress) * 100.0);
        }
        if (temp < 30000) reading.tempC = temp;
        if (CUBE_DIAG > 0) Serial.print("Temp C: ");
        if (CUBE_DIAG > 0) Serial.println(reading.tempC);
        break;
      default:
        break;
    }
    g_tempCount = g_tempCount + 1;
    if (g_tempCount > 2) g_tempCount = 0;

    lastPublishTime = now;
    boolean successful = BlinkyPicoW.publishCubeData((uint8_t*) &setting, (uint8_t*) &reading, false);
  }

  boolean newSettings = BlinkyPicoW.retrieveCubeSetting((uint8_t*) &setting);
  if (newSettings)
  {
    if (setting.publishInterval < 4000) setting.publishInterval = 4000;
  }
}
