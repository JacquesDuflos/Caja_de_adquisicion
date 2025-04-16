#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>

template <int order> // order is 1 or 2
class LowPass
{
  private:
    float a[order];
    float b[order+1];
    float omega0;
    float dt;
    bool adapt;
    float tn1 = 0;
    float x[order+1]; // Raw values
    float y[order+1]; // Filtered values

  public:  
    LowPass(float f0, float fs, bool adaptive){
      // f0: cutoff frequency (Hz)
      // fs: sample frequency (Hz)
      // adaptive: boolean flag, if set to 1, the code will automatically set
      // the sample frequency based on the time history.
      
      omega0 = 6.28318530718*f0;
      dt = 1.0/fs;
      adapt = adaptive;
      tn1 = -dt;
      for(int k = 0; k < order+1; k++){
        x[k] = 0;
        y[k] = 0;        
      }
      setCoef();
    }

    void setCoef(){
      if(adapt){
        float t = micros()/1.0e6;
        dt = t - tn1;
        tn1 = t;
      }
      
      float alpha = omega0*dt;
      if(order==1){
        a[0] = -(alpha - 2.0)/(alpha+2.0);
        b[0] = alpha/(alpha+2.0);
        b[1] = alpha/(alpha+2.0);        
      }
      if(order==2){
        float alphaSq = alpha*alpha;
        float beta[] = {1, sqrt(2), 1};
        float D = alphaSq*beta[0] + 2*alpha*beta[1] + 4*beta[2];
        b[0] = alphaSq/D;
        b[1] = 2*b[0];
        b[2] = b[0];
        a[0] = -(2*alphaSq*beta[0] - 8*beta[2])/D;
        a[1] = -(beta[0]*alphaSq - 2*beta[1]*alpha + 4*beta[2])/D;      
      }
    }

    float filt(float xn){
      // Provide me with the current raw value: x
      // I will give you the current filtered value: y
      if(adapt){
        setCoef(); // Update coefficients if necessary      
      }
      y[0] = 0;
      x[0] = xn;
      // Compute the filtered values
      for(int k = 0; k < order; k++){
        y[0] += a[k]*y[k+1] + b[k]*x[k];
      }
      y[0] += b[order]*x[order];

      // Save the historical values
      for(int k = order; k > 0; k--){
        y[k] = y[k-1];
        x[k] = x[k-1];
      }
  
      // Return the filtered value    
      return y[0];
    }
};

// Filter instance
LowPass<2> lp1(1,1e3,true);
LowPass<2> lp2(1,1e3,true);


float V1;
float V2;
float I1;
float I1offset; // obtained by calibrating
float I2;
float I2offset; // obtained by calibrating
float P; // El poder, calculado a partir del v y i selecionado
float E; // la energia, calculada integrando el poder
unsigned long lastTime = 0; // to get the calculate the delta between 2 loops

int SVState; // estado del boton de cambio de voltimetro
int SIState; // estado del boton de cambio de amperimetro
int RBState; // estado del boton de resetear
int LastSVState = LOW; // estado presednete del boton de voltimetro
int LastSIState = LOW; // estado presedente del boton de amperimetro
int LastRBState = LOW; // estado presedente del boton de resetear
char * vDisplayed = "V1"; // la tension para mostrar en lcd
char * iDisplayed = "I1"; // la intensidad para mostrar en lcd
char * vCalc = ""; // la tension para calcular la energia
char * iCalc = ""; // la intensidad para calcular la energia

const int Vmetro1 = A0;
const int Vmetro2 = A1;
const int Imetro1 = A2;
const int Imetro2 = A3;
const int SwitchV = 2;
const int SwitchI = 3;
const int ResetButton = 4;
bool forceRefresh = false;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50; 
unsigned long iPushedTime = 0; // to detect if the i button was manained 
bool isSampling = false;
int nSample = 0; // the iteration of samle
float sampleI1 = 0; // the sum of samples for I1
float sampleI2 = 0; // the sum of samples for I2
const int sampleSize = 50; // the total number of sample

String line1;
String line2;
String vegal;
String iegal;

LiquidCrystal_I2C lcd(0x27,  16, 2);
unsigned long lastRefresh = 0; // Last time the screen was updated
const float refreshPeriode = 1; // In seconds, how often the screen is refreshed.

void setup() {
  // Los pins de botones
  pinMode(SwitchV, INPUT);
  pinMode(SwitchI, INPUT);
  pinMode(ResetButton, INPUT);
  // Start serial comunication
  Serial.begin(9600);
  // initialize lcd screen
  lcd.init();
  // turn on the backlight, or not
  // lcd.backlight();
  Serial.println("setup compleat");
}

void loop() {
  // Getting the infos
  // the volts are sensed directly by analog input, so 0 to 1023 val are mapped to 0-5v
  V1 = mapfloat (analogRead(A0), 0, 1023, 0, 5);
  //delay(5);
  V2 = mapfloat (analogRead(A1), 0, 1023, 0, 5);
  //delay(5);

  // The intensity come from a ASC712 B05 sensor with a sensitivity of 185 mV / A
  // So I map from the 0-1023 to 0-5 then from 2.5 - 2.685 to 0-1A
  int I1_analog = map (analogRead(A2), 0, 1023, 0, 5000);
  I1_analog = map (I1_analog, 2500, 2685, 0, 1000);
  float I1_unfilter = float(I1_analog)/1000.0;
  int I2_analog =  map (analogRead(A3), 0, 1023, 0, 5000);
  I2_analog = map (I2_analog, 2500, 2685, 0, 1000);
  float I2_unfilter = float(I2_analog)/1000.0;
  if (isSampling){
    sampleI1 += I1_unfilter;
    sampleI2 += I2_unfilter;
    nSample ++;
    if (nSample >= sampleSize){
      I1offset = sampleI1 / sampleSize;
      I2offset = sampleI2 / sampleSize;
      isSampling = false;
    }
  }
  else{
    
    I1_unfilter -= I1offset;
    I1 = lp1.filt(I1_unfilter);
    //delay(5);
    
    I2_unfilter -= I2offset;
    I2 = lp2.filt(I2_unfilter);
  }

  // Create the JSON document
  StaticJsonDocument<200> Json_enviar;
 
  Json_enviar["ProductName"] = "ModuloDidactico";
  Json_enviar["V1"] = V1;
  Json_enviar["V2"] = V2;
  Json_enviar["I1"] = I1;
  Json_enviar["I2"] = I2;

/*
  Json_enviar["I_filtre"]= I1;
  Json_enviar["I_non_filtre"] = I1_unfilter;
*/
  serializeJson(Json_enviar, Serial);
  Serial.println();

  // Read the buttons
  forceRefresh = false;
  int reading = digitalRead(SwitchV);
  if (reading != LastSVState){
    lastDebounceTime = millis();
    //Serial.println("Bounce");
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    //Serial.println("long touch");
    if (reading != SVState){
      //Serial.println("button flipped");
      SVState = reading;
      if (SVState == HIGH){
        //Serial.println("BOUTON V TOUCHE");
        forceRefresh = true;
        if (vDisplayed == "V1"){
          vDisplayed = "V2";
        }
        else{
          vDisplayed = "V1";
        }
      }
    }
  }
  LastSVState = reading;

  reading = digitalRead(SwitchI);
  if (reading != LastSIState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (reading != SIState){
      // When a change of state is confirmed
      SIState = reading;
      if (SIState == HIGH){
        iPushedTime = millis();
      }
      else{
        if (not isSampling){
          forceRefresh = true;
          if (iDisplayed == "I1"){
            iDisplayed = "I2";
          }
          else{
            iDisplayed = "I1";
          }
        }
      }
    }
    else{
      // when the state has not changed
      if ((SIState == HIGH) and ((millis() - iPushedTime) > 1500)){
        Serial.println("appuy prologe detecte");
        if (not isSampling){
          Serial.println("demarrage sampling");
          isSampling = true;
          nSample = 0;
          sampleI1 = 0;
          sampleI2 = 0;
        }
      }
    }
  }
  LastSIState = reading;

  reading = digitalRead(ResetButton);
  if (reading != LastRBState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (reading != RBState){
      RBState = reading;
      if (RBState == HIGH){
        forceRefresh = true;
        vCalc = vDisplayed;
        iCalc = iDisplayed;
        E = 0;
      }
    }
  }
  LastRBState = reading;

  // calculate P and create strings
  char buf_V[10];
  if (vDisplayed == "V1"){
    P = V1;
    dtostrf(V1, 4, 1, buf_V);
  } else {
    P = V2;
    dtostrf(V2, 4, 1, buf_V);
  }
  char buf_I[10];
  char buf_P[10];
  if (isSampling){
    strcpy(buf_I, "calib");
    strcpy(buf_P, "calib");
  }
  else{
    if (iDisplayed == "I1"){
      P *= I1;
      dtostrf(I1, 4, 1, buf_I);
    } else {
      P*= I2;
      dtostrf(I2, 4, 1, buf_I);
    }
    dtostrf(P, 4, 1, buf_P);
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
    dE *= (millis() - lastTime)/1000.0;
    E += dE;
    lastTime = millis();
  }

  char buf_E[20] = "";
  char * nameE = "";
  if (iCalc == iDisplayed and vCalc == vDisplayed){
    // Conversion énergie en notation scientifique : 3 chiffres de mantisse + exposant
    // Exemple : 3.48e+08
    int exposant = 0;
    float abs_e = fabs(E);

  while (abs_e >= 10.0) {
    abs_e /= 10.0;
    exposant++;
  }

    if (E < 0) {
      abs_e = -abs_e;
    }
    dtostrf(abs_e, 4, 1, buf_E);
    snprintf(buf_E, sizeof(buf_E), "%se%d", buf_E, exposant);
    nameE = "E";
  }

  if ((millis() - lastRefresh)/1000.0 > refreshPeriode or forceRefresh){
    lastRefresh = millis();
    // printing to LCD
    lcd.clear();
    lcd.setCursor(0,0);
    // Buffers pour conversion
    char ligne[33]; // 32 caractères + \0

    // Construction de la ligne 1 complète
    snprintf(ligne, sizeof(ligne), "%s %s %s %s", vDisplayed, buf_V, iDisplayed, buf_I);

    // Affichage sur le LCD
    lcd.setCursor(0, 0);
    lcd.print(ligne);

    // Construction de la ligne 2 complète
    snprintf(ligne, sizeof(ligne), "%s %s %s %s", "P", buf_P, nameE, buf_E);

    lcd.setCursor(0,1);
    lcd.print(ligne);
  }

  delay(10);
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}