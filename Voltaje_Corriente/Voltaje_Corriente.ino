#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
float V1;
float V2;
float I1;
float I2;

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
  V1 = analogRead(A0);
  delay(5);
  V2 = analogRead(A1);
  delay(5);
  I1 = analogRead(A2);
  delay(5);
  I2 = analogRead(A3);

  // printing to LCD
  lcd.setCursor(0,0);
  lcd.print("V1, I1, P1");

  lcd.setCursor(1,0);
  lcd.print("V2, I2, P2");

  // Create the JSON document
  StaticJsonDocument<200> Json_enviar;
  Json_enviar["V1"] = V1;
  Json_enviar["V2"] = V2;
  Json_enviar["I1"] = I1;
  Json_enviar["I2"] = I2;
  serializeJson(Json_enviar, Serial);
  Serial.println();

  delay(1000);
}