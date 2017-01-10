#pragma once
// FmiNetCdfQueryData.h

#include <string>
#include <vector>
#include "NFmiGrid.h"
#include "NFmiLevelBag.h"
#include "NFmiMetTime.h"
#include "NFmiQueryInfo.h"
#include "NFmiTimeList.h"

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
  int itsParId;        // t‰h‰n haluttu FMI par id
  int itsIndex;        // muuttujan (var) indeksi NC-tiedostossa
  float itsFillValue;  // mik‰ arvo on nc:ss‰ puuttuva arvo (_FillValue -atribuutti), n‰m‰ pit‰‰
                       // sitten korvata kFloatMissing:ill‰
  FmiNcLevelType itsNcLevelType;  // tarkoittaa ett‰ t‰m‰ muuttuja kuuluu joko pinta dataan tai
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
  FmiNcLevelType itsNcLevelType;  // tarkoittaa ett‰ t‰m‰ muuttuja kuuluu joko pinta dataan tai
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
  // Muuttuja nimet t‰ss‰ ovat samoja kuin nc-datassa.
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

  bool fDataOk;                         // onko t‰m‰ initialisoitu ja onko annettu NcFile ollut ok.
  FmiTDimVarInfo itsTInfo;              // aika muuttujan tiedot t‰h‰n
  FmiXYDimVarInfo itsXInfo;             // X eli hilan longitude piste muuttujan tiedot t‰h‰n
  FmiXYDimVarInfo itsYInfo;             // Y eli hilan latitude piste muuttujan tiedot t‰h‰n
  FmiProjectionInfo itsProjectionInfo;  // jos datassa on lat-lon:ista poikkeava projektio, ker‰t‰‰n
                                        // t‰h‰n tarvittavat tiedot
  std::vector<FmiVarInfo> itsNormalParameters;  // t‰ss‰ on ns. normaalien muutuja parametrien
                                                // lista, joissa on ulottuvuuksina edelliset 4
                                                // muuttuja infot
  NFmiGrid itsGrid;                             // t‰m‰ hila m‰‰r‰t‰‰n, kun X, ja Y arvot on saatu
  NFmiProducer itsProducer;
  FmiNcMetaData itsSurfaceMetaData;
  FmiNcMetaData itsHeightLevelMetaData;
  FmiNcMetaData itsPressureLevelMetaData;
  FmiNcMetaData itsHybridLevelMetaData;

  std::string itsErrorMessage;  // talletetaan t‰h‰n mahdollinen virheilmoitus
  std::map<std::string, FmiParameterName> itsKnownParameterMap;
};
