#ifndef NFMIDATAMODIFIERDATACHECKING_H
#define NFMIDATAMODIFIERDATACHECKING_H

#include <newbase/NFmiDataModifier.h>

// ***************************************************************
// ***************   Tehd‰‰n Modifier luokat t‰ss‰ !!! ***********
// ***************************************************************

class NFmiDataModifierDataChecking : public NFmiDataModifier
{
 public:
  NFmiDataModifierDataChecking() = default;
  ~NFmiDataModifierDataChecking() override = default;
  void Clear() override
  {
    itsCheckedDataCount = 0;
    itsFoundDataCount = 0;
  };
  int CheckedDataCount() const { return itsCheckedDataCount; }
  int FoundDataCount() const { return itsFoundDataCount; }
  using NFmiDataModifier::CalculationResult;
  float CalculationResult() override
  {
    float result =
        itsCheckedDataCount ? itsFoundDataCount / float(itsCheckedDataCount) * 100 : kFloatMissing;
    return result;
  };

 protected:
  int itsCheckedDataCount{0};
  int itsFoundDataCount{0};
};

class NFmiDataModifierDataMissing : public NFmiDataModifierDataChecking
{
 public:
  NFmiDataModifierDataMissing(){};
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
  NFmiDataModifierDataStraight() : itsLastValue(kFloatMissing){};
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
