//
//  Uses ESP32 Dev board to connect to Bluetooth OBD scanner.
//  Continuously reads the vehicle speed and updates the
//  LCD display and outputs speed pulses.
//

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PinLED 2

LiquidCrystal_I2C lcd(0x27, 16, 2);

#include "BluetoothSerial.h"

//#define BT_USE_NAME

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth not enabled
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth SPP not enabled
#endif

BluetoothSerial SerialBT;

String DevName = "OBDII";
//uint8_t DevAddr[6] = {0x1C,0xA1,0x35,0x69,0x8D,0xC5};
uint8_t DevAddr[6] = {0x00,0x10,0xCC,0x4F,0x36,0x03};
const char *DevPin="1234";

#include <driver/timer.h>

// ISR for timer interrupts
bool IRAM_ATTR timer0_event(void *args)
{
  digitalWrite(PinLED,!digitalRead(PinLED));
  return 0;
}

timer_config_t timer0 = {
  .alarm_en = TIMER_ALARM_EN,
  .counter_en = TIMER_PAUSE,
  .intr_type = TIMER_INTR_LEVEL,
  .counter_dir = TIMER_COUNT_UP,
  .auto_reload = TIMER_AUTORELOAD_EN,
  .divider = 80
};


unsigned long pulsePeriod;
int pulseFreq=0;
char resp[20];
char *strings[6];
char *ptr = NULL;
char *vals[6];


void setup()
{
  // initialize PinLED
  pinMode(PinLED, OUTPUT);

  // initialize UART
  Serial.begin(115200);
  Serial.println("Connecting... ");

  // initialize LCD
  lcd.begin();
  lcd.clear();
  lcd.setBacklight(1);
  lcd.setCursor(0,0);
  lcd.print("Connecting... ");

  // initialize Bluetooth
  SerialBT.begin("ESP32Test", true);
  SerialBT.setPin(DevPin);
#if defined(BT_USE_NAME)
  if (!SerialBT.connect(DevName))
#else
  if (!SerialBT.connect(DevAddr))
#endif
  {
    Serial.println("BT Connect: Failed");
    lcd.setCursor(0,0);
    lcd.print("BT: FAILED      ");
    while(1);
  }
  Serial.println("BT Connect: Ok");
  lcd.setCursor(0,0);
  lcd.print("BT: OK          ");
  delay(500);

  // connect to OBD
  SerialBT.print("AT I\r");
  getResponse();
  //Serial.println(resp);
  getStrings(resp);
  //Serial.println(strings[1]);
  if (strncmp(strings[1],"ELM327",6) != 0)
  {
    Serial.println("OBD Connect: Failed");
    lcd.setCursor(0,0);
    lcd.print("OBD: FAILED     ");
    while(1);
  }
  Serial.println("OBD Connect: Ok");
  lcd.setCursor(0,0);
  lcd.print("OBD: OK         ");
  delay(500);

  // connect to car
  lcd.setCursor(0,0);
  lcd.print("Car: ...        ");
  while (1)
  {
    SerialBT.print("010D\r");
    delay(500);
    getResponse();
    Serial.println(resp);
    getStrings(resp);
    getVals(strings[1]);
    if (strcmp(vals[0],"41") == 0)
    {
      lcd.setCursor(0,0);
      lcd.print("Car: Connected  ");
      break;
    }
  }
  delay(500);

  int carspeed = 1;
  pulseFreq = carspeed*2; // 2 pulses per kph
  if (pulseFreq != 0)
  {
    pulsePeriod = (1000000L/2)/pulseFreq;
  }
  else
  {
    pulsePeriod = (1000000L);    
  }
  //Serial.println(pulsePeriod);

  // initialize timer
#if 0
  timer_config_t timer0 = {
    .alarm_en = TIMER_ALARM_EN,
    .counter_en = TIMER_PAUSE,
    .intr_type = TIMER_INTR_LEVEL,
    .counter_dir = TIMER_COUNT_UP,
    .auto_reload = TIMER_AUTORELOAD_EN,
    .divider = 80
  };
#endif
  timer_init(TIMER_GROUP_0, TIMER_0, &timer0);
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 500000);
  timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer0_event, NULL, ESP_INTR_FLAG_IRAM);
  timer_start(TIMER_GROUP_0, TIMER_0);
}


void loop()
{
  char c;
  // get speed
  SerialBT.print("010D\r");
  getResponse();
  //Serial.println(resp);
  getStrings(resp);
  //Serial.println(strings[1]);
  getVals(strings[1]);
  //Serial.println(vals[2]);
  if (strcmp(vals[0],"41") == 0)
  {
    int carspeed = strtol(vals[2],NULL,16);
    pulseFreq = carspeed*2; // 2 pulses per kph
    if (pulseFreq != 0)
    {
      pulsePeriod = (1000000L/2)/pulseFreq;
      timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, pulsePeriod);
      //timer_start(TIMER_GROUP_0, TIMER_0);
      pinMode(PinLED, OUTPUT);
    }
    else
    {
      //pulsePeriod = (1000000L);
      //timer_pause(TIMER_GROUP_0, TIMER_0);
      pinMode(PinLED, INPUT_PULLDOWN);
    }
    char str[20];
    sprintf(str,"Speed: %3d KPH",carspeed);
    Serial.println(str);
    lcd.setCursor(0,0);
    lcd.print(str);
  }
  
  while(Serial.available())
  {
    c = Serial.read();
  }
  delay(100);
}


void getResponse()
{
  char c;
  int index = 0;
  while (1)
  {
    while (!SerialBT.available());
    c = SerialBT.read();
    if (c != '>')
    {
      if (c != '\r')
        resp[index++] = c;
      else
        resp[index++] = ':';
    }
    else
    {
      resp[index++] = c;
      resp[index++] = '\0';    
      break;
    }
  }
}


void getStrings(char *str)
{
  int index = 0;
  ptr = strtok(str,":");
  while (ptr != NULL)
  {
    strings[index++] = ptr;
    ptr = strtok(NULL,":");
  }
}


void getVals(char *str)
{
  int index = 0;
  ptr = strtok(str," ");
  while (ptr != NULL)
  {
    vals[index++] = ptr;
    ptr = strtok(NULL," ");
  }
}
