//**********************************************************
// C++ Class Name : NFmiQueryDataChecker
// ---------------------------------------------------------
// Filetype: (HEADER)
// Filepath: D:/projekti/GDPro/GDTemp/NFmiQueryDataChecker.h
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
#ifndef NFMIQUERYDATACHECKER_H
#define NFMIQUERYDATACHECKER_H

#include "NFmiParamCheckData.h"

#include <NFmiParamBag.h>
#include <NFmiLocationBag.h>
#include <NFmiTimeDescriptor.h>

#include <vector>

class NFmiQueryData;
class NFmiFastQueryInfo;
class NFmiDataModifierDataChecking;
class NFmiDataModifier;
class NFmiOhjausData;

// Luokka tarkastaa NFmiQueryData olion sis‰llˆn halutuilta osilta.
// Tarkistus voidaan rajata seuraavasti:
// Halutut asemat (NFmiLocationBag) tai n kpl pisteita sattumanvaraisesti
// tai kaikki pisteet.
// Halutut parametrit (NFmiParamBag) tai kaikki parametrit (aliparametrit
// mukaan lukien).
// M‰‰r‰tyt ajat (NFmiTiemDescriptor) tai kaikki ajat.
//
// Tarkistaa datasta seuraavat asiat halutessa:
// Puuttuuko dataa.
// Onko data ajassa muuttumatonta (suoraa viivaa) Huom! joktin parametrit
// voivat olla (esim. kokonais pilvisyys)!
// Onko parametrilla ‰lyttˆmi‰ arvoja (min ja max arvot annetaan haluttaessa
// parambagin yhteydess‰).
class NFmiQueryDataChecker
{
 public:
  NFmiQueryDataChecker(NFmiOhjausData* theOhjausData);
  ~NFmiQueryDataChecker(void);
  bool DoTotalCheck(void);
  void ParamBag(const NFmiParamBag& value);
  bool ParamBag(const NFmiString& theFileName);
  const NFmiParamBag& ParamBag(void) const { return itsParamBag; }
  const NFmiParamBag* DatasParamBag(void) const;
  void CheckOnlyWantedParams(bool value);
  bool CheckOnlyWantedParams(void) const { return fCheckOnlyWantedParams; }
  void CheckedTimeDescriptor(const NFmiTimeDescriptor& value);
  const NFmiTimeDescriptor& CheckedTimeDescriptor(void) const { return itsCheckedTimeDescriptor; }
  void CheckOnlyWantedTimes(bool value);
  bool CheckOnlyWantedTimes(void) const { return fCheckOnlyWantedTimes; }
  void CheckedLocations(const NFmiLocationBag& value);
  bool CheckedLocations(const NFmiString& theFileName);
  const NFmiLocationBag& CheckedLocations(void) const { return itsCheckedLocations; }
  void RandomlyCheckedLocationCount(int value);
  int RandomlyCheckedLocationCount(void) const { return itsRandomlyCheckedLocationCount; }
  void LocationCheckType(int value) { itsLocationCheckType = value; }
  int LocationCheckType(void) const { return itsLocationCheckType; }
  // checklist 0=ei mit‰‰n, 1=missing data,2=straight data, 4=limit check (e.g. 1+4=5 = missing and
  // limit check)
  void CheckList(int value) { itsCheckList = value; }
  int CheckList(void) const { return itsCheckList; }
  void Data(const NFmiQueryData* value);     // tekee kopion!
  bool Data(const NFmiString& theFileName);  // tekee kopion!
  const NFmiQueryData* Data(void) const { return itsData; }
  NFmiFastQueryInfo* Info(void) const { return itsInfo; }
  void SetRandomLocationIndexies(
      const std::vector<int>& value);  // jos arvonta ei kelpaa, aseta itse indeksit
  const std::vector<int>& RandomLocationIndexies(void) const { return itsRandomLocationIndexies; }
  void DoIndexRandomizing(bool value) { fDoIndexRandomizing = value; }
  bool DoIndexRandomizing(void) { return fDoIndexRandomizing; }
 private:
  bool DoMissingDataCheck(void);
  bool DoStraightDataCheck(void);
  bool DoOutOfLimitsDataCheck(void);
  void MakeRandomLocationIndexies(void);
  bool GoThroughData(NFmiDataModifier* theModifier);
  bool GoThroughLocationDataDataInTime(NFmiDataModifier* theModifier);
  bool GoThroughRandomLocationsDataInTime(NFmiDataModifier* theModifier);
  bool GoThroughAllLocationsDataInTime(NFmiDataModifier* theModifier);
  void CheckOutOfLimitsTerms(NFmiParamCheckData& theParamCheckData, NFmiDataModifier* theModifier);

  // Tarkastettavat parametrit pit‰‰ olla aktiivisia.
  NFmiParamBag itsParamBag;
  // Jos true, tarkastetaan parambagin parametrit.
  bool fCheckOnlyWantedParams;
  // Sis‰lt‰‰ testattavan timebagin tai timelistan.
  NFmiTimeDescriptor itsCheckedTimeDescriptor;
  // Jos t‰m‰ on true tarkista vain halutu ajat, muuten kaikki ajat.
  bool fCheckOnlyWantedTimes;
  // Haluttaessa voidaan testi rajata n‰ihin paikkoihin.
  NFmiLocationBag itsCheckedLocations;
  // Jos halutaan testata n kpl satunnaisia pisteit‰, annetaan t‰lle arvo.
  int itsRandomlyCheckedLocationCount;
  // Miten tutkitaan paikkoja: 0=kaikki, 1=halutut locationit, 2=n kpl satunnaisia paikkoja
  int itsLocationCheckType;
  // Mit‰ juttuja tarkistetaan (bitti on/off):
  // 1=puuttuvaa dataa
  // 2=onko suoraa viivaa
  // 4=onko luvut j‰rkevi‰
  // eli esim. 7= tarkista kaikki
  int itsCheckList;
  // Tarkistettava data (data omistetaan, tuhotaan).
  NFmiQueryData* itsData;
  // Datan info.
  NFmiFastQueryInfo* itsInfo;
  // Tarvittaessa t‰h‰n laitetaan sattumanvaraiset tarkistettavat paikkaindeksit.
  std::vector<int> itsRandomLocationIndexies;
  bool fDoIndexRandomizing;  // jos false ja jos on jo olemassa indeksit, pid‰ ne kun tehd‰‰n
                             // tarkistus ilman ett‰ arvotaan uudet indeksit

  NFmiOhjausData*
      itsOhjausData;  // (ei omista) t‰nne talletetaan parametrikohtaiset tiedost tarkastelusta
  int itsCheckOperationIndex;  // 1=missing, 2=straight, 4=out-of-limit. ohjausdata rakennetta
                               // p‰ivitet‰‰n operaation mukaan
  int itsParamIdIndex;         // ohjausdata rakennetta p‰ivitet‰‰n parametrin mukaan
};
#endif
