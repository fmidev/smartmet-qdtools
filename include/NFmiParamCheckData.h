
#ifndef NFMIPARAMCHECKDATA_H
#define NFMIPARAMCHECKDATA_H

#include "NFmiGlobals.h"
#include "NFmiEnumConverter.h"
#include <boost/lexical_cast.hpp>

static NFmiEnumConverter converter;

#include <vector>

typedef struct NFmiParamCheckData__
{
  NFmiParamCheckData__(void)
      : itsParamId(0),
        itsParamMissingDataMaxProcent(),
        itsParamStraightDataMaxProcent(),
        itsParamLowerLimits(),
        itsParamUpperLimits(),
        itsCheckedParamMissingDataMaxProcent(kFloatMissing),
        itsCheckedParamStraightDataMaxProcent(kFloatMissing),
        itsCheckedParamOutOfLimitWarning(false),
        itsCheckedParamOutOfLimitError(false),
        itsCheckedParamMinValue(kFloatMissing),
        itsCheckedParamMaxValue(kFloatMissing)
  {
  }

  int itsParamId;
  std::vector<float> itsParamMissingDataMaxProcent;
  std::vector<float> itsParamStraightDataMaxProcent;
  std::vector<float> itsParamLowerLimits;  // eri parametreille, jotta voidaan tarkistaa, ett‰ ei
                                           // mene hulluja arvoja tietokantaan.
  std::vector<float> itsParamUpperLimits;  // annetaan eri tasoiset yl‰ ja alarajat

  float itsCheckedParamMissingDataMaxProcent;   // t‰nne lasketaan parametri kohtaiset tulokset
  float itsCheckedParamStraightDataMaxProcent;  // t‰nne lasketaan parametri kohtaiset tulokset
  bool itsCheckedParamOutOfLimitWarning;  // t‰nne laitetaan true, jos on mennyt upper tai lower
                                          // rajan varoitus raja rikki
  bool itsCheckedParamOutOfLimitError;  // t‰nne laitetaan true, jos on mennyt upper tai lower rajan
                                        // virhe/fatal-virhe raja rikki
  float itsCheckedParamMinValue;        // t‰nne haetaan parametri kohtainen minimi
  float itsCheckedParamMaxValue;        // t‰nne haetaan parametri kohtainen maksimi
} NFmiParamCheckData;

inline std::ostream& operator<<(std::ostream& os, NFmiParamCheckData& item)
{
  os << item.itsParamId << " ";
  int errLevelSize = item.itsParamMissingDataMaxProcent.size();
  os << errLevelSize << " ";

  int i;
  for (i = 0; i < errLevelSize; i++)
    os << item.itsParamMissingDataMaxProcent[i] << " ";
  for (i = 0; i < errLevelSize; i++)
    os << item.itsParamStraightDataMaxProcent[i] << " ";
  for (i = 0; i < errLevelSize; i++)
    os << item.itsParamLowerLimits[i] << " ";
  for (i = 0; i < errLevelSize; i++)
    os << item.itsParamUpperLimits[i] << " ";

  os << std::endl;
  return os;
}
inline std::istream& operator>>(std::istream& is, NFmiParamCheckData& item)
{
  // convert name/number to enum
  std::string param;
  is >> param;
  item.itsParamId = FmiParameterName(converter.ToEnum(param));
  if (item.itsParamId == kFmiBadParameter)
  {
    try
    {
      item.itsParamId = boost::lexical_cast<unsigned int>(param);
    }
    catch (...)
    {
      throw std::runtime_error("Failed to convert '" + param + "' to a known parameter number");
    }
  }

  int errLevelSize = 0;
  is >> errLevelSize;
  item.itsParamMissingDataMaxProcent.resize(errLevelSize);
  item.itsParamStraightDataMaxProcent.resize(errLevelSize);
  item.itsParamLowerLimits.resize(errLevelSize);
  item.itsParamUpperLimits.resize(errLevelSize);

  int i;
  for (i = 0; i < errLevelSize; i++)
    is >> item.itsParamMissingDataMaxProcent[i];
  for (i = 0; i < errLevelSize; i++)
    is >> item.itsParamStraightDataMaxProcent[i];
  for (i = 0; i < errLevelSize; i++)
    is >> item.itsParamLowerLimits[i];
  for (i = 0; i < errLevelSize; i++)
    is >> item.itsParamUpperLimits[i];

  return is;
}

class NFmiOhjausData
{
 public:
  int ParamIdIndex(int theParId)
  {
    for (unsigned int i = 0; i < itsParamIdCheckList.size(); i++)
      if (theParId == itsParamIdCheckList[i].itsParamId) return i;
    return -1;  // ei lˆytynyt vastaavaa!!
  }
  int itsLocationCheckingType;  // 0=kaikki pisteet, 1=haluttu locationbagi, 2=n kpl satunnaisia
                                // pisteit‰
  int itsRandomPointCount;
  std::vector<NFmiParamCheckData> itsParamIdCheckList;
  std::vector<int>
      itsMinDataLengthInHours;  // aikatarkasteluja: minimi tuntim‰‰r‰ kuinka paljon dataa on oltava
  std::vector<int> itsMaxDataStartHourDifferenceBackwardInHours;  // maksimi alkuaika-heitto
                                                                  // nykyhetkest‰ taaksep‰in
                                                                  // tunteina (eli data alkaa max 6
                                                                  // tuntia ennen nykyhetke‰, 0= ei
                                                                  // v‰litet‰)
  std::vector<int> itsMaxDataStartHourDifferenceForwardInHours;   // maksimi alkuaika-heitto
                                                                  // nykyhetkest‰ eteenp‰in tunteina
  // (eli data alkaa enint‰‰n esim. 1
  // tuntia j‰lkeen nykyhetken, -1=
  // ei v‰litet‰)
  int itsWantedTimeStepInMinutes;
};

#endif
