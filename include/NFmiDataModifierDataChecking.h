#ifndef NFMIDATAMODIFIERDATACHECKING_H
#define NFMIDATAMODIFIERDATACHECKING_H

#include <newbase/NFmiDataModifier.h>

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

class NFmiDataModifierDataChecking : public NFmiDataModifier
{
 public:
  NFmiDataModifierDataChecking(void) : itsCheckedDataCount(0), itsFoundDataCount(0){};
  ~NFmiDataModifierDataChecking(void) override {};
  void Clear(void) override
  {
    itsCheckedDataCount = 0;
    itsFoundDataCount = 0;
  };
  int CheckedDataCount(void) { return itsCheckedDataCount; }
  int FoundDataCount(void) { return itsFoundDataCount; }
  using NFmiDataModifier::CalculationResult;
  float CalculationResult(void) override
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
  ~NFmiDataModifierDataMissing(void) override {};
  using NFmiDataModifierDataChecking::Calculate;
  void Calculate(float theValue) override
  {
    itsCheckedDataCount++;
    if (theValue == kFloatMissing)
      itsFoundDataCount++;
  };
};
class NFmiDataModifierDataStraight : public NFmiDataModifierDataChecking
{
 public:
  NFmiDataModifierDataStraight(void)
      : NFmiDataModifierDataChecking(), itsLastValue(kFloatMissing){};
  ~NFmiDataModifierDataStraight(void) override {};
  void Clear(void) override
  {
    NFmiDataModifierDataChecking::Clear();
    itsLastValue = kFloatMissing;
  };
  using NFmiDataModifierDataChecking::Calculate;
  void Calculate(float theValue) override
  {
    itsCheckedDataCount++;
    if (theValue == itsLastValue)
      itsFoundDataCount++;
    itsLastValue = theValue;
  };

 protected:
  float itsLastValue;
};

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

#endif
