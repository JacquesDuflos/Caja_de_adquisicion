#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
float V1;
float V2;
float I1;
float I2;
float P; // El poder, calculado a partir del v y i selecionado
float E; // la energia, calculada integrando el poder
unsigned long lastTime = 0; // to get the calculate the delta between 2 loops

int SVState; // estado del boton de cambio de voltimetro
int SIState; // estado del boton de cambio de amperimetro
int RBState; // estado del boton de resetear
int LastSVState = LOW; // estado presednete del boton de voltimetro
int LastSIState = LOW; // estado presedente del boton de amperimetro
int LastRBState = LOW; // estado presedente del boton de resetear
String vDisplayed = "V1"; // la tension para mostrar en lcd
String iDisplayed = "I1"; // la intensidad para mostrar en lcd
String vCalc = ""; // la tension para calcular la energia
String iCalc = ""; // la intensidad para calcular la energia

const int Vmetro1 = A0;
const int Vmetro2 = A1;
const int Imetro1 = A2;
const int Imetro2 = A3;
const int SwitchV = 2;
const int SwitchI = 3;
const int ResetButton = 4;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50; 

String message;
String vegal;
String iegal;
int sample = 5;

LiquidCrystal_I2C lcd(0x27,  16, 2);

void setup() {
  // Los pins de botones
  pinMode(SwitchV, INPUT);
  pinMode(SwitchI, INPUT);
  pinMode(ResetButton, INPUT);
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


  // Create the JSON document
  StaticJsonDocument<200> Json_enviar;
  Json_enviar["ProductName"] = "ModuloDidactico";
  Json_enviar["V1"] = V1;
  Json_enviar["V2"] = V2;
  Json_enviar["I1"] = I1;
  Json_enviar["I2"] = I2;
  serializeJson(Json_enviar, Serial);
  Serial.println();

  // Read the buttons
  int reading = digitalRead(SwitchV);
  if (reading != LastSVState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (reading != SVState){
      SVState = reading;
      if (SVState == HIGH){
        if (vDisplayed == "V1"){
          vDisplayed = "V2";
        }
        else{
          vDisplayed = "V1";
        }
      }
    }
  }
  reading = digitalRead(SwitchI);
  if (reading != LastSIState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (reading != SIState){
      SIState = reading;
      if (SIState == HIGH){
        if (iDisplayed == "I1"){
          iDisplayed = "I2";
        }
        else{
          iDisplayed = "I1";
        }
      }
    }
  }
  reading = digitalRead(SwitchV);
  if (reading != LastRBState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (reading != RBState){
      RBState = reading;
      if (RBState == HIGH){
        vCalc = vDisplayed;
        iCalc = iDisplayed;
        E = 0;
      }
    }
  }

  // calculate P
  if (vDisplayed == "V1"){
    P = V1;
  } else {
    P = V2;
  }
  if (iDisplayed == "I1"){
    P *= I1;
  } else {
    P*= I2;
  }

  // calculate E
  if (iCalc != "" and vCalc != ""){
    float dE;
    if (iCalc == "I1"){
      dE = I1;
    } else {
      dE = I2;
    }
    if (vCalc == "V1") {
      dE *= V1;
    } else {
      dE *= V2;
    }
    dE *= millis() - lastTime;
    E += dE;
  }

  // printing to LCD
  char buffer[1];
  buffer[0] = "1";
  //sprintf(buffer, "%S", "a string");
  //printf(buffer,"%s %f %s %f",
  //    vDisplayed,Json_enviar[vDisplayed],iDisplayed,Json_enviar[iDisplayed]);
  lcd.setCursor(0,0);
  lcd.print(buffer);

  lcd.setCursor(0,1);
  lcd.print("V2, I2, P2");


  delay(100);
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}