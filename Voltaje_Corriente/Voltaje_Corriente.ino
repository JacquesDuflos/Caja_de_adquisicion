#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>



float V1; // Value of the 1st voltmeter (0-5 v) // EL valor detectado por le voltimetro 1
float V2; // Value of the 2nd voltmeter (0-30 v) // EL valor detectado por el voltimetro 2
float I1; // El valor detectado por el voltimetro 1
float I1offset = 0; // obtained by calibrating
float I2; // El valor detectado por el voltimitro 2
float I2offset = 0; // obtained by calibrating
float P1; // El poder, calculado a partir del v y i selecionado
float P2;
float E1; // la energia, calculada integrando el poder
float E2; // la energia, calculada integrando el poder
unsigned long lastTime = 0; // to get the calculate the delta between 2 loops

int M1State; // estado del boton de medision 1 (como Mesure 1 state)
int M2State; // estado del boton de medision 2 (como Mesure 2 state)

int LastM1State = LOW; // estado presednete del boton de measure 1
int LastM2State = LOW; // estado presedente del boton de measure 2

const int Vmetro1 = A0; // pin del voltimetro 1
const int Vmetro2 = A1; // pin del voltimetro 2
const int Imetro1 = A2; // pin del amperimetro 1
const int Imetro2 = A3; // pin del amperimetro 2
const int Measure1 = 2; // pin del boton de medision 1
const int Measure2 = 3; // pin del boton de medicion 2
bool forceRefresh = false;

unsigned long lastDebounceTime1 = 0;  // the last time the output pin was toggled
unsigned long lastDebounceTime2 = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50; 
bool isSampling1 = false; // si se esta calibrando el I1
bool isSampling2 = false; // si se esta calibrando el I2
int nSample1 = 0; // the iteration of sample for I1
int nSample2 = 0; // the itaration of sample for I2
float sampleI1 = 0; // the sum of samples for I1
float sampleI2 = 0; // the sum of samples for I2
const int sampleSize = 50; // the total number of sample

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
  calibrate(600);
  Serial.println("setup compleat");
}

void loop() {
  // Getting the infos
  // the volts are sensed directly by analog input, so 0 to 1023 val are mapped to 0-5v
  V1 = mapfloat (analogRead(A0), 0, 1023, 0, 30);
  //delay(5);
  V2 = mapfloat (analogRead(A1), 0, 1023, 0, 10);
  //delay(5);

  // The intensity come from a ASC712 B05 sensor with a sensitivity of 185 mV / A
  // So I map from the 0-1023 to 0-5 then from 2.5 - 2.685 to 0-1A
  float I1_mean = promedio(50, Imetro1);
/*  Serial.print(I1_mean);
  Serial.print(",");
  Serial.print(analogRead(Imetro1));*/
  int I1_analog = map (I1_mean, 0, 1023, 0, 5000);
  I1_analog = map (I1_analog, 2500, 2685, 0, 1000);
  float I1_unfilter = float(I1_analog)/1000.0;
  float I2_mean = promedio(50, Imetro2);
/*  Serial.print(",");
  Serial.print(I2_mean);
  Serial.print(",");
  Serial.print(analogRead(Imetro2));
  Serial.println();*/
  int I2_analog =  map (I2_mean, 0, 1023, 0, 5000);
  I2_analog = map (I2_analog, 2500, 2685, 0, 1000);
  float I2_unfilter = float(I2_analog)/1000.0;
  
  I1_unfilter -= I1offset;
  I1 = I1_unfilter;

  I2_unfilter -= I2offset;
  I2 = I2_unfilter;

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
      if (M1State == HIGH){
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
      if (M2State == HIGH){
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


  // calculate P and create strings for lcd display
  char buf_V1[10];
  char buf_V2[10];
  floatToStr(V1, 4, 1, buf_V1);
  floatToStr(V2, 4, 1, buf_V2);

  char buf_I1[10];
  char buf_I2[10];
  char buf_P1[10];
  char buf_P2[10];
  if (isSampling1){
    strcpy(buf_I1, "calib");
    strcpy(buf_P1, "calib");
  }
  else{
    P1 = V1 * I1;
    floatToStr(I1, 4, 2, buf_I1);
    floatToStr(P1, 4, 1, buf_P1);
  }
  if (isSampling2){
    strcpy(buf_I2, "calib");
    strcpy(buf_P2, "calib");
  }
  else{
    P2 = V2 * I2;
    floatToStr(I2, 4, 2, buf_I2);
    floatToStr(P2, 4, 1, buf_P2);
  }
  // calculate E
  float dE;
  dE = I1 * V1;
  dE *= (millis() - lastTime)/1000.0;
  E1 += dE;

  dE = I2 * V2;
  dE *= (millis() - lastTime)/1000.0;
  E2 += dE;

  lastTime = millis();

  char buf_E1[20] = "";
  char buf_E2[20] = "";

  // Conversion énergie en notation scientifique : 3 chiffres de mantisse + exposant
  // Exemple : 3.48e+08
  int exposant = 0;
  float abs_e = fabs(E1);

  while (abs_e >= 10.0) {
    abs_e /= 10.0;
    exposant++;
  }

  if (E1 < 0) {
    abs_e = -abs_e;
  }
  floatToStr(abs_e, 4, 1, buf_E1);
  snprintf(buf_E1, sizeof(buf_E1), "%se%d", buf_E1, exposant);

  exposant = 0;
  abs_e = fabs(E2);

  while (abs_e >= 10.0) {
    abs_e /= 10.0;
    exposant++;
  }

  if (E2 < 0) {
    abs_e = -abs_e;
  }
  floatToStr(abs_e, 4, 1, buf_E2);
  snprintf(buf_E2, sizeof(buf_E2), "%se%d", buf_E2, exposant);

  if ((millis() - lastRefresh)/1000.0 > refreshPeriode or forceRefresh){
    lastRefresh = millis();
    nPeriods++;
    if (nPeriods > clearEveryNPeriods){
      lcd.clear();
      nPeriods = 0;
    }

    //send_json();
    //send_4_floats();
    // printing to LCD
    //lcd.clear();
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


// Calibrate the ampereters, displaying an animation on the screen
void calibrate(int nSamples){
  int I1_analog;
  float I1_unfilter;
  int I2_analog;
  float I2_unfilter;
  int iSample = 0;
  int gearIndex = 0;
  int lightIndex = 5;
  int loop_size = nSamples / 20;
  sampleI1 = 0;
  sampleI2 = 0;
  if (loop_size < 2){
    loop_size = 2;
  }
  for (int rep = 0; rep < 20; rep++) {   // répéter 20 fois
    for (int s = 0; s < loop_size; s++) {
      // The intensity come from a ASC712 B05 sensor with a sensitivity of 185 mV / A
      // So I map from the 0-1023 to 0-5 then from 2.5 - 2.685 to 0-1A
      I1_analog = map (analogRead(A2), 0, 1023, 0, 5000);
      I1_analog = map (I1_analog, 2500, 2685, 0, 1000);
      I1_unfilter = float(I1_analog)/1000.0;
      sampleI1 += I1_unfilter;
      I2_analog =  map (analogRead(A3), 0, 1023, 0, 5000);
      I2_analog = map (I2_analog, 2500, 2685, 0, 1000);
      I2_unfilter = float(I2_analog)/1000.0;
      sampleI2 += I2_unfilter;
    }
    lcd.setCursor(5, 2);
    lcd.print("calibrando");
    lcd.setCursor(2, 0);
    lcd.write(lightIndex);
    lcd.setCursor(2, 1);
    lcd.write(gearIndex);
    lcd.setCursor(2, 2);
    lcd.write(lightIndex);
    lcd.setCursor(17, 0);
    lcd.write(lightIndex);
    lcd.setCursor(17, 1);
    lcd.write(gearIndex);
    lcd.setCursor(17, 2);
    lcd.write(lightIndex);
    lcd.setCursor(rep, 3);               // positionner colonne = rep, ligne = 3 (4ème ligne)
    lcd.write((byte)0xFF);               // écrire un carré noir
    if (lightIndex == 6){
      lightIndex = 5;
    }
    else{
      lightIndex = 6;
    }
    gearIndex++;
    if (gearIndex > 4){
      gearIndex = 0;
      lcd.clear();
    }
    delay(80);                          // pause pour voir le remplissage
  }
  I1offset = sampleI1 / 20 / loop_size;
  I2offset = sampleI1 / 20 / loop_size;
  lcd.clear();
  lcd.setCursor(7,1);
  lcd.print("LUDIX");
  lcd.setCursor(7,1);
  lcd.print("listo !");
  delay(500);
}

// Returns the mean of the next "nSamples" values read on the "pin" analog pin
float promedio(int nSamples, int pin){
  int q = 0;
  for(int i = 0; i < nSamples; i++){
    q += analogRead(pin);
  }
  int mean = (float)q/nSamples;
  return mean;
}

// Displays a welcoming screen 
void splashScreen(float t){
/*-------------------¬
|  z              z  |
|  o    LUDIX     o  |
|  z  arrancando  z  |
|                    |
-------------------¬*/
  lcd.clear();
  lcd.setCursor(7, 1);
  lcd.print("LUDIX");
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
}


// Envoie 4 floats sous forme de 16 octets dans l'ordre V1, V2, I1, I2
void send_4_floats()
{
  // Mettre les valeurs dans un tableau
  float valeurs[4] = {V1, V2, I1, I2};

  // Envoyer les 16 octets directement
  Serial.write((uint8_t*)valeurs, sizeof(valeurs));
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}