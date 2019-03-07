#include <Arduino.h>
#include <LiquidCrystal.h>
#include <NewPing.h>

#define BTN_RIGHT  0
#define BTN_UP     1
#define BTN_DOWN   2
#define BTN_LEFT   3
#define BTN_SELECT 4
#define BTN_NONE   5

#define MODE_MANUAL 0
#define MODE_AUTO   1
#define MODE_NONE   -1

const char* modeNames[] = {
  "Manual",
  "Auto  "
};

int numberModes = sizeof(modeNames)/sizeof(char*);

#define PHASE_LOAD   0
#define PHASE_AIR    1
#define PHASE_WAIT   2
#define PHASE_UNLOAD 3
#define PHASE_NONE   -1

const char* phaseNames[] = {
  "Cargar estanque",
  "Airear         ",
  "Reposar        ",
  "Regar          "
};

void loadTankSetup();
void airSetup();
void waitSetup();
void unloadSetup();
void (*phasePointers[])() = {
  &loadTankSetup,
  &airSetup,
  &waitSetup,
  &unloadSetup
};

int numberPhases = sizeof(phaseNames)/sizeof(char*);

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define BLANK_LINE "                "

int readBtn();
double readDistance();
void updateScreen();
void turnOffRelays();

/******** Configuracion estatica **********/
#define PIN_LOAD_PUMP_RELAY    2
#define PIN_UNLOAD_PUMP_RELAY  3
#define PIN_AIR_PUMP_RELAY     11
#define PIN_WATER_LEVEL_TRIG   12
#define PIN_WATER_LEVEL_ECHO   13

#define WAIT_TIME_SECS     20  //10800  // 3 horas
#define AIR_TIME_SECS      20  //10800  // 3 horas
#define WATER_LEVEL_LOW    30.0D //200 //cm
#define WATER_LEVEL_HIGH   10.0D  //cm

/******** Configuracion dinamica **********/
int selectedMode;
int selectedPhase;

/******** Estado interno **********/
void (*phasePtr)();
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
NewPing sensor(PIN_WATER_LEVEL_TRIG, PIN_WATER_LEVEL_ECHO, 200);

unsigned long startTimeMillis;
char overflow;

int currentPhase;
char force;

/****** Funciones de Fases **********/
void displayStatusNumbers(double status, double reference) {
  lcd.setCursor(0, 1);
  lcd.print(BLANK_LINE);
  lcd.setCursor(0, 1);
  lcd.print(status);
  lcd.print("/");
  lcd.print(reference);
}

char checkManualSkip(int btn, void (*nextPhasePtr)()) {
  if(selectedMode == MODE_MANUAL && btn == BTN_RIGHT) {
    phasePtr = nextPhasePtr;
    lcd.setCursor(0, 1);
    lcd.print("Siguiente fase...");
    delay(1000);
    return 1;
  }
  return 0;
}

char checkForce(int btn) {
  if(selectedMode == MODE_MANUAL && btn == BTN_SELECT) {
    delay(2000);
    int second = readBtn();
    if(second == BTN_SELECT) {
      force = 1;
      return 1;
    }
  }
  return 0;
}

void notifyAndSkipPhase(void (*nextPhasePtr)()) {
  phasePtr = nextPhasePtr;
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("Siguiente fase...");
  delay(1000);
}

void loadTankLoop() {
  int btn = readBtn();

  if(checkManualSkip(btn, &airSetup)) return;
  checkForce(btn);

  double distanceCm = readDistance();
  double reference = WATER_LEVEL_HIGH;
  if(selectedMode == MODE_MANUAL) reference = 0;
  displayStatusNumbers(distanceCm, reference);

  if(distanceCm > 0 && distanceCm <= WATER_LEVEL_HIGH) {
    if(selectedMode == MODE_AUTO) {
      digitalWrite(PIN_LOAD_PUMP_RELAY, RELAY_OFF);
      notifyAndSkipPhase(&airSetup);
      return;
    }
    else if(selectedMode == MODE_MANUAL && force) {
      digitalWrite(PIN_LOAD_PUMP_RELAY, RELAY_ON);
    }
    else {
      digitalWrite(PIN_LOAD_PUMP_RELAY, RELAY_OFF);
    }
  }
  delay(500);
}

void loadTankSetup() {
  currentPhase = PHASE_LOAD;
  updateScreen();
  phasePtr = &loadTankLoop;
  force = 0;
  turnOffRelays();
  digitalWrite(PIN_LOAD_PUMP_RELAY, RELAY_ON);
  delay(500);
}

void airLoop() {
  int btn = readBtn();

  if(checkManualSkip(btn, &waitSetup)) return;
  checkForce(btn);

  int ellapsedMs = millis() - startTimeMillis;
  int ellapsedSecs = (int) (ellapsedMs / 1000);

  int reference = AIR_TIME_SECS;
  if(selectedMode == MODE_MANUAL) reference = 0;
  displayStatusNumbers(ellapsedSecs, reference);

  if(ellapsedSecs >= AIR_TIME_SECS) {
    if(selectedMode == MODE_AUTO) {
      digitalWrite(PIN_AIR_PUMP_RELAY, RELAY_OFF);
      phasePtr = &waitSetup;
      delay(1000);
      lcd.setCursor(0, 1);
      lcd.print("Siguiente fase...");
      delay(1000);
      return;
    }
    else if(selectedMode == MODE_MANUAL && force) {
      digitalWrite(PIN_AIR_PUMP_RELAY, RELAY_ON);
    }
    else {
      digitalWrite(PIN_AIR_PUMP_RELAY, RELAY_OFF);
    }
  }
  delay(500);
}

void airSetup() {
  currentPhase = PHASE_AIR;
  updateScreen();
  phasePtr = &airLoop;
  startTimeMillis = millis();
  force = 0;
  turnOffRelays();
  digitalWrite(PIN_AIR_PUMP_RELAY, RELAY_ON);
  delay(500);
}

void waitLoop() {
  int btn = readBtn();

  if(checkManualSkip(btn, &unloadSetup)) return;

  int ellapsedMs = millis() - startTimeMillis;
  int ellapsedSecs = (int) (ellapsedMs / 1000);
  int reference = WAIT_TIME_SECS;
  if(selectedMode == MODE_MANUAL) reference = 0;
  displayStatusNumbers(ellapsedSecs, reference);

  if(ellapsedSecs >= WAIT_TIME_SECS) {
    if(selectedMode == MODE_AUTO) {
      phasePtr = &unloadSetup;
      delay(1000);
      lcd.setCursor(0, 1);
      lcd.print("Siguiente fase...");
      delay(1000);
      return;
    }
  }
  delay(500);
}

void waitSetup() {
  currentPhase = PHASE_WAIT;
  turnOffRelays();
  updateScreen();
  phasePtr = &waitLoop;
  startTimeMillis = millis();
  force = 0;
  delay(500);
}

void unloadLoop() {
  int btn = readBtn();

  if(checkManualSkip(btn, &loadTankSetup)) return;
  checkForce(btn);

  double distanceCm = readDistance();
  displayStatusNumbers(distanceCm, WATER_LEVEL_LOW);
  if(distanceCm > 0 && distanceCm >= WATER_LEVEL_LOW) {
    if(selectedMode == MODE_AUTO) {
      digitalWrite(PIN_UNLOAD_PUMP_RELAY, RELAY_OFF);
      phasePtr = &loadTankSetup;
      delay(1000);
      lcd.setCursor(0, 1);
      lcd.print("Siguiente fase...");
      delay(1000);
      return;
    }
    else if(selectedMode == MODE_MANUAL && force) {
      digitalWrite(PIN_UNLOAD_PUMP_RELAY, RELAY_ON);
    }
    else {
      digitalWrite(PIN_UNLOAD_PUMP_RELAY, RELAY_OFF);
    }
  }
  delay(500);
}

void unloadSetup() {
  currentPhase = PHASE_UNLOAD;
  updateScreen();
  phasePtr = &unloadLoop;
  force = 0;
  turnOffRelays();
  digitalWrite(PIN_UNLOAD_PUMP_RELAY, RELAY_ON);
  delay(500);
}


/****** Funciones de configuracion inicial **********/
int menuPtr;
void selectPhase() {
  int btn = readBtn();
  if(btn == BTN_NONE) return;

  if(btn == BTN_UP || btn == BTN_RIGHT) menuPtr++;
  if(btn == BTN_DOWN || btn == BTN_LEFT) menuPtr--;

  if(menuPtr >= numberPhases) menuPtr = 0;
  if(menuPtr < 0) menuPtr = numberPhases -1;

  lcd.setCursor(0 ,1);
  lcd.print(phaseNames[menuPtr]);

  if(btn == BTN_UP || btn == BTN_RIGHT || btn == BTN_DOWN || btn == BTN_LEFT) {
    delay(300);
    return;
  }

  if(btn == BTN_SELECT) {
    selectedPhase = menuPtr;

    lcd.clear();
    lcd.setCursor(0 ,0);
    lcd.print("Seleccionado:");
    lcd.setCursor(0 ,1);
    lcd.print(phaseNames[selectedPhase]);
    delay(1000);

    phasePtr = phasePointers[selectedPhase];
    menuPtr = 0;
    lcd.clear();
  }
}

void selectMode() {
  int btn = readBtn();
  if(btn == BTN_NONE) return;

  if(btn == BTN_UP || btn == BTN_RIGHT) menuPtr++;
  if(btn == BTN_DOWN || btn == BTN_LEFT) menuPtr--;

  if(menuPtr >= numberModes) menuPtr = 0;
  if(menuPtr < 0) menuPtr = numberModes -1;

  lcd.setCursor(0 ,1);
  lcd.print(modeNames[menuPtr]);

  if(btn == BTN_UP || btn == BTN_RIGHT || btn == BTN_DOWN || btn == BTN_LEFT) {
    delay(300);
    return;
  }

  if(btn == BTN_SELECT) {
    selectedMode = menuPtr;

    lcd.clear();
    lcd.setCursor(0 ,0);
    lcd.print("Seleccionado:");
    lcd.setCursor(0 ,1);
    lcd.print(modeNames[selectedMode]);
    delay(1000);

    phasePtr = &selectPhase;
    menuPtr = 0;
    lcd.clear();
    lcd.setCursor(0 ,0);
    lcd.print("Elija fase:");
    lcd.setCursor(0 ,1);
    lcd.print(phaseNames[menuPtr]);
  }

}

void configSetup() {
  selectedMode = MODE_NONE;
  selectedPhase = PHASE_NONE;
  menuPtr = 0;

  lcd.clear();
  lcd.print("Elija modo:");
  lcd.setCursor(0, 1);
  lcd.print(modeNames[menuPtr]);
  phasePtr = &selectMode;
}

void turnOffRelays() {
  digitalWrite(PIN_LOAD_PUMP_RELAY, RELAY_OFF);
  digitalWrite(PIN_UNLOAD_PUMP_RELAY, RELAY_OFF);
  digitalWrite(PIN_AIR_PUMP_RELAY, RELAY_OFF);
}

void setup() {
  pinMode(PIN_LOAD_PUMP_RELAY, OUTPUT);
  pinMode(PIN_UNLOAD_PUMP_RELAY, OUTPUT);
  pinMode(PIN_AIR_PUMP_RELAY, OUTPUT);
  turnOffRelays();

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Iniciando...");

  phasePtr = &configSetup;
  selectedMode = MODE_NONE;
  selectedPhase = PHASE_NONE;
}

void loop() {
  (*phasePtr)();
}

int readBtn() {
    int adc_key_in = analogRead(0);
    if (adc_key_in > 1000) return BTN_NONE;
    if (adc_key_in < 50)   return BTN_RIGHT;
    if (adc_key_in < 195)  return BTN_UP;
    if (adc_key_in < 380)  return BTN_DOWN;
    if (adc_key_in < 555)  return BTN_LEFT;
    if (adc_key_in < 790)  return BTN_SELECT;
    return BTN_NONE;
}

void updateScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(modeNames[selectedMode]);
  lcd.print("-");
  lcd.print(phaseNames[currentPhase]);
}

double readDistance() {
  double uSecs = (double) sensor.ping_median();
  return uSecs / US_ROUNDTRIP_CM;
}
