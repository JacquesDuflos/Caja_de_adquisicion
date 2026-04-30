#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina1;
Adafruit_INA219 ina2(0x41);

float V1; // Value of the 1st voltmeter (0-25 v) // EL valor detectado por le voltimetro 1
float V2; // Value of the 2nd voltmeter (0-5 v) // EL valor detectado por el voltimetro 2
float I1; // El valor detectado por el voltimetro 1
float I2; // El valor detectado por el voltimitro 2
float P1; // El poder, calculado a partir del v y i selecionado
float P2;
float E1; // la energia, calculada integrando el poder
float E2; // la energia, calculada integrando el poder
unsigned long lastTime = 0; // to get the calculate the delta between 2 loops

int M1State; // estado del boton de medision 1 (como Mesure 1 state)
int M2State; // estado del boton de medision 2 (como Mesure 2 state)

int LastM1State = LOW; // estado presednete del boton de measure 1
int LastM2State = LOW; // estado presedente del boton de measure 2

const float V1max = 26.0; // el valor maximo medible a partir de cual se indicara FR(fuera de rango)
const float V2max = 26.0; // el valor maximo medible a partir de cual se indicara FR(fuera de rango)
const int Measure1 = 2; // pin del boton de medision 1
const int Measure2 = 3; // pin del boton de medicion 2
bool forceRefresh = false;

unsigned long lastDebounceTime1 = 0;  // the last time the output pin was toggled
unsigned long lastDebounceTime2 = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50; 
const float iThreashold = 0.002;

byte gear1[] = {
  B00000,
  B00100,
  B11111,
  B01010,
  B11111,
  B00100,
  B00000,
  B00000
};
byte gear2[] = {
  B00000,
  B10010,
  B01110,
  B11011,
  B01110,
  B01001,
  B00000,
  B00000
};
byte gear3[] = {
  B00000,
  B01001,
  B11110,
  B01010,
  B01111,
  B10010,
  B00000,
  B00000
};
byte gear4[] = {
  B00000,
  B10100,
  B01111,
  B01010,
  B11110,
  B00101,
  B00000,
  B00000
};
byte gear5[] = {
  B00000,
  B01001,
  B01110,
  B11011,
  B01110,
  B10010,
  B00000,
  B00000
};
byte lightning1[] = {
  B00100,
  B00100,
  B01100,
  B11111,
  B00110,
  B00100,
  B00100,
  B00000
};
byte lightning2[] = {
  B00000,
  B00100,
  B00100,
  B01100,
  B11111,
  B00110,
  B00100,
  B00100
};

LiquidCrystal_I2C lcd(0x27,  20, 4);
unsigned long lastRefresh = 0; // Last time the screen was updated
const float refreshPeriode = 0.3; // In seconds, how often the screen is refreshed.
const uint8_t header = 0xAA;
int clearEveryNPeriods = 10; // the screen will clear every n refresh
int nPeriods = 0; //how many refresh til the last clear
int beginRefreshPeriod = 10; // In minuts, how often the lcd.begin methode is called
unsigned long lastBegin = 0; // las time in milis that has been called lcd.begin methode

void setup() {
  // Los pins de botones
  pinMode(Measure1, INPUT_PULLUP);
  pinMode(Measure2, INPUT_PULLUP);
  // Start serial comunication
  Serial.begin(9600);
  // initialize lcd screen
  lcd.init();
  lcd.begin(20, 4);
  // turn on the backlight, or not
  lcd.backlight();
  lcd.createChar(0, gear1);
  lcd.createChar(1, gear2);
  lcd.createChar(2, gear3);
  lcd.createChar(3, gear4);
  lcd.createChar(4, gear5);
  lcd.createChar(5, lightning1);
  lcd.createChar(6, lightning2);
  splashScreen(2);
  if (! ina1.begin()) {
    Serial.println("Failed to find INA219 1 chip");
    while (1) { delay(10); }
  }
  if (! ina2.begin()) {
    Serial.println("Failed to find INA219 2 chip");
    while (1) { delay(10); }
  }
  Serial.println("setup compleat");
}

void loop() {
  // Getting the infos
  float shuntvoltage1 = 0;
  float busvoltage1 = 0;
  float current_mA1 = 0;
  float loadvoltage1 = 0;
  float power_mW1 = 0;

  shuntvoltage1 = ina1.getShuntVoltage_mV();
  busvoltage1 = ina1.getBusVoltage_V();
  current_mA1 = ina1.getCurrent_mA();
  power_mW1 = ina1.getPower_mW();
  loadvoltage1 = busvoltage1 + (shuntvoltage1 / 1000);

  float shuntvoltage2 = 0;
  float busvoltage2 = 0;
  float current_mA2 = 0;
  float loadvoltage2 = 0;
  float power_mW2 = 0;

  shuntvoltage2 = ina2.getShuntVoltage_mV();
  busvoltage2 = ina2.getBusVoltage_V();
  current_mA2 = ina2.getCurrent_mA();
  power_mW2 = ina2.getPower_mW();
  loadvoltage2 = busvoltage2 + (shuntvoltage2 / 1000);

  
  V1 = busvoltage1;
  V2 = busvoltage2;

  // The intensity come from a INA219, so it is in mA, I convert to A
  I1 = current_mA1 / 1000.0;
  I2 = current_mA2 / 1000.0;

  if(I1 < iThreashold)
  {
    I1 = 0.0;
  }
  if(I2 < iThreashold)
  {
    I2 = 0.0;
  }

  // Read the buttons
  forceRefresh = false;
  int reading = digitalRead(Measure1);
  if (reading != LastM1State){
    lastDebounceTime1 = millis();
    //Serial.println("Bounce");
  }
  if ((millis() - lastDebounceTime1) > debounceDelay){
    //Serial.println("long touch");
    if (reading != M1State){
      //Serial.println("button flipped");
      M1State = reading;
      if (M1State == LOW){
        //
        //Serial.println("BOUTON M1 TOUCHE");
        //
        E1 = 0;
        lcd.setCursor(10, 1);
        lcd.print("resetando ");
        delay(500);
        forceRefresh = true;
      }
    }
  }
  LastM1State = reading;

  reading = digitalRead(Measure2);
  if (reading != LastM2State){
    lastDebounceTime2 = millis();
    //Serial.println("Bounce");
  }
  if ((millis() - lastDebounceTime2) > debounceDelay){
    //Serial.println("long touch");
    if (reading != M2State){
      //Serial.println("button flipped");
      M2State = reading;
      if (M2State == LOW){
        //
        //Serial.println("BOUTON M2 TOUCHE");
        //
        E2 = 0;
        lcd.setCursor(10, 3);
        lcd.print("resetando ");
        delay(500);
        forceRefresh = true;
      }
    }
  }
  LastM2State = reading;

  // Displays V values
  char buf_V1[10];
  char buf_V2[10];
  if (V1 == V1max)
  {
    strcpy(buf_V1, "  F/R");
  }
  else
  {
    floatToStr(V1, 4, 1, buf_V1);
  }
  if (V2 == V2max)
  {
    strcpy(buf_V2, "  F/R");
  }
  else
  {
    floatToStr(V2, 4, 1, buf_V2);
  }

  // Get P and create strings for lcd display
  char buf_I1[10];
  char buf_I2[10];
  char buf_P1[10];
  char buf_P2[10];

  // Power from INA219 is in mW, I convert to W
  P1 = power_mW1 / 1000.0;
  floatToStr(I1, 4, 3, buf_I1);
  floatToStr(P1, 4, 2, buf_P1);

  P2 = power_mW2 / 1000.0;
  floatToStr(I2, 4, 3, buf_I2);
  floatToStr(P2, 4, 2, buf_P2);

  // calculate E
  float dE;
  dE = P1 * (millis() - lastTime)/1000.0;
  E1 += dE;

  dE = P2 * (millis() - lastTime)/1000.0;
  E2 += dE;

  lastTime = millis();

  char buf_E1[20] = "";
  char buf_E2[20] = "";

  // Conversion énergie en notation scientifique : 3 chiffres de mantisse + exposant
  // Exemple : 3.48e+08
  float abs_e = fabs(E1);
  int exposant = 0;
  if (abs_e > 100000){

    while (abs_e >= 10.0) {
      abs_e /= 10.0;
      exposant++;
    }

    if (E1 < 0) {
      abs_e = -abs_e;
    }
    floatToStr(abs_e, 4, 1, buf_E1);
    snprintf(buf_E1, sizeof(buf_E1), "%se%d", buf_E1, exposant);
  }
  else{
    floatToStr(E1, 4, 2, buf_E1);
  }

  abs_e = fabs(E2);

  exposant = 0;
  if (abs_e > 100000){
    while (abs_e >= 10.0) {
      abs_e /= 10.0;
      exposant++;
    }

    if (E2 < 0) {
      abs_e = -abs_e;
    }
    floatToStr(abs_e, 4, 1, buf_E2);
    snprintf(buf_E2, sizeof(buf_E2), "%se%d", buf_E2, exposant);
  }
  else{
    floatToStr(E2, 4, 2, buf_E2);
  }

  if ((millis() - lastRefresh)/1000.0 > refreshPeriode or forceRefresh){
    lastRefresh = millis();
    nPeriods++;
    if (nPeriods > clearEveryNPeriods){
      //lcd.clear();
      nPeriods = 0;
    }

    //send_json();
    send_4_floats();
    // printing to LCD
    
    // Buffers pour conversion
    char ligne[21]; // 32 caractères + \0

    // Construction de la ligne 1 complète
    snprintf(ligne, sizeof(ligne), "%s %s   %s %s", "V1", buf_V1, "I1", buf_I1);
    int len = strlen(ligne);
    for (int i = len; i < 20; i++) {
      ligne[i] = ' ';
    }
    // Ajouter le caractère de fin de chaîne
    ligne[20] = '\0';

    // Affichage sur le LCD
    lcd.setCursor(0, 0);
    lcd.print(ligne);

    // Construction de la ligne 2 complète
    snprintf(ligne, sizeof(ligne), "%s %s   %s %s", "P1", buf_P1, "E1", buf_E1);
    len = strlen(ligne);
    for (int i = len; i < 20; i++) {
      ligne[i] = ' ';
    }
    // Ajouter le caractère de fin de chaîne
    ligne[20] = '\0';

    lcd.setCursor(0, 1);
    lcd.print(ligne);

    // Construction de la ligne 3 complète
    snprintf(ligne, sizeof(ligne), "%s %s   %s %s", "V2", buf_V2, "I2", buf_I2);
    len = strlen(ligne);
    for (int i = len; i < 20; i++) {
      ligne[i] = ' ';
    }
    // Ajouter le caractère de fin de chaîne
    ligne[20] = '\0';

    // Affichage sur le LCD
    lcd.setCursor(0, 2);
    lcd.print(ligne);

    // Construction de la ligne 4 complète
    snprintf(ligne, sizeof(ligne), "%s %s   %s %s", "P2", buf_P2, "E2", buf_E2);
    len = strlen(ligne);
    for (int i = len; i < 20; i++) {
      ligne[i] = ' ';
    }
    // Ajouter le caractère de fin de chaîne
    ligne[20] = '\0';

    lcd.setCursor(0, 3);
    lcd.print(ligne);
  }
  /*
  if (millis() - lastBegin > beginRefreshPeriod * 1000 * 60){
    lastBegin = millis();
    lcd.begin(20, 4);
  }
  */
  delay(10);
}


// Returns the mean of the next "nSamples" values read on the "pin" analog pin
float promedio(int nSamples, int pin){
  int q = 0;
  for(int i = 0; i < nSamples; i++){
    q += analogRead(pin);
    delay(10);
  }
  int mean = (float)q/nSamples;
  return mean;
}

// Displays a welcoming screen 
/*-------------------¬
|  z              z  |
|  o    LUDICX    o  |
|  z  arrancando  z  |
|                    |
-------------------¬*/
void splashScreen(float t){

  lcd.clear();
  lcd.setCursor(7, 1);
  lcd.print("LUDICX");
  lcd.setCursor(5, 2);
  lcd.print("arrancando");
  lcd.setCursor(2, 0);
  lcd.write(5);
  lcd.setCursor(2, 1);
  lcd.write(0);
  lcd.setCursor(2, 2);
  lcd.write(5);
  lcd.setCursor(17, 0);
  lcd.write(5);
  lcd.setCursor(17, 1);
  lcd.write(0);
  lcd.setCursor(17, 2);
  lcd.write(5);
  delay(1000 * t);
}

// Cree une chaine de carractere sans signe negatif si proche de zero
void floatToStr(float val, int width, int prec, char *buf) {
  dtostrf(val, width, prec, buf);
  if (strcmp(buf, "-0.0") == 0) {
    buf[0] = ' ';  // Remplacer le signe par un espace
  }
  // Si le nombre est positif, decaler la chaine et ajouter un espace
  if (val >= 0) {
    int len = strlen(buf);
    memmove(buf + 1, buf, len + 1);  // +1 pour copier aussi le '\0'
    buf[0] = ' ';
  }
}


// Envoie 4 floats sous forme de 16 octets dans l'ordre V1, V2, I1, I2
void send_4_floats()
{
  // Mettre les valeurs dans un tableau
  float valeurs[4] = {V1, V2, I1, I2};

  // Envoyer les 16 octets directement
  Serial.write(header);
  Serial.write((uint8_t*)valeurs, sizeof(valeurs));
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}