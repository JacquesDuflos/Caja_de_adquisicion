#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
float V1;
float V2;
float I1;
float I2;
String message;
String vegal;
String iegal;

LiquidCrystal_I2C lcd(0x27,  16, 2);

void setup() {
  // Start serial comunication
  Serial.begin(9600);
  // initialize lcd screen
  lcd.init();
  // turn on the backlight
  lcd.backlight();
}

void loop() {
  // Getting the infos
  // the volts are sensed directly by analog input, so 0 to 1023 val are mapped to 0-5v
  V1 = mapfloat (analogRead(A0), 0, 1023, 0, 5);
  delay(5);
  V2 = mapfloat (analogRead(A1), 0, 1023, 0, 5);
  delay(5);
  // The intensity come from a ASC712 B05 sensor with a sensitivity of 185 mV / A
  // So I map from the 0-1023 to 0-5 then from 2.5 - 2.685 to 0-1A
  I1 = map (analogRead(A2), 0, 1023, 0, 5000);
  I1 = map (I1, 2500, 2685, 0, 1000);
  I1 = float(I1)/1000.0;
  delay(5);
  I2 = map (analogRead(A3), 0, 1023, 0, 5000);
  I2 = map (I2, 2500, 2685, 0, 1000);
  I2 = float(I2)/1000.0;

  // printing to LCD
  vegal = "V1=";
  iegal = "I1=";
  message = vegal + V1 + iegal + I1;
  lcd.setCursor(0,0);
  lcd.print(message);

  lcd.setCursor(0,1);
  lcd.print("V2, I2, P2");

  // Create the JSON document
  StaticJsonDocument<200> Json_enviar;
  Json_enviar["ProductName"] = "ModuloDidactico";
  Json_enviar["V1"] = V1;
  Json_enviar["V2"] = V2;
  Json_enviar["I1"] = I1;
  Json_enviar["I2"] = I2;
  serializeJson(Json_enviar, Serial);
  Serial.println();

  delay(1000);
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}