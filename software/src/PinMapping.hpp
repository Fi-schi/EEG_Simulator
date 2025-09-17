#ifndef PINMAPPING_HPP
#define PINMAPPING_HPP

// Steuerleitungen
#define RST         42 
#define Load_Data   41
#define CS          3

// Datenleitungen
#define DB0         40
#define DB1         39
#define DB2         38
#define DB3         15    //37
#define DB4         16    //36
#define DB5         17    //35
#define DB6         18   //48
#define DB7          8   //47
#define DB8         21
#define DB9         14
#define DB10        13
#define DB11        12


// Adressleitungen 
#define ADD0        9
#define ADD1        10


// Steuerleitungen (weitere)
#define R_W        11        

void initPinModes();
#endif // PINMAPPING_HPP

