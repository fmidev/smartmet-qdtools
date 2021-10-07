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
  FmiVarInfo()
      :

        itsFillValue(kFloatMissing)

  {
  }

  // private:
  std::string itsVarName;
  std::string itsDimName;
  int itsParId{0};     // t‰h‰n haluttu FMI par id
  int itsIndex{-1};    // muuttujan (var) indeksi NC-tiedostossa
  float itsFillValue;  // mik‰ arvo on nc:ss‰ puuttuva arvo (_FillValue -atribuutti), n‰m‰ pit‰‰
                       // sitten korvata kFloatMissing:ill‰
  FmiNcLevelType itsNcLevelType{kFmiNcNoLevelType};  // tarkoittaa ett‰ t‰m‰ muuttuja kuuluu joko
                                                     // pinta dataan tai multi-level-dataan
};

class FmiXYDimVarInfo
{
 public:
  FmiXYDimVarInfo() {}
  bool IsEmpty() const { return (itsIndex == -1); }
  // private:
  std::string itsVarName;
  int itsIndex{-1};  // muuttujan (var) indeksi NC-tiedostossa
  std::vector<float> itsValues;
};

class FmiZDimVarInfo
{
 public:
  FmiZDimVarInfo() {}

  // private:
  std::string itsVarName;
  std::string itsDimName;
  std::string itsLevelUnitName;
  int itsIndex{-1};     // muuttujan (var) indeksi NC-tiedostossa
  int itsLevelType{0};  // NC type vai FMI type?
  std::vector<float> itsValues;
  NFmiLevelBag itsLevels;
  FmiNcLevelType itsNcLevelType{kFmiNcNoLevelType};  // tarkoittaa ett‰ t‰m‰ muuttuja kuuluu joko
                                                     // pinta dataan tai multi-level-dataan
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
  FmiTDimVarInfo() : itsTimeList() {}
  void CalcTimeList();

  // private:
  std::string itsVarName;
  int itsIndex{-1};  // muuttujan (var) indeksi NC-tiedostossa
  std::vector<long> itsOffsetValues;
  std::string itsEpochTimeStr;
  NFmiMetTime itsEpochTime;
  NFmiTimeList itsTimeList;
  FmiNcTimeOffsetType itsTimeOffsetType{kFmiNcNoTimeType};
};

enum FmiNcProjectionType
{
  kFmiNcNoProjection = 0,
  kFmiNcStreographic = kNFmiStereographicArea
};

class FmiProjectionInfo
{
 public:
  FmiProjectionInfo()
      : La1(kFloatMissing),
        Lo1(kFloatMissing),

        LoV(kFloatMissing),
        Latin1(kFloatMissing),
        Latin2(kFloatMissing),
        Dx(kFloatMissing),
        Dy(kFloatMissing)
  {
  }

  // private:
  FmiNcProjectionType itsProjectionType{kFmiNcNoProjection};

  // Seuraavia arvoja tarvitaan ainakin LAPS-datassa stereograafisessa projektiossa
  // Muuttuja nimet t‰ss‰ ovat samoja kuin nc-datassa.
  float La1;     // projektion 1. kulmapisteen lat-arvo (bottom-left?)
  float Lo1;     // projektion 1. kulmapisteen lon-arvo (bottom-left?)
  long Nx{-1};   // hilan x-koko
  long Ny{-1};   // hilan y-koko
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
  ~FmiNcMetaData();

  void MakeMetaInfo(FmiTDimVarInfo &theTimeInfo, NFmiGrid &theGrid);
  std::string GetLevelDimName() const;
  FmiNcLevelType GetLevelType() const;

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
  FmiNetCdfQueryData();
  ~FmiNetCdfQueryData();

  bool DataOk() const { return fDataOk; }
  std::vector<NFmiQueryData *> CreateQueryDatas(const std::string &theNcFileName);
  void Producer(const NFmiProducer &theProducer) { itsProducer = theProducer; }
  const std::string &ErrorMessage() const { return itsErrorMessage; }

 private:
  void InitMetaInfo(NcFile &theNcFile);  // throws exceptions!
  void MakeAllMetaInfos();
  void InitKnownParamMap();
  void Clear();
  void InitTimeDim(NcVar &theVar, const std::string &theVarName, int theIndex);
  void CalcTimeList();
  void InitZDim(NcVar &theVar,
                const std::string &theVarName,
                int theIndex,
                FmiNcLevelType theLevelType);
  void InitNormalVar(NcVar &theVar, const std::string &theVarName, int theIndex);
  void MakeWantedGrid();
  void MakeWantedParamBag();
  FmiParameterName GetParameterName(NcVar &theVar, FmiParameterName theDefaultParName);
  void SeekProjectionInfo(NcFile &theNcFile);
  void InitializeStreographicGrid();
  FmiZDimVarInfo &GetLevelInfo(FmiNcLevelType theLevelType);
  bool IsSurfaceVariable(NcVar &theVar) const;
  void MakesureSurfaceMetaDataIsInitialized();

  bool fDataOk{false};                  // onko t‰m‰ initialisoitu ja onko annettu NcFile ollut ok.
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
