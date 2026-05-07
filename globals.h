#ifndef GLOBALS_H
#define GLOBALS_H

#include <QMap>
#include <QVector>

extern QMap<int,int> savedClickState;
extern QVector<int> savedSelected;

extern double savedAmplitude;
extern double savedRampUp;
extern double savedCoast;
extern double savedRampDown;

#endif
