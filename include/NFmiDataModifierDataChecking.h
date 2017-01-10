#ifndef NFMIDATAMODIFIERDATACHECKING_H
#define NFMIDATAMODIFIERDATACHECKING_H

#include "NFmiDataModifier.h"

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

class NFmiDataModifierDataChecking : public NFmiDataModifier
{
 public:
  NFmiDataModifierDataChecking(void) : itsCheckedDataCount(0), itsFoundDataCount(0){};
  virtual ~NFmiDataModifierDataChecking(void){};
  void Clear(void)
  {
    itsCheckedDataCount = 0;
    itsFoundDataCount = 0;
  };
  int CheckedDataCount(void) { return itsCheckedDataCount; }
  int FoundDataCount(void) { return itsFoundDataCount; }
  using NFmiDataModifier::CalculationResult;
  float CalculationResult(void)
  {
    float result =
        itsCheckedDataCount ? itsFoundDataCount / float(itsCheckedDataCount) * 100 : kFloatMissing;
    return result;
  };

 protected:
  int itsCheckedDataCount;
  int itsFoundDataCount;
};

class NFmiDataModifierDataMissing : public NFmiDataModifierDataChecking
{
 public:
  NFmiDataModifierDataMissing(void) : NFmiDataModifierDataChecking(){};
  virtual ~NFmiDataModifierDataMissing(void){};
  using NFmiDataModifierDataChecking::Calculate;
  virtual void Calculate(float theValue)
  {
    itsCheckedDataCount++;
    if (theValue == kFloatMissing) itsFoundDataCount++;
  };
};
class NFmiDataModifierDataStraight : public NFmiDataModifierDataChecking
{
 public:
  NFmiDataModifierDataStraight(void)
      : NFmiDataModifierDataChecking(), itsLastValue(kFloatMissing){};
  virtual ~NFmiDataModifierDataStraight(void){};
  void Clear(void)
  {
    NFmiDataModifierDataChecking::Clear();
    itsLastValue = kFloatMissing;
  };
  using NFmiDataModifierDataChecking::Calculate;
  virtual void Calculate(float theValue)
  {
    itsCheckedDataCount++;
    if (theValue == itsLastValue) itsFoundDataCount++;
    itsLastValue = theValue;
  };

 protected:
  float itsLastValue;
};

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

#endif
