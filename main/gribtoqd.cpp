#ifdef _MSC_VER
#pragma warning(disable : 4786 4996)  // poistaa n kpl VC++ kääntäjän varoitusta (liian pitkä nimi
                                      // >255 merkkiä
// joka johtuu 'puretuista' STL-template nimistä)
#endif

#include "GribTools.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiAreaTools.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiDataMatrixUtils.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiInterpolation.h>
#include <newbase/NFmiMilliSecondTimer.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiTotalWind.h>
#include <newbase/NFmiValueString.h>
#include <functional>
#include <grib_api.h>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace std;

bool jscan_is_negative(grib_handle *theHandle)
{
  long direction = 0;
  int status = grib_get_long(theHandle, "jScansPositively", &direction);
  if (status != 0)
    return false;
  return (direction == 0);
}

void check_jscan_direction(grib_handle *theHandle)
{
  long direction = 0;
  int status = grib_get_long(theHandle, "jScansPositively", &direction);
  if (status != 0)
    return;
  if (direction == 0)
    throw std::runtime_error("GRIBs with a negative j-scan direction are not supported");
}

// template<typename T>
struct PointerDestroyer
{
  template <typename T>
  void operator()(T *thePtr)
  {
    delete thePtr;
  }
};

struct GeneratedHybridParamInfo
{
  GeneratedHybridParamInfo(void)
      : fCalcHybridParam(false), itsGeneratedHybridParam(), itsHelpParamId(kFmiBadParameter)
  {
  }

  bool fCalcHybridParam;
  NFmiParam itsGeneratedHybridParam;
  FmiParameterName itsHelpParamId;
};

class GridSettingsPackage
{
 public:
  std::shared_ptr<NFmiGrid> itsBaseGrid;
  std::shared_ptr<NFmiGrid> itsPressureGrid;
  std::shared_ptr<NFmiGrid> itsHybridGrid;
};

class Reduced_ll_grib_exception
{
  // gribejä yritetään purkaa ensin käyttäen ECMWF:n grib_api-kirjastoa.
  // Jos kohdataan yksikin reduced_ll gribi, heitetään tämä poikkeus ja sen jälkeen
  // kaikki gribit yritetään purkaa käyttäen NOAA:n wgrib:iä.
};

struct GribFilterOptions
{
  GribFilterOptions(void)
      : itsOutputFileName(),
        fUseOutputFile(false),
        itsReturnStatus(0),
        itsIgnoredLevelList(),
        itsGeneratedDatas(),
        itsAcceptOnlyLevelTypes(),
        itsGridInfoPrintCount(0),
        itsParamChangeTable(),
        fCropParamsNotMensionedInTable(false),
        fDoAtlanticFix(false),
        fDoPacificFix(false),
        fDoYAxisFlip(false),
        fTryAreaCombination(false),
        fDoLeftRightSwap(false),
        fIgnoreReducedLLData(false),
        fUseLevelTypeFileNaming(false),
        itsWantedSurfaceProducer(),
        itsWantedPressureProducer(),
        itsWantedHybridProducer(),
        itsHybridPressureInfo(),
        itsGroundPressureInfo(),
        itsLatlonCropRect(gMissingCropRect),
        itsGridSettings(),
        fVerbose(false),
        itsInputFileNameStr(),
        itsInputFile(0),
        itsStepRangeCheckedParams(),
        itsWantedStepRange(0)
  {
  }

  ~GribFilterOptions(void)
  {
    if (itsInputFile)
      ::fclose(itsInputFile);  // suljetaan tiedosto, josta gribi luettu
  }

  bool DoGlobalFix(void) const
  {
    if (fDoAtlanticFix || fDoPacificFix || fDoLeftRightSwap)
      return true;
    else
      return false;
  }

  string itsOutputFileName;  // -o optio tai sitten tulostetann cout:iin
  bool fUseOutputFile;
  int itsReturnStatus;               // 0 = ok
  NFmiLevelBag itsIgnoredLevelList;  // lista missä yksittäisiä leveleitä, mitkä halutaan jättää
                                     // pois laskuista
  vector<std::shared_ptr<NFmiQueryData> > itsGeneratedDatas;
  vector<FmiLevelType> itsAcceptOnlyLevelTypes;  // lista jossa ainoat hyväksyttävät level typet
  int itsGridInfoPrintCount;
  vector<ParamChangeItem> itsParamChangeTable;
  bool fCropParamsNotMensionedInTable;
  bool fDoAtlanticFix;  // 0->360 global data ==> -180->180
  bool fDoPacificFix;   // -180->180 global data ==> 0->360
  bool fDoYAxisFlip;
  bool fTryAreaCombination;  // tietyt datat koostuvat palasista, kokeillaan jos saa palasista saa
                             // yhdistettyä isompia kokonaisuuksia
  bool fDoZigzagMode;  // data luetaan rivi kerrallaan eri suuntiin, ensin vasemmalta oikealle ja
                       // sitten oikealta vasemmalle, sitten taas vasemmalta oikealle jne.
  bool fDoLeftRightSwap;
  bool fIgnoreReducedLLData;  // joskus kannattaa vain purkaa gribit käyttäen grib_api:a ja ohittaa
                              // ns. reduced_ll datat,
  // koska niitä on niin vähän tai datoja ei voi purkaa wgrib:illä. Esim. Tyynenmeren ecmwf-data,
  // siinä on pieni osa reduced_ll dataa ja muista tulee moskaa wgrib:illä.
  bool fUseLevelTypeFileNaming;  // n optiolla voidaan laittaa output tiedostojen nimen perään esim.
                                 // _levelType_100
  NFmiProducer itsWantedSurfaceProducer;
  NFmiProducer itsWantedPressureProducer;
  NFmiProducer itsWantedHybridProducer;
  GeneratedHybridParamInfo itsHybridPressureInfo;
  GeneratedHybridParamInfo itsHybridRelativeHumidityInfo;
  GeneratedHybridParamInfo itsGroundPressureInfo;
  NFmiRect itsLatlonCropRect;
  GridSettingsPackage itsGridSettings;
  bool fVerbose;
  string itsInputFileNameStr;
  FILE *itsInputFile;
  // Joissain datoissa on sama parametri (yleensä kumulatiivinen) kahdesti, mutta eri aikajaksoilla.
  // Esim. NCEP:in NAM mallissa on APCP -parametri kahdesti, joista toinen on 3h sademäärä (3h
  // resoluutio datssa).
  // Toinen APCP on härämä 12h jaksotettu: 1. puuttuvaa, 2. 6h kertymä, 3. 9h kertymä ja 4. 12h
  // kertymä ja sitten taas alusta.
  // Nyt halutaan siis valita toinen näistä dataan. Optio, joka annetaan gribtoqd -filtterille:
  // -R haluttuJakso:parid1[,parid2,...]
  // Esim. NAM tapaus:
  // -R 3:354
  std::set<unsigned long> itsStepRangeCheckedParams;  // Tähä on listattu kaikki ne parametrit,
                                                      // joista halutaan tehdä valinta (jos niistä
                                                      // on kakksi eri jaksoista parametria datassa)
  int itsWantedStepRange;  // Jos tämä on 3, valitaan NAM:in tapauksessa se 3h-sade, jos tämä on -3,
                           // valitaan se toinen (hidden feature).
};

// Poistin TotalQDataCollector -luokan, koska ainakaan grib_api ei tue multi-threaddausta näihin
// aikoihin ja se sekoitti koodia

struct LevelLessThan
{
  bool operator()(const NFmiLevel &l1, const NFmiLevel &l2) const
  {
    if (l1.LevelType() < l2.LevelType())
      return true;
    else if (l1.LevelType() == l2.LevelType())
    {
      if (l1.LevelValue() < l2.LevelValue())
        return true;
    }
    return false;
  }
};

namespace
{
vector<std::shared_ptr<NFmiQueryData> > gTotalQDataCollector;
}

const unsigned long gMissLevelValue = 9999999;  // ignore levels with this value

/*
Kari niemelän sähköpostista suhteellisen kosteuden laskusta ominaiskosteuden avulla:

Suhteellisen kosteuden laskemiseen tarvitaan paine ja lämpötila
ominaiskosteuden lisäksi.

Hilakkeessa asia tehdään seuraavasti:
-- lasketaan vesihöyryn kyllästysosapaine kaavalla (T on celsiuksia, ^
on potenssi)
es = 6.107 * 10 ^ (7.5 * T / (237.0 + T)) , jos lämpimämpää kuin -5 C
eli veden suhteen
es = 6.107 * 10 ^ (9.5 * T / (265.5 + T)) , jos kylmempää kuin -5 C eli
jään suhteen
-- lasketaan RH kaavalla
RH = (P * Q / 0.622 / ES) * (P - ES) / (P - Q * P / 0.622)
RH saadaan 0...1, mutta on vielä tarkistettava ettei mene alle nollan
tai yli yhden
*/

// Lasketaan vesihöyryn osapaine ES veden suhteen.
// Oletus T ei ole puuttuvaa ja on celsiuksina.
float CalcESWater(float T)
{
  float es = 6.107f * pow(10.f, (7.5f * T / (237.0f + T)));
  return es;
}

// Lasketaan vesihöyryn osapaine ES jään suhteen.
// Oletus T ei ole puuttuvaa ja on celsiuksina.
float CalcESIce(float T)
{
  float es = 6.107f * pow(10.f, (9.5f * T / (265.5f + T)));
  return es;
}

// P on hPa, T on celsiuksia ja Q (specific humidity) on kg/kg
float CalcRH(float P, float T, float Q)
{
  if (P == kFloatMissing || T == kFloatMissing || Q == kFloatMissing)
    return kFloatMissing;
  else
  {
    float ES = (T >= -5) ? CalcESWater(T) : CalcESIce(T);
    float RH = (P * Q / 0.622f / ES) * (P - ES) / (P - Q * P / 0.622f);
    if (RH > 1.f)
      RH = 1.f;
    if (RH < 0.f)
      RH = 0.f;
    return RH * 100;
  }
}

bool GetGribLongValue(grib_handle *theHandle,
                      const std::string &theDefinitionName,
                      long &theLongValueOut)
{
  return grib_get_long(theHandle, theDefinitionName.c_str(), &theLongValueOut) == 0;
}

bool GetGribDoubleValue(grib_handle *theHandle,
                        const std::string &theDefinitionName,
                        double &theDoubleValueOut)
{
  return grib_get_double(theHandle, theDefinitionName.c_str(), &theDoubleValueOut) == 0;
}

string GetFileNameAreaStr(const NFmiArea *theArea)
{
  string str("_area_");
  str += theArea->ProjStr();
  str += "_bbox_";
  str += std::to_string(theArea->WorldRect().Left());
  str += "_";
  str += std::to_string(theArea->WorldRect().Bottom());
  str += "_";
  str += std::to_string(theArea->WorldRect().Right());
  str += "_";
  str += std::to_string(theArea->WorldRect().Top());

  boost::algorithm::replace_all(str, "+", "");
  boost::algorithm::replace_all(str, " ", "_");
  boost::algorithm::replace_all(str, ".", "_");

  return str;
}

bool GetGribStringValue(grib_handle *theHandle,
                        const std::string &theDefinitionName,
                        std::string &theStringValueOut)
{
  static const std::string unacceptableStringValue =
      "unknown";  // tämä arvo tulee jos kyseistä definitiota ei ole määrätty gribissä kun tekee
                  // grib_get_string -kutsun
  char stringValue[128] = "";
  size_t stringValueBufferSize = sizeof(stringValue);
  int getFailed =
      grib_get_string(theHandle, theDefinitionName.c_str(), stringValue, &stringValueBufferSize);
  if (!getFailed)
  {
    std::string testParamName = stringValue;
    if (testParamName != unacceptableStringValue)
    {
      theStringValueOut = testParamName;
      return true;
    }
  }
  return false;
}

NFmiMetTime GetTime(grib_handle *theHandle, const std::string &dateStr, const std::string &timeStr)
{
  long dataDate = 0;
  long dataTime = 0;

  int err1 = grib_get_long(theHandle, dateStr.c_str(), &dataDate);
  int err2 = grib_get_long(theHandle, timeStr.c_str(), &dataTime);

  if (err1)
    throw runtime_error("Could not extract dataDate from GRIB");
  if (err2)
    throw runtime_error("Could not extract dataTime from GRIB");

  short year = static_cast<short>(dataDate / 10000);
  short month = static_cast<short>((dataDate / 100) % 100);
  short day = static_cast<short>(dataDate % 100);
  short hour = static_cast<short>((dataTime / 100) % 100);
  short min = static_cast<short>(dataTime % 100);

  NFmiMetTime aTime(year, month, day, hour, min, 0, 1);
  NFmiMetTime checkTime;
  if (abs(checkTime.GetYear() - year) > 1000 || month > 12 || day > 31 ||
      hour > 24)  // jos saadussa päiväyksessä on yli 1000 vuoden heitto nykyhetkeen, oletetaan että
                  // ajassa on jotain vikaa (WAFS datassa oli dataa vuodelta 12875)
  {
    string dateStr("YYYY=");
    dateStr += NFmiStringTools::Convert(year);
    dateStr += " MM=";
    dateStr += NFmiStringTools::Convert(month);
    dateStr += " DD=";
    dateStr += NFmiStringTools::Convert(day);
    dateStr += " HH=";
    dateStr += NFmiStringTools::Convert(hour);
    throw runtime_error(
        string(
            "Error in GetTime-function: Current date value was strange and probably errourness: ") +
        dateStr);
  }

  return aTime;
}

NFmiMetTime GetOrigTime(grib_handle *theHandle)
{
  return GetTime(theHandle, "dataDate", "dataTime");
}

NFmiMetTime GetValidTime(grib_handle *theHandle)
{
  return GetTime(theHandle, "validityDate", "validityTime");
}

long GetUsedLevelType(grib_handle *theHandle)
{
  string name;
  bool ok = GetGribStringValue(theHandle, "vertical.typeOfLevel", name);

  if (!ok)
  {
    bool ok = GetGribStringValue(theHandle, "typeOfFirstFixedSurface", name);
    if (!ok)
    {
      return kFmiNoLevelType;
    }
    else
    {
      try
      {
        return NFmiStringTools::Convert<long>(name);
      }
      catch (...)
      {
        return kFmiNoLevelType;
      }
    }
  }

  // See: http://www.ecmwf.int/publications/manuals/d/gribapi/fm92/grib1/detail/ctables/3/

  if (name == "surface")
    return kFmiGroundSurface;
  if (name == "cloudBase")
    return 2;
  if (name == "cloudTop")
    return 3;
  if (name == "isothermZero")
    return 4;
  if (name == "adiabaticCondensation")
    return 5;
  if (name == "maxWind")
    return 6;
  if (name == "tropopause")
    return 7;
  if (name == "nominalTop")
    return 8;
  if (name == "seaBottom")
    return 9;
  if (name == "mostUnstableParcel")
    return 17;
  if (name == "isothermal")
    return 20;
  if (name == "isobaricInhPa")
    return kFmiPressureLevel;
  if (name == "isobaricInPa")
    return kFmiPressureLevel;
  if (name == "isobaricLayer")
    return 101;
  if (name == "meanSea")
    return kFmiMeanSeaLevel;
  if (name == "heightAboveSea")
    return kFmiAltitude;
  if (name == "heightAboveSeaLayer")
    return 104;
  if (name == "heightAboveGround")
    return kFmiHeight;
  if (name == "heightAboveGroundLayer")
    return 106;
  if (name == "sigma")
    return 107;
  if (name == "sigmaLayer")
    return 108;
  if (name == "hybrid")
    return kFmiHybridLevel;
  if (name == "hybridLayer")
    return 110;
  if (name == "depthBelowLand")
    return 111;
  if (name == "depthBelowLandLayer")
    return 112;
  if (name == "theta")
    return 113;
  if (name == "thetaLayer")
    return 114;
  if (name == "pressureFromGround")
    return 115;
  if (name == "pressureFromGroundLayer")
    return 116;
  if (name == "potentialVorticity")
    return 117;
  if (name == "eta")
    return 119;
  if (name == "depthBelowSea")
    return 160;
  if (name == "entireAtmosphere" || name == "atmosphere" || name == "atmosphereSingleLayer")
    return 200;
  if (name == "entireOcean")
    return 201;

  return kFmiNoLevelType;
}

NFmiLevel GetLevel(grib_handle *theHandle)
{
  long usedLevelType = GetUsedLevelType(theHandle);

  long levelValue = 0;
  bool levelValueOk = GetGribLongValue(theHandle, "vertical.level", levelValue);

  if (!levelValueOk)
    cerr << "Warning: Couldn't get level from given grib_handle, assuming value zero\n.";

  // Note: a missing value is encoded as a 16-bit -1, which is converted to max int by
  // grib_get_long. Bizarre API, if there is no better way to test for missing values

  if (levelValue == std::numeric_limits<int>::max())
    throw runtime_error("MISSING level value in data");

  return NFmiLevel(
      usedLevelType, NFmiStringTools::Convert(levelValue), static_cast<float>(levelValue));
}

long GetMissingValue(grib_handle *theHandle)
{
  // DUMP(theHandle);
  // missingValue is not in edition independent docs

  long missingValue = 0;
  int status = grib_get_long(theHandle, "missingValue", &missingValue);

  if (status == 0)
    return missingValue;

  throw runtime_error("Couldn't get missingValue from given grib_handle.");
}

NFmiRect GetLatlonCropRect(const string &theBoundsStr)
{
  vector<string> boundStrList = NFmiStringTools::Split(theBoundsStr, ",");
  vector<double> coords = NFmiStringTools::Split<vector<double> >(theBoundsStr);
  if (coords.size() != 4)
    throw runtime_error(
        "-G option must have exactly 4 comma separated numbers (like -G lat1,lon1,lat2,lon2)");

  const double lon1 = coords[0];
  const double lat1 = coords[1];
  const double lon2 = coords[2];
  const double lat2 = coords[3];

  const NFmiPoint bottomleft(lon1, lat1);
  const NFmiPoint topright(lon2, lat2);
  return NFmiRect(bottomleft, topright);
}

const NFmiPoint gMissingGridSize(kFloatMissing, kFloatMissing);

std::shared_ptr<NFmiGrid> GetGridFromProjectionStr(string &theProjectionStr,
                                                   int gridSizeX = -1,
                                                   int gridSizeY = -1)
{
  vector<string> projectionPartsStr = NFmiStringTools::Split(theProjectionStr, ":");
  if (projectionPartsStr.size() < 2)
    throw runtime_error(
        std::string("Unable to create area and grid from given projection string: ") +
        theProjectionStr);

  string areaStr;
  string gridStr;
  if (projectionPartsStr.size() >= 2)
    areaStr += projectionPartsStr[0] + ":" + projectionPartsStr[1];
  if (gridSizeX > 0 && gridSizeY > 0)
  {
    gridStr = NFmiStringTools::Convert(gridSizeX);
    gridStr += ",";
    gridStr += NFmiStringTools::Convert(gridSizeY);
  }
  else if (projectionPartsStr.size() >= 3)
    gridStr += projectionPartsStr[2];
  else
  {
    string areaStr;
    string gridStr;
    if (projectionPartsStr.size() >= 2)
      areaStr += projectionPartsStr[0] + ":" + projectionPartsStr[1];
    if (gridSizeX > 0 && gridSizeY > 0)
    {
      gridStr = NFmiStringTools::Convert(gridSizeX);
      gridStr += ",";
      gridStr += NFmiStringTools::Convert(gridSizeY);
    }
    else if (projectionPartsStr.size() >= 3)
      gridStr += projectionPartsStr[2];
    else
    {
      gridStr +=
          "50,50";  // jos oli kaksi osainen projektio stringi, laitetaan oletus kooksi 50 x 50
      cerr << "Warning, using default grid-size for wanted projection (50 x 50)" << endl;
    }

    std::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(areaStr);
    std::vector<double> values = NFmiStringTools::Split<std::vector<double> >(gridStr, ",");
    if (values.size() != 2)
      throw runtime_error("Given GridSize was invlid, has to be two numbers (e.g. x,y).");
    NFmiPoint gridSize(values[0], values[1]);
    std::shared_ptr<NFmiGrid> grid(new NFmiGrid(area->Clone(),
                                                static_cast<unsigned int>(gridSize.X()),
                                                static_cast<unsigned int>(gridSize.Y())));
    return grid;
  }

  std::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(areaStr);

  std::vector<double> values = NFmiStringTools::Split<std::vector<double> >(gridStr, ",");
  if (values.size() != 2)
    throw runtime_error("Given GridSize was invlid, has to be two numbers (e.g. x,y).");

  NFmiPoint gridSize(values[0], values[1]);
  std::shared_ptr<NFmiGrid> grid(new NFmiGrid(area->Clone(),
                                              static_cast<unsigned int>(gridSize.X()),
                                              static_cast<unsigned int>(gridSize.Y())));
  return grid;
}

std::shared_ptr<NFmiGrid> CreateDeepGribCopy(std::shared_ptr<NFmiGrid> &theGrid)
{
  if (!theGrid)
    return {};
  return std::make_shared<NFmiGrid>(*theGrid);
}

void HandleSeparateProjections(vector<string> &theSeparateProjectionsStr,
                               GridSettingsPackage &theGridSettings)
{
  if (theSeparateProjectionsStr.size() == 1)
  {
    theGridSettings.itsBaseGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = CreateDeepGribCopy(theGridSettings.itsBaseGrid);
    theGridSettings.itsHybridGrid = CreateDeepGribCopy(theGridSettings.itsBaseGrid);
  }
  else if (theSeparateProjectionsStr.size() == 2)
  {
    theGridSettings.itsBaseGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[1]);
    theGridSettings.itsHybridGrid = CreateDeepGribCopy(theGridSettings.itsPressureGrid);
  }
  if (theSeparateProjectionsStr.size() >= 3)
  {
    theGridSettings.itsBaseGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[1]);
    theGridSettings.itsHybridGrid = GetGridFromProjectionStr(theSeparateProjectionsStr[2]);
  }
}

// Funktio tekee annetusta projektio stringistä halutun projektio gridin. Lisäksi siinä tarkistetaan
// onko annettu erilaisille datoille hilakokoja. Jos ei ole, kaikille datoille tulee koko 50x50.
// Jos löytyy yksi koko, laitetaan kaikille datoille halutuksi hila kooksi se. Jos annettu toinenkin
// hila koko, annetaan se koko painepinta ja mallipinta datoilla ja kaikille muillekin datoille.
//
// Projektio stringi on muotoa: stereographic,20,90,60:6,51.3,49,70.2:82,91
// Jossa toisen kaksoispisteen jälkeen tulee hilakoot. Niitä voi siis olla pilkuilla eroteltuina 3
// paria.

void HandleProjectionString(const string &theProjStr, GridSettingsPackage &theGridSettings)
{
  vector<string> separateProjectionsStr = NFmiStringTools::Split(theProjStr, ";");
  if (separateProjectionsStr.size() >= 2)
  {
    HandleSeparateProjections(separateProjectionsStr, theGridSettings);
  }
  else
  {
    vector<string> projParts = NFmiStringTools::Split(theProjStr, ":");
    if (projParts.size() < 2)
      throw std::runtime_error("Error with -P option, the projection string was incomplete.");
    string areaStr = projParts[0] + ":" + projParts[1];
    string gridSizeStr = "50,50";  // tämä on defaultti hila koko jos sitä ei ole annettu
    if (projParts.size() >= 3)
      gridSizeStr = projParts[2];
    vector<int> gridSizes = NFmiStringTools::Split<vector<int> >(gridSizeStr, ",");
    if (gridSizes.size() < 2)
      throw std::runtime_error(
          "Error with -P option, the projection string with incomplete grid size given.");

    theGridSettings.itsBaseGrid = GetGridFromProjectionStr(areaStr, gridSizes[0], gridSizes[1]);
    int indexOffset = 0;
    if (gridSizes.size() >= 4)
      indexOffset = 2;
    theGridSettings.itsPressureGrid =
        GetGridFromProjectionStr(areaStr, gridSizes[0 + indexOffset], gridSizes[1 + indexOffset]);

    if (gridSizes.size() >= 6)
      indexOffset = 4;
    theGridSettings.itsHybridGrid =
        GetGridFromProjectionStr(areaStr, gridSizes[0 + indexOffset], gridSizes[1 + indexOffset]);
  }
}

bool IsDifferentGridFileNamesUsed(vector<std::shared_ptr<NFmiQueryData> > &theDatas)
{
  size_t ssize = theDatas.size();

  if (ssize > 1)
  {
    std::set<FmiLevelType> levels;
    for (size_t i = 0; i < ssize; i++)
      levels.insert(theDatas[i]->Info()->Level()->LevelType());

    if (levels.size() != ssize)
      return true;
  }
  return false;
}

bool GetIgnoreLevelList(NFmiCmdLine &theCmdLine, NFmiLevelBag &theIgnoredLevelListOut)
{
  if (theCmdLine.isOption('l'))
  {
    string ignoredLevelsStr = theCmdLine.OptionValue('l');
    vector<string> ignoredLevelsStrVector = NFmiStringTools::Split(ignoredLevelsStr);
    for (unsigned int i = 0; i < ignoredLevelsStrVector.size(); i++)
    {  // yksittäiset stringit siis muotoa t105v3 jne. eli leveltyyppi 105 ja sen arvo 3
      vector<string> levelStrVec =
          NFmiStringTools::Split(NFmiStringTools::LowerCase(ignoredLevelsStrVector[i]), "v");
      if (levelStrVec.size() == 2 && levelStrVec[0].at(0) == 't')
      {
        string levelTypeStr(levelStrVec[0].begin() + 1, levelStrVec[0].end());
        unsigned long levelType = Fmi::stoul(levelTypeStr);
        float levelValue = gMissLevelValue;
        if (levelStrVec[1] != "*")
          levelValue = Fmi::stof(levelStrVec[1]);
        theIgnoredLevelListOut.AddLevel(NFmiLevel(levelType, levelStrVec[1], levelValue));
      }
      else
      {
        cerr << "Error with l-option, check the syntax, it should be like: -l "
                "t109v3[,t105v255,...]. Now it was: "
             << ignoredLevelsStr << endl;
        return false;
      }
    }
  }
  return true;
}

bool GribDefinitionPath(NFmiCmdLine &theCmdLine)
{
  if (theCmdLine.isOption('D'))
  {
    string definitionsPath = theCmdLine.OptionValue('D');
    string environmentSettingStr = "GRIB_DEFINITION_PATH=";
    environmentSettingStr += definitionsPath;
    return putenv(const_cast<char *>(environmentSettingStr.c_str())) == 0;
  }
  return true;
}

vector<FmiLevelType> GetAcceptedLevelTypes(NFmiCmdLine &theCmdLine)
{
  vector<FmiLevelType> acceptOnlyLevelTypes;
  if (theCmdLine.isOption('L'))
  {
    vector<string> acceptOnlyLevelTypesStrVector =
        NFmiStringTools::Split(theCmdLine.OptionValue('L'));
    for (unsigned int i = 0; i < acceptOnlyLevelTypesStrVector.size(); i++)
    {
      unsigned long levelType = Fmi::stoul(acceptOnlyLevelTypesStrVector[i]);
      acceptOnlyLevelTypes.push_back(static_cast<FmiLevelType>(
          levelType));  // stringtools-convert ei osaa heti tehdä FmiLevelType-tyyppiä
    }
  }
  return acceptOnlyLevelTypes;
}

GeneratedHybridParamInfo GetGeneratedHybridParamInfo(NFmiCmdLine &theCmdLine,
                                                     char theOptionLetter,
                                                     unsigned long theDefaultParamId,
                                                     const std::string &theDefaultParamName)
{
  GeneratedHybridParamInfo hybridParamInfo;
  if (theCmdLine.isOption(theOptionLetter))
  {
    vector<string> hybridInfoStrings =
        NFmiStringTools::Split(theCmdLine.OptionValue(theOptionLetter));
    if (hybridInfoStrings.size() >= 1)
      hybridParamInfo.itsHelpParamId =
          static_cast<FmiParameterName>(Fmi::stoi(hybridInfoStrings[0]));
    unsigned long hybridPressureId = theDefaultParamId;
    if (hybridInfoStrings.size() >= 2)
      hybridPressureId = Fmi::stoul(hybridInfoStrings[1]);
    string hybridPressureName = theDefaultParamName;
    if (hybridInfoStrings.size() >= 3)
      hybridPressureName = hybridInfoStrings[2];

    hybridParamInfo.itsGeneratedHybridParam = NFmiParam(hybridPressureId,
                                                        hybridPressureName,
                                                        kFloatMissing,
                                                        kFloatMissing,
                                                        kFloatMissing,
                                                        kFloatMissing,
                                                        "%.1f",
                                                        kLinearly);
    hybridParamInfo.fCalcHybridParam = true;
  }
  return hybridParamInfo;
}

long GetLevelType(std::shared_ptr<NFmiQueryData> &theQData)
{
  if (!theQData)
    return -1;

  theQData->Info()->FirstLevel();
  return theQData->Info()->Level()->LevelType();
}

void StoreQueryDatas(GribFilterOptions &theGribFilterOptions)
{
  int returnStatus = 0;  // 0 = ok
  if (theGribFilterOptions.itsGeneratedDatas.empty())
  {
    cerr << "Could not create output data." << endl;
    returnStatus = 1;
  }
  else
  {
    bool useDifferentFileNamesOnDifferentGrids =
        IsDifferentGridFileNamesUsed(theGribFilterOptions.itsGeneratedDatas);

    int ssize = static_cast<int>(theGribFilterOptions.itsGeneratedDatas.size());
    for (int i = 0; i < ssize; i++)
    {
      NFmiStreamQueryData streamData;
      if (theGribFilterOptions.fUseOutputFile)
      {
        string usedFileName(theGribFilterOptions.itsOutputFileName);
        if (theGribFilterOptions.fUseLevelTypeFileNaming)
        {
          usedFileName += "_levelType_";
          usedFileName +=
              NFmiStringTools::Convert(GetLevelType(theGribFilterOptions.itsGeneratedDatas[i]));
          // TÄMÄ PITÄÄ vielä korjata, jos saman level tyypillä on erilaisia hila/area määrityksiä,
          // pitää ne nimetä!!!
          if (useDifferentFileNamesOnDifferentGrids)
          {
            string areaStr =
                GetFileNameAreaStr(theGribFilterOptions.itsGeneratedDatas[i]->Info()->Area());
            usedFileName += areaStr;
          }
        }
        else
        {
          if (i > 0)
          {
            usedFileName += "_";
            usedFileName += NFmiStringTools::Convert(i);
          }
        }
        if (!streamData.WriteData(usedFileName, theGribFilterOptions.itsGeneratedDatas[i].get()))
        {
          cerr << "could not open qd-file to write: " << theGribFilterOptions.itsOutputFileName
               << endl;
          returnStatus |= 0;
        }
      }
      else if (ssize > 1)  // jos qdatoja syntyy enemmän kuin 1, pitää antaa output-tiedoston nimi,
                           // että muut tiedostonimet voidaan generoida
      {
        cerr << "GRIB will generate several sqd-files." << endl;
        cerr << "Use the -o option to give one name" << endl;
        cerr << "the others will be generated automatically." << endl;
        returnStatus = 1;
      }
      else
      {
        if (!streamData.WriteCout(theGribFilterOptions.itsGeneratedDatas[i].get()))
        {
          cerr << "could not open qd-file to stdout" << endl;
          returnStatus = 1;
        }
      }
    }
  }
  theGribFilterOptions.itsReturnStatus = returnStatus;
}

bool GetGridOptions(NFmiCmdLine &theCmdLine, GridSettingsPackage &theGridSettings)
{
  if (theCmdLine.isOption('P'))
  {
    string opt_wantedGrid = theCmdLine.OptionValue('P');
    try
    {
      HandleProjectionString(opt_wantedGrid, theGridSettings);
    }
    catch (std::exception &e)
    {
      cerr << e.what() << endl;
      return false;
    }
  }
  return true;
}

std::string GetDirectory(const std::string &theFileFilter)
{
  NFmiFileString fileStr(theFileFilter);
  NFmiString str;
  str += fileStr.Device();
  str += fileStr.Path();

  return static_cast<char *>(str);
}

std::vector<std::string> MakeFullPathFileList(list<string> &theFileNameList,
                                              const std::string &theDirectory)
{
  vector<string> fullFileNameVector;
  for (list<string>::iterator it = theFileNameList.begin(); it != theFileNameList.end(); ++it)
  {
    std::string fullFileName = theDirectory;
    if (fullFileName.size() && fullFileName[fullFileName.size() - 1] != kFmiDirectorySeparator)
      fullFileName += kFmiDirectorySeparator;
    fullFileName += *it;
    fullFileNameVector.push_back(fullFileName);
  }
  return fullFileNameVector;
}

vector<string> GetDataFiles(const NFmiCmdLine &cmdline)
{
  vector<string> all_files;

  for (int i = 1; i <= cmdline.NumberofParameters(); i++)
  {
    auto path = cmdline.Parameter(i);

    if (NFmiFileSystem::DirectoryExists(path))
    {
      auto files = NFmiFileSystem::DirectoryFiles(path);
      auto files2 = MakeFullPathFileList(files, path);
      copy(files2.begin(), files2.end(), back_inserter(all_files));
    }
    else if (NFmiFileSystem::FileExists(path))
    {
      all_files.push_back(path);
    }
    else
    {
      auto files = NFmiFileSystem::PatternFiles(path);
      auto directory = GetDirectory(path);
      auto files2 = MakeFullPathFileList(files, directory);
      copy(files2.begin(), files2.end(), back_inserter(all_files));
    }
  }

  return all_files;
}

class CombineDataStructureSearcher
{
 public:
  CombineDataStructureSearcher(void)
      : itsGridCounts(),
        itsValidTimes(),
        itsOrigTimes(),
        itsLevels(),
        itsParams(),
        itsWantedProducers()
  {
  }

  void AddGrid(const NFmiHPlaceDescriptor &theHPlaceDesc)
  {
    if (theHPlaceDesc.Grid())
    {
      MyGrid tmpGrid(*(theHPlaceDesc.Grid()));
      pair<map<MyGrid, int>::iterator, bool> it =
          itsGridCounts.insert(make_pair(tmpGrid, 1));  // laitetaan hila ja counter arvo 1  mappiin
      if (it.second == false)
        (*it.first)
            .second++;  // jos kyseinen hila oli jo mapissa, kasvatetaan counterin arvo yhdellä
    }
  }

  void AddTimes(const NFmiTimeDescriptor &theTimeDesc)
  {
    NFmiTimeDescriptor &timeDesc = const_cast<NFmiTimeDescriptor &>(
        theTimeDesc);  // rumaa mutta pitää kikkailla const:ien kanssa
    for (timeDesc.Reset(); timeDesc.Next();)
      itsValidTimes.insert(timeDesc.Time());
    itsOrigTimes.insert(timeDesc.OriginTime());
  }

  void AddLevels(const NFmiVPlaceDescriptor &theVPlaceDesc)
  {
    NFmiVPlaceDescriptor &vplaceDesc = const_cast<NFmiVPlaceDescriptor &>(
        theVPlaceDesc);  // rumaa mutta pitää kikkailla const:ien kanssa
    for (vplaceDesc.Reset(); vplaceDesc.Next();)
      itsLevels.insert(*(vplaceDesc.Level()));
  }

  void AddParams(const NFmiParamDescriptor &theParamDesc)
  {
    NFmiParamDescriptor &paramDesc = const_cast<NFmiParamDescriptor &>(
        theParamDesc);  // rumaa mutta pitää kikkailla const:ien kanssa
    for (paramDesc.Reset(); paramDesc.Next();)
    {
      itsParams.insert(*(paramDesc.Param().GetParam()));
      itsWantedProducers.insert(*(paramDesc.Param().GetProducer()));
    }
  }

  NFmiHPlaceDescriptor GetGrid(void)
  {
    int maxCount = -1;
    MyGrid mostPopularGrid;  // tähä etsitään 'suosituin' hila, ja sitä käytetään datan pohjana
    for (map<MyGrid, int>::iterator it = itsGridCounts.begin(); it != itsGridCounts.end(); ++it)
    {
      if ((*it).second > maxCount)
      {
        mostPopularGrid = (*it).first;
        maxCount = (*it).second;
      }
    }
    return NFmiHPlaceDescriptor(
        NFmiGrid(mostPopularGrid.itsArea, mostPopularGrid.itsNX, mostPopularGrid.itsNY));
  }

  size_t GetGridCount(void) { return itsGridCounts.size(); }
  NFmiHPlaceDescriptor GetGrid(size_t index)
  {
    if (index >= itsGridCounts.size())
      throw runtime_error(
          "Error in program, in function GetGrid(index), index was too high value, stopping...");
    map<MyGrid, int>::iterator it = itsGridCounts.begin();
    std::advance(it, index);
    MyGrid tmpGrid = it->first;
    return NFmiHPlaceDescriptor(NFmiGrid(tmpGrid.itsArea, tmpGrid.itsNX, tmpGrid.itsNY));
  }

  NFmiTimeDescriptor GetTimes(void)
  {
    NFmiTimeList times;
    for (set<NFmiMetTime>::iterator it = itsValidTimes.begin(); it != itsValidTimes.end(); ++it)
    {
      times.Add(new NFmiMetTime(*it));
    }
    set<NFmiMetTime>::iterator origTimeIt =
        itsOrigTimes.begin();  // otetaan vain 1. origtime, en jaksa nyt miettiä mikä voisi olla
                               // sopivin, jos on useita vaihtoehtoja
    return NFmiTimeDescriptor(*origTimeIt, times);
  }

  NFmiVPlaceDescriptor GetLevels(void)
  {
    NFmiLevelBag levels;
    for (set<NFmiLevel>::iterator it = itsLevels.begin(); it != itsLevels.end(); ++it)
    {
      levels.AddLevel(*it);
    }
    return NFmiVPlaceDescriptor(levels);
  }

  NFmiParamDescriptor GetParams(void)
  {
    set<NFmiProducer>::iterator prodIt =
        itsWantedProducers.begin();  // otetaan vain 1. producer, en jaksa nyt miettiä mikä voisi
                                     // olla sopivin, jos on useita vaihtoehtoja
    NFmiParamBag params;
    for (set<NFmiParam>::iterator it = itsParams.begin(); it != itsParams.end(); ++it)
    {
      params.Add(NFmiDataIdent(*it, *prodIt));
    }
    return NFmiParamDescriptor(params);
  }

 private:
  map<MyGrid, int> itsGridCounts;  // tähän lasketaan tämän level tyypin eri hilat ja kuinka monta
                                   // kertaa kukin hila on esiintynyt datoissa
  set<NFmiMetTime> itsValidTimes;
  set<NFmiMetTime> itsOrigTimes;
  set<NFmiLevel> itsLevels;
  set<NFmiParam> itsParams;
  set<NFmiProducer> itsWantedProducers;
};

set<long> FindAllLevelTypes(vector<std::shared_ptr<NFmiQueryData> > &theTotalQDataCollector)
{
  set<long> levelTypes;
  for (size_t i = 0; i < theTotalQDataCollector.size(); i++)
  {
    levelTypes.insert(GetLevelType(theTotalQDataCollector[i]));
  }
  return levelTypes;
}

bool HasValidData(const NFmiDataMatrix<float> &theValues)
{
  for (size_t j = 0; j < theValues.NY(); j++)
    for (size_t i = 0; i < theValues.NX(); i++)
      if (theValues[i][j] != kFloatMissing)
        return true;
  return false;
}

bool FillQData(std::shared_ptr<NFmiQueryData> &theQData,
               vector<std::shared_ptr<NFmiQueryData> > &theTotalQDataCollector)
{
  if (!theQData)
    return false;

  bool filledAnyData = false;
  long levelType = GetLevelType(theQData);
  NFmiFastQueryInfo destInfo(theQData.get());

  for (size_t i = 0; i < theTotalQDataCollector.size(); i++)
  {
    if (levelType == GetLevelType(theTotalQDataCollector[i]))
    {
      NFmiFastQueryInfo sourceInfo(theTotalQDataCollector[i].get());
      if (destInfo.HPlaceDescriptor() == sourceInfo.HPlaceDescriptor())
        for (sourceInfo.ResetParam(); sourceInfo.NextParam();)
          if (destInfo.Param(static_cast<FmiParameterName>(sourceInfo.Param().GetParamIdent())))
            for (sourceInfo.ResetLevel(); sourceInfo.NextLevel();)
              if (destInfo.Level(*(sourceInfo.Level())))
                for (sourceInfo.ResetTime(); sourceInfo.NextTime();)
                  if (destInfo.Time(sourceInfo.Time()))
                  {
                    auto values = sourceInfo.Values();
                    if (HasValidData(values))
                    {
                      destInfo.SetValues(values);
                      filledAnyData = true;
                    }
                  }
    }
  }
  return filledAnyData;
}

struct ConnectionEdgeInfo
{
  // 1. grid1 ja grid2 pitää olla latlonAreoita
  // 2. grid1 ja grid2 yhdistävällä reunalla pitää olla yhtä paljon hilapisteitä
  // 3. grid1 ja grid2 yhdistävällä reunalla pitää olla samat kulmapisteet
  // 4. Datoilla pitää olla sama level tyyppi
  // 5. Alueilla pitää olla sama leveys longitudeissa
  // 6. Alueilla pitää olla sama korkeus latitudeissa

  ConnectionEdgeInfo(std::shared_ptr<NFmiQueryData> &data1, std::shared_ptr<NFmiQueryData> &data2)
      : connectionDirection(kNoDirection), levelType(kFmiNoLevelType)
  {
    NFmiFastQueryInfo finfo1(data1.get());
    NFmiFastQueryInfo finfo2(data2.get());

    if (finfo1.Grid() && finfo2.Grid() &&
        finfo1.Grid()->Area()->ClassId() == finfo2.Grid()->Area()->ClassId())
    {
      if (finfo1.Grid()->Area()->ClassId() != kNFmiLatLonArea)
        throw runtime_error(
            "Only latlon-area types are supported when combining data areas, stopping...");

      if (HasSameLatlonWidthAndHeigth(*finfo1.Grid()->Area(), *finfo2.Grid()->Area()))
      {
        if (finfo1.Level()->LevelType() == finfo2.Level()->LevelType())
        {
          connectionDirection = TryConnect(finfo1.Grid()->Area(), finfo2.Grid()->Area());
          if (connectionDirection != kNoDirection)
          {
            grid1 = *(finfo1.Grid());
            grid2 = *(finfo2.Grid());
            levelType = finfo1.Level()->LevelType();
          }
        }
      }
    }
  }

  bool HasConnection() const { return connectionDirection != kNoDirection; }
  FmiDirection TryConnect(const NFmiArea *area1, const NFmiArea *area2)
  {
    // area1:sta verrataan area2:een sivuttaissuunnassa
    if (area1->BottomRightLatLon() == area2->BottomLeftLatLon() &&
        area1->TopRightLatLon() == area2->TopLeftLatLon())
      return kRight;
    // area2:sta verrataan area1:een sivuttaissuunnassa
    if (area2->BottomRightLatLon() == area1->BottomLeftLatLon() &&
        area2->TopRightLatLon() == area1->TopLeftLatLon())
      return kLeft;
    // area1:sta verrataan area2:een pystysuunnassa
    if (area1->BottomLeftLatLon() == area2->TopLeftLatLon() &&
        area1->BottomRightLatLon() == area2->TopRightLatLon())
      return kDown;
    // area2:sta verrataan area1:een pystysuunnassa
    if (area2->BottomLeftLatLon() == area1->TopLeftLatLon() &&
        area2->BottomRightLatLon() == area1->TopRightLatLon())
      return kUp;

    return kNoDirection;
  }

  bool HasSameLatlonWidthAndHeigth(const NFmiArea &area1, const NFmiArea &area2)
  {
    double lonDiff1 = CalcLongitudeWidth(area1);
    double latDiff1 = CalcLatitudeWidth(area1);
    double lonDiff2 = CalcLongitudeWidth(area2);
    double latDiff2 = CalcLatitudeWidth(area2);
    if (lonDiff1 == lonDiff2 && latDiff1 == latDiff2)
    {
      longitudeDiff = lonDiff1;
      latitudeDiff = latDiff1;
      return true;
    }
    return false;
  }

  double CalcLongitudeWidth(const NFmiArea &area)
  {
    return area.TopRightLatLon().X() - area.BottomLeftLatLon().X();
  }

  double CalcLatitudeWidth(const NFmiArea &area)
  {
    return area.TopRightLatLon().Y() - area.BottomLeftLatLon().Y();
  }

  NFmiGrid grid1;
  NFmiGrid grid2;
  size_t data1Index;                 // 1. datan indeksi queryData vektorissa
  size_t data2Index;                 // 2. datan indeksi....
  FmiDirection connectionDirection;  // mihin suuntaan grid1 on suhteessa grid2:een, arvo on
                                     // kNoDirection jos ei ole yhteyttä
  FmiLevelType levelType;
  double longitudeDiff;
  double latitudeDiff;
};

bool IsConnectionEdgeIndexUsed(
    vector<pair<set<size_t>, set<size_t> > > &connectedDataIndexiesVector, size_t index)
{
  for (size_t i = 0; i < connectedDataIndexiesVector.size(); i++)
  {
    set<size_t>::iterator it = connectedDataIndexiesVector[i].first.find(index);
    if (it != connectedDataIndexiesVector[i].first.end())
      return true;
  }
  return false;
}

bool CalcConnectedDataIndexies(set<size_t> &connectionDataIndexies,
                               const ConnectionEdgeInfo &edgeInfo)
{
  set<size_t>::iterator it = connectionDataIndexies.find(edgeInfo.data1Index);
  if (it != connectionDataIndexies.end())
    return true;
  it = connectionDataIndexies.find(edgeInfo.data2Index);
  if (it != connectionDataIndexies.end())
    return true;

  return false;
}

void CheckForConnectingDataIndexies(vector<ConnectionEdgeInfo> &connectionEdgeInfoVector,
                                    set<size_t> &connectionDataIndexies,
                                    set<size_t> &connectionEdgeIndexies,
                                    size_t startingIndex)
{
  for (size_t i = startingIndex; i < connectionEdgeInfoVector.size(); i++)
  {
    if (CalcConnectedDataIndexies(connectionDataIndexies, connectionEdgeInfoVector[i]))
    {
      connectionDataIndexies.insert(connectionEdgeInfoVector[i].data1Index);
      connectionDataIndexies.insert(connectionEdgeInfoVector[i].data2Index);
      connectionEdgeIndexies.insert(i);
    }
  }
}

// Laskee vektorin jossa on parina yhteen kuuluvien datojen connectionEdgeInfoVector-indeksit
// setissä
// ja yhdistettävien datojen indeksit (origDataVector toisaalla ohjelmassa) toisessa setissä.
vector<pair<set<size_t>, set<size_t> > > CalcConnectedDataIndexies(
    vector<ConnectionEdgeInfo> &connectionEdgeInfoVector)
{
  vector<pair<set<size_t>, set<size_t> > > connectedDataIndexiesVector;
  for (size_t i = 0; i < connectionEdgeInfoVector.size(); i++)
  {
    if (IsConnectionEdgeIndexUsed(connectedDataIndexiesVector, i) == false)
    {
      set<size_t> connectionDataIndexies;
      connectionDataIndexies.insert(connectionEdgeInfoVector[i].data1Index);
      connectionDataIndexies.insert(connectionEdgeInfoVector[i].data2Index);
      set<size_t> connectionEdgeIndexies;
      connectionEdgeIndexies.insert(i);
      CheckForConnectingDataIndexies(
          connectionEdgeInfoVector, connectionDataIndexies, connectionEdgeIndexies, i + 1);
      // Tämä on viritys, jos connectionEdgeInfoVector:issa on data sopivassa järjestyksessä, jää
      // osa indekseistä
      // löytymättä ensimmäisellä ajolla, siksi ajetaan varmuuden vuoksi tämä funktio kahdesti (tämä
      // ei ole hidastava juttu).
      CheckForConnectingDataIndexies(
          connectionEdgeInfoVector, connectionDataIndexies, connectionEdgeIndexies, i + 1);
      if (connectionEdgeIndexies.size())
        connectedDataIndexiesVector.push_back(
            make_pair(connectionEdgeIndexies, connectionDataIndexies));
    }
  }
  return connectedDataIndexiesVector;
}

NFmiPoint CalcNewBottomLeftLatlon(const NFmiPoint &p1, const NFmiPoint &p2)
{
  double minLon = std::min(p1.X(), p2.X());
  double minLat = std::min(p1.Y(), p2.Y());
  return NFmiPoint(minLon, minLat);
}

NFmiPoint CalcNewTopRightLatlon(const NFmiPoint &p1, const NFmiPoint &p2)
{
  double maxLon = std::max(p1.X(), p2.X());
  double maxLat = std::max(p1.Y(), p2.Y());
  return NFmiPoint(maxLon, maxLat);
}

void CalcAreaConnection(const ConnectionEdgeInfo &connectionEdgeInfo,
                        NFmiPoint &bottomLeft,
                        NFmiPoint &topRight,
                        unsigned long &xSize,
                        unsigned long &ySize)
{
  if (connectionEdgeInfo.connectionDirection == kRight)
  {
    if (bottomLeft == NFmiPoint::gMissingLatlon)
    {  // aloitetaan alueen rakennus yhdistämällä liittyneet alueet
      bottomLeft = connectionEdgeInfo.grid1.Area()->BottomLeftLatLon();
      topRight = connectionEdgeInfo.grid2.Area()->TopRightLatLon();
      xSize = connectionEdgeInfo.grid1.XNumber() + connectionEdgeInfo.grid2.XNumber() -
              1;  // oletus, molemmissa alueissa on yhteinen reuna, siksi -1 yhteen lasketusta hila
                  // määrästä
      ySize = FmiMax(connectionEdgeInfo.grid1.YNumber(),
                     connectionEdgeInfo.grid2.YNumber());  // otetaan varmuuden vuoksi maksimi
                                                           // korkeus, pitäisi kait olla samoja
                                                           // molemmissa
    }
    else
    {  // laajennetaan jo olemassa olevaa aluetta
      if (bottomLeft.X() > connectionEdgeInfo.grid1.Area()->BottomLeftLatLon().X())
      {  // lisätään yhdistelmän leveyttä vasemmalle
        bottomLeft = CalcNewBottomLeftLatlon(bottomLeft,
                                             connectionEdgeInfo.grid1.Area()->BottomLeftLatLon());
        xSize += connectionEdgeInfo.grid1.XNumber() - 1;
      }
      else if (topRight.X() < connectionEdgeInfo.grid2.Area()->TopRightLatLon().X())
      {  // lisätään yhdistelmän leveyttä oikealle
        topRight =
            CalcNewTopRightLatlon(topRight, connectionEdgeInfo.grid2.Area()->TopRightLatLon());
        xSize += connectionEdgeInfo.grid2.XNumber() - 1;
      }
    }
  }
  else if (connectionEdgeInfo.connectionDirection == kLeft)
  {
    if (bottomLeft == NFmiPoint::gMissingLatlon)
    {  // aloitetaan alueen rakennus yhdistämällä liittyneet alueet
      bottomLeft = connectionEdgeInfo.grid2.Area()->BottomLeftLatLon();
      topRight = connectionEdgeInfo.grid1.Area()->TopRightLatLon();
      xSize = connectionEdgeInfo.grid1.XNumber() + connectionEdgeInfo.grid2.XNumber() -
              1;  // oletus, molemmissa alueissa on yhteinen reuna, siksi -1 yhteen lasketusta hila
                  // määrästä
      ySize = FmiMax(connectionEdgeInfo.grid1.YNumber(),
                     connectionEdgeInfo.grid2.YNumber());  // otetaan varmuuden vuoksi maksimi
                                                           // korkeus, pitäisi kait olla samoja
                                                           // molemmissa
    }
    else
    {  // laajennetaan jo olemassa olevaa aluetta
      if (bottomLeft.X() > connectionEdgeInfo.grid2.Area()->BottomLeftLatLon().X())
      {  // lisätään yhdistelmän leveyttä vasemmalle
        bottomLeft = CalcNewBottomLeftLatlon(bottomLeft,
                                             connectionEdgeInfo.grid2.Area()->BottomLeftLatLon());
        xSize += connectionEdgeInfo.grid2.XNumber() - 1;
      }
      else if (topRight.X() < connectionEdgeInfo.grid1.Area()->TopRightLatLon().X())
      {  // lisätään yhdistelmän leveyttä oikealle
        topRight =
            CalcNewTopRightLatlon(topRight, connectionEdgeInfo.grid1.Area()->TopRightLatLon());
        xSize += connectionEdgeInfo.grid1.XNumber() - 1;
      }
    }
  }
  else if (connectionEdgeInfo.connectionDirection == kUp)
  {
    if (bottomLeft == NFmiPoint::gMissingLatlon)
    {  // aloitetaan alueen rakennus yhdistämällä liittyneet alueet
      bottomLeft = connectionEdgeInfo.grid1.Area()->BottomLeftLatLon();
      topRight = connectionEdgeInfo.grid2.Area()->TopRightLatLon();
      xSize = FmiMax(connectionEdgeInfo.grid1.XNumber(),
                     connectionEdgeInfo.grid2.XNumber());  // otetaan varmuuden vuoksi maksimi
                                                           // korkeus, pitäisi kait olla samoja
                                                           // molemmissa
      ySize = connectionEdgeInfo.grid1.YNumber() + connectionEdgeInfo.grid2.YNumber() -
              1;  // oletus, molemmissa alueissa on yhteinen reuna, siksi -1 yhteen lasketusta hila
                  // määrästä
    }
    else
    {  // laajennetaan jo olemassa olevaa aluetta
      if (bottomLeft.Y() > connectionEdgeInfo.grid1.Area()->BottomLeftLatLon().Y())
      {  // lisätään yhdistelmän korkeutta alas
        bottomLeft = CalcNewBottomLeftLatlon(bottomLeft,
                                             connectionEdgeInfo.grid1.Area()->BottomLeftLatLon());
        ySize += connectionEdgeInfo.grid1.YNumber() - 1;
      }
      else if (topRight.Y() < connectionEdgeInfo.grid2.Area()->TopRightLatLon().Y())
      {  // lisätään yhdistelmän leveyttä oikealle
        topRight =
            CalcNewTopRightLatlon(topRight, connectionEdgeInfo.grid2.Area()->TopRightLatLon());
        ySize += connectionEdgeInfo.grid2.YNumber() - 1;
      }
    }
  }
  else if (connectionEdgeInfo.connectionDirection == kDown)
  {
    if (bottomLeft == NFmiPoint::gMissingLatlon)
    {  // aloitetaan alueen rakennus yhdistämällä liittyneet alueet
      bottomLeft = connectionEdgeInfo.grid2.Area()->BottomLeftLatLon();
      topRight = connectionEdgeInfo.grid1.Area()->TopRightLatLon();
      xSize = FmiMax(connectionEdgeInfo.grid1.XNumber(),
                     connectionEdgeInfo.grid2.XNumber());  // otetaan varmuuden vuoksi maksimi
                                                           // korkeus, pitäisi kait olla samoja
                                                           // molemmissa
      ySize = connectionEdgeInfo.grid1.YNumber() + connectionEdgeInfo.grid2.YNumber() -
              1;  // oletus, molemmissa alueissa on yhteinen reuna, siksi -1 yhteen lasketusta hila
                  // määrästä
    }
    else
    {  // laajennetaan jo olemassa olevaa aluetta
      if (bottomLeft.Y() > connectionEdgeInfo.grid2.Area()->BottomLeftLatLon().Y())
      {  // lisätään yhdistelmän korkeutta alas
        bottomLeft = CalcNewBottomLeftLatlon(bottomLeft,
                                             connectionEdgeInfo.grid2.Area()->BottomLeftLatLon());
        ySize += connectionEdgeInfo.grid2.YNumber() - 1;
      }
      else if (topRight.Y() < connectionEdgeInfo.grid1.Area()->TopRightLatLon().Y())
      {  // lisätään yhdistelmän leveyttä oikealle
        topRight =
            CalcNewTopRightLatlon(topRight, connectionEdgeInfo.grid1.Area()->TopRightLatLon());
        ySize += connectionEdgeInfo.grid1.YNumber() - 1;
      }
    }
  }
}

// HUOM! tämä yhdistely algoritmi toimii vain latlon tyyppisille areoille.
// Luultavasti toimii vain 1x2, 2x1, 2x2 alue yhdistelyille.
// Jos on enemmän hila osioita, onnistuminen on luultavasti tuurista kiinni. Pitäisi ehkä sortata
// connectionEdgeInfoVector jotenkin.
vector<pair<NFmiGrid, FmiLevelType> > CalcGrids2(
    vector<ConnectionEdgeInfo> &connectionEdgeInfoVector)
{
  // Tämä on CalcGrids versio 2.
  // 1. Katso kaikki yhteen kuuluvien connectionEdgejen data indeksit (siis indeksit
  // connectionEdgeInfoVector:iin, pair:in first on set niitä).
  vector<pair<set<size_t>, set<size_t> > > connectedEdgeDataIndexies =
      CalcConnectedDataIndexies(connectionEdgeInfoVector);
  // Käy sitten läpi kaikki ne datat ja laske kyseisten datojen latlon-areoiden nurkkapisteiden
  // avulla
  // uuden yhdistelmä hilan nurkkapisteet ja hilakoot.
  vector<pair<NFmiGrid, FmiLevelType> > gridVector;
  for (size_t i = 0; i < connectedEdgeDataIndexies.size(); i++)
  {
    NFmiPoint bottomLeft = NFmiPoint::gMissingLatlon;
    NFmiPoint topRight = NFmiPoint::gMissingLatlon;
    unsigned long xSize = 0;
    unsigned long ySize = 0;
    FmiLevelType leveltype = kFmiNoLevelType;
    set<size_t> connectedEdgeIndexies = connectedEdgeDataIndexies[i].first;
    for (set<size_t>::iterator it = connectedEdgeIndexies.begin();
         it != connectedEdgeIndexies.end();
         ++it)
    {
      ConnectionEdgeInfo &connectionEdgeInfo = connectionEdgeInfoVector[*it];
      if (leveltype == kFmiNoLevelType)
        leveltype = connectionEdgeInfo.levelType;
      ::CalcAreaConnection(connectionEdgeInfo, bottomLeft, topRight, xSize, ySize);
    }

    auto *area = NFmiAreaTools::CreateLegacyLatLonArea(bottomLeft, topRight);

    NFmiGrid grid(area, xSize, ySize);
    gridVector.push_back(make_pair(grid, leveltype));
  }
  return gridVector;
}

NFmiQueryInfo MakeQueryInfo(pair<NFmiGrid, FmiLevelType> &newGridInfo,
                            vector<std::shared_ptr<NFmiQueryData> > &origDataVector)
{
  NFmiParamDescriptor combinedParams;
  NFmiLevelBag combinedLevelBag;
  NFmiTimeList combinedTimeList;
  for (size_t i = 0; i < origDataVector.size(); i++)
  {
    NFmiFastQueryInfo info(origDataVector[i].get());
    if (newGridInfo.second == info.Level()->LevelType())
    {
      combinedParams = combinedParams.Combine(info.ParamDescriptor());
      combinedLevelBag = combinedLevelBag.Combine(info.VPlaceDescriptor().LevelBag());
      if (info.TimeDescriptor().ValidTimeList())  // HUOM! pitää olla timelist!!
        combinedTimeList = combinedTimeList.Combine(*info.TimeDescriptor().ValidTimeList());
    }
  }

  NFmiVPlaceDescriptor combinedLevels(combinedLevelBag);
  NFmiHPlaceDescriptor hplaceDesc(newGridInfo.first);
  combinedTimeList.First();
  NFmiTimeDescriptor combinedTimes(*combinedTimeList.Current(), combinedTimeList);
  NFmiQueryInfo innerInfo(combinedParams, combinedTimes, hplaceDesc, combinedLevels);
  return innerInfo;
}

void FillCombinedAreaData(std::shared_ptr<NFmiQueryData> &newData,
                          vector<std::shared_ptr<NFmiQueryData> > &origDataVector)
{
  NFmiFastQueryInfo info(newData.get());
  for (size_t i = 0; i < origDataVector.size(); i++)
  {
    info.First();  // tämä mm. asettaa osoittamaan 1. leveliin
    NFmiFastQueryInfo sourceInfo(origDataVector[i].get());
    if (info.Level()->LevelType() == sourceInfo.Level()->LevelType())
      for (info.ResetParam(); info.NextParam();)
        if (sourceInfo.Param(static_cast<FmiParameterName>(info.Param().GetParamIdent())))
          for (info.ResetLevel(); info.NextLevel();)
            if (sourceInfo.Level(*info.Level()))
              for (info.ResetTime(); info.NextTime();)
                if (sourceInfo.Time(info.Time()))
                  for (info.ResetLocation(); info.NextLocation();)
                    if (info.FloatValue() == kFloatMissing)
                      info.FloatValue(sourceInfo.InterpolatedValue(info.LatLon()));
  }
}

// muista että kaikki datat eivät välttämättä mene yhdistettyihin datoihin, miten ne säilytetään?
// => connectionEdgeInfoVector:in sisällön perusteella voidaan päätellä mitkä data indeksit eivät
// ole yhdistettynä mihinkään (data1Index ja data2Index)!!!!!
void AddNotCombinedDataToVector(vector<std::shared_ptr<NFmiQueryData> > &areaCombinedDataVector,
                                vector<std::shared_ptr<NFmiQueryData> > &origDataVector,
                                vector<ConnectionEdgeInfo> &connectionEdgeInfoVector)
{
  set<size_t> combinedDataIndexies;
  for (size_t i = 0; i < connectionEdgeInfoVector.size(); i++)
  {
    combinedDataIndexies.insert(connectionEdgeInfoVector[i].data1Index);
    combinedDataIndexies.insert(connectionEdgeInfoVector[i].data2Index);
  }

  set<size_t>::iterator it = combinedDataIndexies.begin();
  for (size_t i = 0; i < origDataVector.size(); i++)
  {
    if (it == combinedDataIndexies.end() || i < *it)
      areaCombinedDataVector.push_back(origDataVector[i]);
    else
      ++it;
  }
}

// ÄLÄ POISTA! Tämä funktio on jätetty mahdollisten alueellisiin yhdistelyihin
// liittyvien ongelmien etsimiseen.
#if 0
void PrintConnectionInfoVector_Debug(vector<std::shared_ptr<NFmiQueryData> > &origDataVector, vector<ConnectionEdgeInfo> &connectionEdgeInfoVector)
{
    cerr << "Original data vector areas:" << endl;
    for(size_t i = 0; i < origDataVector.size(); i++)
    {
        cerr << i << ": " << origDataVector[i]->Info()->Area()->AreaStr() << endl;
    }

    cerr << endl << "Connection info vector:" << endl;
    for(size_t i = 0; i < connectionEdgeInfoVector.size(); i++)
    {
        cerr << i << ": d1=" << connectionEdgeInfoVector[i].data1Index << " d2=" << connectionEdgeInfoVector[i].data2Index << endl;;
    }
}
#endif

vector<std::shared_ptr<NFmiQueryData> > TryAreaCombination(
    vector<std::shared_ptr<NFmiQueryData> > &origDataVector)
{
  vector<std::shared_ptr<NFmiQueryData> > areaCombinedDataVector;
  if (origDataVector.size() > 1)
  {
    vector<ConnectionEdgeInfo> connectionEdgeInfoVector;
    for (size_t i = 0; i < origDataVector.size() - 1; i++)
    {
      for (size_t j = i + 1; j < origDataVector.size(); j++)
      {
        ConnectionEdgeInfo connectionEdgeInfo(origDataVector[i], origDataVector[j]);
        if (connectionEdgeInfo.HasConnection())
        {
          connectionEdgeInfo.data1Index = i;
          connectionEdgeInfo.data2Index = j;
          connectionEdgeInfoVector.push_back(connectionEdgeInfo);
        }
      }
    }

    //        PrintConnectionInfoVector_Debug(origDataVector, connectionEdgeInfoVector);

    vector<pair<NFmiGrid, FmiLevelType> > gridVector = CalcGrids2(connectionEdgeInfoVector);
    if (gridVector.size())
    {
      for (size_t i = 0; i < gridVector.size(); i++)
      {
        NFmiQueryInfo innerInfo = MakeQueryInfo(gridVector[i], origDataVector);
        std::shared_ptr<NFmiQueryData> newData(NFmiQueryDataUtil::CreateEmptyData(innerInfo));
        if (newData)
        {
          FillCombinedAreaData(newData, origDataVector);
          areaCombinedDataVector.push_back(newData);
        }
      }
      // muista että kaikki datat eivät välttämättä mene yhdistettyihin datoihin, miten ne
      // säilytetään?
      // => connectionEdgeInfoVector:in sisällön perusteella voidaan päätellä mitkä data indeksit
      // eivät ole yhdistettynä mihinkään (data1Index ja data2Index)!!!!!
      if (areaCombinedDataVector.size())
        AddNotCombinedDataToVector(
            areaCombinedDataVector, origDataVector, connectionEdgeInfoVector);
    }
  }
  return areaCombinedDataVector;
}

vector<NFmiHPlaceDescriptor> GetUniqueHPlaceDescriptors(
    vector<std::shared_ptr<NFmiQueryData> > &theTotalQDataCollector)
{
  vector<NFmiHPlaceDescriptor> hplaceDescriptors;
  for (size_t i = 0; i < theTotalQDataCollector.size(); i++)
  {
    vector<NFmiHPlaceDescriptor>::iterator it =
        std::find(hplaceDescriptors.begin(),
                  hplaceDescriptors.end(),
                  theTotalQDataCollector[i]->Info()->HPlaceDescriptor());
    if (it == hplaceDescriptors.end())
      hplaceDescriptors.push_back(theTotalQDataCollector[i]->Info()->HPlaceDescriptor());
  }
  return hplaceDescriptors;
}

void InsertAllParams(set<NFmiParam> &params, NFmiFastQueryInfo &info)
{
  for (info.ResetParam(); info.NextParam();)
    params.insert(*info.Param().GetParam());
}

void InsertAllValidTimes(set<NFmiMetTime> &validTimes, NFmiFastQueryInfo &info)
{
  for (info.ResetTime(); info.NextTime();)
    validTimes.insert(info.Time());
}

void InsertAllLevels(set<float> &levels, NFmiFastQueryInfo &info)
{
  for (info.ResetLevel(); info.NextLevel();)
    levels.insert(info.Level()->LevelValue());
}

std::shared_ptr<NFmiQueryData> CreateEmptyQData(
    long levelType,
    const NFmiHPlaceDescriptor &hplaceDescriptor,
    vector<std::shared_ptr<NFmiQueryData> > &theTotalQDataCollector)
{
  set<NFmiParam> params;
  set<NFmiMetTime> validTimes;
  set<float> levels;
  NFmiMetTime origTime;
  NFmiProducer producer;
  for (size_t i = 0; i < theTotalQDataCollector.size(); i++)
  {
    NFmiFastQueryInfo info(theTotalQDataCollector[i].get());
    if (levelType == static_cast<long>(info.Level()->GetIdent()))
    {
      if (hplaceDescriptor == info.HPlaceDescriptor())
      {
        producer = *info.Producer();
        origTime = info.OriginTime();
        InsertAllParams(params, info);
        InsertAllValidTimes(validTimes, info);
        InsertAllLevels(levels, info);
      }
    }
  }

  std::shared_ptr<NFmiQueryData> qData;
  if (!params.empty() && !validTimes.empty() && !levels.empty())
  {
    NFmiTimeList timeList;
    for (set<NFmiMetTime>::iterator it = validTimes.begin(); it != validTimes.end(); ++it)
      timeList.Add(new NFmiMetTime(*it));
    NFmiTimeDescriptor timeDescriptor(origTime, timeList);
    NFmiParamBag paramBag;
    for (set<NFmiParam>::iterator it = params.begin(); it != params.end(); ++it)
      paramBag.Add(NFmiDataIdent(*it, producer));
    NFmiParamDescriptor paramDescriptor(paramBag);
    NFmiLevelBag levelBag;
    for (set<float>::iterator it = levels.begin(); it != levels.end(); ++it)
      levelBag.AddLevel(NFmiLevel(levelType, NFmiStringTools::Convert(*it), *it));
    NFmiVPlaceDescriptor vplaceDescriptor(levelBag);

    NFmiQueryInfo innerInfo(paramDescriptor, timeDescriptor, hplaceDescriptor, vplaceDescriptor);
    qData = std::shared_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(innerInfo));
  }
  return qData;
}

void DoErrorReporting(const std::string &theBaseString,
                      const std::string &theSecondString,
                      const std::string &theFileName,
                      std::exception *theException = 0,
                      std::string *theErrorString = 0)
{
  std::string errStr = theBaseString;
  errStr += "\n";
  errStr += theFileName;
  errStr += "\n";
  errStr += theSecondString;
  if (theException)
  {
    errStr += "\n";
    errStr += theException->what();
  }
  if (theErrorString)
  {
    errStr += "\n";
    errStr += *theErrorString;
  }
  cerr << errStr << std::endl;
}

std::string GetParamName(grib_handle *theHandle)
{
  std::string name, units;
  bool ok1 = GetGribStringValue(theHandle, "parameter.name", name);
  bool ok2 = GetGribStringValue(theHandle, "parameter.units", units);

  if (ok1 && ok2)
    return (name + " [" + units + "]");

  // Let's try other methods to obtain name
  std::string cfName;
  bool cfNameOk = GetGribStringValue(theHandle, "cfName", cfName);
  std::string shortName;
  bool shortNameOk = GetGribStringValue(theHandle, "shortName", shortName);

  std::string usedParameterName;
  if (shortNameOk)
    usedParameterName = shortName;
  if (usedParameterName.empty() == false && cfNameOk)
    usedParameterName += " - ";
  if (cfNameOk)
    usedParameterName += cfName;

  if (ok2)
  {  // jos onnistui, liitetään yksikkö hakasuluissa parametrin nimen perään
    if (usedParameterName.empty() == false)
      usedParameterName += " ";
    usedParameterName += "[" + units + "]";
  }

  return usedParameterName;
}

long GetUsedParamId(grib_handle *theHandle)
{
  // 1. Kokeillaan löytyykö paramId -hakusanalla
  long paramid;
  bool ok = ::GetGribLongValue(theHandle, "paramId", paramid);
  if (ok && paramid != 0)
    return paramid;

  // 2. Katsotaan onko indicatorOfParameter -id käytössä, jos on, käytetään sitä.
  long indicatorOfParameter = 0;
  bool indicatorOfParameterOk =
      ::GetGribLongValue(theHandle, "indicatorOfParameter", indicatorOfParameter);
  if (indicatorOfParameterOk)
    return indicatorOfParameter;

  // 3. kokeillaan parameterCategory + parameterNumber yhdistelmää, jossa lopullinen arvo saadaan
  // ((paramCategory * 1000) + paramNumber)
  long parameterCategory = 0;
  bool parameterCategoryOk = GetGribLongValue(theHandle, "parameterCategory", parameterCategory);
  long parameterNumber = 0;
  bool parameterNumberOk = ::GetGribLongValue(theHandle, "parameterNumber", parameterNumber);
  if (parameterCategoryOk && parameterNumberOk)
    return (parameterCategory * 1000) + parameterNumber;

  // 4. Kokeillaan löytyykö parameter -hakusanalla
  long parameter = 0;
  bool parameterOk = ::GetGribLongValue(theHandle, "parameter", parameter);
  if (parameterOk)
    return parameter;

  throw runtime_error("Couldn't get paramId from given grib_handle.");
}

#if 0
void PrintAllParamInfo_forDebugging(grib_handle *theHandle)
{
  std::string name, units, cfName, shortName;
  if(GetGribStringValue(theHandle, "parameter.name", name))
    cerr << "\n\nparameter.name: " << name << endl;
  if(GetGribStringValue(theHandle, "parameter.units", units))
    cerr << "parameter.units: " << units << endl;
  if(GetGribStringValue(theHandle, "cfName", cfName))
    cerr << "cfName: " << cfName << endl;
  if(GetGribStringValue(theHandle, "shortName", shortName))
    cerr << "shortName: " << shortName << endl;
  
  long paramid = 0;
  long indicatorOfParameter = 0;
  long parameterCategory = 0;
  long parameterNumber = 0;
  long parameter = 0;
  if(GetGribLongValue(theHandle, "paramId", paramid))
    cerr << "paramId: " << paramid << endl;
  if(GetGribLongValue(theHandle, "indicatorOfParameter", indicatorOfParameter))
    cerr << "indicatorOfParameter: " << indicatorOfParameter << endl;
  if(GetGribLongValue(theHandle, "parameterCategory", parameterCategory))
    cerr << "parameterCategory: " << parameterCategory << endl;
  if(GetGribLongValue(theHandle, "parameterNumber", parameterNumber))
    cerr << "parameterNumber: " << parameterNumber << endl;
  if(GetGribLongValue(theHandle, "parameter", parameter))
    cerr << "parameter: " << parameter << endl;
  
  string levelName;
  if(GetGribStringValue(theHandle, "vertical.typeOfLevel", levelName))
    cerr << "vertical.typeOfLevel: " << levelName << endl;
  if(GetGribStringValue(theHandle, "typeOfFirstFixedSurface", levelName))
    cerr << "typeOfFirstFixedSurface: " << levelName;
  double levelValue = 0;
  if(GetGribDoubleValue(theHandle, "vertical.level", levelValue))
    cerr << "vertical.level: " << levelValue << endl;
  
  cerr << endl;
}
#endif

NFmiDataIdent GetParam(grib_handle *theHandle, const NFmiProducer &theWantedProducer)
{
  // TODO producerin voisi päätellä originating center numeroista

  std::string usedParameterName = ::GetParamName(theHandle);
  long usedParId = GetUsedParamId(theHandle);
  if (usedParameterName.empty())
    usedParameterName = NFmiStringTools::Convert(usedParId);
  return NFmiDataIdent(NFmiParam(usedParId,
                                 usedParameterName,
                                 kFloatMissing,
                                 kFloatMissing,
                                 kFloatMissing,
                                 kFloatMissing,
                                 "%.1f",
                                 kLinearly),
                       theWantedProducer);
}

void FixPacificLongitude(NFmiPoint &lonLat)
{
  if (lonLat.X() < 0)
  {
    NFmiLongitude lon(lonLat.X(), true);
    lonLat.X(lon.Value());
  }
}

const double g_AtlanticFixLongitudeLimit = 358;
const double g_PacificFixLongitudeLimit = 178;

void DoPossibleGlobalLongitudeFixes(double &Lo1,
                                    double &Lo2,
                                    GribFilterOptions &theGribFilterOptions)
{
  // Fix GRIB2 range 0...360 to QD range -180...180
  if (Lo1 > 180)
    Lo1 -= 360;
  if (Lo2 > 180)
    Lo2 -= 360;

  // If input was 0...360 this will fix it back to 0...360 (BAM data)
  if (Lo1 == Lo2)
    Lo2 += 360;

  // Select Atlantic or Pacific view

  if (theGribFilterOptions.fDoAtlanticFix && Lo1 == 0 &&
      (Lo2 < 0 || Lo2 > g_AtlanticFixLongitudeLimit))
  {
    Lo1 = -180;
    if (Lo2 < 0)
      Lo2 = -Lo2;
    else
      Lo2 = Lo2 - 180;
  }
  else if (theGribFilterOptions.fDoPacificFix && Lo1 == -180 && Lo2 > g_PacificFixLongitudeLimit)
  {
    Lo1 = 0;
    Lo2 = Lo2 + 180;
  }
}

// ----------------------------------------------------------------------
/* Version independent keys:
 *
 * Ni
 * Nj
 * iDirectionIncrementInDegrees
 * iScansNegatively
 * jDirectionIncrementInDegrees
 * jPointsAreConsecutive
 * jScansPositivelyjScansPositively
 * latitudeOfFirstGridPointInDegrees
 * latitudeOfLastGridPointInDegrees
 * longitudeOfFirstGridPointInDegrees
 * longitudeOfLastGridPointInDegrees
 */
// ----------------------------------------------------------------------

NFmiArea *CreateLatlonArea(grib_handle *theHandle, GribFilterOptions &theGribFilterOptions)
{
  double La1 = 0, Lo1 = 0, La2 = 0, Lo2 = 0;
  int status1 = grib_get_double(theHandle, "latitudeOfFirstGridPointInDegrees", &La1);
  int status2 = grib_get_double(theHandle, "longitudeOfFirstGridPointInDegrees", &Lo1);
  int status3 = grib_get_double(theHandle, "latitudeOfLastGridPointInDegrees", &La2);
  int status4 = grib_get_double(theHandle, "longitudeOfLastGridPointInDegrees", &Lo2);

  if (status1 != 0 || status2 != 0 || status3 != 0 || status4 != 0)
    throw runtime_error("Unable to retrieve latlon-projection information from grib.");

  // We ignore the status of the version check intentionally and assume V2
  long version = 2;
  grib_get_long(theHandle, "editionNumber", &version);

  // Fix BAM data which uses zero for both longitudes
  if (Lo1 == 0 && Lo2 == 0)
    Lo2 = 360;

  // Not needed:
  // check_jscan_direction(theHandle);

  long iScansNegatively = 0;
  int iScansNegativelyStatus = grib_get_long(theHandle, "iScansNegatively", &iScansNegatively);

  if (iScansNegativelyStatus == 0 && !iScansNegatively)
  {
    // ei swapata longitude arvoja, mutta voidaan fiksailla niitä, jos annettu Lo1 on suurempi
    // kuin annettu Lo2
    if (Lo1 > Lo2 && Lo1 >= 180)
      Lo1 -= 360;  // fiksataan vasenmman reunan pacific arvo atlantiseksi
  }
  else if (Lo1 > Lo2)
    std::swap(Lo1, Lo2);

  DoPossibleGlobalLongitudeFixes(Lo1, Lo2, theGribFilterOptions);

  if (La1 > La2)
    std::swap(La1, La2);

  NFmiPoint bl(Lo1, La1);
  NFmiPoint tr(Lo2, La2);
  bool usePacificView = NFmiArea::IsPacificView(bl, tr);
  if (usePacificView)
    ::FixPacificLongitude(tr);

  return NFmiAreaTools::CreateLegacyLatLonArea(bl, tr, usePacificView);
}

// laske sellainen gridi, joka menee originaali hilan hilapisteikön mukaan, mutta peittää sen
// alueen,
// joka on annettu itsLatlonCropRect:issä siten että uusi hila menee juuri seuraaviin hilapisteisiin
// originaali hilassa että se on juuri isompi kuin croppi alue, paitsi jos croppi alue menee
// originaali alueen ulkopuolelle, tällöin croppihila lasketaan todella niin että originaali hila
// jatkuisi
// alueen ulkopuolelle. Seurauksena tulosdata, jossa on sitten orig alueen ulkopuolella puuttuvaa
// dataa.
void CalcCroppedGrid(GridRecordData *theGridRecordData)
{
  NFmiGrid grid(theGridRecordData->itsOrigGrid.itsArea,
                theGridRecordData->itsOrigGrid.itsNX,
                theGridRecordData->itsOrigGrid.itsNY);

  NFmiPoint xy1 = grid.LatLonToGrid(
      theGridRecordData->itsLatlonCropRect
          .TopLeft());  // HUOM! rect maailmassa pitää olla TopLeft, eikä BottomLeft!
  xy1.X(floor(xy1.X()));
  xy1.Y(floor(xy1.Y()));
  NFmiPoint xy2 = grid.LatLonToGrid(
      theGridRecordData->itsLatlonCropRect
          .BottomRight());  // HUOM! rect maailmassa pitää olla BottomRight, eikä TopRight!
  xy2.X(ceil(xy2.X()));
  xy2.Y(ceil(xy2.Y()));

  NFmiPoint latlon1 = grid.GridToLatLon(xy1);
  NFmiPoint latlon2 = grid.GridToLatLon(xy2);
  NFmiArea *newArea = 0;
  if (theGridRecordData->itsOrigGrid.itsArea->ClassId() == kNFmiLatLonArea)
  {
    newArea = NFmiAreaTools::CreateLegacyLatLonArea(latlon1, latlon2);
  }
  else
    throw runtime_error("CalcCroppedGrid doesn't support this projection yet.");

  std::shared_ptr<NFmiArea> newAreaPtr(newArea);
  theGridRecordData->itsGridPointCropOffset = NFmiPoint(xy1.X(), xy1.Y());
  MyGrid newGrid(
      newArea, static_cast<int>(xy2.X() - xy1.X() + 1), static_cast<int>(xy2.Y() - xy1.Y() + 1));
  theGridRecordData->itsGrid = newGrid;
  theGridRecordData->fDoProjectionConversion = true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Struct for earth shape information
 */
// ----------------------------------------------------------------------

struct EarthShape
{
  std::string datum;  // empty, WGS84 or OSGB36
  double a = 0;       // we set these even if datum is not empty since 'a' is
  double b = 0;       // needed for creating the orthographic projection
};

// ----------------------------------------------------------------------
/*!
 * \brief Return PROJ.4 string for the given earth shape
 */
//----------------------------------------------------------------------

std::string GetEarthProj(const EarthShape &theShape)
{
  if (!theShape.datum.empty())
    return fmt::format("+datum={}", theShape.datum);

  if (theShape.a == theShape.b)
    return fmt::format("+R={} +towgs84=0,0,0", theShape.a);

  return fmt::format("+a={} +b={} +towgs84=0,0,0", theShape.a, theShape.b);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return PROJ.4 string for the given earth shape
 */
//----------------------------------------------------------------------

std::string GetEarthProjFull(const EarthShape &theShape)
{
  if (!theShape.datum.empty())
    return fmt::format("+type=crs +datum={}", theShape.datum);

  if (theShape.a == theShape.b)
    return fmt::format("+type=crs +proj=longlat +R={} +towgs84=0,0,0", theShape.a);

  return fmt::format("+type=crs +proj=longlat +a={} +b={} +towgs84=0,0,0", theShape.a, theShape.b);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract earth shape from the GRIB.
 *
 * Based on pygrib code in GitHub.
 */
// ----------------------------------------------------------------------

EarthShape GetEarthShape(grib_handle *theHandle)
{
  EarthShape shape;

  double scale = 1;
  long scalefactor = 1;

  long shapeOfTheEarth = 0;
  if (!GetGribLongValue(theHandle, "shapeOfTheEarth", shapeOfTheEarth))
  {
    // GRIB1 handling
    if (!GetGribLongValue(theHandle, "earthIsOblate", shapeOfTheEarth))
      throw std::runtime_error("earthIsOblate not set in the GRIB");

    if (shapeOfTheEarth == 0)
    {
      // same as shapeOfTheEarth=0 for GRIB2
      shape.a = 6367470.0;
      shape.b = shape.a;
    }
    else
    {
      // shape as shapeOfTheEarth=2 for GRIB2
      shape.a = 6378160.0;
      shape.b = 6356775.0;
    }
  }

  // Section 3, Code Table 2
  else if (shapeOfTheEarth == 0)
  {
    // Earth assumed spherical with radius = 6,367,470.0 m
    shape.a = 6367470.0;
    shape.b = shape.a;
  }
  else if (shapeOfTheEarth == 1)
  {
    // Earth assumed spherical with radius specified (in m) by data producer
    if (GetGribLongValue(theHandle, "scaleFactorOfRadiusOfSphericalEarth", scalefactor))
      if (scalefactor != GRIB_MISSING_LONG)
        scale = pow(10.0, scalefactor);

    if (!GetGribDoubleValue(theHandle, "scaledValueOfRadiusOfSphericalEarth", shape.a))
      throw std::runtime_error("Failed to read scaledValueOfRadiusOfSphericalEarth");

    shape.a *= scale;
    shape.b = shape.a;
  }
  else if (shapeOfTheEarth == 2)
  {
    // Earth assumed oblate spheroid with size as determined by IAU in 1965 (major axis =
    // 6,378,160.0 m, minor axis = 6,356,775.0 m, f = 1/297.0)
    shape.a = 6378160.0;
    shape.b = 6356775.0;
  }
  else if (shapeOfTheEarth == 3 || shapeOfTheEarth == 7)
  {
    // Earth assumed oblate spheroid with major and minor axes specified (in m or km) by data
    // producer
    if (shapeOfTheEarth == 3)
      scale = 1000;  // from kilometers to meters

    if (!GetGribDoubleValue(theHandle, "scaledValueOfEarthMajorAxis", shape.a) ||
        !GetGribDoubleValue(theHandle, "scaledValueOfEarthMinorAxis", shape.b))
      throw std::runtime_error("Failed to read major/minor axes");
    shape.a *= scale;
    shape.b *= scale;
    if (GetGribLongValue(theHandle, "scaleFactorOfMajorAxisOfOblateSpheroidEarth", scalefactor))
      if (scalefactor != GRIB_MISSING_LONG)
        shape.a *= pow(10.0, scalefactor);
    if (GetGribLongValue(theHandle, "scaleFactorOfMinorAxisOfOblateSpheroidEarth", scalefactor))
      if (scalefactor != GRIB_MISSING_LONG)
        shape.b *= pow(10.0, scalefactor);
  }
  else if (shapeOfTheEarth == 4)
  {
    // Earth assumed oblate spheroid as defined in IAG-GRS80 model (major axis = 6,378,137.0 m,
    // minor axis = 6,356,752.314 m, f = 1/298.257222101)
    shape.a = 6378137.0;
    shape.b = 6356752.314;
  }
  else if (shapeOfTheEarth == 5)
  {
    // Earth assumed represented by WGS84 (as used by ICAO since 1998)
    shape.datum = "WGS84";
    shape.a = 6378137.0;
    shape.b = 6356752.3142;
  }
  else if (shapeOfTheEarth == 6)
  {
    // Earth assumed spherical with radius of 6,371,229.0 m
    shape.a = 6371229.0;
    shape.b = shape.a;
  }
  else if (shapeOfTheEarth == 8)
  {
    // Earth model assumed spherical with radius 6371200 m, but the horizontal datum of the
    // resulting latitude/longitude field is the WGS84 reference frame
    shape.a = 6371200.0;
    shape.b = 6371200.0;
  }
  else if (shapeOfTheEarth == 9)
  {
    // Earth represented by the Ordnance Survey Great Britain 1936 Datum, using the Airy 1830
    // Spheroid, the Greenwich meridian as 0 longitude, and the Newlyn datum as mean sea level, 0
    // height
    shape.datum = "OSGB36";
    shape.a = 6377563.396;  // From Wikipedia on OSGB36
    shape.b = 6356256.909;
  }
  else if (shapeOfTheEarth == 255)
    throw std::runtime_error("Earth shape has value 255, the missing value");
  else
    throw std::runtime_error("Earth shape number " + std::to_string(shapeOfTheEarth) +
                             " is reserved for local use (values 192-254)");
  return shape;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract PROJ.4 string from the GRIB
 *
 * Throws for unsupported projections. Based on pygrib code in GitHub.
 */
// ----------------------------------------------------------------------

std::string GetProjString(grib_handle *theHandle)
{
  auto earth_shape = GetEarthShape(theHandle);
  auto earth_proj = GetEarthProj(earth_shape);

  std::string proj_name;
  if (!GetGribStringValue(theHandle, "gridType", proj_name))
    throw runtime_error("Could not extract gridType from the GRIB metadata");

  if (proj_name == "regular_ll" || proj_name == "regular_gg" || proj_name == "reduced_ll" ||
      proj_name == "reduced_gg")
  {
    // Note:: convert to eqc == legacy NFmiLatLonArea
    return "+type=crs +proj=eqc " + earth_proj;
  }

  // Used only for some projections (when coordinates are not extracted in Degrees)
  long truncateDegrees = 0;
  GetGribLongValue(theHandle, "truncateDegrees", truncateDegrees);

  if (proj_name == "rotated_ll" || proj_name == "rotated_gg")
  {
    double pole_lon = 666, pole_lat = 666, rotation = 666;
    if (!GetGribDoubleValue(theHandle, "longitudeOfSouthernPoleInDegrees", pole_lon) ||
        !GetGribDoubleValue(theHandle, "latitudeOfSouthernPoleInDegrees", pole_lat) ||
        !GetGribDoubleValue(theHandle, "angleOfRotationInDegrees", rotation))
      throw std::runtime_error("Failed to extract rotated_ll parameters");

    if (rotation != 0)
      throw std::runtime_error("Rotated latlon rotation not supported");  // TODO

    // Note:: convert to eqc == legacy NFmiRotatedLatLonArea
    return fmt::format(
        "+type=crs +proj=ob_tran +o_proj=eqc +o_lon_p={} +o_lat_p={} +lon_0={} {} +wktext +over "
        "+no_defs",
        0,
        -pole_lat,
        pole_lon,
        earth_proj);
  }

  if (proj_name == "mercator")
  {
    double lon1, lon2;
    bool ok = true;
    ok &= GetGribDoubleValue(theHandle, "longitudeOfFirstGridPointInDegrees", lon1);
    ok &= GetGribDoubleValue(theHandle, "longitudeOfLastGridPointInDegrees", lon2);
    // We ignore LaDInDegrees on purpose since the setting is meaningless when the corners
    // coordinates are given in degrees.

    if (!ok)
      throw std::runtime_error("Failed to extract mercator parameters");

    if (truncateDegrees)
    {
      lon1 = static_cast<int>(lon1);
      lon2 = static_cast<int>(lon2);
    }
    return fmt::format("+type=crs +proj=merc +lon_0={} {}",
                       // (lon2 > lon1 ? 0.5 * (lon1 + lon2) : 0.5 * (lon1 + lon2 + 360)),
                       (lon2 > lon1 ? 0 : 180),
                       earth_proj);
  }

  if (proj_name == "polar_stereographic")
  {
    double lat_ts, lon_0;
    long proj_centre = 0;  // north pole by default
    if (!GetGribDoubleValue(theHandle, "latitudeWhereDxAndDyAreSpecifiedInDegrees", lat_ts) ||
        !GetGribDoubleValue(theHandle, "orientationOfTheGridInDegrees", lon_0) ||
        !GetGribLongValue(theHandle, "projectionCentreFlag", proj_centre))
      throw std::runtime_error("Failed to extract polar_stereographic parameters");

    return fmt::format("+type=crs +proj=stere +lon_0={} +lat_0={} +lat_ts={} {}",
                       lon_0,
                       (proj_centre == 0 ? 90 : -90),
                       lat_ts,
                       earth_proj);
  }

  if (proj_name == "lambert")
  {
    double lon_0, lat_0, lat_1, lat_2, pole_lon, pole_lat;
    if (!GetGribDoubleValue(theHandle, "LoVInDegrees", lon_0) ||
        !GetGribDoubleValue(theHandle, "LaDInDegrees", lat_0) ||
        !GetGribDoubleValue(theHandle, "Latin1InDegrees", lat_1) ||
        !GetGribDoubleValue(theHandle, "Latin2InDegrees", lat_2) ||
        !GetGribDoubleValue(theHandle, "longitudeOfSouthernPoleInDegrees", pole_lon) ||
        !GetGribDoubleValue(theHandle, "latitudeOfSouthernPoleInDegrees", pole_lat))
      throw std::runtime_error("Failed to extract lambert parameters");

    if (pole_lon != 0 || pole_lat != -90)
      throw std::runtime_error("Rotated Lambert projection not supported");  // TODO

    if (lon_0 > 180)
      lon_0 -= 360;

    return fmt::format("+type=crs +proj=lcc +lon_0={} +lat_0={} +lat_1={} +lat_2={} {}",
                       lon_0,
                       lat_0,
                       lat_1,
                       lat_2,
                       earth_proj);
  }

  if (proj_name == "albers")
  {
    double lon_0, lat_0, lat_1, lat_2;
    if (!GetGribDoubleValue(theHandle, "LoVInDegrees", lon_0) ||
        !GetGribDoubleValue(theHandle, "LaDInDegrees", lat_0) ||
        !GetGribDoubleValue(theHandle, "Latin1InDegrees", lat_1) ||
        !GetGribDoubleValue(theHandle, "Latin2InDegrees", lat_2))
      throw std::runtime_error("Failed to extract albers parameters");

    if (truncateDegrees)
    {
      lon_0 = static_cast<int>(lon_0);
      lat_0 = static_cast<int>(lat_0);
      lat_1 = static_cast<int>(lat_1);
      lat_2 = static_cast<int>(lat_2);
    }

    return fmt::format("+type=crs +proj=aea +lon_0={} +lat_0={} +lat_1={} +lat_2={} {}",
                       lon_0,
                       lat_0,
                       lat_1,
                       lat_2,
                       earth_proj);
  }

  if (proj_name == "space_view")
  {
    double lon_0, lat_0, Nr;
    if (!GetGribDoubleValue(theHandle, "longitudeOfSubSatellitePointInDegrees", lon_0) ||
        !GetGribDoubleValue(theHandle, "latitudeOfSubSatellitePointInDegrees", lat_0) ||
        !GetGribDoubleValue(theHandle, "Nr", Nr))
      throw std::runtime_error("Failed to extract space_view parameters");

    double grib2divider = 1;
    GetGribDoubleValue(theHandle, "grib2divider", grib2divider);

    return fmt::format("+type=crs +proj={} +lon_0={} +lat_0={} +h={} {}",
                       (lat_0 == 0 ? "geos" : "nsper"),
                       lon_0,
                       lat_0,
                       earth_shape.a * (Nr / grib2divider -
                                        1),  // == Nr/scale*a - a, the distance from the surface
                       earth_proj);
  }

  if (proj_name == "equatorial_azimuthal_equidistant")
  {
    double lon_0, lat_0;
    if (!GetGribDoubleValue(theHandle, "standardParallel", lat_0) ||
        !GetGribDoubleValue(theHandle, "centralLongitude", lon_0))
      throw std::runtime_error("Failed to extract equatorial_azimuthal_equidistant parameters");

    return fmt::format(
        "+type=crs +proj=aeqd +lon_0={} +lat_0={} {}", lon_0 / 1e6, lat_0 / 1e6, earth_proj);
  }

  if (proj_name == "lambert_azimuthal_equal_area")
  {
    double lon_0, lat_0;
    if (!GetGribDoubleValue(theHandle, "standardParallel", lat_0) ||
        !GetGribDoubleValue(theHandle, "centralLongitude", lon_0))
      throw std::runtime_error("Failed to extract lambert_azimuthal_equal_area parameters");

    return fmt::format(
        "+type=crs +proj=laea +lon_0={} +lat_0={} {}", lon_0 / 1e6, lat_0 / 1e6, earth_proj);
  }

  throw std::runtime_error("Handling of projection " + proj_name +
                           " found from grib is not implemented");
}

NFmiArea *GetGribArea(grib_handle *theHandle, GribFilterOptions &theGribFilterOptions)
{
  // Establish projection variant
  std::string proj_name;
  if (!GetGribStringValue(theHandle, "gridType", proj_name))
    throw runtime_error("Could not extract gridType from the GRIB metadata");

  if (proj_name == "regular_ll")
    return CreateLatlonArea(theHandle, theGribFilterOptions);

  const std::string err = "Failed to extract {} for '{}' projection";

  // PROJ.4 string
  auto proj = GetProjString(theHandle);

  // Needed for specifying spatial reference for the corner coordinates.
  // Note that rotated latlong needs special handling
  auto earth_shape = GetEarthShape(theHandle);
  auto earth_proj = GetEarthProjFull(earth_shape);

  long truncateDegrees = 0;
  GetGribLongValue(theHandle, "truncateDegrees", truncateDegrees);

  // Grid sizes are Ni,Nj or Nx,Ny
  long ni, nj;
  if (!(GetGribLongValue(theHandle, "Nx", ni) && GetGribLongValue(theHandle, "Ny", nj)) &&
      !(GetGribLongValue(theHandle, "Ni", ni) && GetGribLongValue(theHandle, "Nj", nj)))
    throw std::runtime_error(fmt::format(fmt::runtime(err), "grid size", proj_name));

  if (proj_name == "rotated_ll" || proj_name == "rotated_gg")
  {
    double lon1, lat1, lon2, lat2;
    if (!GetGribDoubleValue(theHandle, "longitudeOfFirstGridPointInDegrees", lon1) ||
        !GetGribDoubleValue(theHandle, "latitudeOfFirstGridPointInDegrees", lat1) ||
        !GetGribDoubleValue(theHandle, "longitudeOfLastGridPointInDegrees", lon2) ||
        !GetGribDoubleValue(theHandle, "latitudeOfLastGridPointInDegrees", lat2))
      throw std::runtime_error(fmt::format(fmt::runtime(err), "latlon bounding box", proj_name));

    double pole_lon, pole_lat, rotation;
    if (!GetGribDoubleValue(theHandle, "longitudeOfSouthernPoleInDegrees", pole_lon) ||
        !GetGribDoubleValue(theHandle, "latitudeOfSouthernPoleInDegrees", pole_lat) ||
        !GetGribDoubleValue(theHandle, "angleOfRotationInDegrees", rotation))
      throw std::runtime_error(fmt::format(fmt::runtime(err), "rotation", proj_name));

    // TODO
    if (rotation != 0)
      throw std::runtime_error("Rotated latlon rotation parameter nonzero, what to do???");

    if (earth_shape.a != earth_shape.b)
      throw std::runtime_error("Nonspherical rotated latlon not supported");

    // the legacy corners are in rotated spherical latlon coordinate.

    return NFmiAreaTools::CreateLegacyRotatedLatLonArea(
        NFmiPoint(lon1, lat1), NFmiPoint(lon2, lat2), NFmiPoint(pole_lon, pole_lat));
  }

  if (proj_name == "mercator")
  {
    double lon1, lat1, lon2, lat2;
    if (!GetGribDoubleValue(theHandle, "longitudeOfFirstGridPointInDegrees", lon1) ||
        !GetGribDoubleValue(theHandle, "latitudeOfFirstGridPointInDegrees", lat1) ||
        !GetGribDoubleValue(theHandle, "longitudeOfLastGridPointInDegrees", lon2) ||
        !GetGribDoubleValue(theHandle, "latitudeOfLastGridPointInDegrees", lat2))
      throw std::runtime_error(fmt::format(fmt::runtime(err), "latlon bounding box", proj_name));

    if (truncateDegrees)
    {
      lon1 = static_cast<int>(lon1);
      lat1 = static_cast<int>(lat1);
      lon2 = static_cast<int>(lon2);
      lat2 = static_cast<int>(lat2);
    }

    return NFmiArea::CreateFromCorners(
        proj, earth_proj, NFmiPoint(lon1, lat1), NFmiPoint(lon2, lat2));
  }

  if (proj_name == "polar_stereographic" || proj_name == "lambert" || proj_name == "albers" ||
      proj_name == "equatorial_azimuthal_equidistant" ||
      proj_name == "lambert_azimuthal_equal_area")
  {
    double lon1 = 0;
    double lat1 = 0;
    double dx = 0;
    double dy = 0;

    if (!GetGribDoubleValue(theHandle, "longitudeOfFirstGridPointInDegrees", lon1) ||
        !GetGribDoubleValue(theHandle, "latitudeOfFirstGridPointInDegrees", lat1))
      throw std::runtime_error(fmt::format(fmt::runtime(err), "latlon corner", proj_name));

    if (proj_name == "polar_stereographic" || proj_name == "lambert")
    {
      if (!GetGribDoubleValue(theHandle, "DxInMetres", dx) ||
          !GetGribDoubleValue(theHandle, "DyInMetres", dy))
        throw std::runtime_error(fmt::format(fmt::runtime(err), "grid step size", proj_name));
    }
    else
    {
      if (!GetGribDoubleValue(theHandle, "Dx", dx) || !GetGribDoubleValue(theHandle, "Dy", dy))
        throw std::runtime_error(fmt::format(fmt::runtime(err), "grid step size", proj_name));
      dx *= 1000;
      dy *= 1000;
    }

    return NFmiArea::CreateFromCornerAndSize(
        proj, earth_proj, NFmiPoint(lon1, lat1), dx * (ni - 1), dy * (nj - 1));
  }

  if (proj_name == "space_view")
  {
    // Sample code: https://github.com/jswhit/pygrib/blob/master/pygrib.pyx#L1707
    throw std::runtime_error("space_view projection not supported yet");
  }

  throw std::runtime_error("Handling of projection " + proj_name +
                           " found from grib is not implemented");
}

void ExtractGridInfo(grib_handle *theHandle,
                     GridRecordData *theGridRecordData,
                     GribFilterOptions &theGribFilterOptions,
                     GridSettingsPackage &theGridSettings)
{
  NFmiArea *area = GetGribArea(theHandle, theGribFilterOptions);

  long nx = 0;
  long ny = 0;

  if ((!GetGribLongValue(theHandle, "numberOfPointsAlongAParallel", nx) &&
       !GetGribLongValue(theHandle, "numberOfPointsAlongXAxis", nx) &&
       !GetGribLongValue(theHandle, "Nx", nx)) ||
      (!GetGribLongValue(theHandle, "numberOfPointsAlongAMeridian", ny) &&
       !GetGribLongValue(theHandle, "numberOfPointsAlongYAxis", ny) &&
       !GetGribLongValue(theHandle, "Ny", ny)))
    throw runtime_error("Couldn't get grid x-y sizes from given grib_handle.");

  if (nx == -1 || nx == static_cast<uint32_t>(-1))
    throw Reduced_ll_grib_exception();

  theGridRecordData->fDoProjectionConversion = false;
  theGridRecordData->itsOrigGrid = MyGrid(area, nx, ny);

  if (theGridSettings.itsBaseGrid)
  {
    MyGrid usedGrid(*theGridSettings.itsBaseGrid);

    if (theGridRecordData->itsLevel.LevelType() == kFmiHybridLevel)
      usedGrid = *theGridSettings.itsHybridGrid;
    else if (theGridRecordData->itsLevel.LevelType() == kFmiPressureLevel)
      usedGrid = *theGridSettings.itsPressureGrid;

    theGridRecordData->itsGrid = usedGrid;
    theGridRecordData->fDoProjectionConversion = true;
  }

  // if no cropping was requested, use original grid
  else if (theGridRecordData->itsLatlonCropRect == gMissingCropRect)
    theGridRecordData->itsGrid = theGridRecordData->itsOrigGrid;

  else
    CalcCroppedGrid(theGridRecordData);
}

void GetLevelVerticalCoordinates(grib_handle *theHandle,
                                 GridRecordData &theGridRecordData,
                                 map<int, pair<double, double> > &theVerticalCoordinateMap)
{
  // 1. Onko data hybrid-dataa?
  if (theGridRecordData.itsLevel.LevelType() == kFmiHybridLevel)
  {
    // 2. Onko tälle levelille jo olemassa kertoimia?
    int level = static_cast<int>(round(theGridRecordData.itsLevel.LevelValue()));
    map<int, pair<double, double> >::iterator it = theVerticalCoordinateMap.find(level);
    if (it == theVerticalCoordinateMap.end())
    {
      // 3. Jos ei, hae koordinaatti taulu (jos löytyy siis)
      long numberOfVerticalCoordinateValues = 0;
      int numberOfVerticalCoordinateValuesOk = grib_get_long(
          theHandle, "numberOfVerticalCoordinateValues", &numberOfVerticalCoordinateValues);

      if (numberOfVerticalCoordinateValuesOk == 0 && numberOfVerticalCoordinateValues > 0)
      {
        vector<double> pv(numberOfVerticalCoordinateValues);
        size_t num_coord = pv.size();
        int pvValuesOk = grib_get_double_array(theHandle, "pv", &pv[0], &num_coord);
        if (pvValuesOk == 0)
        {
          // 4. Tapuksesta riippuen laske a- ja b-kertoimet ja talleta ne level-tauluun (ks. mallia
          // K. Niemelän alla olevasta koodista)
          if (num_coord == 2)
          {  // Hirlam: A, B (Hirlamissa ilmoitetaan jokaiselle hybridi-levelille omat A ja B
             // kertoimensa, helppoa)
            theVerticalCoordinateMap.insert(make_pair(level, make_pair(pv[0], pv[1])));
          }
          else if (level <= static_cast<int>(
                                num_coord / 2))  //   else if (decoded_header->producer.centre_id ==
                                                 //   98 ||
                                                 //   decoded_header->producer.generating_process_id
                                                 //   == 3)
          {  // itse en tutki centre_id tai generating_process_id arvoja, mutta ne ilmeisesti
             // kertovat EC:stä ja Arome:sta
            // ELI EC ja Arome antavat kaikkien hybrid levelien kertoimet jokaisen erillisen
            // hybrid-levelin kanssa,
            // joten taulukosta pitää osata valita oikeat kohdat laskuihin.
            // TODO: Pitää kysyä Karilta selvennystä, miksi lasketaan kertoimista keskiarvo kahden
            // levelin välille?!?!?
            double a = (pv[level - 1] + pv[level]) / 2.;
            double b = (pv[num_coord / 2 + level - 1] + pv[num_coord / 2 + level]) / 2.;
            theVerticalCoordinateMap.insert(make_pair(level, make_pair(a, b)));
          }
        }
      }
    }
  }
}

void ChangeParamSettingsIfNeeded(vector<ParamChangeItem> &theParamChangeTable,
                                 GridRecordData *theGribData,
                                 bool verbose)
{
  if (theParamChangeTable.size() > 0)
  {  // muutetaan tarvittaessa parametrin nimeä ja id:tä
    for (unsigned int i = 0; i < theParamChangeTable.size(); i++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[i];

      if (paramChangeItem.itsOriginalParamId ==
          static_cast<long>(theGribData->itsParam.GetParamIdent()))
      {
        if (paramChangeItem.itsLevel != nullptr)
        {
          if ((*paramChangeItem.itsLevel) == theGribData->itsLevel)
          {
            if (verbose)
            {
              cerr << paramChangeItem.itsOriginalParamId << " changed to "
                   << paramChangeItem.itsWantedParam.GetIdent() << " "
                   << paramChangeItem.itsWantedParam.GetName().CharPtr() << " at level "
                   << theGribData->itsLevel.LevelValue();
            }

            theGribData->ChangeParam(paramChangeItem.itsWantedParam);
            theGribData->itsLevel =
                NFmiLevel(1, "sfc", 0);  // tarkista että tästä tulee pinta level dataa
            if (verbose)
              cerr << " level -> sfc";
            break;
          }
        }
        else
        {
          if (verbose)
          {
            cerr << paramChangeItem.itsOriginalParamId << " changed to "
                 << paramChangeItem.itsWantedParam.GetIdent() << " "
                 << paramChangeItem.itsWantedParam.GetName().CharPtr();
          }
          theGribData->ChangeParam(paramChangeItem.itsWantedParam);
          break;
        }
      }
    }
  }
}

string MakeParamString(const NFmiParam &theParam)
{
  string str("id: ");
  str += NFmiStringTools::Convert(theParam.GetIdent());
  str += " name: ";
  str += theParam.GetName();
  return str;
}

void DoParamConflictError(const NFmiParam &theChangedParam,
                          const NFmiParam &theOrigChangedParam,
                          const NFmiParam &theUnChangedParam)
{
  string errorStr;
  errorStr += "\nError - conflict with changed param and non-changed param.";
  errorStr += "\nChanged param:\n";
  errorStr += MakeParamString(theChangedParam);
  errorStr += "\nChanged param originally:\n";
  errorStr += MakeParamString(theOrigChangedParam);
  errorStr += "\nand unchanged param:\n";
  errorStr += MakeParamString(theUnChangedParam);
  errorStr += "\n\nYou must either change the changed param id to some non-conflicting\n";
  errorStr += " value or you must also change the non-changed param id to some other value.\n";
  errorStr += "Program execution stops here because of the conflict.";
  throw runtime_error(errorStr);
}

void DoParamChecking(GridRecordData &theData,
                     map<unsigned long, pair<NFmiParam, NFmiParam> > &theChangedParams,
                     map<unsigned long, NFmiParam> &theUnchangedParams,
                     bool &fExecutionStoppingError)
{
  unsigned long keyValue = theData.itsParam.GetParamIdent();
  if (theData.fParamChanged)
  {
    theChangedParams.insert(
        make_pair(keyValue, make_pair(*theData.itsParam.GetParam(), theData.itsOrigParam)));

    map<unsigned long, NFmiParam>::iterator it = theUnchangedParams.find(keyValue);
    if (it != theUnchangedParams.end())
    {
      fExecutionStoppingError = true;
      DoParamConflictError(*(theData.itsParam.GetParam()), theData.itsOrigParam, (*it).second);
    }
  }
  else
  {
    theUnchangedParams.insert(make_pair(keyValue, *(theData.itsParam.GetParam())));

    map<unsigned long, pair<NFmiParam, NFmiParam> >::iterator it = theChangedParams.find(keyValue);
    if (it != theChangedParams.end())
    {
      fExecutionStoppingError = true;
      DoParamConflictError((*it).second.first, (*it).second.second, *(theData.itsParam.GetParam()));
    }
  }
}

bool IgnoreThisLevel(GridRecordData *data, NFmiLevelBag &theIgnoredLevelList)
{
  if (theIgnoredLevelList.GetSize() > 0)
  {
    for (theIgnoredLevelList.Reset(); theIgnoredLevelList.Next();)
    {
      if (theIgnoredLevelList.Level()->LevelValue() == gMissLevelValue &&
          theIgnoredLevelList.Level()->LevelType() == data->itsLevel.LevelType())
        return true;  // skipataan jokerin valuen yhteydessä kaikki kyseisen level tyypin kentät
      if (*(theIgnoredLevelList.Level()) == data->itsLevel)
        return true;
    }
  }
  return false;
}

bool AcceptThisLevelType(GridRecordData *data, vector<FmiLevelType> &theAcceptOnlyLevelTypes)
{
  if (theAcceptOnlyLevelTypes.size() > 0)
  {
    for (unsigned int i = 0; i < theAcceptOnlyLevelTypes.size(); i++)
    {
      if (theAcceptOnlyLevelTypes[i] == data->itsLevel.LevelType())
        return true;  // skipataan jokerin valuen yhteydessä kaikki kyseisen level tyypin kentät
    }
    return false;  // jos oli accept lista mutta ei löytynyt leveltypeä siitä, hylkää
  }
  return true;
}

bool CropParam(GridRecordData *gribData,
               bool fCropParamsNotMensionedInTable,
               vector<ParamChangeItem> &theParamChangeTable)
{
  if (fCropParamsNotMensionedInTable && theParamChangeTable.size())
  {
    for (unsigned int i = 0; i < theParamChangeTable.size(); i++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[i];
      if (static_cast<long>(gribData->itsParam.GetParamIdent()) ==
          paramChangeItem.itsWantedParam.GetIdent())
      {
        //				if(paramChangeItem.itsLevel)
        //				{
        //					if(gribData->itsLevel.GetIdent() == 1) // jos param
        // laitettu
        // pinta dataksi cropataan muut paitsi pintalevelit
        //						return true;
        //				}
        //				else
        return false;
      }
    }
    return true;
  }
  return false;
}

std::string MakeGridStr(const NFmiGrid &theGrid)
{
  std::string str;
  str += theGrid.Area()->ProjStr();
  str += ":";
  str += NFmiStringTools::Convert(theGrid.XNumber());
  str += ",";
  str += NFmiStringTools::Convert(theGrid.YNumber());
  return str;
}

void DoGlobalFix(NFmiDataMatrix<float> &theOrigValues, const GribFilterOptions &theOptions)
{
  // Nyt on siis tilanne että halutaan 'korjata' globaali data editoria varten.
  // Latlon-area peittää maapallon longitudeissa 0-360, se pitää muuttaa editoria varten
  // longitudeihin -180 - 180.
  // Eli matriisissa olevia arvoja pitää siirtää niin että vasemmalla puoliskolla olevat laitetaan
  // oikealle
  // puolelle ja toisin päin.

  if (theOptions.fVerbose)
    cerr << " swapping sides";
  int nx = static_cast<int>(theOrigValues.NX());
  int ny = static_cast<int>(theOrigValues.NY());
  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx / 2; i++)
      std::swap(theOrigValues[i][j], theOrigValues[i + nx / 2][j]);
}

void ProjectData(GridRecordData *theGridRecordData,
                 NFmiDataMatrix<float> &theOrigValues,
                 const GribFilterOptions &theOptions)
{
  static std::map<std::string, NFmiDataMatrix<NFmiLocationCache> > locationCacheMap;

  if (theOptions.fVerbose)
    cerr << " p";

  NFmiGrid targetGrid(theGridRecordData->itsGrid.itsArea,
                      theGridRecordData->itsGrid.itsNX,
                      theGridRecordData->itsGrid.itsNY);
  std::string targetStr = MakeGridStr(targetGrid);
  NFmiGrid sourceGrid(theGridRecordData->itsOrigGrid.itsArea,
                      theGridRecordData->itsOrigGrid.itsNX,
                      theGridRecordData->itsOrigGrid.itsNY);
  std::string sourceStr = MakeGridStr(sourceGrid);

  std::string mapKeyStr = targetStr + "+" + sourceStr;
  std::map<std::string, NFmiDataMatrix<NFmiLocationCache> >::iterator it =
      locationCacheMap.find(mapKeyStr);
  NFmiDataMatrix<NFmiLocationCache> locationCacheMatrix;
  if (it == locationCacheMap.end())
  {
    sourceGrid.CalcLatlonCachePoints(targetGrid, locationCacheMatrix);
    locationCacheMap.insert(std::make_pair(mapKeyStr, locationCacheMatrix));
  }
  else
    locationCacheMatrix = (*it).second;

  int targetXSize = theGridRecordData->itsGrid.itsNX;
  int targetYSize = theGridRecordData->itsGrid.itsNY;
  NFmiRect relativeRect(0, 0, sourceGrid.XNumber() - 1, sourceGrid.YNumber() - 1);
  theGridRecordData->itsGridData.Resize(targetXSize, targetYSize);
  int counter = 0;
  FmiParameterName param = FmiParameterName(theGridRecordData->itsParam.GetParam()->GetIdent());
  FmiInterpolationMethod interp = theGridRecordData->itsParam.GetParam()->InterpolationMethod();
  for (targetGrid.Reset(); targetGrid.Next(); counter++)
  {
    NFmiLocationCache &locCache =
        locationCacheMatrix[targetGrid.Index() % targetXSize][targetGrid.Index() / targetXSize];
    int destX = counter % theGridRecordData->itsGrid.itsNX;
    int destY = counter / theGridRecordData->itsGrid.itsNX;
    theGridRecordData->itsGridData[destX][destY] = DataMatrixUtils::InterpolatedValue(
        theOrigValues, locCache.itsGridPoint, relativeRect, param, true, interp);
  }
}

void CropData(GridRecordData *theGridRecordData,
              NFmiDataMatrix<float> &theOrigValues,
              const GribFilterOptions &theOptions)
{
  // tässä raaka hila croppaus
  if (theOptions.fVerbose)
    cerr << " c";
  int x1 = static_cast<int>(theGridRecordData->itsGridPointCropOffset.X());
  int y1 = static_cast<int>(theGridRecordData->itsGridPointCropOffset.Y());
  int destSizeX = theGridRecordData->itsGrid.itsNX;
  int destSizeY = theGridRecordData->itsGrid.itsNY;
  theGridRecordData->itsGridData.Resize(destSizeX, destSizeY);
  int origSizeX = theGridRecordData->itsOrigGrid.itsNX;
  int origSizeY = theGridRecordData->itsOrigGrid.itsNY;
  for (int j = 0; j < destSizeY; j++)
  {
    for (int i = 0; i < destSizeX; i++)
    {
      if ((x1 + i < 0) || (x1 + i >= origSizeX))
        break;
      if ((y1 + j < 0) || (y1 + j >= origSizeY))
        break;
      theGridRecordData->itsGridData[i][j] = theOrigValues[x1 + i][y1 + j];
    }
  }
}

void DoAreaManipulations(GridRecordData *theGridRecordData,
                         NFmiDataMatrix<float> &theOrigValues,
                         const GribFilterOptions &theOptions)
{
  // 2. Kun orig matriisi on saatu täytettyä, katsotaan pitääkö vielä täyttää cropattu alue, vai
  // käytetäänkö originaali dataa suoraan.
  if (theGridRecordData->fDoProjectionConversion == false)
    theGridRecordData->itsGridData = theOrigValues;
  else if (theGridRecordData->fDoProjectionConversion == true &&
           theGridRecordData->itsLatlonCropRect == gMissingCropRect)
    ProjectData(theGridRecordData, theOrigValues, theOptions);
  else
    CropData(theGridRecordData, theOrigValues, theOptions);
}

void MakeParameterConversions(GridRecordData *theGridRecordData,
                              const vector<ParamChangeItem> &theParamChangeTable)
{
  if (theParamChangeTable.size() > 0)
  {  // tehdään tarvittaessa parametrille base+scale muunnos
    for (unsigned int p = 0; p < theParamChangeTable.size(); p++)
    {
      const ParamChangeItem &paramChangeItem = theParamChangeTable[p];
      if (paramChangeItem.itsWantedParam.GetIdent() ==
          static_cast<long>(theGridRecordData->itsParam.GetParamIdent()))  // parametri on jo
                                                                           // muutettu, nyt
                                                                           // katsotaan onko
                                                                           // wantedparam sama
      {
        if (paramChangeItem.itsConversionBase != 0 || paramChangeItem.itsConversionScale != 1)
        {
          int nx = static_cast<int>(theGridRecordData->itsGridData.NX());
          int ny = static_cast<int>(theGridRecordData->itsGridData.NY());
          for (int j = 0; j < ny; j++)
          {
            for (int i = 0; i < nx; i++)
            {
              if (theGridRecordData->itsGridData[i][j] != kFloatMissing)
                theGridRecordData->itsGridData[i][j] =
                    paramChangeItem.itsConversionBase +
                    (theGridRecordData->itsGridData[i][j] * paramChangeItem.itsConversionScale);
            }
          }
          break;
        }
      }
    }
  }
}

void FillGridData(grib_handle *theHandle,
                  GridRecordData *theGridRecordData,
                  const GribFilterOptions &theOptions)
{
  // 1. Täytetään ensin origGridin kokoinen matriisi, koska pitää pystyä tekemään mm. global fix
  size_t values_length = 0;
  int status1 = grib_get_size(theHandle, "values", &values_length);
  vector<double> doubleValues(values_length);
  int status2 = grib_get_double_array(theHandle, "values", &doubleValues[0], &values_length);
  int gridXSize = theGridRecordData->itsOrigGrid.itsNX;
  int gridYSize = theGridRecordData->itsOrigGrid.itsNY;
  NFmiDataMatrix<float> origValues(gridXSize, gridYSize);
  if (status1 || status2)
    throw runtime_error("Couldn't get values-data from given grib_handle.");

  long scanningMode = 0;
  int status4 = grib_get_long(theHandle, "scanningMode", &scanningMode);

  // BIT  VALUE   MEANING
  // 1    0   Points scan in +i direction (128!)
  //      1   Points scan in -i direction
  // 2    0   Points scan in -j direction (64!)
  //      1   Points scan in +j direction
  // 3    0   Adjacent points in i direction are consecutive (32!)
  //          (FORTRAN: (I,J))
  //      1   Adjacent points in j direction are consecutive
  //          (FORTRAN: (J,I))

  if (status4 == 0)
  {
    if (scanningMode == 0)
    {
      for (size_t i = 0; i < values_length; i++)
      {
        if (doubleValues[i] == theGridRecordData->itsMissingValue)
          origValues[i % gridXSize][gridYSize - (i / gridXSize) - 1] = kFloatMissing;
        else
          origValues[i % gridXSize][gridYSize - (i / gridXSize) - 1] =
              static_cast<float>(doubleValues[i]);
      }
    }
    else if (scanningMode == 64)
    {
      for (size_t i = 0; i < values_length; i++)
      {
        if (doubleValues[i] == theGridRecordData->itsMissingValue)
          origValues[i % gridXSize][i / gridXSize] = kFloatMissing;
        else
          origValues[i % gridXSize][i / gridXSize] = static_cast<float>(doubleValues[i]);
      }
    }
    else  // sitten kun tulee lisää ceissejä, lisätään eri täyttö variaatioita
    {
      throw runtime_error("Scanning mode " + boost::lexical_cast<string>(scanningMode) +
                          " not yet implemented.");
    }
  }

  if (theOptions.DoGlobalFix())
    DoGlobalFix(origValues, theOptions);
  DoAreaManipulations(theGridRecordData, origValues, theOptions);

  // 3. Tarkista vielä, jos löytyy paramChangeTablesta parametrille muunnos kaavat jotka pitää tehdä
  MakeParameterConversions(theGridRecordData, theOptions.itsParamChangeTable);
}

vector<NFmiHPlaceDescriptor> GetAllHPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile)
{
  set<MyGrid> gribDataSet;

  for (size_t i = 0; i < theGribRecordDatas.size(); i++)
  {
    MyGrid &tmpGrid = theGribRecordDatas[i]->itsGrid;
    gribDataSet.insert(tmpGrid);
  }

  vector<NFmiHPlaceDescriptor> hPlaces;

  if (useOutputFile)
  {
    set<MyGrid>::iterator it2 = gribDataSet.begin();
    for (; it2 != gribDataSet.end(); ++it2)
    {
      NFmiGrid grid((*it2).itsArea, (*it2).itsNX, (*it2).itsNY);
      hPlaces.push_back(NFmiHPlaceDescriptor(grid));
    }
  }
  else
  {  // tulostus tehdään cout:iin, ja tehdään vain yksi data, toivottavasti se on se merkitsevin,
     // tässä yritetään jättää huomiotta ainakin
    // hirlam gribeissä esiintyvä 2x2 kokoinen yksi hila
    MyGrid &tmpGrid = theGribRecordDatas[theGribRecordDatas.size() > 1 ? 1 : 0]->itsGrid;
    NFmiGrid grid(tmpGrid.itsArea, tmpGrid.itsNX, tmpGrid.itsNY);
    hPlaces.push_back(NFmiHPlaceDescriptor(grid));
  }
  return hPlaces;
}

NFmiVPlaceDescriptor MakeVPlaceDescriptor(vector<GridRecordData *> &theGribRecordDatas,
                                          int theLevelType)
{
  unsigned int i = 0;
  set<NFmiLevel, LevelLessThan> levelSet;
  for (i = 0; i < theGribRecordDatas.size(); i++)
  {
    if (theGribRecordDatas[i]->itsLevel.LevelType() == theLevelType)
    {
      const NFmiLevel &tmpLevel = theGribRecordDatas[i]->itsLevel;
      levelSet.insert(tmpLevel);
    }
  }
  NFmiLevelBag levelBag;
  set<NFmiLevel, LevelLessThan>::iterator it = levelSet.begin();
  for (; it != levelSet.end(); ++it)
    levelBag.AddLevel(*it);
  return NFmiVPlaceDescriptor(levelBag);
}

map<int, int>::iterator FindHighesLevelType(map<int, int> levelTypeCounter)
{
  map<int, int>::iterator it = levelTypeCounter.begin();
  map<int, int>::iterator highestIt = levelTypeCounter.end();
  for (; it != levelTypeCounter.end(); ++it)
  {
    if (highestIt == levelTypeCounter.end())
      highestIt = it;
    else if ((*highestIt).second < (*it).second)
      highestIt = it;
  }
  return highestIt;
}

// tehdään levelbagi kaikista eri tyyppisistä leveleistä.
vector<NFmiVPlaceDescriptor> GetAllVPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile)
{
  // 1. etsitään kaikki erilaiset levelit set:in avulla
  set<NFmiLevel, LevelLessThan> levelSet;
  map<int, int> levelTypeCounter;

  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
  {
    GridRecordData *bs = *it;
    levelSet.insert(bs->itsLevel);
    levelTypeCounter[bs->itsLevel.LevelType()]++;  // kikka vitonen: tämä laskee erityyppiset
                                                   // levelit
  }

  vector<NFmiVPlaceDescriptor> vPlaces;

  if (useOutputFile)
  {
    map<int, int>::iterator lt = levelTypeCounter.begin();
    for (; lt != levelTypeCounter.end(); ++lt)
    {
      NFmiVPlaceDescriptor vDesc = ::MakeVPlaceDescriptor(theGribRecordDatas, lt->first);
      if (vDesc.Size() > 0)
        vPlaces.push_back(vDesc);
    }
  }
  else
  {  // tulostus tehdään cout:iin, ja tehdään vain yksi data, toivottavasti se on se merkitsevin,
     // eli otetaan se minkä tyyppisiä esiintyi eniten
    map<int, int>::iterator lt = FindHighesLevelType(levelTypeCounter);
    if (lt != levelTypeCounter.end())
    {
      NFmiVPlaceDescriptor vDesc = ::MakeVPlaceDescriptor(theGribRecordDatas, lt->first);
      if (vDesc.Size() > 0)
        vPlaces.push_back(vDesc);
    }
  }
  return vPlaces;
}

NFmiVPlaceDescriptor GetVPlaceDesc(vector<GridRecordData *> &theGribRecordDatas)
{
  NFmiLevelBag levelBag;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    levelBag.AddLevel(theGribRecordDatas[i]->itsLevel);  // AddLevel poistaa duplikaatit

  return NFmiVPlaceDescriptor(levelBag);
}

void AddGeneratedHybridParam(NFmiParamBag &theGeneratedParamBag,
                             const GeneratedHybridParamInfo &theGeneratedHybridParamInfo)
{
  if (theGeneratedHybridParamInfo.fCalcHybridParam)
  {
    // lisätään paine parametri hybridi dataan, jos sitä ei ole ja se lasketaan myöhemmin
    if (theGeneratedParamBag.SetCurrent(static_cast<FmiParameterName>(
            theGeneratedHybridParamInfo.itsGeneratedHybridParam.GetIdent())) == false)
      theGeneratedParamBag.Add(theGeneratedHybridParamInfo.itsGeneratedHybridParam);
  }
}

const NFmiLevel &FindFirstLevel(int theLevelValue, vector<GridRecordData *> &theGribRecordDatas)
{
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    if (theLevelValue == static_cast<int>(theGribRecordDatas[i]->itsLevel.LevelValue()))
      return theGribRecordDatas[i]->itsLevel;
  throw runtime_error("Error in program in FindFirstLevel-funktion.");
}

const NFmiDataIdent &FindFirstParam(int theParId, vector<GridRecordData *> &theGribRecordDatas)
{
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    if (theParId == static_cast<int>(theGribRecordDatas[i]->itsParam.GetParamIdent()))
      return theGribRecordDatas[i]->itsParam;
  throw runtime_error("Error in program in FindFirstParam-function.");
}

void InterpolateRowData(vector<float> &theSourceValues, vector<float> &theDestValues)
{
  float ratio = (theSourceValues.size() - 1) / static_cast<float>(theDestValues.size() - 1);
  if (ratio == 1)
    theDestValues = theSourceValues;
  else
  {
    for (unsigned int i = 0; i < theDestValues.size() - 1; i++)
    {
      float relativePos = ratio * i;
      unsigned int lowerIndex = static_cast<unsigned int>(relativePos);
      float relativePosRemains =
          relativePos -
          floor(ratio *
                i);  // otetaan desimaali osa irti sijainnista niin saadaan interpolointi kerroin
      float value1 = theSourceValues[lowerIndex];
      float value2 = theSourceValues[lowerIndex + 1];
      double interpolatedValue = NFmiInterpolation::Linear(relativePosRemains, value1, value2);
      theDestValues[i] = static_cast<float>(interpolatedValue);
    }
    theDestValues[theDestValues.size() - 1] = theSourceValues[theSourceValues.size() - 1];
  }
}

// Etsitään ne parametrit mitkä löytyvät data-setistä.
// Lisäksi hilan ja arean pitää olla sama kuin annetussa hplaceDescriptorissa ja level-tyypin pitää
// olla
// sama kuin vplaceDescriptorissa.
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace,
                                 const GribFilterOptions &theGribFilterOptions)
{
  // Ensin pitää saada 1. levelin level-type talteen vplaceDescriptorista, sillä otamme vain
  // parametreja, mitä löytyy sellaisista hila kentistä missä on tälläinen level id.
  FmiLevelType wantedLevelType = theVplace.Levels()->Level(0)->LevelType();
  set<int> parIds;  // set:in avulla selvitetaan kuinka monta erilaista identtia loytyy
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
  {
    if (theGribRecordDatas[i]->itsGrid == *(theHplace.Grid()))
    {
      if (theGribRecordDatas[i]->itsLevel.LevelType() == wantedLevelType)
        parIds.insert(theGribRecordDatas[i]->itsParam.GetParamIdent());
    }
    else if (theGribFilterOptions.fVerbose)
    {
      cerr << "Discarding parameter " << i << " since the grid is different from the chosen one"
           << endl;
    }
  }

  NFmiParamBag parBag;
  set<int>::iterator it = parIds.begin();
  for (; it != parIds.end(); ++it)
    parBag.Add(FindFirstParam(*it, theGribRecordDatas));

  if (wantedLevelType == kFmiHybridLevel)
  {
    AddGeneratedHybridParam(parBag, theGribFilterOptions.itsHybridPressureInfo);
    AddGeneratedHybridParam(parBag, theGribFilterOptions.itsHybridRelativeHumidityInfo);
  }
  else if ((wantedLevelType == kFmiPressureLevel) || (wantedLevelType == kFmiGroundSurface))
  {
    AddGeneratedHybridParam(parBag, theGribFilterOptions.itsHybridRelativeHumidityInfo);
  }

  if (wantedLevelType == kFmiHybridLevel)
    parBag.SetProducer(theGribFilterOptions.itsWantedHybridProducer);
  if (wantedLevelType == kFmiPressureLevel)
    parBag.SetProducer(theGribFilterOptions.itsWantedPressureProducer);

  return NFmiParamDescriptor(parBag);
}

bool ConvertTimeList2TimeBag(NFmiTimeList &theTimeList, NFmiTimeBag &theTimeBag)
{
  // Examine if times are at equal intervals, and there are at least 3 times
  // so that creating a TimeBag makes sense.

  if (theTimeList.NumberOfItems() <= 2)
    return false;

  theTimeList.First();
  theTimeList.Next();
  int resolution = theTimeList.CurrentResolution();
  for (; theTimeList.Next();)
  {
    if (resolution != theTimeList.CurrentResolution())
      return false;  // jos yhdenkin aikavälin resoluutio poikkeaa, ei voida tehdä bagia
  }
  theTimeBag = NFmiTimeBag(theTimeList.FirstTime(), theTimeList.LastTime(), resolution);
  return true;
}

// HUOM! jos datassa on 'outoja' valid-aikoja esim. 1919 jne., joita näyttää tulevan esim.
// hirlamista liittyen johonkin
// kontrolli grideihin (2x2 hila ja muuta outoa). Tälläiset hilat jätetään huomiotta.
// Timebagin rakentelussa tarkastellaan myös että hila ja level-type ovat halutunlaiset.
NFmiTimeDescriptor GetTimeDesc(vector<GridRecordData *> &theGribRecordDatas,
                               NFmiHPlaceDescriptor &theHplace,
                               NFmiVPlaceDescriptor &theVplace)
{
  MyGrid grid(*theHplace.Grid());
  theVplace.Reset();
  theVplace.Next();
  FmiLevelType levelType = theVplace.Level()->LevelType();
  // set:in avulla selvitetaan kuinka monta erilaista timea loytyy.
  set<NFmiMetTime> timesSet;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    if (theGribRecordDatas[i]->itsLevel.LevelType() == levelType)
      if (theGribRecordDatas[i]->itsGrid == grid)
        timesSet.insert(theGribRecordDatas[i]->itsValidTime);

  // Tehdaan aluksi timelist, koska se on helpompi,
  // myohemmin voi miettia saisiko aikaan timebagin.
  NFmiTimeList timeList;
  set<NFmiMetTime>::iterator it = timesSet.begin();
  NFmiMetTime dummyTime(1950, 1, 1);  // laitoin tälläisen dummytime rajoittimen, koska Pekon
                                      // antamassa datassa oli aika 1919 vuodelta ja se ja
                                      // nykyaikainen aika sekoitti mm. metkun editorin pahasti
  for (; it != timesSet.end(); ++it)
    if (*it > dummyTime)
      timeList.Add(new NFmiMetTime(*it));

  NFmiTimeBag timeBag;
  bool fUseTimeBag = ConvertTimeList2TimeBag(timeList, timeBag);  // jos mahd.

  // Oletus kaikki origintimet ovat samoja, en tutki niita nyt yhtaan.
  if (fUseTimeBag)
    return NFmiTimeDescriptor(theGribRecordDatas[0]->itsOrigTime, timeBag);

  return NFmiTimeDescriptor(theGribRecordDatas[0]->itsOrigTime, timeList);
}

bool FillQDataWithGribRecords(std::shared_ptr<NFmiQueryData> &theQData,
                              vector<GridRecordData *> &theGribRecordDatas,
                              bool verbose)
{
  NFmiFastQueryInfo info(theQData.get());
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  GridRecordData *tmp = 0;
  int filledGridCount = 0;
  if (verbose)
    cerr << "Filling qdata grids ";
  for (int i = 0; i < gribCount; i++)
  {
    tmp = theGribRecordDatas[i];
    if (tmp->itsGrid == *info.Grid())  // vain samanlaisia hiloja laitetaan samaan qdataan
    {
      if (info.Time(tmp->itsValidTime) && info.Level(tmp->itsLevel) && info.Param(tmp->itsParam))
      {
        if (!info.SetValues(tmp->itsGridData))
          throw runtime_error("qdatan täyttö gribi datalla epäonnistui, lopetetaan...");
        filledGridCount++;
        if (verbose)
          cerr << NFmiStringTools::Convert(filledGridCount) << " ";
      }
    }
  }
  if (verbose)
    cerr << endl;
  return filledGridCount > 0;
}

std::shared_ptr<NFmiQueryData> CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                                               NFmiHPlaceDescriptor &theHplace,
                                               NFmiVPlaceDescriptor &theVplace,
                                               GribFilterOptions &theGribFilterOptions)
{
  std::shared_ptr<NFmiQueryData> qdata;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    NFmiParamDescriptor params(
        GetParamDesc(theGribRecordDatas, theHplace, theVplace, theGribFilterOptions));
    NFmiTimeDescriptor times(GetTimeDesc(theGribRecordDatas, theHplace, theVplace));
    if (params.Size() == 0 || times.Size() == 0)
      return qdata;  // turha jatkaa jos toinen näistä on tyhjä
    NFmiQueryInfo innerInfo(params, times, theHplace, theVplace);
    qdata = std::shared_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(innerInfo));
    bool anyDataFilled =
        FillQDataWithGribRecords(qdata, theGribRecordDatas, theGribFilterOptions.fVerbose);

    if (anyDataFilled == false)
      qdata = std::shared_ptr<NFmiQueryData>();
  }
  return qdata;
}

std::shared_ptr<NFmiQueryData> GetSurfaceData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas,
                                              FmiParameterName thePressureAtStationParId)
{
  std::shared_ptr<NFmiQueryData> surfaceData;
  for (size_t i = 0; i < theQdatas.size(); i++)
  {
    if (theQdatas[i]->Info()->SizeLevels() == 1)
    {
      if (theQdatas[i]->Info()->Param(thePressureAtStationParId))
      {
        surfaceData = theQdatas[i];
        break;
      }
    }
  }
  return surfaceData;
}

std::shared_ptr<NFmiQueryData> GetHybridData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas,
                                             FmiParameterName pressureId)
{
  std::shared_ptr<NFmiQueryData> data;
  for (size_t i = 0; i < theQdatas.size(); i++)
  {
    // Huom! ei tarvitse olla useita leveleitä datassa, koska yksittäinen grib-tiedosto voi sisältää
    // vain yhden levelin dataa
    theQdatas[i]->Info()->FirstLevel();
    if (theQdatas[i]->Info()->Level()->LevelType() == kFmiHybridLevel)  // pitää olla hybrid tyyppiä
    {
      if (theQdatas[i]->Info()->Param(pressureId))  // pitää löytyä paine parametri
      {
        data = theQdatas[i];
        break;
      }
    }
  }
  return data;
}

std::shared_ptr<NFmiQueryData> GetPressureData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas)
{
  std::shared_ptr<NFmiQueryData> data;
  for (size_t i = 0; i < theQdatas.size(); i++)
  {
    // Huom! ei tarvitse olla useita leveleitä datassa, koska yksittäinen grib-tiedosto voi sisältää
    // vain yhden levelin dataa
    theQdatas[i]->Info()->FirstLevel();
    if (theQdatas[i]->Info()->Level()->LevelType() ==
        kFmiPressureLevel)  // pitää olla pressure tyyppiä
    {
      data = theQdatas[i];
      break;
    }
  }
  return data;
}

std::shared_ptr<NFmiQueryData> GetGroundData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas)
{
  std::shared_ptr<NFmiQueryData> data;
  for (size_t i = 0; i < theQdatas.size(); i++)
  {
    theQdatas[i]->Info()->FirstLevel();
    if (theQdatas[i]->Info()->Level()->LevelType() ==
        kFmiGroundSurface)  // pitää olla ground tyyppiä
    {
      data = theQdatas[i];
      break;
    }
  }
  return data;
}

// Jos surfacePressure tulee hPa:na, pitää se laskuissa se pitää muuttaa Pa:ksi.
// Palautetaan kuitenkin aina laskettu paine hPa-yksikössä.
float CalcHybridPressure(double a, double b, float surfacePressure)
{
  if (surfacePressure == kFloatMissing)
    return kFloatMissing;
  else
  {
    double pressureScale = 1.;
    if (surfacePressure < 1500)  // Tämä on karkea arvio, onko pintapaine hPa vai Pa yksikössä, jos
                                 // pintapaine oli hPa-yksikössä, pitää tehdä muunnoksia
      pressureScale = 100.;
    double value = a + b * surfacePressure * pressureScale;
    return static_cast<float>(value / 100.);  // paluu aina hPa:na
  }
}

void CalcHybridPressureData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas,
                            map<int, pair<double, double> > &theVerticalCoordinateMap,
                            const GeneratedHybridParamInfo &theHybridPressureInfo)
{
  if (theHybridPressureInfo.fCalcHybridParam)
  {
    // 1. lasketaan hybridi-dataan paine parametri jos löytyy pinta hybridi data listasta
    FmiParameterName pressureAtStationParId = theHybridPressureInfo.itsHelpParamId;
    FmiParameterName hybridPressureId =
        static_cast<FmiParameterName>(theHybridPressureInfo.itsGeneratedHybridParam.GetIdent());
    std::shared_ptr<NFmiQueryData> surfaceData = GetSurfaceData(theQdatas, pressureAtStationParId);
    std::shared_ptr<NFmiQueryData> hybridData = GetHybridData(theQdatas, hybridPressureId);
    if (surfaceData && hybridData)
    {
      NFmiFastQueryInfo surfaceInfo(surfaceData.get());
      surfaceInfo.First();
      NFmiFastQueryInfo hybridInfo(hybridData.get());
      if (surfaceInfo.Param(pressureAtStationParId) && hybridInfo.Param(hybridPressureId))
      {
        for (hybridInfo.ResetTime(); hybridInfo.NextTime();)
        {
          if (surfaceInfo.Time(hybridInfo.Time()))
          {
            for (hybridInfo.ResetLevel(); hybridInfo.NextLevel();)
            {
              int level = static_cast<int>(round(hybridInfo.Level()->LevelValue()));
              map<int, pair<double, double> >::iterator it = theVerticalCoordinateMap.find(level);
              if (it != theVerticalCoordinateMap.end())
              {
                double a = (*it).second.first;
                double b = (*it).second.second;
                for (hybridInfo.ResetLocation(); hybridInfo.NextLocation();)
                {
                  float surfacePressure = surfaceInfo.InterpolatedValue(hybridInfo.LatLon());
                  if (surfacePressure != kFloatMissing)
                    hybridInfo.FloatValue(CalcHybridPressure(a, b, surfacePressure));
                }
              }
            }
          }
        }
      }
    }
  }
}

void CalcRelativeHumidityData(FmiParameterName RH_id,
                              std::shared_ptr<NFmiQueryData> &theData,
                              const GeneratedHybridParamInfo &theHybridRelativeHumidityInfo,
                              const GeneratedHybridParamInfo &theHybridPressureInfo)
{
  // 1. lasketaan hybridi-dataan RH parametri jos löytyy ominaiskosteus parametri datasta
  if (theData)
  {
    FmiParameterName T_id = kFmiTemperature;
    FmiParameterName P_id =
        static_cast<FmiParameterName>(theHybridPressureInfo.itsGeneratedHybridParam.GetIdent());
    FmiParameterName SH_id = theHybridRelativeHumidityInfo.itsHelpParamId;

    NFmiFastQueryInfo RH_info(
        theData.get());  // RH info, suhteellinen kosteus, johon tulokset talletetaan
    RH_info.First();
    bool pressureData = RH_info.Level()->LevelType() == kFmiPressureLevel;

    NFmiFastQueryInfo T_info(RH_info);  // T info, mistä lämpötila
    T_info.First();
    NFmiFastQueryInfo P_info(RH_info);  // P info, mistä paine
    P_info.First();
    NFmiFastQueryInfo SH_info(RH_info);  // SH info, mistä specific humidity
    SH_info.First();

    if (RH_info.Param(RH_id) && T_info.Param(T_id) && SH_info.Param(SH_id) &&
        (pressureData || P_info.Param(P_id)))
    {
      for (RH_info.ResetTime(); RH_info.NextTime();)
      {
        // kaikki infot ovat samasta datasta, joten niissä on sama rakenne ja aika/level/location
        // asetukset ovat helppoja indeksien avulla
        P_info.TimeIndex(RH_info.TimeIndex());
        SH_info.TimeIndex(RH_info.TimeIndex());
        T_info.TimeIndex(RH_info.TimeIndex());

        for (RH_info.ResetLevel(); RH_info.NextLevel();)
        {
          P_info.LevelIndex(RH_info.LevelIndex());
          SH_info.LevelIndex(RH_info.LevelIndex());
          T_info.LevelIndex(RH_info.LevelIndex());

          for (RH_info.ResetLocation(); RH_info.NextLocation();)
          {
            float origRH = RH_info.FloatValue();
            if (origRH ==
                kFloatMissing)  // lasketaan RH:lle arvo, vain jos datassa ei ole jo RH:lle arvoa
            {
              P_info.LocationIndex(RH_info.LocationIndex());
              SH_info.LocationIndex(RH_info.LocationIndex());
              T_info.LocationIndex(RH_info.LocationIndex());

              float P = P_info.FloatValue();
              if (pressureData)
                P = P_info.Level()->LevelValue();
              float T = T_info.FloatValue();
              float SH = SH_info.FloatValue();

              float RH = ::CalcRH(P, T, SH);
              if (RH != kFloatMissing)
                RH_info.FloatValue(RH);
            }
          }
        }
      }
    }
    else
      std::cerr << "Error, couldn't deduce all the parameters needed in Relative Humidity "
                   "calculations for hybrid data.";
  }
}

// HUOM! Tämä pitää ajaa vasta jos ensin on laskettu paine parametri hybridi dataan!!!
void CalcRelativeHumidityData(vector<std::shared_ptr<NFmiQueryData> > &theQdatas,
                              const GeneratedHybridParamInfo &theHybridRelativeHumidityInfo,
                              const GeneratedHybridParamInfo &theHybridPressureInfo,
                              const GeneratedHybridParamInfo &theGroundPressureInfo)
{
  if (theHybridRelativeHumidityInfo.fCalcHybridParam)
  {
    FmiParameterName RH_id = static_cast<FmiParameterName>(
        theHybridRelativeHumidityInfo.itsGeneratedHybridParam.GetIdent());
    std::shared_ptr<NFmiQueryData> hybridData = GetHybridData(theQdatas, RH_id);
    CalcRelativeHumidityData(
        RH_id, hybridData, theHybridRelativeHumidityInfo, theHybridPressureInfo);
    std::shared_ptr<NFmiQueryData> pressureData = GetPressureData(theQdatas);
    CalcRelativeHumidityData(
        RH_id, pressureData, theHybridRelativeHumidityInfo, theHybridPressureInfo);
    std::shared_ptr<NFmiQueryData> groundData = GetGroundData(theQdatas);
    CalcRelativeHumidityData(
        RH_id, groundData, theHybridRelativeHumidityInfo, theGroundPressureInfo);
  }
}

void CreateQueryDatas(vector<GridRecordData *> &theGribRecordDatas,
                      GribFilterOptions &theGribFilterOptions,
                      map<int, pair<double, double> > *theVerticalCoordinateMap)
{
  if (theGribFilterOptions.fVerbose)
    cerr << "Creating querydatas" << endl;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    vector<NFmiHPlaceDescriptor> hPlaceDescriptors =
        GetAllHPlaceDescriptors(theGribRecordDatas, theGribFilterOptions.fUseOutputFile);
    vector<NFmiVPlaceDescriptor> vPlaceDescriptors =
        GetAllVPlaceDescriptors(theGribRecordDatas, theGribFilterOptions.fUseOutputFile);
    for (unsigned int j = 0; j < vPlaceDescriptors.size(); j++)
    {
      for (unsigned int i = 0; i < hPlaceDescriptors.size(); i++)
      {
        if (theGribFilterOptions.fVerbose)
          cerr << "L" << NFmiStringTools::Convert(j) << "H" << NFmiStringTools::Convert(i) << " ";
        std::shared_ptr<NFmiQueryData> qdata = CreateQueryData(
            theGribRecordDatas, hPlaceDescriptors[i], vPlaceDescriptors[j], theGribFilterOptions);
        if (qdata)
          theGribFilterOptions.itsGeneratedDatas.push_back(qdata);
      }
    }

    if (theVerticalCoordinateMap)
      CalcHybridPressureData(theGribFilterOptions.itsGeneratedDatas,
                             *theVerticalCoordinateMap,
                             theGribFilterOptions.itsHybridPressureInfo);
  }
}

void FreeDatas(vector<GridRecordData *> &theGribRecordDatas)
{
  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
    delete *it;
}

// stepRangeStr on muotoa "6-9", jolloin paluuarvo on 3. Jos stringi on mitenkään vääränlainen,
// palauu arvo on -1.
int GetStepRange(const std::string &stepRangeStr)
{
  try
  {
    std::vector<std::string> stepRangeVector = NFmiStringTools::Split(stepRangeStr, "-");
    if (stepRangeVector.size() == 2)
    {
      int range1 = NFmiStringTools::Convert<int>(stepRangeVector[0]);
      int range2 = NFmiStringTools::Convert<int>(stepRangeVector[1]);
      return range2 - range1;
    }
  }
  catch (...)
  {
  }
  return -1;
}

// Joillain parametreilla on annettu sama parametri eri skaaloissa, esim. sademäärä on vaikka 3h
// resolla tai 6h resolla tai kumulatiivinen.
// Tämän funktion on tarkoitus etsiä paras resoluutioinen niistä.
bool IsStepRangeCorrect(grib_handle *theHandle,
                        const NFmiDataIdent &theParam,
                        const std::set<unsigned long> &stepRangeCheckedParams,
                        int wantedStepRange)
{
  if (wantedStepRange)  // 0 on default arvo, jolloin ei tehdä mitään tarkasteluja
  {
    // 1. Tutkitaan onko käsittelyssä oleva parametri tutkittavien listassa
    const std::set<unsigned long>::iterator it =
        stepRangeCheckedParams.find(theParam.GetParamIdent());
    if (it != stepRangeCheckedParams.end())
    {
      // 2. Katsotaan löytyykö parametrille gribistä stepRange -määritys
      std::string stepRange;
      if (GetGribStringValue(theHandle, "stepRange", stepRange))
      {
        int stepRangeValue = GetStepRange(stepRange);
        if (stepRangeValue > 0)
        {
          if (wantedStepRange < 0)  // negatiivisen wantedStepRange:n avulla voidaan pyytää se
                                    // toinen parametri, jolla ei ole haluttua steppiä
          {
            if (stepRangeValue == -wantedStepRange)  // negatiivisen wantedStepRange:n avulla
              // voidaan pyytää se toinen parametri, jolla ei
              // ole haluttua steppiä
              return false;
          }
          else if (stepRangeValue != wantedStepRange)
            return false;  // Jos tulos oli ei-virheellinen, mutta ei haluttu, palautetaan false,
                           // eli tämä parametri on tarkoitus hylätä
        }
      }
    }
  }

  return true;  // Jos tänne päästään, on parametri ok
}

void ConvertGrib(GribFilterOptions &theGribFilterOptions)
{
  vector<GridRecordData *> gribRecordDatas;
  bool executionStoppingError = false;
  map<int, pair<double, double> > verticalCoordinateMap;

  try
  {
    grib_handle *gribHandle = nullptr;
    grib_context *gribContext = grib_context_get_default();
    grib_multi_support_on(0);

    int err = 0;
    int counter = 0;
    map<unsigned long, pair<NFmiParam, NFmiParam> > changedParams;
    map<unsigned long, NFmiParam> unchangedParams;
    NFmiMetTime firstValidTime;

    while ((gribHandle = grib_handle_new_from_file(
                gribContext, theGribFilterOptions.itsInputFile, &err)) != nullptr)
    {
      if (err != GRIB_SUCCESS)
        throw runtime_error("Failed to open grib handle in file  " +
                            theGribFilterOptions.itsInputFileNameStr);

      counter++;

      if (theGribFilterOptions.fVerbose)
        cerr << counter << " ";
      GridRecordData *tmpData = new GridRecordData;

      theGribFilterOptions.fDoYAxisFlip = jscan_is_negative(gribHandle);

      tmpData->itsLatlonCropRect = theGribFilterOptions.itsLatlonCropRect;
      try
      {
        // param ja level tiedot pitää hanskata ennen hilan koon määritystä
        //                PrintAllParamInfo_forDebugging(gribHandle);
        tmpData->itsParam = GetParam(gribHandle, theGribFilterOptions.itsWantedSurfaceProducer);
        tmpData->itsLevel = GetLevel(gribHandle);
        ExtractGridInfo(
            gribHandle, tmpData, theGribFilterOptions, theGribFilterOptions.itsGridSettings);
        tmpData->itsOrigTime = GetOrigTime(gribHandle);
        tmpData->itsValidTime = GetValidTime(gribHandle);
        tmpData->itsMissingValue = GetMissingValue(gribHandle);
        GetLevelVerticalCoordinates(gribHandle, *tmpData, verticalCoordinateMap);

        if (theGribFilterOptions.fVerbose)
        {
          cerr << tmpData->itsValidTime.ToStr("YYYYMMDDHHmm", kEnglish).CharPtr() << ";";
          cerr << tmpData->itsParam.GetParamName().CharPtr() << ";";
          cerr << tmpData->itsLevel.GetIdent() << ";";
          cerr << tmpData->itsLevel.LevelValue() << ";";
        }
        ChangeParamSettingsIfNeeded(
            theGribFilterOptions.itsParamChangeTable, tmpData, theGribFilterOptions.fVerbose);

        DoParamChecking(*tmpData, changedParams, unchangedParams, executionStoppingError);

        // filtteri jättää huomiotta ns. kontrolli hilan, joka on ainakin hirlam datassa 1.. se on
        // muista poikkeava 2x2 hila latlon-area.
        // Aiheuttaisi turhia ongelmia jatkossa monessakin paikassa.
        bool gribFieldUsed = false;

        if (!(tmpData->itsOrigGrid.itsNX <= 2 && tmpData->itsOrigGrid.itsNY <= 2))
        {
          if (IgnoreThisLevel(tmpData, theGribFilterOptions.itsIgnoredLevelList) == false)
          {
            if (AcceptThisLevelType(tmpData, theGribFilterOptions.itsAcceptOnlyLevelTypes))
            {
              if (CropParam(tmpData,
                            theGribFilterOptions.fCropParamsNotMensionedInTable,
                            theGribFilterOptions.itsParamChangeTable) == false)
              {
                if (IsStepRangeCorrect(gribHandle,
                                       tmpData->itsParam,
                                       theGribFilterOptions.itsStepRangeCheckedParams,
                                       theGribFilterOptions.itsWantedStepRange))
                {
                  FillGridData(gribHandle, tmpData, theGribFilterOptions);
                  gribRecordDatas.push_back(tmpData);  // taman voisi optimoida, luomalla aluksi
                                                       // niin iso vektori kuin tarvitaan
                  gribFieldUsed = true;
                }
                else if (theGribFilterOptions.fVerbose)
                  cerr << "\nWarning: Parameter was discarded due stepRange check" << endl;
              }
            }
          }
        }
        if (gribFieldUsed == false)
        {
          if (theGribFilterOptions.fVerbose)
            cerr << static_cast<long>(tmpData->itsParam.GetParamIdent()) << " (skipped)" << endl;
          delete tmpData;
        }
        else if (theGribFilterOptions.fVerbose)
          cerr << endl;
      }
      catch (Reduced_ll_grib_exception &)
      {
        delete tmpData;
        if (theGribFilterOptions.fIgnoreReducedLLData)
          continue;
        else
          throw;
      }
      catch (exception &e)
      {
        delete tmpData;
        if (executionStoppingError)
          throw;
        else
          cerr << "\nProblem with grib field " << NFmiStringTools::Convert(counter) << ":"
               << e.what() << endl;
      }
      catch (...)
      {
        delete tmpData;
        if (executionStoppingError)
          throw;
        else
          cerr << "\nUnknown problem with grib field " << NFmiStringTools::Convert(counter) << endl;
      }
      grib_handle_delete(gribHandle);
    }  // while-loop
    CreateQueryDatas(gribRecordDatas, theGribFilterOptions, &verticalCoordinateMap);

    if (err)
      throw runtime_error(grib_get_error_message(err));
  }
  catch (...)
  {
    FreeDatas(gribRecordDatas);
    throw;
  }

  FreeDatas(gribRecordDatas);
}

void ConvertOneGribFile(const GribFilterOptions &theGribFilterOptionsIn,
                        const string &theGribFileName)
{
  GribFilterOptions gribFilterOptionsLocal = theGribFilterOptionsIn;
  gribFilterOptionsLocal.itsInputFileNameStr = theGribFileName;
  if ((gribFilterOptionsLocal.itsInputFile =
           fopen(gribFilterOptionsLocal.itsInputFileNameStr.c_str(), "rb")) == nullptr)
  {
    cerr << "could not open input file: " << gribFilterOptionsLocal.itsInputFileNameStr << endl;
    return;
  }

  try
  {
    ConvertGrib(gribFilterOptionsLocal);
    gTotalQDataCollector.insert(gTotalQDataCollector.end(),
                                gribFilterOptionsLocal.itsGeneratedDatas.begin(),
                                gribFilterOptionsLocal.itsGeneratedDatas.end());
  }
  catch (Reduced_ll_grib_exception &)
  {
    if (gribFilterOptionsLocal.fIgnoreReducedLLData)
    {
      std::string errorStr(
          "Encountered reduced_ll grib data, ignoring it and won't use wgrib to unpack it");
      DoErrorReporting(
          "Error accured when converting grib-file:", "with error:", theGribFileName, 0, &errorStr);
    }
    else
    {
      // tyhjennetään mahd. jo kerätyt datat, koska *kaikki* grib konversiot tehdään uusiksi
      // wgrib-funktioilla
      gTotalQDataCollector.clear();
      if (gribFilterOptionsLocal.fVerbose)
        cerr << "Grib file has reduced_ll type of data, changing to use wgrib library, file was:\n"
             << gribFilterOptionsLocal.itsInputFileNameStr << endl;
      throw;
    }
  }
  catch (std::exception &e)
  {
    DoErrorReporting(
        "Error accured when converting grib-file:", "with error:", theGribFileName, &e);
  }
  catch (...)
  {
    DoErrorReporting("Unknown error accured when converting grib-file:", "", theGribFileName);
  }
}

// **************************************************************************
#include "wgrib_functions.h"

namespace wgrib2qd
{
NFmiLevel GetLevel(unsigned char *pds)
{
  int levelType = PDS_L_TYPE(pds);
  int highLevValue = PDS_LEVEL1(pds);
  int lowLevValue = PDS_LEVEL2(pds);
  float levelValue = static_cast<float>((highLevValue << 8) + lowLevValue);
  string levelName(NFmiStringTools::Convert(levelValue));
  return NFmiLevel(levelType, levelName, levelValue);
}

NFmiDataIdent GetParam(unsigned char *pds, NFmiProducer theWantedProducer)
{
  const char *nam = k5toa(pds);
  static_cast<void>(k5_comments(pds));
  int parId = PDS_PARAM(pds);
  return NFmiDataIdent(NFmiParam(parId,
                                 nam,
                                 kFloatMissing,
                                 kFloatMissing,
                                 kFloatMissing,
                                 kFloatMissing,
                                 "%.1f",
                                 kLinearly),
                       theWantedProducer);
}

NFmiMetTime GetGribTime(unsigned char *pds, bool fOrigTime)
{
  int year, month, day, hour;

  if (fOrigTime)
  {
    year = PDS_Year4(pds);
    month = PDS_Month(pds);
    day = PDS_Day(pds);
    hour = PDS_Hour(pds);
  }
  else  // valid time
  {
    if (verf_time(pds, &year, &month, &day, &hour) != 0)
      cerr << "Ongelma ajan purussa" << endl;
  }
  return NFmiMetTime(static_cast<short>(year),
                     static_cast<short>(month),
                     static_cast<short>(day),
                     static_cast<short>(hour),
                     0,
                     0);
}

// Tämä laittaa kaiken tiedon hilasta ja projektiosta stringiin.
string GetGridInfoStr(unsigned char *gds, unsigned char *bds)
{
  string str;
  string variyngRowLengthStr;
  if (gds)
  {
    long int nxny;
    int nx, ny;
    vector<long> variableLengthRows;
    long usedOutputGridRowLength =
        0;  // jos reduced grid (vaihtuva rivi leveys), tehdää tämän levyinen loppu hila
    GDS_grid(gds, bds, &nx, &ny, &nxny, variableLengthRows, &usedOutputGridRowLength);
    char buffer[1500] = "";

    if (GDS_LatLon(gds) && nx != -1)
    {
      sprintf(buffer,
              "  latlon: lat  %f to %f by %f  nxny %ld\n"
              "          long %f to %f by %f, (%d x %d) scan %d "
              "mode %d bdsgrid %d\n",
              0.001 * GDS_LatLon_La1(gds),
              0.001 * GDS_LatLon_La2(gds),
              0.001 * GDS_LatLon_dy(gds),
              nxny,
              0.001 * GDS_LatLon_Lo1(gds),
              0.001 * GDS_LatLon_Lo2(gds),
              0.001 * GDS_LatLon_dx(gds),
              nx,
              ny,
              GDS_LatLon_scan(gds),
              GDS_LatLon_mode(gds),
              BDS_Grid(bds));
    }
    else if (GDS_LatLon(gds) && nx == -1)
    {
      sprintf(buffer,
              "  thinned latlon: lat  %f to %f by %f  nxny %ld\n"
              "          long %f to %f, %ld grid pts   (%d x %d) scan %d"
              " mode %d bdsgrid %d\n",
              0.001 * GDS_LatLon_La1(gds),
              0.001 * GDS_LatLon_La2(gds),
              0.001 * GDS_LatLon_dy(gds),
              nxny,
              0.001 * GDS_LatLon_Lo1(gds),
              0.001 * GDS_LatLon_Lo2(gds),
              nxny,
              nx,
              ny,
              GDS_LatLon_scan(gds),
              GDS_LatLon_mode(gds),
              BDS_Grid(bds));
      //		  GDS_prt_thin_lon(gds);
      variyngRowLengthStr = "Variable row lengths are here: ";
      for (unsigned int i = 0; i < variableLengthRows.size(); i++)
      {
        variyngRowLengthStr += NFmiStringTools::Convert(variableLengthRows[i]);
        if (i < variableLengthRows.size() - 1)
          variyngRowLengthStr += ",";
      }
      variyngRowLengthStr += "\n";
    }
    else if (GDS_Gaussian(gds) && nx != -1)
    {
      sprintf(buffer,
              "  gaussian: lat  %f to %f\n"
              "            long %f to %f by %f, (%d x %d) scan %d"
              " mode %d bdsgrid %d\n",
              0.001 * GDS_LatLon_La1(gds),
              0.001 * GDS_LatLon_La2(gds),
              0.001 * GDS_LatLon_Lo1(gds),
              0.001 * GDS_LatLon_Lo2(gds),
              0.001 * GDS_LatLon_dx(gds),
              nx,
              ny,
              GDS_LatLon_scan(gds),
              GDS_LatLon_mode(gds),
              BDS_Grid(bds));
    }
    else if (GDS_Gaussian(gds) && nx == -1)
    {
      sprintf(buffer,
              "  thinned gaussian: lat  %f to %f\n"
              "     lon %f   %ld grid pts   (%d x %d) scan %d"
              " mode %d bdsgrid %d  nlat:\n",
              0.001 * GDS_LatLon_La1(gds),
              0.001 * GDS_LatLon_La2(gds),
              0.001 * GDS_LatLon_Lo1(gds),
              nxny,
              nx,
              ny,
              GDS_LatLon_scan(gds),
              GDS_LatLon_mode(gds),
              BDS_Grid(bds));
      //		  GDS_prt_thin_lon(gds);
    }
    else if (GDS_Polar(gds))
    {
      sprintf(buffer,
              "  polar stereo: Lat1 %f Long1 %f Orient %f\n"
              "     %s pole (%d x %d) Dx %d Dy %d scan %d mode %d\n",
              0.001 * GDS_Polar_La1(gds),
              0.001 * GDS_Polar_Lo1(gds),
              0.001 * GDS_Polar_Lov(gds),
              GDS_Polar_pole(gds) == 0 ? "north" : "south",
              nx,
              ny,
              GDS_Polar_Dx(gds),
              GDS_Polar_Dy(gds),
              GDS_Polar_scan(gds),
              GDS_Polar_mode(gds));
    }
    else if (GDS_Lambert(gds))
    {
      sprintf(buffer,
              "  Lambert Conf: Lat1 %f Lon1 %f Lov %f\n"
              "      Latin1 %f Latin2 %f LatSP %f LonSP %f\n"
              "      %s (%d x %d) Dx %f Dy %f scan %d mode %d\n",
              0.001 * GDS_Lambert_La1(gds),
              0.001 * GDS_Lambert_Lo1(gds),
              0.001 * GDS_Lambert_Lov(gds),
              0.001 * GDS_Lambert_Latin1(gds),
              0.001 * GDS_Lambert_Latin2(gds),
              0.001 * GDS_Lambert_LatSP(gds),
              0.001 * GDS_Lambert_LonSP(gds),
              GDS_Lambert_NP(gds) ? "North Pole" : "South Pole",
              GDS_Lambert_nx(gds),
              GDS_Lambert_ny(gds),
              0.001 * GDS_Lambert_dx(gds),
              0.001 * GDS_Lambert_dy(gds),
              GDS_Lambert_scan(gds),
              GDS_Lambert_mode(gds));
    }
    else if (GDS_Mercator(gds))
    {
      sprintf(buffer,
              "  Mercator: lat  %f to %f by %f km  nxny %ld\n"
              "          long %f to %f by %f km, (%d x %d) scan %d"
              " mode %d Latin %f bdsgrid %d\n",
              0.001 * GDS_Merc_La1(gds),
              0.001 * GDS_Merc_La2(gds),
              0.001 * GDS_Merc_dy(gds),
              nxny,
              0.001 * GDS_Merc_Lo1(gds),
              0.001 * GDS_Merc_Lo2(gds),
              0.001 * GDS_Merc_dx(gds),
              nx,
              ny,
              GDS_Merc_scan(gds),
              GDS_Merc_mode(gds),
              0.001 * GDS_Merc_Latin(gds),
              BDS_Grid(bds));
    }
    else if (GDS_ssEgrid(gds))
    {
      sprintf(buffer,
              "  Semi-staggered Arakawa E-Grid: lat0 %f lon0 %f nxny %d\n"
              "    dLat %f dLon %f (%d x %d) scan %d mode %d\n",
              0.001 * GDS_ssEgrid_La1(gds),
              0.001 * GDS_ssEgrid_Lo1(gds),
              GDS_ssEgrid_n(gds) * GDS_ssEgrid_n_dum(gds),
              0.001 * GDS_ssEgrid_dj(gds),
              0.001 * GDS_ssEgrid_di(gds),
              GDS_ssEgrid_Lo2(gds),
              GDS_ssEgrid_La2(gds),
              GDS_ssEgrid_scan(gds),
              GDS_ssEgrid_mode(gds));
    }
    else if (GDS_ss2dEgrid(gds))
    {
      sprintf(buffer,
              "  Semi-staggered Arakawa E-Grid (2D): lat0 %f lon0 %f nxny %d\n"
              "    dLat %f dLon %f (tlm0d %f tph0d %f) scan %d mode %d\n",
              0.001 * GDS_ss2dEgrid_La1(gds),
              0.001 * GDS_ss2dEgrid_Lo1(gds),
              GDS_ss2dEgrid_nx(gds) * GDS_ss2dEgrid_ny(gds),
              0.001 * GDS_ss2dEgrid_dj(gds),
              0.001 * GDS_ss2dEgrid_di(gds),
              0.001 * GDS_ss2dEgrid_Lo2(gds),
              0.001 * GDS_ss2dEgrid_La2(gds),
              GDS_ss2dEgrid_scan(gds),
              GDS_ss2dEgrid_mode(gds));
    }
    else if (GDS_fEgrid(gds))
    {
      sprintf(buffer,
              "  filled Arakawa E-Grid: lat0 %f lon0 %f nxny %d\n"
              "    dLat %f dLon %f (%d x %d) scan %d mode %d\n",
              0.001 * GDS_fEgrid_La1(gds),
              0.001 * GDS_fEgrid_Lo1(gds),
              GDS_fEgrid_n(gds) * GDS_fEgrid_n_dum(gds),
              0.001 * GDS_fEgrid_dj(gds),
              0.001 * GDS_fEgrid_di(gds),
              GDS_fEgrid_Lo2(gds),
              GDS_fEgrid_La2(gds),
              GDS_fEgrid_scan(gds),
              GDS_fEgrid_mode(gds));
    }
    else if (GDS_RotLL(gds))
    {
      sprintf(buffer,
              "  rotated LatLon grid  lat %f to %f  lon %f to %f\n"
              "    nxny %ld  (%d x %d)  dx %d dy %d  scan %d  mode %d\n"
              "    transform: south pole lat %f lon %f  rot angle %f\n",
              0.001 * GDS_RotLL_La1(gds),
              0.001 * GDS_RotLL_La2(gds),
              0.001 * GDS_RotLL_Lo1(gds),
              0.001 * GDS_RotLL_Lo2(gds),
              nxny,
              GDS_RotLL_nx(gds),
              GDS_RotLL_ny(gds),
              GDS_RotLL_dx(gds),
              GDS_RotLL_dy(gds),
              GDS_RotLL_scan(gds),
              GDS_RotLL_mode(gds),
              0.001 * GDS_RotLL_LaSP(gds),
              0.001 * GDS_RotLL_LoSP(gds),
              GDS_RotLL_RotAng(gds));
    }
    else if (GDS_Gnomonic(gds))
    {
      sprintf(buffer, "  Gnomonic grid\n");
    }
    else if (GDS_Harmonic(gds))
    {
      sprintf(buffer,
              "  Harmonic (spectral):  pentagonal spectral truncation: nj %d nk %d nm %d\n",
              GDS_Harmonic_nj(gds),
              GDS_Harmonic_nk(gds),
              GDS_Harmonic_nm(gds));
    }
    else
      sprintf(buffer, "  Tuntematon projektio.\n");
    /*
                    if (GDS_Harmonic_type(gds) == 1)
                    {
                      sprintf(buffer, "  Associated Legendre polynomials\n");
                    }
                    else if (GDS_Triangular(gds))
                    {
                            sprintf(buffer, "  Triangular grid:  nd %d ni %d (= 2^%d x 3^%d)\n",
                            GDS_Triangular_nd(gds), GDS_Triangular_ni(gds),
                                            GDS_Triangular_ni2(gds), GDS_Triangular_ni3(gds) );
                    }
    */
    str += buffer;
    str += variyngRowLengthStr;
  }
  else
    str += "There were no gds-data.";
  return str;
}

NFmiArea *CreateLatlonArea(unsigned char *gds,
                           unsigned char * /* bds */,
                           int scanIModePos,
                           int scanJModePos,
                           GribFilterOptions &theGribFilterOptions,
                           bool fDoYAxisFlip)
{
  double lo1 = 0.001 * GDS_LatLon_Lo1(gds);
  double lo2 = 0.001 * GDS_LatLon_Lo2(gds);
  double la1 = 0.001 * GDS_LatLon_La1(gds);
  double la2 = 0.001 * GDS_LatLon_La2(gds);

  DoPossibleGlobalLongitudeFixes(lo1, lo2, theGribFilterOptions);

  if (!scanIModePos)
    std::swap(lo1, lo2);
  if (!scanJModePos)
    std::swap(la1, la2);
  if (fDoYAxisFlip)
    std::swap(la1, la2);  // jos y-akselin kääntö, pitää la1 ja la2 kanssa kääntää
  else if (la1 > la2)
    std::swap(la1, la2);  // en tiedä miten pitäisi fiksata, mutta rjtp datan reduced_ll pitää vielä
                          // fiksata latitudien kanssa

  NFmiPoint bl(lo1, la1);
  NFmiPoint tr(lo2, la2);

  return NFmiAreaTools::CreateLegacyLatLonArea(bl, tr);
}

NFmiArea *CreateStereographicArea(unsigned char *gds, unsigned char *bds)
{
  int nx, ny;
  long int nxny;
  vector<long> variableLengthRows;
  long usedOutputGridRowLength =
      0;  // jos reduced grid (vaihtuva rivi leveys), tehdää tämän levyinen loppu hila
  GDS_grid(gds, bds, &nx, &ny, &nxny, variableLengthRows, &usedOutputGridRowLength);

  NFmiPoint bl(0.001 * GDS_Polar_Lo1(gds), 0.001 * GDS_Polar_La1(gds));
  double orientation = 0.001 * GDS_Polar_Lov(gds);
  double widthInMeters = GDS_Polar_Dx(gds) * (nx - 1);
  double heightInMeters = GDS_Polar_Dy(gds) * (ny - 1);
  double centralLatitude = GDS_Polar_pole(gds) == 0 ? 90. : -90;
  double trueLatitude = GDS_Polar_pole(gds) == 0 ? 60. : -60;

  auto proj = fmt::format(
      "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +R={:.0f} +units=m +wktext "
      "+towgs84=0,0,0 +no_defs",
      centralLatitude,
      trueLatitude,
      orientation,
      kRearth);

  return NFmiArea::CreateFromCornerAndSize(proj, "FMI", bl, widthInMeters, heightInMeters);
}

NFmiArea *CreateRotatedLatlonArea(unsigned char *gds, unsigned char * /* bds */, bool fDoYAxisFlip)
{
  double lo1 = 0.001 * GDS_RotLL_Lo1(gds);
  double lo2 = 0.001 * GDS_RotLL_Lo2(gds);
  double la1 = 0.001 * GDS_RotLL_La1(gds);
  double la2 = 0.001 * GDS_RotLL_La2(gds);
  if (fDoYAxisFlip)
    std::swap(la1, la2);  // jos y-akselin kääntö, pitää la1 ja la2 kanssa kääntää
  NFmiPoint bl(lo1, la1);
  NFmiPoint tr(lo2, la2);
  NFmiPoint southernPole(0.001 * GDS_RotLL_LoSP(gds), 0.001 * GDS_RotLL_LaSP(gds));

  auto proj = fmt::format(
      "+to_meter=.0174532925199433 +proj=ob_tran +o_proj=eqc +o_lon_p={} +o_lat_p={} "
      "+R={:.0f} +wktext +over +towgs84=0,0,0 +no_defs",
      southernPole.X(),
      -southernPole.Y(),
      kRearth);

  return NFmiArea::CreateFromBBox(proj, bl, tr);
}

NFmiArea *CreateMercatorArea(unsigned char *gds, unsigned char * /* bds */, bool fDoYAxisFlip)
{
  double lo1 = 0.001 * GDS_Merc_Lo1(gds);
  double lo2 = 0.001 * GDS_Merc_Lo2(gds);
  double la1 = 0.001 * GDS_Merc_La1(gds);
  double la2 = 0.001 * GDS_Merc_La2(gds);
  if (fDoYAxisFlip)
    std::swap(la1, la2);  // jos y-akselin kääntö, pitää la1 ja la2 kanssa kääntää
  NFmiPoint bl(lo1, la1);
  NFmiPoint tr(lo2, la2);

  return NFmiAreaTools::CreateLegacyMercatorArea(bl, tr);
}

NFmiArea *CreateArea(unsigned char *gds,
                     unsigned char *bds,
                     int scanIModePos,
                     int scanJModePos,
                     GribFilterOptions &theGribFilterOptions,
                     bool fDoYAxisFlip)
{
  NFmiArea *area = 0;
  if (GDS_LatLon(gds))
    area = wgrib2qd::CreateLatlonArea(
        gds, bds, scanIModePos, scanJModePos, theGribFilterOptions, fDoYAxisFlip);
  else
  {
    if (GDS_Polar(gds))
      area = wgrib2qd::CreateStereographicArea(gds, bds);
    else if (GDS_RotLL(gds))
      area = wgrib2qd::CreateRotatedLatlonArea(gds, bds, fDoYAxisFlip);
    else if (GDS_Mercator(gds))
      area = wgrib2qd::CreateMercatorArea(gds, bds, fDoYAxisFlip);
    //		else if(GDS_Lambert(gds)) // HUOM! tämä on kokeilu feikkiä, ota tämä pois ja
    // lambertin
    // projektio pitäisi hanskata
    //			area = wgrib2qd::CreateLatlonArea(gds, bds, scanIModePos, scanJModePos,
    // doGlobeFix);
  }

  if (!area)
  {
    string errStr("Kyseistä projektiota/grid:ia ei (vielä) voida konvertoida ohjelmalla.\n");
    errStr += wgrib2qd::GetGridInfoStr(gds, bds);
    throw runtime_error(errStr);
  }

  return area;
}

void FillGridInfo(unsigned char *gds,
                  unsigned char *bds,
                  MyGrid &theGrid,
                  int &scanIModePos,
                  int &scanJModePos,
                  int &adjacentIMode,
                  GribFilterOptions &theGribFilterOptions,
                  bool fDoYAxisFlip)
{
  // TABLE 8. SCANNING MODE FLAG
  // (GDS Octet 28)
  // BIT 	VALUE 	MEANING
  // 1 	0 	Points scan in +i direction
  //  	1 	Points scan in -i direction
  // 2 	0 	Points scan in -j direction
  //  	1 	Points scan in +j direction
  // 3 	0 	Adjacent points in i direction are consecutive
  //  		(FORTRAN: (I,J))
  //  	1 	Adjacent points in j direction are consecutive
  //  		(FORTRAN: (J,I))
  static const int i_scan_mode_bit = 128;
  static const int j_scan_mode_bit = 64;
  static const int scan_mode_ij_adjacent_order_bit = 32;
  int GDSscanMode = GDS_LatLon_scan(gds);
  scanIModePos = (GDSscanMode & i_scan_mode_bit) == 0;
  scanJModePos = (GDSscanMode & j_scan_mode_bit) ? 0 : 1;  // jos 2. bitti päällä (64), n +j suunta,
                                                           // muuten -j suunta ja +j suunta on
  // etelästä pohjoiseen kuten newbase:ssa
  adjacentIMode = (GDSscanMode & scan_mode_ij_adjacent_order_bit) == 0;
  NFmiArea *area = wgrib2qd::CreateArea(
      gds, bds, scanIModePos, scanJModePos, theGribFilterOptions, fDoYAxisFlip);
  int nx, ny;
  long int nxny;
  vector<long> variableLengthRows;
  long usedOutputGridRowLength =
      0;  // jos reduced grid (vaihtuva rivi leveys), tehdää tämän levyinen loppu hila
  GDS_grid(gds, bds, &nx, &ny, &nxny, variableLengthRows, &usedOutputGridRowLength);

  theGrid = MyGrid(area, nx == -1 ? usedOutputGridRowLength : nx, ny);
}

float CheckAndFixMissingValues(float value)
{
  if (value >= UNDEFINED && value <= 9.9990004e+020)  // piti laittaa undefined (=gribin puuttuva
                                                      // arvolle haarukka, koska ilmeisesti
                                                      // laskuissa tulee pientä epätarkkuutta)
    return kFloatMissing;
  else
    return value;
}

void InterpolateRowData(vector<float> &theSourceValues, vector<float> &theDestValues)
{
  float ratio = (theSourceValues.size() - 1) / static_cast<float>(theDestValues.size() - 1);
  if (ratio == 1)
    theDestValues = theSourceValues;
  else
  {
    for (unsigned int i = 0; i < theDestValues.size() - 1; i++)
    {
      float relativePos = ratio * i;
      unsigned int lowerIndex = static_cast<unsigned int>(relativePos);
      float relativePosRemains =
          relativePos -
          floor(ratio *
                i);  // otetaan desimaali osa irti sijainnista niin saadaan interpolointi kerroin
      float value1 = theSourceValues[lowerIndex];
      float value2 = theSourceValues[lowerIndex + 1];
      double interpolatedValue = NFmiInterpolation::Linear(relativePosRemains, value1, value2);
      theDestValues[i] = static_cast<float>(interpolatedValue);
    }
    theDestValues[theDestValues.size() - 1] = theSourceValues[theSourceValues.size() - 1];
  }
}

// TODO ei osaa vielä hanskata scanmodeja
void FillGridDataWithVariableLengthData(float *theArray,
                                        GridRecordData *theGribData,
                                        int /* scanIModePos */,
                                        int /* scanJModePos */,
                                        int /* adjacentIMode */,
                                        vector<long> &theVariableLengthRows)
{
  NFmiDataMatrix<float> &gridData = theGribData->itsGridData;
  NFmiArea *area = theGribData->itsGrid.itsArea;
  NFmiRect rect(area->XYArea());

  vector<float> rowValues;                       // vaihtuva rivisen datan yhden rivin arvot
  vector<float> matrixRowValues(gridData.NX());  // normaalin datamatriisiin laskettavat arvot
                                                 // (lasketaan siis vaihtuvan pituisien rivien datan
                                                 // avulla)
  long totalArrayCounter = 0;
  for (unsigned int row = 0; row < theVariableLengthRows.size(); row++)
  {
    // Täytetään ensin vaihtuva pituisen rivin data ja niiden suhteelliset sijainnit.
    long rowLength = theVariableLengthRows[row];
    if (rowLength == 0)
      throw runtime_error("Zero division in FillGridDataWithVariableLengthData-function.");

    rowValues.resize(rowLength);
    for (long i = 0; i < rowLength; i++)
    {
      rowValues[i] = wgrib2qd::CheckAndFixMissingValues(theArray[totalArrayCounter]);
      totalArrayCounter++;
    }
    // laske eri pituisen datan rivista matriisiin rivi
    wgrib2qd::InterpolateRowData(rowValues, matrixRowValues);
    // talleta rivi tulos matriisiin.
    for (unsigned int j = 0; j < matrixRowValues.size(); j++)
      gridData[j][row] = matrixRowValues[j];
  }
}

void MakeParameterConversions(GridRecordData *theGridRecordData,
                              vector<ParamChangeItem> &theParamChangeTable)
{
  if (theParamChangeTable.size() > 0)
  {  // tehdään tarvittaessa parametrille base+scale muunnos
    for (unsigned int i = 0; i < theParamChangeTable.size(); i++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[i];
      if (paramChangeItem.itsWantedParam.GetIdent() ==
          static_cast<long>(theGridRecordData->itsParam.GetParamIdent()))  // parametri on jo
                                                                           // muutettu, nyt
                                                                           // katsotaan onko
                                                                           // wantedparam sama
      {
        if (paramChangeItem.itsConversionBase != 0 || paramChangeItem.itsConversionScale != 1)
        {
          int nx = static_cast<int>(theGridRecordData->itsGridData.NX());
          int ny = static_cast<int>(theGridRecordData->itsGridData.NY());
          for (int j = 0; j < ny; j++)
          {
            for (int i = 0; i < nx; i++)
              theGridRecordData->itsGridData[i][j] =
                  paramChangeItem.itsConversionBase +
                  (theGridRecordData->itsGridData[i][j] * paramChangeItem.itsConversionScale);
          }
          break;
        }
      }
    }
  }
}

void FillGridData(float *theArray,
                  GridRecordData *theGribData,
                  int scanIModePos,
                  int scanJModePos,
                  int adjacentIMode,
                  GribFilterOptions &theGribFilterOptions,
                  vector<long> &theVariableLengthRows,
                  vector<ParamChangeItem> &theParamChangeTable,
                  bool zigzagMode)
{
  NFmiDataMatrix<float> &gridData = theGribData->itsGridData;

  if (theVariableLengthRows.size() > 0)
    wgrib2qd::FillGridDataWithVariableLengthData(
        theArray, theGribData, scanIModePos, scanJModePos, adjacentIMode, theVariableLengthRows);
  else
  {
    int nx = static_cast<int>(gridData.NX());
    int ny = static_cast<int>(gridData.NY());
    int nxny = nx * ny;

    if (zigzagMode)  // pika viritys lapps-dataa varten (en tiedä mistä tämän voisi tulkita)
    {
      // vasemmalta oikealle ja oikealta vasemmalle täyttö. Rivit alhaalta ylös.
      int xInd = 0;
      int yInd = 0;
      for (int i = 0; i < nxny; i++)
      {
        if ((i / nx) % 2 == 0)
        {
          xInd = i % nx;
          yInd = i / nx;
        }
        else
        {
          xInd = nx - (i % nx) - 1;
          yInd = i / nx;
        }
        gridData[xInd][yInd] = wgrib2qd::CheckAndFixMissingValues(theArray[i]);
      }
    }
    else if (scanIModePos && scanJModePos && adjacentIMode)
    {
      // normaali i ja j suunnat positiivisia ja i peräkkäin
      // eli ts. vasen alakulma alkaa rivi kerralaan ylöspäin.
      for (int i = 0; i < nxny; i++)
        gridData[i % nx][i / nx] = wgrib2qd::CheckAndFixMissingValues(theArray[i]);
    }
    else if (scanIModePos && (!scanJModePos) && adjacentIMode)
    {
      // käänteinen y-akseli eli i positiivinen ja j negatiivinen ja i peräkkäin
      // eli ts. vasen yläkulma alkaa rivi kerralaan alaspäin.
      for (int i = 0; i < nxny; i++)
        gridData[i % nx][ny - (i / nx) - 1] = wgrib2qd::CheckAndFixMissingValues(theArray[i]);
    }
    else
      throw runtime_error(
          "Unsupported scanmode in FillGridData-function, someone should implement it...");
  }

  if (theGribFilterOptions.DoGlobalFix())
  {
    // Nyt on siis tilanne että halutaan 'korjata' globaali data editoria varten.
    // Latlon-area peittää maapallon longitudeissa 0-360, se pitää muuttaa editoria varten
    // longitudeihin -180 - 180.
    // Eli matriisissa olevia arvoja pitää siirtää niin että vasemmalla puoliskolla olevat laitetaan
    // oikealle
    // puolelle ja toisin päin.
    int nx = static_cast<int>(gridData.NX());
    int ny = static_cast<int>(gridData.NY());
    for (int j = 0; j < ny; j++)
      for (int i = 0; i < nx / 2; i++)
        std::swap(gridData[i][j], gridData[i + nx / 2][j]);
  }

  // Tarkista vielä, jos löytyy paramChangeTablesta parametrille muunnos kaavat jotka pitää tehdä
  wgrib2qd::MakeParameterConversions(theGribData, theParamChangeTable);
}

struct GridLessThan
{
  bool operator()(const MyGrid &lhs, const MyGrid &rhs) const { return lhs < rhs; }
};

vector<NFmiHPlaceDescriptor> GetAllHPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile)
{
  set<MyGrid, wgrib2qd::GridLessThan> gribDataSet;

  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
  {
    GridRecordData *bs = *it;
    gribDataSet.insert(bs->itsGrid);
  }

  vector<NFmiHPlaceDescriptor> hPlaces;

  if (useOutputFile)
  {
    set<MyGrid, wgrib2qd::GridLessThan>::iterator it2 = gribDataSet.begin();
    for (; it2 != gribDataSet.end(); ++it2)
    {
      NFmiGrid grid((*it2).itsArea, (*it2).itsNX, (*it2).itsNY);
      hPlaces.push_back(NFmiHPlaceDescriptor(grid));
    }
  }
  else
  {  // tulostus tehdään cout:iin, ja tehdään vain yksi data, toivottavasti se on se merkitsevin,
     // tässä yritetään jättää huomiotta ainakin
    // hirlam gribeissä esiintyvä 2x2 kokoinen yksi hila
    MyGrid &tmpGrid = theGribRecordDatas[theGribRecordDatas.size() > 1 ? 1 : 0]->itsGrid;
    NFmiGrid grid(tmpGrid.itsArea, tmpGrid.itsNX, tmpGrid.itsNY);
    hPlaces.push_back(NFmiHPlaceDescriptor(grid));
  }
  return hPlaces;
}

NFmiVPlaceDescriptor MakeVPlaceDescriptor(vector<GridRecordData *> &theGribRecordDatas,
                                          int theLevelType)
{
  unsigned int i = 0;
  set<NFmiLevel, LevelLessThan> levelSet;
  for (i = 0; i < theGribRecordDatas.size(); i++)
  {
    if (theGribRecordDatas[i]->itsLevel.LevelType() == theLevelType)
    {
      const NFmiLevel &tmpLevel = theGribRecordDatas[i]->itsLevel;
      levelSet.insert(tmpLevel);
    }
  }
  NFmiLevelBag levelBag;
  set<NFmiLevel, LevelLessThan>::iterator it = levelSet.begin();
  for (; it != levelSet.end(); ++it)
    levelBag.AddLevel(*it);
  return NFmiVPlaceDescriptor(levelBag);
}

map<int, int>::iterator FindHighesLevelType(map<int, int> levelTypeCounter)
{
  map<int, int>::iterator it = levelTypeCounter.begin();
  map<int, int>::iterator highestIt = levelTypeCounter.end();
  for (; it != levelTypeCounter.end(); ++it)
  {
    if (highestIt == levelTypeCounter.end())
      highestIt = it;
    else if ((*highestIt).second < (*it).second)
      highestIt = it;
  }
  return highestIt;
}

// tehdään levelbagi kaikista eri tyyppisistä leveleistä.
vector<NFmiVPlaceDescriptor> GetAllVPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile)
{
  // 1. etsitään kaikki erilaiset levelit set:in avulla
  set<NFmiLevel, LevelLessThan> levelSet;
  map<int, int> levelTypeCounter;

  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
  {
    GridRecordData *bs = *it;
    levelSet.insert(bs->itsLevel);
    levelTypeCounter[bs->itsLevel.LevelType()]++;  // kikka vitonen: tämä laskee erityyppiset
                                                   // levelit
  }

  vector<NFmiVPlaceDescriptor> vPlaces;

  if (useOutputFile)
  {
    map<int, int>::iterator it = levelTypeCounter.begin();
    for (; it != levelTypeCounter.end(); ++it)
    {
      NFmiVPlaceDescriptor vDesc = wgrib2qd::MakeVPlaceDescriptor(theGribRecordDatas, (*it).first);
      if (vDesc.Size() > 0)
        vPlaces.push_back(vDesc);
    }
  }
  else
  {  // tulostus tehdään cout:iin, ja tehdään vain yksi data, toivottavasti se on se merkitsevin,
     // eli otetaan se minkä tyyppisiä esiintyi eniten
    map<int, int>::iterator it = wgrib2qd::FindHighesLevelType(levelTypeCounter);
    if (it != levelTypeCounter.end())
    {
      NFmiVPlaceDescriptor vDesc = wgrib2qd::MakeVPlaceDescriptor(theGribRecordDatas, (*it).first);
      if (vDesc.Size() > 0)
        vPlaces.push_back(vDesc);
    }
  }
  return vPlaces;
}

// Etsitään ne parametrit mitkä löytyvät data-setistä.
// Lisäksi hilan ja arean pitää olla sama kuin annetussa hplaceDescriptorissa ja level-tyypin pitää
// olla
// sama kuin vplaceDescriptorissa.
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace)
{
  // Ensin pitää saada 1. levelin level-type talteen vplaceDescriptorista, sillä otamme vain
  // parametreja, mitä löytyy sellaisista hila kentistä missä on tälläinen level id.
  FmiLevelType wantedLevelType = theVplace.Levels()->Level(0)->LevelType();
  set<int> parIds;  // set:in avulla selvitetaan kuinka monta erilaista identtia loytyy
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
  {
    if (theGribRecordDatas[i]->itsGrid == *(theHplace.Grid()))
    {
      if (theGribRecordDatas[i]->itsLevel.LevelType() == wantedLevelType)
        parIds.insert(theGribRecordDatas[i]->itsParam.GetParamIdent());
    }
  }

  NFmiParamBag parBag;
  set<int>::iterator it = parIds.begin();
  for (; it != parIds.end(); ++it)
    parBag.Add(FindFirstParam(*it, theGribRecordDatas));

  return NFmiParamDescriptor(parBag);
}

bool FillQDataWithGribRecords(NFmiQueryData &theQData, vector<GridRecordData *> &theGribRecordDatas)
{
  NFmiFastQueryInfo info(&theQData);
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  GridRecordData *tmp = 0;
  int filledGridCount = 0;
  for (int i = 0; i < gribCount; i++)
  {
    tmp = theGribRecordDatas[i];
    if (tmp->itsGrid == *info.Grid())  // vain samanlaisia hiloja laitetaan samaan qdataan
    {
      if (info.Time(tmp->itsValidTime) && info.Level(tmp->itsLevel) && info.Param(tmp->itsParam))
      {
        if (!info.SetValues(tmp->itsGridData))
          throw runtime_error("qdatan täyttö gribi datalla epäonnistui, lopetetaan...");
        filledGridCount++;
      }
      //			else
      //				throw runtime_error("qdatan täyttö ohjelma ei saanut
      // asetettua aikaa/parametria/leveliä, lopetetaan...");
    }
  }
  return filledGridCount > 0;
}

// HUOM! jos datassa on 'outoja' valid-aikoja esim. 1919 jne., joita näyttää tulevan esim.
// hirlamista liittyen johonkin
// kontrolli grideihin (2x2 hila ja muuta outoa). Tälläiset hilat jätetään huomiotta.
NFmiTimeDescriptor GetTimeDesc(vector<GridRecordData *> &theGribRecordDatas)
{
  // set:in avulla selvitetaan kuinka monta erilaista timea loytyy.
  set<NFmiMetTime> timesSet;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    timesSet.insert(theGribRecordDatas[i]->itsValidTime);

  // Tehdaan aluksi timelist, koska se on helpompi,
  // myohemmin voi miettia saisiko aikaan timebagin.
  NFmiTimeList timeList;
  set<NFmiMetTime>::iterator it = timesSet.begin();
  NFmiMetTime dummyTime(1950, 1, 1);  // laitoin tälläisen dummytime rajoittimen, koska Pekon
                                      // antamassa datassa oli aika 1919 vuodelta ja se ja
                                      // nykyaikainen aika sekoitti mm. metkun editorin pahasti
  for (; it != timesSet.end(); ++it)
    if (*it > dummyTime)
      timeList.Add(new NFmiMetTime(*it));

  timeList.First();
  return NFmiTimeDescriptor(*timeList.Current(), timeList);
}

std::shared_ptr<NFmiQueryData> CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                                               NFmiHPlaceDescriptor &theHplace,
                                               NFmiVPlaceDescriptor &theVplace)
{
  std::shared_ptr<NFmiQueryData> qdata;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    NFmiParamDescriptor params(wgrib2qd::GetParamDesc(theGribRecordDatas, theHplace, theVplace));
    NFmiTimeDescriptor times(wgrib2qd::GetTimeDesc(theGribRecordDatas));
    if (params.Size() == 0 || times.Size() == 0)
      return qdata;  // turha jatkaa jos toinen näistä on tyhjä
    NFmiQueryInfo innerInfo(params, times, theHplace, theVplace);
    qdata.reset(NFmiQueryDataUtil::CreateEmptyData(innerInfo));
    bool anyDataFilled = wgrib2qd::FillQDataWithGribRecords(*qdata, theGribRecordDatas);
    if (anyDataFilled == false)
    {
      qdata.reset();
    }
  }
  return qdata;
}

vector<std::shared_ptr<NFmiQueryData> > CreateQueryDatas(
    vector<GridRecordData *> &theGribRecordDatas, bool useOutputFile)
{
  vector<std::shared_ptr<NFmiQueryData> > qdatas;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    vector<NFmiHPlaceDescriptor> hPlaceDescriptors =
        wgrib2qd::GetAllHPlaceDescriptors(theGribRecordDatas, useOutputFile);
    vector<NFmiVPlaceDescriptor> vPlaceDescriptors =
        wgrib2qd::GetAllVPlaceDescriptors(theGribRecordDatas, useOutputFile);
    for (unsigned int j = 0; j < vPlaceDescriptors.size(); j++)
    {
      for (unsigned int i = 0; i < hPlaceDescriptors.size(); i++)
      {
        std::shared_ptr<NFmiQueryData> qdata = wgrib2qd::CreateQueryData(
            theGribRecordDatas, hPlaceDescriptors[i], vPlaceDescriptors[j]);
        if (qdata)
          qdatas.push_back(qdata);
      }
    }
  }
  return qdatas;
}

void FreeDatas(vector<GridRecordData *> &theGribRecordDatas, float *array, unsigned char *buffer)
{
  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
    delete *it;

  if (array)
    free(array);
  if (buffer)
    free(buffer);
}

void ConvertGrib(GribFilterOptions &theGribFilterOptions)
{
  vector<std::shared_ptr<NFmiQueryData> > datas;
  unsigned char *buffer, *msg, *pds, *gds, *bms = 0, *bds, *pointer;
  long int len_grib, pos = 0, nxny = 0, last_nxny = 0, buffer_size, count = 1;
  int nx, ny;
  double temp;
  float *array = 0;
  vector<long> variableLengthRows;
  long usedOutputGridRowLength =
      0;  // jos reduced grid (vaihtuva rivi leveys), tehdää tämän levyinen loppu hila

  vector<GridRecordData *> gribRecordDatas;

  if ((buffer = (unsigned char *)malloc(BUFF_ALLOC0)) == nullptr)
    throw runtime_error("buffer 0, not enough memory\n");
  buffer_size = BUFF_ALLOC0;

  bool executionStoppingError = false;
  map<unsigned long, pair<NFmiParam, NFmiParam> > changedParams;
  map<unsigned long, NFmiParam> unchangedParams;

  try
  {
    int gdsPrintCounter = 0;  // kuinkan monta on jo printattu
    int gdsCounter = 0;
    int bmsCounter = 0;
    for (;;)
    {
      msg = seek_grib(theGribFilterOptions.itsInputFile, &pos, &len_grib, buffer, MSEEK);
      if (msg == nullptr)
        break;  // tultiin recordien loppuun

      /* read all whole grib record */
      if (len_grib + msg - buffer > buffer_size)
      {
        buffer_size = static_cast<long>(len_grib + msg - buffer + 1000);
        buffer = (unsigned char *)realloc((void *)buffer, buffer_size);
        if (buffer == nullptr)
          throw runtime_error("ran out of memory");
      }
      read_grib(theGribFilterOptions.itsInputFile, pos, len_grib, buffer);

      // parse grib message
      msg = buffer;
      pds = (msg + 8);
      pointer = pds + PDS_LEN(pds);
      if (PDS_HAS_GDS(pds))
      {
        gdsCounter++;
        gds = pointer;
        pointer += GDS_LEN(gds);
      }
      else
      {
        gds = nullptr;
      }

      if (PDS_HAS_BMS(pds))
      {
        bmsCounter++;
        //			throw runtime_error("gribissä oli bitmap tavaraa, jota tämä ohjelnma
        // ei
        // hoida, lopetetaan!");
        bms = pointer;
        pointer += BMS_LEN(bms);
        //			continue;
      }
      else
      {
        bms = nullptr;
      }

      bds = pointer;
      pointer += BDS_LEN(bds);

      // tulostetaan err:iin gds infot, jos niin on haluttu

      if (theGribFilterOptions.itsGridInfoPrintCount < 0 ||
          gdsPrintCounter < theGribFilterOptions.itsGridInfoPrintCount)
      {
        cerr << "GDS Nr. " << gdsPrintCounter + 1 << endl;
        cerr << wgrib2qd::GetGridInfoStr(gds, bds) << endl;
        gdsPrintCounter++;
      }

      // end section - "7777" in ascii
      if (pointer[0] != 0x37 || pointer[1] != 0x37 || pointer[2] != 0x37 || pointer[3] != 0x37)
      {
        cerr << "Warning: Missing end section 7777, trying to continue to make data..." << endl;
        break;
        //				throw runtime_error("Missing end section 7777,
        // lopetetaan!");
      }

      // figure out size of array
      if (gds != nullptr)
      {
        GDS_grid(gds, bds, &nx, &ny, &nxny, variableLengthRows, &usedOutputGridRowLength);
      }
      else if (bms != nullptr)
      {
        nxny = nx = BMS_nxny(bms);
        ny = 1;
      }
      else
      {
        if (BDS_NumBits(bds) == 0)
        {
          nxny = nx = 1;
          fprintf(stderr,
                  "Missing GDS, constant record .. cannot determine number of data points\n");
        }
        else
        {
          nxny = nx = BDS_NValues(bds);
        }
        ny = 1;
      }

      if (nxny > last_nxny)  // tarkistetaan pitääkö varata isompi taulukko
      {
        if (array)
          free(array);
        if ((array = (float *)malloc(sizeof(float) * nxny)) == nullptr)
          throw runtime_error("End of memory, exiting...");
        last_nxny = nxny;
      }

      temp = int_power(10.0, -PDS_DecimalScale(pds));
      int n_bits = BDS_NumBits(bds);
      float pureRefValue = static_cast<float>(BDS_RefValue(bds));
      if (n_bits == 0)
      {  // tämä vakiokenttä tapaus on ilmeisesti erikoistapaus ja siinä arvoiksi laitetaan kaikkiin
        // suoraan referenssi-arvo (kun taas else haarassa referenssi-arvo pitää skaalata)
        for (long i = 0; i < nxny; i++)
          array[i] = pureRefValue;
      }
      else
        BDS_unpack(array,
                   bds,
                   BMS_bitmap(bms),
                   n_bits,
                   nxny,
                   temp * pureRefValue,
                   temp * int_power(2.0, BDS_BinScale(bds)));

      pos += len_grib;
      count++;

      GridRecordData *tmpData = new GridRecordData;

      tmpData->itsOrigTime = wgrib2qd::GetGribTime(pds, true);
      tmpData->itsValidTime = wgrib2qd::GetGribTime(pds, false);
      tmpData->itsParam = wgrib2qd::GetParam(pds, theGribFilterOptions.itsWantedSurfaceProducer);
      tmpData->itsLevel = wgrib2qd::GetLevel(pds);
      int scanIModePos = 0;   // ks. FillGridInfo-funktiosta kommentteja
      int scanJModePos = 0;   // ks. FillGridInfo-funktiosta kommentteja
      int adjacentIMode = 0;  // ks. FillGridInfo-funktiosta kommentteja
      wgrib2qd::FillGridInfo(gds,
                             bds,
                             tmpData->itsGrid,
                             scanIModePos,
                             scanJModePos,
                             adjacentIMode,
                             theGribFilterOptions,
                             theGribFilterOptions.fDoYAxisFlip);
      if (theGribFilterOptions.fDoYAxisFlip)
        scanJModePos = scanJModePos == false;
      tmpData->itsGridData.Resize(tmpData->itsGrid.itsNX, tmpData->itsGrid.itsNY);

      ChangeParamSettingsIfNeeded(
          theGribFilterOptions.itsParamChangeTable, tmpData, theGribFilterOptions.fVerbose);

      DoParamChecking(*tmpData, changedParams, unchangedParams, executionStoppingError);

      // filtteri jättää huomiotta ns. kontrolli hilan, joka on ainakin hirlam datassa 1.. se on
      // muista poikkeava 2x2 hila latlon-area.
      // Aiheuttaisi turhia ongelmia jatkossa monessakin paikassa.
      bool gribFieldUsed = false;
      if (!(tmpData->itsGrid.itsNX <= 2 && tmpData->itsGrid.itsNY <= 2))
      {
        if (IgnoreThisLevel(tmpData, theGribFilterOptions.itsIgnoredLevelList) == false)
        {
          if (AcceptThisLevelType(tmpData, theGribFilterOptions.itsAcceptOnlyLevelTypes))
          {
            if (CropParam(tmpData,
                          theGribFilterOptions.fCropParamsNotMensionedInTable,
                          theGribFilterOptions.itsParamChangeTable) == false)
            {
              wgrib2qd::FillGridData(array,
                                     tmpData,
                                     scanIModePos,
                                     scanJModePos,
                                     adjacentIMode,
                                     theGribFilterOptions,
                                     variableLengthRows,
                                     theGribFilterOptions.itsParamChangeTable,
                                     theGribFilterOptions.fDoZigzagMode);
              gribRecordDatas.push_back(tmpData);  // taman voisi optimoida, luomalla aluksi niin
                                                   // iso vektori kuin tarvitaan
              gribFieldUsed = true;
            }
          }
        }
      }
      if (gribFieldUsed == false)
      {
        delete tmpData;
        if (theGribFilterOptions.fVerbose)
        {
          cerr << " (skipped)" << endl;
        }
      }
      else
      {
        if (theGribFilterOptions.fVerbose)
          cerr << endl;
      }

    }  // for-loop

    CreateQueryDatas(gribRecordDatas, theGribFilterOptions, 0);
  }  // end of try
  catch (...)
  {
    wgrib2qd::FreeDatas(gribRecordDatas, array, buffer);
    throw;
  }

  wgrib2qd::FreeDatas(gribRecordDatas, array, buffer);
}

void ConvertOneGribFile(const GribFilterOptions &theGribFilterOptionsIn,
                        const string &theGribFileName)
{
  GribFilterOptions gribFilterOptionsLocal = theGribFilterOptionsIn;
  gribFilterOptionsLocal.itsInputFileNameStr = theGribFileName;
  if ((gribFilterOptionsLocal.itsInputFile =
           fopen(gribFilterOptionsLocal.itsInputFileNameStr.c_str(), "rb")) == nullptr)
  {
    cerr << "could not open input file: " << gribFilterOptionsLocal.itsInputFileNameStr << endl;
    return;
  }

  try
  {
    wgrib2qd::ConvertGrib(gribFilterOptionsLocal);
    gTotalQDataCollector.insert(gTotalQDataCollector.end(),
                                gribFilterOptionsLocal.itsGeneratedDatas.begin(),
                                gribFilterOptionsLocal.itsGeneratedDatas.end());
  }
  catch (std::exception &e)
  {
    DoErrorReporting(
        "Error occured when converting grib-file:", "with error:", theGribFileName, &e);
  }
  catch (...)
  {
    DoErrorReporting("Unknown error occured when converting grib-file:", "", theGribFileName);
  }
}

}  // namespace wgrib2qd
// **************************************************************************

void MakeTotalCombineQDatas(vector<std::shared_ptr<NFmiQueryData> > &theTotalQDataCollector,
                            GribFilterOptions &theGribFilterOptionsOut)
{
  if (theTotalQDataCollector.size() == 0)
    throw runtime_error("Unable to create any data.");
  else if (theTotalQDataCollector.size() == 1)
  {
    theGribFilterOptionsOut.itsGeneratedDatas =
        theTotalQDataCollector;  // Jos vain yhdestä grib-tiedostosta dataa, laitetaan ne
                                 // sellaisenaan output-datoiksi
  }
  else
  {  // Jos useita grib-lähteitä, yritetään muodostaan tässä niistä kokooma-qdatoja.
    // Tässä luodaan luultavasti aikayhdistelmiä, jos lähde gribit ovat olleet per aika-askel
    // tyyliin.
    // 0. Etsi kaikki mahdolliset levelTypet
    set<long> levelTypes = FindAllLevelTypes(theTotalQDataCollector);
    // 1. Etsi kaikki mahdolliset HPlaceDescriptorit
    vector<NFmiHPlaceDescriptor> hplaceDescriptors =
        GetUniqueHPlaceDescriptors(theTotalQDataCollector);
    // 2. Hae jokaiselle leveltyyppi ja hplaceDescriptri parille yhteiset parametrit ja ajat
    vector<std::shared_ptr<NFmiQueryData> > generatedEmptyQDatas;
    for (set<long>::iterator it = levelTypes.begin(); it != levelTypes.end(); ++it)
    {
      for (size_t i = 0; i < hplaceDescriptors.size(); i++)
      {
        std::shared_ptr<NFmiQueryData> qData =
            CreateEmptyQData(*it, hplaceDescriptors[i], theTotalQDataCollector);
        if (qData)
          generatedEmptyQDatas.push_back(qData);
      }
    }
    // 3. täytä eri datat
    vector<std::shared_ptr<NFmiQueryData> >
        generatedFilledQDatas;  // kaikkia datoja ei välttämättä täytetä, on olemassa kombinaatioita
    for (size_t i = 0; i < generatedEmptyQDatas.size(); i++)
    {
      if (FillQData(generatedEmptyQDatas[i], theTotalQDataCollector))
        generatedFilledQDatas.push_back(generatedEmptyQDatas[i]);
    }
    // 4. Jos haluataan alueellisia yhdistemiä, se tehdään tässä
    if (theGribFilterOptionsOut.fTryAreaCombination)
    {
      vector<std::shared_ptr<NFmiQueryData> > areaCombinedDataVector =
          TryAreaCombination(generatedFilledQDatas);
      if (areaCombinedDataVector.size())
        generatedFilledQDatas = areaCombinedDataVector;  // jos jonkinlainen yhdistely onnistui,
                                                         // otetaan yhdistelmä datat käyttöön
                                                         // talletusta varten
    }
    theGribFilterOptionsOut.itsGeneratedDatas = generatedFilledQDatas;
  }

  // Tämä pitää laskea vasta kun eri gribeissä olleet datat on laskettu yhteen, muuten ei ehkä löydy
  // tarvittavia parametreja
  CalcRelativeHumidityData(theGribFilterOptionsOut.itsGeneratedDatas,
                           theGribFilterOptionsOut.itsHybridRelativeHumidityInfo,
                           theGribFilterOptionsOut.itsHybridPressureInfo,
                           theGribFilterOptionsOut.itsGroundPressureInfo);
}

int BuildAndStoreAllDatas(vector<string> &theFileList, GribFilterOptions &theGribFilterOptions)
{
  size_t fileCount = theFileList.size();
  try
  {
    for (size_t i = 0; i < fileCount; i++)
      ConvertOneGribFile(theGribFilterOptions, theFileList[i]);
  }
  catch (Reduced_ll_grib_exception &)
  {
    // Use wgrib-kirjastoa only if eccodes cannot handle the data (such as reduced_ll)
    for (size_t i = 0; i < fileCount; i++)
      wgrib2qd::ConvertOneGribFile(theGribFilterOptions, theFileList[i]);
  }

  MakeTotalCombineQDatas(gTotalQDataCollector, theGribFilterOptions);
  StoreQueryDatas(theGribFilterOptions);

  return theGribFilterOptions.itsReturnStatus;
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cerr << "Usage: grib2qd [options] inputfile1 [inputfile2 ...]  > outputqdata" << endl
       << endl
       << "Convert GRIB files to querydata." << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-o output\tDefault data is writen to stdout." << endl
       << "\t-l t105v3,t109v255,...\tIgnore following individual levels" << endl
       << "\t\t(e.g. 1. type 105, value 3, 2. type 109, value 255, etc.)" << endl
       << "\t\tYou can use *-wildcard with level value to ignore" << endl
       << "\t\tcertain level type all together (e.g. t109v*)" << endl
       << "\t-L 105,109,...\tAccept only following level types." << endl
       << "\t-p 230,hir\tMake result datas producer id and name as wanted." << endl
       << "\t-D <definitions path>\tWhere is grib_api definitions files located," << endl
       << "\t\tdefault uses path from settings" << endl
       << "\t\t(GRIB_DEFINITION_PATH=...)." << endl
       << "\t-a do atlantic-centered-fix If data is in latlon projection and covers" << endl
       << "\t\tthe globe 0->360, and its wanted to cover -180->180, use this option" << endl
       << "\t-A do pacific-centered-fix If data is in latlon projection and covers" << endl
       << "\t\tthe globe -180->180, and its wanted to cover 0->360, use this option" << endl
       << "\t-S   Swap left and right hand halves in the data" << endl
       << "\t-n   Names output files by level type. E.g. output.sqd_levelType_100" << endl
       << "\t-t   Reports run-time to the stderr at the end of execution" << endl
       << "\t-v   verbose mode" << endl
       << "\t-C   try to combine larger areas" << endl
       << "\t-z   read data lines in zig-zag fashion, starting left to rigth" << endl
       << "\t-i   Ignore reduced_ll data, keep using grib_api for conversion" << endl
       << "\t-d   Crop all params except those mensioned in paramChangeTable" << endl
       << "\t\t(and their mensioned levels)" << endl
       << "\t-c paramChangeTableFile\tIf params id and name changes are done here is" << endl
       << "\t\tfile name where is the wanted change table." << endl
       << "\t-g printed-grid-info-count\tIf you want to print out (to cerr)" << endl
       << "\t\tinfo about different grids and projection  information, give" << endl
       << "\t\tnumber of first grids here. If number is -1, every grids info" << endl
       << "\t\tis printed." << endl
       << "\t-H <sfcPresId,hybridPreId=1,hybridPreName=P>\tCalculate and add" << endl
       << "\t\t pressure parameter to hybrid data. Give surfacePressure-id," << endl
       << "\t\tgenerated pressure idand name are optional." << endl

       << "\t-r <specHumId,hybridRHId=13,hybridRHName=RH>\tCalculate and add" << endl
       << "\t\t relative humidity parameter to hybrid data." << endl

       << "\t-G <x1,y1,x2,y2>" << endl
       << "\t\tDefine the minimal subgrid to be cropped by the bottom left" << endl
       << "\t\tand top right longitude and latitude." << endl

       << "\t-P <proj-string>  Define projection that the data will be " << endl
       << "\t\t projected. Proj-string form is the one used with newbase " << endl
       << "\t\t NFmiAreaFactory-class. Give also the grid size or default" << endl
       << "\t\t 50x50 will be used. E.g. FMI scand area is" << endl
       << "\t\t stereographic,20,90,60:6,51.3,49,70.2:82,91 (where 82,91 is" << endl
       << "\t\t used grid size. You can also set pressurelevel and hybrid-" << endl
       << "\t\t level output data grid sizes separately by adding them after" << endl
       << "\t\t first grid size e.g. after 82,91 you add ,50,40,35,25 and" << endl
       << "\t\t pressure data has 50x40 and hybrid datat has 35x25 grid sizes)" << endl
       << "\t\t You can also give three different projections for surface-," << endl
       << "\t\t pressure- and hybrid-data. Give two or three projections " << endl
       << "\t\t separated by semicolons ';'. E.g." << endl
       << "\t\t proj1:gridSize1[;proj2:gridSize2][;proj3:gridSize3]" << endl
       << endl;
}

bool DoCommandLineCheck(NFmiCmdLine &theCmdLine)
{
  if (theCmdLine.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl
         << theCmdLine.Status().ErrorLog().CharPtr() << endl;

    Usage();
    return false;
  }

  if (theCmdLine.NumberofParameters() < 1)
  {
    cerr << "Error: At least 1 input file or directory expected\n\n";
    Usage();
    return false;
  }

  return true;
}

bool GetProducer(NFmiCmdLine &theCmdLine, GribFilterOptions &theGribFilterOptions)
{
  if (theCmdLine.isOption('p'))
  {
    std::vector<std::string> strVector = NFmiStringTools::Split(theCmdLine.OptionValue('p'), ",");
    if (strVector.size() < 2)
    {
      cerr << "Error: with p-option (producer) 2 comma separated parameters expected (e.g. "
              "230,hir)\n\n";
      Usage();
      return false;
    }
    unsigned long prodId = Fmi::stoul(strVector[0]);
    theGribFilterOptions.itsWantedSurfaceProducer = NFmiProducer(prodId, strVector[1]);
    theGribFilterOptions.itsWantedPressureProducer = theGribFilterOptions.itsWantedSurfaceProducer;
    theGribFilterOptions.itsWantedHybridProducer = theGribFilterOptions.itsWantedSurfaceProducer;
    if (strVector.size() >= 3)
      theGribFilterOptions.itsWantedPressureProducer.SetName(strVector[2]);
    if (strVector.size() >= 4)
      theGribFilterOptions.itsWantedHybridProducer.SetName(strVector[3]);
  }
  return true;
}

int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption)
{
  NFmiValueString valStr(theCmdline.OptionValue(theOption));
  if (valStr.IsInt())
    return static_cast<int>(valStr);
  throw runtime_error(string("Error: '") + theOption +
                      "' option value must be integer, exiting...");
}

void GetStepRangeOptions(const NFmiCmdLine &theCmdline, GribFilterOptions &theGribFilterOptions)
{
  if (theCmdline.isOption('R'))
  {
    string stepRangeOptionString = theCmdline.OptionValue('R');
    std::vector<std::string> stepRangeOptionsVector =
        NFmiStringTools::Split(stepRangeOptionString, ":");
    if (stepRangeOptionsVector.size() == 2)
    {
      try
      {
        int wantedStepRange = NFmiStringTools::Convert<int>(stepRangeOptionsVector[0]);
        std::vector<unsigned long> stepRangeParamsVector =
            NFmiStringTools::Split<std::vector<unsigned long> >(stepRangeOptionsVector[1], ",");
        if (stepRangeParamsVector.size())
        {
          theGribFilterOptions.itsWantedStepRange = wantedStepRange;
          theGribFilterOptions.itsStepRangeCheckedParams =
              std::set<unsigned long>(stepRangeParamsVector.begin(), stepRangeParamsVector.end());
          return;
        }
      }
      catch (...)
      {
      }
    }

    throw std::runtime_error(
        "StepRange option (-R) was ilformatted, use following option format: -R "
        "wantedStep:parid1[,parid2,...]");
  }
}

int GetOptions(NFmiCmdLine &theCmdLine, GribFilterOptions &theGribFilterOptions)
{
  if (theCmdLine.isOption('o'))
  {
    theGribFilterOptions.itsOutputFileName = theCmdLine.OptionValue('o');
    theGribFilterOptions.fUseOutputFile = true;
  }

#ifdef UNIX
  string paramChangeTableFileName = "/usr/share/smartmet/formats/grib.conf";
#else
  string paramChangeTableFileName = "";
#endif
  if (theCmdLine.isOption('c'))
    paramChangeTableFileName = theCmdLine.OptionValue('c');
  if (!paramChangeTableFileName.empty())
    theGribFilterOptions.itsParamChangeTable = ReadGribConf(paramChangeTableFileName);

  if (theCmdLine.isOption('g'))
    theGribFilterOptions.itsGridInfoPrintCount = Fmi::stoi(theCmdLine.OptionValue('g'));
  if (theCmdLine.isOption('d'))
    theGribFilterOptions.fCropParamsNotMensionedInTable = true;

  if (theCmdLine.isOption('a'))
    theGribFilterOptions.fDoAtlanticFix = true;
  if (theCmdLine.isOption('A'))
    theGribFilterOptions.fDoPacificFix = true;
  if (theGribFilterOptions.fDoAtlanticFix && theGribFilterOptions.fDoPacificFix)
    throw runtime_error(
        "Error with pacific and atlantic fix options, both 'a' and 'A' options can't be on at "
        "the "
        "same time");

  if (theCmdLine.isOption('y'))
  {
    std::cerr << "Warning: Option -y is deprecated. The need to flip grib data is now detected "
                 "automatically from 'jScansPositively' setting"
              << std::endl;
  }

  if (theCmdLine.isOption('S'))
    theGribFilterOptions.fDoLeftRightSwap = true;

  if (theCmdLine.isOption('i'))
    theGribFilterOptions.fIgnoreReducedLLData = true;

  if (theCmdLine.isOption('z'))
    theGribFilterOptions.fDoZigzagMode = true;

  if (theCmdLine.isOption('C'))
    theGribFilterOptions.fTryAreaCombination = true;

  if (theCmdLine.isOption('n'))
    theGribFilterOptions.fUseLevelTypeFileNaming = true;

  if (::GetProducer(theCmdLine, theGribFilterOptions) == false)
    return 1;

  if (::GetIgnoreLevelList(theCmdLine, theGribFilterOptions.itsIgnoredLevelList) == false)
    return 8;

  theGribFilterOptions.itsAcceptOnlyLevelTypes = GetAcceptedLevelTypes(theCmdLine);

  theGribFilterOptions.itsHybridPressureInfo =
      GetGeneratedHybridParamInfo(theCmdLine, 'H', kFmiPressure, "P");
  theGribFilterOptions.itsHybridRelativeHumidityInfo =
      GetGeneratedHybridParamInfo(theCmdLine, 'r', kFmiHumidity, "RH");
  theGribFilterOptions.itsGroundPressureInfo =
      GetGeneratedHybridParamInfo(theCmdLine, 'r', kFmiPressureAtStationLevel, "P");

  if (theCmdLine.isOption('m'))
    std::cerr << "Warning: option -m is deprecated, sizes are now unlimited\n";

  if (theCmdLine.isOption('G'))
  {
    string opt_bounds = theCmdLine.OptionValue('G');
    theGribFilterOptions.itsLatlonCropRect = GetLatlonCropRect(opt_bounds);
  }

  if (::GetGridOptions(theCmdLine, theGribFilterOptions.itsGridSettings) == false)
    return 9;

  if (theCmdLine.isOption('v'))
    theGribFilterOptions.fVerbose = true;

  GetStepRangeOptions(theCmdLine, theGribFilterOptions);

  return 0;  // 0 on ok paluuarvo
}

int Run(int argc, const char **argv, bool &fReportExecutionTime)
{
  // Optiot:
  GribFilterOptions gribFilterOptions;

  NFmiCmdLine cmdline(argc, argv, "o!m!l!g!p!aASnL!G!c!dvP!D!tH!r!yzCiR!");

  // Jonkin näistä avulla muodostetaan lista, jossa voi olla 0-n kpl tiedoston nimiä.
  if (::DoCommandLineCheck(cmdline) == false)
    return 1;

  if (cmdline.isOption('t'))
    fReportExecutionTime = true;
  if (::GribDefinitionPath(cmdline) == false)
    return 1;
  int status = ::GetOptions(cmdline, gribFilterOptions);
  if (status != 0)
    return status;

  vector<string> fileList = GetDataFiles(cmdline);

  if (fileList.empty())
    throw runtime_error("Error there were no matching grib files or given directory was empty");

  return BuildAndStoreAllDatas(fileList, gribFilterOptions);
}

/*
Kari Niemelän antamaa koodia vertikaalikoordinaattien laskemista varten
void get_vertcoord(grib_handle *h, grib2_header_t *decoded_header )
{
// ******* Vertical Coordinate Parameters *****
   size_t num_coord = decoded_header->geompsnt.number_vert_coord;

   double *pv = malloc(num_coord*sizeof(double));
   GRIB_CHECK(grib_get_double_array(h,"pv",pv,&num_coord),0);

   vertcoord_pointer = pv;
   vertcoord_number = num_coord;

   int level = decoded_header->product.level.level_1;
   if (num_coord == 2)
   {  // Hirlam: A, B
     decoded_header->geompsnt.vert_coord_par_a = pv[0];
     decoded_header->geompsnt.vert_coord_par_b = pv[1];
   }
// enemmän parametreja, poimitaan tämän pinnan parametrit
//       AROME, ECMWF: A, A, A,... B, B, B,...
   else if (decoded_header->producer.centre_id == 98 ||
decoded_header->producer.generating_process_id == 3)
   {
     level = decoded_header->product.level.level_1;
     if (level <= num_coord)
         {
      decoded_header->geompsnt.vert_coord_par_a = (pv[level -1] + pv[level]) / 2.;
      decoded_header->geompsnt.vert_coord_par_b = (pv[num_coord/2 + level -1] + pv[num_coord/2 +
level]) / 2.;
     }
   }
// enemmän parametreja, poimitaan tämän pinnan parametrit
//       Seuraavat ehdot on keksittyjä: A, B, A, B, A, B,...
   else if (decoded_header->producer.centre_id == 7 ||
decoded_header->producer.generating_process_id == 13)
   {
     level = decoded_header->product.level.level_1;
     if (level <= num_coord)
         {
      decoded_header->geompsnt.vert_coord_par_a = (pv[level*2 -2] + pv[level*2]) / 2.;
      decoded_header->geompsnt.vert_coord_par_b = (pv[level*2 -1] + pv[level*2 +1]) / 2.;
     }
   }
}
*/

int main(int argc, const char **argv)
{
  int returnStatus = 0;  // 0 = ok
  bool reportExecutionTime = false;
  NFmiMilliSecondTimer timer;
  timer.StartTimer();

  try
  {
    returnStatus = Run(argc, argv, reportExecutionTime);
  }
  catch (exception &e)
  {
    cerr << "Error: " << e.what() << endl;
    returnStatus = 1;
  }
  catch (...)
  {
    cerr << "Unknown error: error in program or data? Stopping the execution..." << endl;
    returnStatus = 1;
  }

  timer.StopTimer();
  if (reportExecutionTime)
    cerr << "Execution run time: " << timer.EasyTimeDiffStr() << endl;

  return returnStatus;
}
