//**********************************************************
// C++ Class Name : NFmiQueryDataChecker
// ---------------------------------------------------------
// Filetype: (SOURCE)
// Filepath: D:/projekti/GDPro/GDTemp/NFmiQueryDataChecker.cpp
//
//
// GDPro Properties
// ---------------------------------------------------
//  - GD Symbol Type    : CLD_Class
//  - GD Method         : UML ( 4.0 )
//  - GD System Name    : editori virityksi‰ 2000 syksy
//  - GD View Type      : Class Diagram
//  - GD View Name      : datan tarkastaja otus
// ---------------------------------------------------
//  Author         : pietarin
//  Creation Date  : Mon - Nov 13, 2000
//
//  Change Log     :
//
//**********************************************************

#include "NFmiQueryDataChecker.h"
#include "NFmiDataModifierDataChecking.h"
#include "NFmiParamCheckData.h"

#include <newbase/NFmiDataModifier.h>
#include <newbase/NFmiDataModifierMinMax.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <time.h>

//--------------------------------------------------------
// Constructor/Destructor
//--------------------------------------------------------
NFmiQueryDataChecker::NFmiQueryDataChecker(NFmiOhjausData* theOhjausData)
    : itsParamBag(),
      fCheckOnlyWantedParams(false),
      itsCheckedTimeDescriptor(),
      fCheckOnlyWantedTimes(false),
      itsCheckedLocations(),
      itsRandomlyCheckedLocationCount(0),
      itsLocationCheckType(0),
      itsCheckList(0),
      itsData(0),
      itsInfo(0),
      itsRandomLocationIndexies(),
      fDoIndexRandomizing(true),
      itsOhjausData(theOhjausData)
{
}
NFmiQueryDataChecker::~NFmiQueryDataChecker(void)
{
  delete itsData;
  delete itsInfo;
}
//--------------------------------------------------------
// DoTotalCheck
//--------------------------------------------------------
bool NFmiQueryDataChecker::DoTotalCheck(void)
{
  bool status = true;
  if (!itsInfo) return false;
  itsInfo->First();

  if (fDoIndexRandomizing) MakeRandomLocationIndexies();
  if (itsCheckList & 1) status &= DoMissingDataCheck();
  if (itsCheckList & 2) status &= DoStraightDataCheck();
  if (itsCheckList & 4) status &= DoOutOfLimitsDataCheck();

  return status;
}

void NFmiQueryDataChecker::ParamBag(const NFmiParamBag& value)
{
  fCheckOnlyWantedParams = true;
  itsParamBag = value;
}

bool NFmiQueryDataChecker::ParamBag(const NFmiString& theFileName)
{
  std::ifstream in(theFileName);
  if (in)
  {
    fCheckOnlyWantedParams = true;
    in >> itsParamBag;  // muista ett‰ haluttujen parametrien pit‰‰ olla aktiivisia jo tiedostossa
    in.close();
    return true;
  }
  return false;
}

bool NFmiQueryDataChecker::CheckedLocations(const NFmiString& theFileName)
{
  std::ifstream in(theFileName);
  if (in)
  {
    itsLocationCheckType = 1;  // 1=halutut paikat tarkistetaan
    in >> itsCheckedLocations;
    in.close();
    return true;
  }
  return false;
}

const NFmiParamBag* NFmiQueryDataChecker::DatasParamBag(void) const
{
  NFmiParamBag* params = 0;
  if (itsInfo)
  {
    return &itsInfo->ParamBag();
  }
  return params;
}

void NFmiQueryDataChecker::CheckOnlyWantedParams(bool value)
{
  fCheckOnlyWantedParams = value;
  if (!fCheckOnlyWantedParams && itsInfo)
  {
    itsParamBag = itsInfo->ParamBag();
    itsParamBag.SetActivities(true);
  }
}

void NFmiQueryDataChecker::CheckedTimeDescriptor(const NFmiTimeDescriptor& value)
{
  fCheckOnlyWantedTimes = true;
  itsCheckedTimeDescriptor = value;
}

void NFmiQueryDataChecker::CheckedLocations(const NFmiLocationBag& value)
{
  itsLocationCheckType = 1;  // 1=halutut paikat tarkistetaan
  itsCheckedLocations = value;
}

void NFmiQueryDataChecker::RandomlyCheckedLocationCount(int value)
{
  itsLocationCheckType = 2;  // 2=satunnaiset paikat tarkastetaan
  itsRandomlyCheckedLocationCount = value;
}

// tekee kopion datasta!
void NFmiQueryDataChecker::Data(const NFmiQueryData* value)
{
  if (value)
  {
    if (itsData)
    {
      delete itsData;
      delete itsInfo;
      itsData = 0;
      itsInfo = 0;
    }
    itsData = value->Clone();
    itsInfo = new NFmiFastQueryInfo(itsData);
  }
}

bool NFmiQueryDataChecker::Data(const NFmiString& theFileName)
{
  std::ifstream in(theFileName, std::ios::binary);
  if (in)
  {
    if (itsData)
    {
      delete itsData;
      delete itsInfo;
      itsData = 0;
      itsInfo = 0;
    }
    itsData = new NFmiQueryData;
    try
    {
      in >> *itsData;
    }
    catch (...)
    {
    }
    if (itsData)
    {
      itsInfo = new NFmiFastQueryInfo(itsData);
      return true;
    }
  }
  return false;
}

// jos arvonta ei kelpaa, aseta itse indeksit
void NFmiQueryDataChecker::SetRandomLocationIndexies(const std::vector<int>& value)
{
  fDoIndexRandomizing = false;
  itsRandomLocationIndexies = value;
}

void NFmiQueryDataChecker::CheckOnlyWantedTimes(bool value)
{
  fCheckOnlyWantedTimes = value;
  if (!fCheckOnlyWantedTimes && itsInfo)
  {
    itsCheckedTimeDescriptor =
        itsInfo->TimeDescriptor();  // otetaan infon timedesc, joka k‰yd‰‰n l‰pi
  }
}

//--------------------------------------------------------
// DoMissingDataCheck
//--------------------------------------------------------
bool NFmiQueryDataChecker::DoMissingDataCheck(void)
{
  NFmiDataModifierDataMissing modifier;
  itsCheckOperationIndex = 1;

  return GoThroughData(&modifier);
}
//--------------------------------------------------------
// DoStraightDataCheck
//--------------------------------------------------------
bool NFmiQueryDataChecker::DoStraightDataCheck(void)
{
  NFmiDataModifierDataStraight modifier;
  itsCheckOperationIndex = 2;
  return GoThroughData(&modifier);
}
//--------------------------------------------------------
// DoOutOfLimitsDataCheck
//--------------------------------------------------------
bool NFmiQueryDataChecker::DoOutOfLimitsDataCheck(void)
{
  NFmiDataModifierMinMax modifier;
  itsCheckOperationIndex = 4;
  return GoThroughData(&modifier);
}

// ei k‰y l‰pi leveleit‰!!!!
bool NFmiQueryDataChecker::GoThroughData(NFmiDataModifier* theModifier)
{
  if (itsInfo)
  {
    itsInfo->First();
    for (itsParamBag.Reset();
         itsParamBag.Next(false);)  // parametrien pit‰‰ olla ulommaisia loopissa!!!!
    {
      if (itsParamBag.Current(false)->IsActive())
      {
        if (itsInfo->Param(*itsParamBag.Current(false)))
        {
          int parId = itsInfo->Param().GetParamIdent();
          int itsParamIdIndex = itsOhjausData->ParamIdIndex(parId);
          if (itsParamIdIndex >= 0)  // t‰m‰n pit‰‰ onnistua!!!
          {
            theModifier->Clear();
            switch (itsLocationCheckType)
            {
              case 0:  // kaikki
                GoThroughAllLocationsDataInTime(theModifier);
                break;
              case 1:  // halutut locationit
                GoThroughLocationDataDataInTime(theModifier);
                break;
              case 2:  // random locationindex
                GoThroughRandomLocationsDataInTime(theModifier);
                break;
            }

            float result = theModifier->CalculationResult();
            switch (itsCheckOperationIndex)
            {
              case 1:  // missing data
                itsOhjausData->itsParamIdCheckList[itsParamIdIndex]
                    .itsCheckedParamMissingDataMaxProcent = result;
                break;
              case 2:  // straight data
                itsOhjausData->itsParamIdCheckList[itsParamIdIndex]
                    .itsCheckedParamStraightDataMaxProcent = result;
                break;
              case 4:  // out-of-limit
                CheckOutOfLimitsTerms(itsOhjausData->itsParamIdCheckList[itsParamIdIndex],
                                      theModifier);
                break;
            }
          }
          else
          {
            // mit‰ ihmett‰, ei sen t‰nne pit‰nyt menn‰ (VIRHETILANNE!!!)
          }
        }
      }
    }
    return true;
  }
  return false;
}

// asetetaan varoitus ja error flagit p‰‰lle, jos tapahtunut rajojen yli/alituksia
void NFmiQueryDataChecker::CheckOutOfLimitsTerms(NFmiParamCheckData& theParamCheckData,
                                                 NFmiDataModifier* theModifier)
{
  NFmiDataModifierMinMax* modifier =
      static_cast<NFmiDataModifierMinMax*>(theModifier);  // rumaa, mutta voi voi
  float minValue = modifier->MinValue();
  float maxValue = modifier->MaxValue();
  theParamCheckData.itsCheckedParamMinValue = minValue;
  theParamCheckData.itsCheckedParamMaxValue = maxValue;
  if (minValue != kFloatMissing)
  {
    if (theParamCheckData.itsParamLowerLimits[2] != kFloatMissing &&
        minValue < theParamCheckData.itsParamLowerLimits[2])
      theParamCheckData.itsCheckedParamOutOfLimitWarning = true;
    if (theParamCheckData.itsParamLowerLimits[1] != kFloatMissing &&
        minValue < theParamCheckData.itsParamLowerLimits[1])
      theParamCheckData.itsCheckedParamOutOfLimitError = true;
    // HUOM!! fataalirajan tarkastusta ei tehd‰!!!
  }
  if (maxValue != kFloatMissing)
  {
    if (theParamCheckData.itsParamUpperLimits[2] != kFloatMissing &&
        maxValue > theParamCheckData.itsParamUpperLimits[2])
      theParamCheckData.itsCheckedParamOutOfLimitWarning = true;
    if (theParamCheckData.itsParamUpperLimits[1] != kFloatMissing &&
        maxValue > theParamCheckData.itsParamUpperLimits[1])
      theParamCheckData.itsCheckedParamOutOfLimitError = true;
    // HUOM!! fataalirajan tarkastusta ei tehd‰!!!
  }
}

// HUOM!! t‰t‰ funktiota pit‰‰ kutsua GoThroughData():sta
bool NFmiQueryDataChecker::GoThroughLocationDataDataInTime(NFmiDataModifier* theModifier)
{
  for (itsCheckedLocations.Reset(); itsCheckedLocations.Next();)
  {
    if (itsInfo->IsGrid())
    {
      // ajan pit‰‰ juosta sisimm‰ss‰ loopissa, ett‰ voidaan tarkistaa 'suoraa' dataa!!!!
      for (itsCheckedTimeDescriptor.Reset(); itsCheckedTimeDescriptor.Next();)
      {
        if (itsInfo->Time(itsCheckedTimeDescriptor.Time()))
        {
          theModifier->Calculate(
              itsInfo->InterpolatedValue(itsCheckedLocations.Location()->GetLocation()));
        }
      }
    }
    else if (itsInfo->Location(*itsCheckedLocations.Location()))
    {
      // ajan pit‰‰ juosta sisimm‰ss‰ loopissa, ett‰ voidaan tarkistaa 'suoraa' dataa!!!!
      for (itsCheckedTimeDescriptor.Reset(); itsCheckedTimeDescriptor.Next();)
      {
        if (itsInfo->Time(itsCheckedTimeDescriptor.Time()))
        {
          theModifier->Calculate(itsInfo->FloatValue());
        }
      }
    }
  }
  return true;
}

// HUOM!! t‰t‰ funktiota pit‰‰ kutsua GoThroughData():sta
bool NFmiQueryDataChecker::GoThroughRandomLocationsDataInTime(NFmiDataModifier* theModifier)
{
  int locationIndexSize = itsRandomLocationIndexies.size();
  for (int i = 0; i < locationIndexSize; i++)
  {
    if (itsInfo->LocationIndex(itsRandomLocationIndexies[i]))
    {
      // ajan pit‰‰ juosta sisimm‰ss‰ loopissa, ett‰ voidaan tarkistaa 'suoraa' dataa!!!!
      for (itsCheckedTimeDescriptor.Reset(); itsCheckedTimeDescriptor.Next();)
      {
        if (itsInfo->Time(itsCheckedTimeDescriptor.Time()))
        {
          theModifier->Calculate(itsInfo->FloatValue());
        }
      }
    }
  }
  return true;
}

// HUOM!! t‰t‰ funktiota pit‰‰ kutsua GoThroughData():sta
bool NFmiQueryDataChecker::GoThroughAllLocationsDataInTime(NFmiDataModifier* theModifier)
{
  for (itsInfo->ResetLocation(); itsInfo->NextLocation();)
  {
    // ajan pit‰‰ juosta sisimm‰ss‰ loopissa, ett‰ voidaan tarkistaa 'suoraa' dataa!!!!
    for (itsCheckedTimeDescriptor.Reset(); itsCheckedTimeDescriptor.Next();)
    {
      if (itsInfo->Time(itsCheckedTimeDescriptor.Time()))
      {
        theModifier->Calculate(itsInfo->FloatValue());
      }
    }
  }
  return true;
}

// HUOM!!! laita t‰m‰ viimeiseksi niin varoitukset tulevat loppuun.
// tyhjent‰‰ ja taytt‰‰ paikkaindex listan
// K‰ytet‰‰n apuna set luokkaa, ett‰ ei tule samoja indeksej‰ vahingossa.
void NFmiQueryDataChecker::MakeRandomLocationIndexies(void)
{
  if (itsInfo)
  {
    int locationCount = itsInfo->SizeLocations();
    if (itsRandomlyCheckedLocationCount >= locationCount)
    {  // jos haluttu luku ylitt‰‰ location lukum‰‰r‰n, t‰llˆin k‰yd‰‰n kaikki indeksit vain l‰pi
      itsRandomLocationIndexies.resize(locationCount);
      for (int i = 0; i < locationCount; i++)
        itsRandomLocationIndexies[i] = i;
    }
    else
    {
      // muista set avuksi!!
      srand(static_cast<unsigned>(time(nullptr)));
      std::set<int> helpSet;
      int index = 0;
      for (; static_cast<int>(helpSet.size()) < itsRandomlyCheckedLocationCount;)
      {
        index = rand() % (locationCount - 1);
        helpSet.insert(index);
      }
      itsRandomLocationIndexies.clear();
      std::copy(helpSet.begin(), helpSet.end(), std::back_inserter(itsRandomLocationIndexies));
    }
  }
}
