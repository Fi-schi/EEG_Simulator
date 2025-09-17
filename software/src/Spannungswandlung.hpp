#ifndef SPANNUNGSWANDLUNG_H
#define SPANNUNGSWANDLUNG_H
#include <Arduino.h>
#include <vector>
#include <map>
#include <cmath>


void ausgabe(char Channel, uint16_t Data);

void startAbspielTask();

extern int ausgabeFrequenzHz;

#endif // SPANNUNGSWANDLUNG_H