#ifndef NFMIDATAMODIFIERDATACHECKING_H
#define NFMIDATAMODIFIERDATACHECKING_H

#include <newbase/NFmiDataModifier.h>

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

class NFmiDataModifierDataChecking : public NFmiDataModifier
{
 public:
  NFmiDataModifierDataChecking() : itsCheckedDataCount(0), itsFoundDataCount(0) {};
  ~NFmiDataModifierDataChecking() override = default;
  void Clear() override
  {
    itsCheckedDataCount = 0;
    itsFoundDataCount = 0;
  };
  int CheckedDataCount() { return itsCheckedDataCount; }
  int FoundDataCount() { return itsFoundDataCount; }
  using NFmiDataModifier::CalculationResult;
  float CalculationResult() override
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
  NFmiDataModifierDataMissing() : NFmiDataModifierDataChecking() {};
  ~NFmiDataModifierDataMissing() override = default;
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
  NFmiDataModifierDataStraight() : NFmiDataModifierDataChecking(), itsLastValue(kFloatMissing) {};
  ~NFmiDataModifierDataStraight() override = default;
  void Clear() override
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
