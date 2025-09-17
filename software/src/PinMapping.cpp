#include <Arduino.h>
#include "PinMapping.hpp"

void initPinModes() {
  // Datenleitungen
  pinMode(DB0, OUTPUT);
  pinMode(DB1, OUTPUT);
  pinMode(DB2, OUTPUT);
  pinMode(DB3, OUTPUT);
  pinMode(DB4, OUTPUT);
  pinMode(DB5, OUTPUT);
  pinMode(DB6, OUTPUT);
  pinMode(DB7, OUTPUT);
  pinMode(DB8, OUTPUT);
  pinMode(DB9, OUTPUT);
  pinMode(DB10, OUTPUT);
  pinMode(DB11, OUTPUT);

  // Adressleitungen
  pinMode(ADD0, OUTPUT);
  pinMode(ADD1, OUTPUT);

  // Steuerleitungen
  pinMode(RST, OUTPUT);
  pinMode(Load_Data, OUTPUT);
  pinMode(R_W, OUTPUT);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH); // Inaktiv setzen
  
}
