#pragma once
// FmiNetCdfQueryData.h

#include <newbase/NFmiGrid.h>
#include <newbase/NFmiLevelBag.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiQueryInfo.h>
#include <newbase/NFmiTimeList.h>
#include <string>
#include <vector>

class NFmiQueryData;
class NcFile;
class NcVar;

enum FmiNcLevelType
{
  kFmiNcNoLevelType = 0,
  kFmiNcSurface = 1,
  kFmiNcPressureLevel = kFmiPressureLevel,
  kFmiNcHeight = kFmiHeight,
  kFmiNcHybrid = kFmiHybridLevel
};

class FmiVarInfo
{
 public:
  FmiVarInfo(void)
      : itsVarName(),
        itsDimName(),
        itsParId(0),
        itsIndex(-1),
        itsFillValue(kFloatMissing),
        itsNcLevelType(kFmiNcNoLevelType)
  {
  }

  // private:
  std::string itsVarName;
  std::string itsDimName;
  int itsParId;        // tähän haluttu FMI par id
  int itsIndex;        // muuttujan (var) indeksi NC-tiedostossa
  float itsFillValue;  // mikä arvo on nc:ssä puuttuva arvo (_FillValue -atribuutti), nämä pitää
                       // sitten korvata kFloatMissing:illä
  FmiNcLevelType itsNcLevelType;  // tarkoittaa että tämä muuttuja kuuluu joko pinta dataan tai
                                  // multi-level-dataan
};

class FmiXYDimVarInfo
{
 public:
  FmiXYDimVarInfo(void) : itsVarName(), itsIndex(-1), itsValues() {}
  bool IsEmpty(void) { return (itsIndex == -1); }
  // private:
  std::string itsVarName;
  int itsIndex;  // muuttujan (var) indeksi NC-tiedostossa
  std::vector<float> itsValues;
};

class FmiZDimVarInfo
{
 public:
  FmiZDimVarInfo(void)
      : itsVarName(),
        itsDimName(),
        itsLevelUnitName(),
        itsIndex(-1),
        itsLevelType(0),
        itsValues(),
        itsLevels(),
        itsNcLevelType(kFmiNcNoLevelType)
  {
  }

  // private:
  std::string itsVarName;
  std::string itsDimName;
  std::string itsLevelUnitName;
  int itsIndex;      // muuttujan (var) indeksi NC-tiedostossa
  int itsLevelType;  // NC type vai FMI type?
  std::vector<float> itsValues;
  NFmiLevelBag itsLevels;
  FmiNcLevelType itsNcLevelType;  // tarkoittaa että tämä muuttuja kuuluu joko pinta dataan tai
                                  // multi-level-dataan
};

enum FmiNcTimeOffsetType
{
  kFmiNcNoTimeType = 0,
  kFmiNcSeconds = 1,
  kFmiNcMinutes = 2
};

class FmiTDimVarInfo
{
 public:
  FmiTDimVarInfo(void)
      : itsVarName(),
        itsIndex(-1),
        itsOffsetValues(),
        itsEpochTimeStr(),
        itsEpochTime(),
        itsTimeList(),
        itsTimeOffsetType(kFmiNcNoTimeType)
  {
  }
  void CalcTimeList(void);

  // private:
  std::string itsVarName;
  int itsIndex;  // muuttujan (var) indeksi NC-tiedostossa
  std::vector<long> itsOffsetValues;
  std::string itsEpochTimeStr;
  NFmiMetTime itsEpochTime;
  NFmiTimeList itsTimeList;
  FmiNcTimeOffsetType itsTimeOffsetType;
};

enum FmiNcProjectionType
{
  kFmiNcNoProjection = 0,
  kFmiNcStreographic = kNFmiStereographicArea
};

class FmiProjectionInfo
{
 public:
  FmiProjectionInfo(void)
      : itsProjectionType(kFmiNcNoProjection),
        La1(kFloatMissing),
        Lo1(kFloatMissing),
        Nx(-1),
        Ny(-1),
        LoV(kFloatMissing),
        Latin1(kFloatMissing),
        Latin2(kFloatMissing),
        Dx(kFloatMissing),
        Dy(kFloatMissing)
  {
  }

  // private:
  FmiNcProjectionType itsProjectionType;

  // Seuraavia arvoja tarvitaan ainakin LAPS-datassa stereograafisessa projektiossa
  // Muuttuja nimet tässä ovat samoja kuin nc-datassa.
  float La1;     // projektion 1. kulmapisteen lat-arvo (bottom-left?)
  float Lo1;     // projektion 1. kulmapisteen lon-arvo (bottom-left?)
  long Nx;       // hilan x-koko
  long Ny;       // hilan y-koko
  float LoV;     // orientation of grid (central-longitude)
  float Latin1;  // orientation of grid (true-latitude)
  float Latin2;  // orientation of grid (central-latitude)
  float Dx;      // grid length in metres
  float Dy;      // grid height in metres
};

class FmiNcMetaData
{
 public:
  FmiNcMetaData(bool useSurfaceInfo);
  ~FmiNcMetaData(void);

  void MakeMetaInfo(FmiTDimVarInfo &theTimeInfo, NFmiGrid &theGrid);
  std::string GetLevelDimName(void);
  FmiNcLevelType GetLevelType(void);

  // private:
  bool fUseSurfaceInfo;
  FmiZDimVarInfo itsMultiLevelInfo;
  FmiVarInfo itsSurfaceLevelInfo;
  NFmiParamBag itsParams;
  NFmiQueryInfo *itsMetaInfo;
};

class FmiNetCdfQueryData
{
 public:
  FmiNetCdfQueryData(void);
  ~FmiNetCdfQueryData(void);

  bool DataOk(void) const { return fDataOk; }
  std::vector<NFmiQueryData *> CreateQueryDatas(const std::string &theNcFileName);
  void Producer(const NFmiProducer &theProducer) { itsProducer = theProducer; }
  const std::string &ErrorMessage(void) const { return itsErrorMessage; }

 private:
  void InitMetaInfo(NcFile &theNcFile);  // throws exceptions!
  void MakeAllMetaInfos(void);
  void InitKnownParamMap(void);
  void Clear(void);
  void InitTimeDim(NcVar &theVar, const std::string &theVarName, int theIndex);
  void CalcTimeList(void);
  void InitZDim(NcVar &theVar,
                const std::string &theVarName,
                int theIndex,
                FmiNcLevelType theLevelType);
  void InitNormalVar(NcVar &theVar, const std::string &theVarName, int theIndex);
  void MakeWantedGrid(void);
  void MakeWantedParamBag(void);
  FmiParameterName GetParameterName(NcVar &theVar, FmiParameterName theDefaultParName);
  void SeekProjectionInfo(NcFile &theNcFile);
  void InitializeStreographicGrid(void);
  FmiZDimVarInfo &GetLevelInfo(FmiNcLevelType theLevelType);
  bool IsSurfaceVariable(NcVar &theVar);
  void MakesureSurfaceMetaDataIsInitialized();

  bool fDataOk;                         // onko tämä initialisoitu ja onko annettu NcFile ollut ok.
  FmiTDimVarInfo itsTInfo;              // aika muuttujan tiedot tähän
  FmiXYDimVarInfo itsXInfo;             // X eli hilan longitude piste muuttujan tiedot tähän
  FmiXYDimVarInfo itsYInfo;             // Y eli hilan latitude piste muuttujan tiedot tähän
  FmiProjectionInfo itsProjectionInfo;  // jos datassa on lat-lon:ista poikkeava projektio, kerätään
                                        // tähän tarvittavat tiedot
  std::vector<FmiVarInfo> itsNormalParameters;  // tässä on ns. normaalien muutuja parametrien
                                                // lista, joissa on ulottuvuuksina edelliset 4
                                                // muuttuja infot
  NFmiGrid itsGrid;                             // tämä hila määrätään, kun X, ja Y arvot on saatu
  NFmiProducer itsProducer;
  FmiNcMetaData itsSurfaceMetaData;
  FmiNcMetaData itsHeightLevelMetaData;
  FmiNcMetaData itsPressureLevelMetaData;
  FmiNcMetaData itsHybridLevelMetaData;

  std::string itsErrorMessage;  // talletetaan tähän mahdollinen virheilmoitus
  std::map<std::string, FmiParameterName> itsKnownParameterMap;
};
