#include <ArduinoJson.h>

float V1;
float V2;
float I1;
float I2;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  V1 = analogRead(A0);
  delay(5);
  V2 = analogRead(A1);
  delay(5);
  I1 = analogRead(A2);
  delay(5);
  I2 = analogRead(A3);

  // Create the JSON document
  StaticJsonDocument<200> Json_enviar;
  Json_enviar["V1"] = V1;
  Json_enviar["V2"] = V2;
  Json_enviar["I1"] = I1;
  Json_enviar["I2"] = I2;
  serializeJson(Json_enviar, Serial);
  Serial.println();

  delay(100);
}