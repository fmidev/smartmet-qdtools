/*!
 *  \file
 *  Tekij‰: Marko (19.8.2003)
 *
 *  T‰m‰ ohjelma lukee grib-tiedoston (hallitsee myˆs grib2-formaatin) ja konvertoi sen
 *fqd-tiedostoksi.
 *
 * \code
 * Optiot:
 *
 *  -o output-tiedosto      Oletusarvoisesti tulostetaan sdtout:iin.
 *	-m max-data-size-MB [200] Kuinka suuri data paketti tehd‰‰n
 *                              maksimissaan megatavuissa (vahinkojen varalle)
 *  -l t105v3,t109v255,...   J‰t‰ pois laskuista seuraavat yksitt‰iset levelit (1. type 105, value
 *3, jne.)
 *  -g printed-grid-info-count Eli kuinka monesta ensimm‰isest‰ hilasta haluat tulostaa cerr:iin.
 *Jos count -1, tulostaa kaikista.
 *
 * Esimerkkeja:
 *
 *	grib2ToQD.cpp -o output.fqd input.grib
 *
 *	grib2ToQD.cpp input.grib > output.fqd
 * \endcode
 */

// TODO Pit‰isi irroittaa hilojen grib-handle k‰sittelyt ja apuluokat omaan sourceen (mm.
// GribFilterOptions-luokka ja
// CreatePolarStereographicArea -funktio ja vastaavat) tai namespaceen. Nyt tein korjauksia
// grib2toqd.cpp:hen, jotka
// pit‰isi saada k‰yttˆˆn myˆs gribtoqd.cpp:ss‰. gribtoqd.cpp on kopio alkuper‰isest‰
// grib2toqd.cpp-tiedostosta ja
// siihen on tehty mm. asetuksien s‰‰dˆt NFmiSettings-luokan avulla linux-maailman mukaisiksi.

#ifdef _MSC_VER
#pragma warning(disable : 4786 4996)  // poistaa n kpl VC++ k‰‰nt‰j‰n varoitusta (liian pitk‰ nimi
                                      // >255 merkki‰
                                      // joka johtuu 'puretuista' STL-template nimist‰)
#endif

#include "GribTools.h"

#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiCommentStripper.h>
#include <newbase/NFmiDataMatrixUtils.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiInterpolation.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiMercatorArea.h>
#include <newbase/NFmiMilliSecondTimer.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiRotatedLatLonArea.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiStereographicArea.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiTotalWind.h>
#include <newbase/NFmiValueString.h>

#include <grib_api.h>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>

using namespace std;

void check_jscan_direction(grib_handle *theGribHandle)
{
  long direction = 0;
  int status = grib_get_long(theGribHandle, "jScansPositively", &direction);
  if (status != 0) return;
  if (direction == 0)
    throw std::runtime_error("GRIBs with a negative j-scan direction are not supported");
  return;
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
  boost::shared_ptr<NFmiGrid> itsBaseGrid;
  boost::shared_ptr<NFmiGrid> itsPressureGrid;
  boost::shared_ptr<NFmiGrid> itsHybridGrid;
};

static const size_t gMBsize = 1024 * 1024;

struct GribFilterOptions
{
  GribFilterOptions(void)
      : itsOutputFileName(),
        fUseOutputFile(false),
        itsMaxQDataSizeInBytes(1024 * gMBsize),
        itsReturnStatus(0),
        itsIgnoredLevelList(),
        itsGeneratedDatas(),
        itsAcceptOnlyLevelTypes(),
        itsGridInfoPrintCount(0),
        itsParamChangeTable(),
        fCropParamsNotMensionedInTable(false),
        fDoGlobeFix(true),
        fUseLevelTypeFileNaming(false),
        itsWantedSurfaceProducer(),
        itsWantedPressureProducer(),
        itsWantedHybridProducer(),
        itsHybridPressureInfo(),
        itsLatlonCropRect(gMissingCropRect),
        itsGridSettings(),
        fVerbose(false),
        fTreatSingleLevelsAsSurfaceData(false),
        itsInputFileNameStr(),
        itsInputFile(0)
  {
  }

  ~GribFilterOptions(void)
  {
    if (itsInputFile) ::fclose(itsInputFile);  // suljetaan tiedosto, josta gribi luettu
  }

  string itsOutputFileName;  // -o optio tai sitten tulostetann cout:iin
  bool fUseOutputFile;
  size_t itsMaxQDataSizeInBytes;  // default max koko 1 GB
  int itsReturnStatus;            // 0 = ok
  NFmiLevelBag itsIgnoredLevelList;  // lista miss‰ yksitt‰isi‰ leveleit‰, mitk‰ halutaan j‰tt‰‰
                                     // pois laskuista
  vector<boost::shared_ptr<NFmiQueryData> > itsGeneratedDatas;
  vector<FmiLevelType> itsAcceptOnlyLevelTypes;  // lista jossa ainoat hyv‰ksytt‰v‰t level typet
  int itsGridInfoPrintCount;
  vector<ParamChangeItem> itsParamChangeTable;
  bool fCropParamsNotMensionedInTable;
  bool fDoGlobeFix;
  bool fUseLevelTypeFileNaming;  // n optiolla voidaan laittaa output tiedostojen nimen per‰‰n esim.
                                 // _levelType_100
  NFmiProducer itsWantedSurfaceProducer;
  NFmiProducer itsWantedPressureProducer;
  NFmiProducer itsWantedHybridProducer;
  GeneratedHybridParamInfo itsHybridPressureInfo;
  GeneratedHybridParamInfo itsHybridRelativeHumidityInfo;
  NFmiRect itsLatlonCropRect;
  GridSettingsPackage itsGridSettings;
  bool fVerbose;
  bool fTreatSingleLevelsAsSurfaceData;  // jos t‰m‰ on true, laitetaan kaikki eri tyyppiset
                                         // level-datat, joissa on vain yksi erillinen leveli,
                                         // yhteen ja samaan surface-dataan
  string itsInputFileNameStr;
  FILE *itsInputFile;
};

class TotalQDataCollector
{
  typedef boost::shared_mutex MutexType;
  typedef boost::shared_lock<MutexType>
      ReadLock;  // Read-lockia ei oikeasti tarvita, mutta laitan sen t‰h‰n, jos joskus tarvitaankin
  typedef boost::unique_lock<MutexType> WriteLock;

 public:
  typedef list<vector<boost::shared_ptr<NFmiQueryData> > > Container;

  TotalQDataCollector(void) : itsAddCount(0), itsTotalGeneratedQDatas() {}
  void AddData(vector<boost::shared_ptr<NFmiQueryData> > &theDataList)
  {
    if (theDataList.size())
    {
      WriteLock lock(itsMutex);
      itsAddCount++;
      itsTotalGeneratedQDatas.push_back(theDataList);
    }
  }

  Container &TotalGeneratedQDatas(void) { return itsTotalGeneratedQDatas; }
  int AddCount(void) const { return itsAddCount; }

 private:
  MutexType itsMutex;
  int itsAddCount;                    // kuinka monta 'onnistunutta' data lis‰yst‰ on tehty
  Container itsTotalGeneratedQDatas;  // t‰h‰n listaan lis‰t‰‰n kustakin gribist‰ luodut queryDatat
};

namespace
{
TotalQDataCollector gTotalQDataCollector;
}

void Usage(void);
void ConvertGrib2QData(GribFilterOptions &theGribFilterOptions);
void CreateQueryDatas(vector<GridRecordData *> &theGribRecordDatas,
                      GribFilterOptions &theGribFilterOptions,
                      map<int, pair<double, double> > &theVerticalCoordinateMap);
boost::shared_ptr<NFmiQueryData> CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                                                 NFmiHPlaceDescriptor &theHplace,
                                                 NFmiVPlaceDescriptor &theVplace,
                                                 GribFilterOptions &theGribFilterOptions);
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace,
                                 const GribFilterOptions &theGribFilterOptions);
const NFmiDataIdent &FindFirstParam(int theParId, vector<GridRecordData *> &theGribRecordDatas);
NFmiVPlaceDescriptor GetVPlaceDesc(vector<GridRecordData *> &theGribRecordDatas);
const NFmiLevel &FindFirstLevel(int theLevelValue, vector<GridRecordData *> &theGribRecordDatas);
NFmiTimeDescriptor GetTimeDesc(vector<GridRecordData *> &theGribRecordDatas,
                               NFmiHPlaceDescriptor &theHplace,
                               NFmiVPlaceDescriptor &theVplace);
bool FillQDataWithGribRecords(boost::shared_ptr<NFmiQueryData> &theQData,
                              vector<GridRecordData *> &theGribRecordDatas,
                              bool verbose);
int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption);
void CheckInfoSize(const NFmiQueryInfo &theInfo, size_t theMaxQDataSizeInBytes);
vector<NFmiHPlaceDescriptor> GetAllHPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile);

static const unsigned long gMissLevelValue =
    9999999;  // t‰ll‰ ignoorataan kaikki tietyn level tyypin hilat

static string GetFileNameAreaStr(const NFmiArea *theArea)
{
  string str("_area_");
  str += theArea->AreaStr();
  return str;
}

static void ReplaceChar(string &theFileName, char replaceThis, char toThis)
{
  for (string::size_type i = 0; i < theFileName.size(); i++)
  {
    if (theFileName[i] == replaceThis) theFileName[i] = toThis;
  }
}

static NFmiRect GetLatlonCropRect(const string &theBoundsStr)
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

// Lukee initialisointi tiedostosta parametreja koskevat muunnos tiedot.
// param tiedoston formaatti on seuraava (kommentit sallittuja, mutta itse param rivit
// ilman kommentti merkkej‰) ja kenttien erottimina on ;-merkki.
//
// // origParId;wantedParId;wantedParName[;base][;scale][;levelType;level]
// 129;4;Temperature;-273.15;1;103;2  // (here e.g. T change from kelvins to celsius and changing
// from height 2m level to surface data)
// 11;1;Pressure;0;0.01    // (here e.g. changing the pressure from pascals to mbars)
static void InitParamChangeTable(const string &theParamChangeTableFileName,
                                 vector<ParamChangeItem> &theParamChangeTable)
{
  if (theParamChangeTableFileName.empty())
    throw std::runtime_error(
        "InitParamChangeTable: empty ParamChangeTableFileName filename given.");

  NFmiCommentStripper stripComments;
  if (stripComments.ReadAndStripFile(theParamChangeTableFileName))
  {
#ifdef OLDGCC
    std::istrstream in(stripComments.GetString().c_str());
#else
    std::stringstream in(stripComments.GetString());
#endif

    const int maxBufferSize = 1024 + 1;  // kuinka pitk‰ yhden rivin maksimissaan oletetaan olevan
    std::string buffer;
    ParamChangeItem paramChangeItem;
    int i = 0;
    int counter = 0;
    do
    {
      buffer.resize(maxBufferSize);
      in.getline(&buffer[0], maxBufferSize);

      size_t realSize = strlen(buffer.c_str());
      buffer.resize(realSize);
      if (::GetParamChangeItemFromString(buffer, theParamChangeTableFileName, paramChangeItem))
      {
        counter++;
        theParamChangeTable.push_back(paramChangeItem);
      }
      i++;
    } while (in.good());
  }
  else
    throw std::runtime_error(std::string("InitParamChangeTable: trouble reading file: ") +
                             theParamChangeTableFileName);
}

const NFmiPoint gMissingGridSize(kFloatMissing, kFloatMissing);

static boost::shared_ptr<NFmiGrid> GetGridFromProjectionStr(string &theProjectionStr,
                                                            int gridSizeX = -1,
                                                            int gridSizeY = -1)
{
  vector<string> projectionPartsStr = NFmiStringTools::Split(theProjectionStr, ":");
  if (projectionPartsStr.size() < 2)
    throw runtime_error(
        std::string("Unable to create area and grid from given projection string: ") +
        theProjectionStr);
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

    boost::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(areaStr);
    std::vector<double> values = NFmiStringTools::Split<std::vector<double> >(gridStr, ",");
    if (values.size() != 2)
      throw runtime_error("Given GridSize was invlid, has to be two numbers (e.g. x,y).");
    NFmiPoint gridSize(values[0], values[1]);
    boost::shared_ptr<NFmiGrid> grid(new NFmiGrid(area->Clone(),
                                                  static_cast<unsigned int>(gridSize.X()),
                                                  static_cast<unsigned int>(gridSize.Y())));
    return grid;
  }
}

static boost::shared_ptr<NFmiGrid> CreateDeepGribCopy(boost::shared_ptr<NFmiGrid> &theGrid)
{
  if (theGrid)
    return boost::shared_ptr<NFmiGrid>(new NFmiGrid(*theGrid));
  else
    return boost::shared_ptr<NFmiGrid>();
}

static void HandleSeparateProjections(vector<string> &theSeparateProjectionsStr,
                                      GridSettingsPackage &theGridSettings)
{
  if (theSeparateProjectionsStr.size() == 1)
  {
    theGridSettings.itsBaseGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = ::CreateDeepGribCopy(theGridSettings.itsBaseGrid);
    theGridSettings.itsHybridGrid = ::CreateDeepGribCopy(theGridSettings.itsBaseGrid);
  }
  else if (theSeparateProjectionsStr.size() == 2)
  {
    theGridSettings.itsBaseGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[1]);
    theGridSettings.itsHybridGrid = ::CreateDeepGribCopy(theGridSettings.itsPressureGrid);
  }
  if (theSeparateProjectionsStr.size() >= 3)
  {
    theGridSettings.itsBaseGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[0]);
    theGridSettings.itsPressureGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[1]);
    theGridSettings.itsHybridGrid = ::GetGridFromProjectionStr(theSeparateProjectionsStr[2]);
    ;
  }
}

// Funktio tekee annetusta projektio stringist‰ halutun projektio gridin. Lis‰ksi siin‰ tarkistetaan
// onko annettu
// erilaisille datoille hilakokoja. Jos ei ole, kaikille datoille tulee koko 50x50.
// Jos lˆytyy yksi koko, laitetaan kaikille datoille halutuksi hila kooksi se. Jos annettu toinenkin
// hila koko, annetaan
// se koko painepinta ja mallipinta datoilla ja kaikille muillekin datoille.
// Projektio stringi on muotoa:
// stereographic,20,90,60:6,51.3,49,70.2:82,91
// Jossa toisen kaksoispisteen j‰lkeen tulee hilakoot. Niit‰ voi siis olla pilkuilla eroteltuina 3
// paria.
static void HandleProjectionString(const string &theProjStr, GridSettingsPackage &theGridSettings)
{
  vector<string> separateProjectionsStr = NFmiStringTools::Split(theProjStr, ";");
  if (separateProjectionsStr.size() >= 2)
  {
    ::HandleSeparateProjections(separateProjectionsStr, theGridSettings);
  }
  else
  {
    vector<string> projParts = NFmiStringTools::Split(theProjStr, ":");
    if (projParts.size() < 2)
      throw std::runtime_error("Error with -P option, the projection string was incomplete.");
    string areaStr = projParts[0] + ":" + projParts[1];
    string gridSizeStr = "50,50";  // t‰m‰ on defaultti hila koko jos sit‰ ei ole annettu
    if (projParts.size() >= 3) gridSizeStr = projParts[2];
    vector<int> gridSizes = NFmiStringTools::Split<vector<int> >(gridSizeStr, ",");
    if (gridSizes.size() < 2)
      throw std::runtime_error(
          "Error with -P option, the projection string with incomplete grid size given.");

    theGridSettings.itsBaseGrid = ::GetGridFromProjectionStr(areaStr, gridSizes[0], gridSizes[1]);
    int indexOffset = 0;
    if (gridSizes.size() >= 4) indexOffset = 2;
    theGridSettings.itsPressureGrid =
        ::GetGridFromProjectionStr(areaStr, gridSizes[0 + indexOffset], gridSizes[1 + indexOffset]);

    if (gridSizes.size() >= 6) indexOffset = 4;
    theGridSettings.itsHybridGrid =
        ::GetGridFromProjectionStr(areaStr, gridSizes[0 + indexOffset], gridSizes[1 + indexOffset]);
  }
}

static bool IsDifferentGridFileNamesUsed(vector<boost::shared_ptr<NFmiQueryData> > &theDatas)
{
  size_t ssize = theDatas.size();
  if (ssize > 1)
  {
    std::set<FmiLevelType> levels;
    for (size_t i = 0; i < ssize; i++)
    {
      levels.insert(theDatas[i]->Info()->Level()->LevelType());
    }
    if (levels.size() != ssize) return true;
  }
  return false;
}

static bool DoCommandLineCheck(NFmiCmdLine &theCmdLine, std::string &theFilePatternOrDirectoryOut)
{
  if (theCmdLine.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl
         << theCmdLine.Status().ErrorLog().CharPtr() << endl;

    ::Usage();
    return false;
  }

  if (theCmdLine.NumberofParameters() != 1)
  {
    cerr << "Error: 1 parameter expected, 'inputfile'\n\n";
    ::Usage();
    return false;
  }
  else
    theFilePatternOrDirectoryOut = theCmdLine.Parameter(1);
  return true;
}

static bool GetIgnoreLevelList(NFmiCmdLine &theCmdLine, NFmiLevelBag &theIgnoredLevelListOut)
{
  if (theCmdLine.isOption('l'))
  {
    string ignoredLevelsStr = theCmdLine.OptionValue('l');
    vector<string> ignoredLevelsStrVector = NFmiStringTools::Split(ignoredLevelsStr);
    for (unsigned int i = 0; i < ignoredLevelsStrVector.size(); i++)
    {  // yksitt‰iset stringit siis muotoa t105v3 jne. eli leveltyyppi 105 ja sen arvo 3
      vector<string> levelStrVec =
          NFmiStringTools::Split(NFmiStringTools::LowerCase(ignoredLevelsStrVector[i]), "v");
      if (levelStrVec.size() == 2 && levelStrVec[0].at(0) == 't')
      {
        string levelTypeStr(levelStrVec[0].begin() + 1, levelStrVec[0].end());
        unsigned long levelType = boost::lexical_cast<unsigned long>(levelTypeStr);
        float levelValue = gMissLevelValue;
        if (levelStrVec[1] != "*") levelValue = boost::lexical_cast<float>(levelStrVec[1]);
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

static bool GetProducer(NFmiCmdLine &theCmdLine, GribFilterOptions &theGribFilterOptions)
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
    unsigned long prodId = boost::lexical_cast<unsigned long>(strVector[0]);
    theGribFilterOptions.itsWantedSurfaceProducer = NFmiProducer(prodId, strVector[1]);
    theGribFilterOptions.itsWantedPressureProducer = theGribFilterOptions.itsWantedSurfaceProducer;
    theGribFilterOptions.itsWantedHybridProducer = theGribFilterOptions.itsWantedSurfaceProducer;
    if (strVector.size() >= 3) theGribFilterOptions.itsWantedPressureProducer.SetName(strVector[2]);
    if (strVector.size() >= 4) theGribFilterOptions.itsWantedHybridProducer.SetName(strVector[3]);
  }
  return true;
}

static bool GribDefinitionPath(NFmiCmdLine &theCmdLine)
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

static vector<FmiLevelType> GetAcceptedLevelTypes(NFmiCmdLine &theCmdLine)
{
  vector<FmiLevelType> acceptOnlyLevelTypes;
  if (theCmdLine.isOption('L'))
  {
    vector<string> acceptOnlyLevelTypesStrVector =
        NFmiStringTools::Split(theCmdLine.OptionValue('L'));
    for (unsigned int i = 0; i < acceptOnlyLevelTypesStrVector.size(); i++)
    {
      unsigned long levelType =
          boost::lexical_cast<unsigned long>(acceptOnlyLevelTypesStrVector[i]);
      acceptOnlyLevelTypes.push_back(static_cast<FmiLevelType>(
          levelType));  // stringtools-convert ei osaa heti tehd‰ FmiLevelType-tyyppi‰
    }
  }
  return acceptOnlyLevelTypes;
}

static GeneratedHybridParamInfo GetGeneratedHybridParamInfo(NFmiCmdLine &theCmdLine,
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
          static_cast<FmiParameterName>(boost::lexical_cast<int>(hybridInfoStrings[0]));
    unsigned long hybridPressureId = theDefaultParamId;
    if (hybridInfoStrings.size() >= 2)
      hybridPressureId = boost::lexical_cast<unsigned long>(hybridInfoStrings[1]);
    string hybridPressureName = theDefaultParamName;
    if (hybridInfoStrings.size() >= 3) hybridPressureName = hybridInfoStrings[2];

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

static long GetLevelType(boost::shared_ptr<NFmiQueryData> &theQData)
{
  if (theQData)
  {
    theQData->Info()->FirstLevel();
    return theQData->Info()->Level()->LevelType();
  }
  else
    return -1;
}

static void StoreQueryDatas(GribFilterOptions &theGribFilterOptions)
{
  int returnStatus = 0;  // 0 = ok
  if (!theGribFilterOptions.itsGeneratedDatas.empty())
  {
    bool useDifferentFileNamesOnDifferentGrids =
        ::IsDifferentGridFileNamesUsed(theGribFilterOptions.itsGeneratedDatas);
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
              NFmiStringTools::Convert(::GetLevelType(theGribFilterOptions.itsGeneratedDatas[i]));
          // TƒMƒ PITƒƒ viel‰ korjata, jos saman level tyypill‰ on erilaisia hila/area m‰‰rityksi‰,
          // pit‰‰ ne nimet‰!!!
          if (useDifferentFileNamesOnDifferentGrids)
          {
            string areaStr =
                GetFileNameAreaStr(theGribFilterOptions.itsGeneratedDatas[i]->Info()->Area());
            ::ReplaceChar(areaStr, ':', '_');
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
      else if (ssize > 1)  // jos qdatoja syntyy enemm‰n kuin 1, pit‰‰ antaa output-tiedoston nimi,
                           // ett‰ muut tiedostonimet voidaan generoida
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
  else
  {
    cerr << "Could not create output data." << endl;
    returnStatus = 1;
  }
  theGribFilterOptions.itsReturnStatus = returnStatus;
}

static bool GetGridOptions(NFmiCmdLine &theCmdLine, GridSettingsPackage &theGridSettings)
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

static int GetOptions(NFmiCmdLine &theCmdLine, GribFilterOptions &theGribFilterOptions)
{
  if (theCmdLine.isOption('o'))
  {
    theGribFilterOptions.itsOutputFileName = theCmdLine.OptionValue('o');
    theGribFilterOptions.fUseOutputFile = true;
  }

  if (theCmdLine.isOption('c'))
  {
    string paramChangeTableFileName = theCmdLine.OptionValue('c');
    ::InitParamChangeTable(paramChangeTableFileName, theGribFilterOptions.itsParamChangeTable);
  }
  if (theCmdLine.isOption('g'))
    theGribFilterOptions.itsGridInfoPrintCount =
        boost::lexical_cast<int>(theCmdLine.OptionValue('g'));
  if (theCmdLine.isOption('d')) theGribFilterOptions.fCropParamsNotMensionedInTable = true;

  if (theCmdLine.isOption('f')) theGribFilterOptions.fDoGlobeFix = false;

  if (theCmdLine.isOption('n')) theGribFilterOptions.fUseLevelTypeFileNaming = true;

  if (::GetProducer(theCmdLine, theGribFilterOptions) == false) return 1;

  if (::GetIgnoreLevelList(theCmdLine, theGribFilterOptions.itsIgnoredLevelList) == false) return 8;

  theGribFilterOptions.itsAcceptOnlyLevelTypes = ::GetAcceptedLevelTypes(theCmdLine);

  theGribFilterOptions.itsHybridPressureInfo =
      ::GetGeneratedHybridParamInfo(theCmdLine, 'H', kFmiPressure, "P");
  theGribFilterOptions.itsHybridRelativeHumidityInfo =
      ::GetGeneratedHybridParamInfo(theCmdLine, 'r', kFmiHumidity, "RH");

  if (theCmdLine.isOption('m'))
    theGribFilterOptions.itsMaxQDataSizeInBytes = GetIntegerOptionValue(theCmdLine, 'm') * gMBsize;

  if (theCmdLine.isOption('G'))
  {
    string opt_bounds = theCmdLine.OptionValue('G');
    theGribFilterOptions.itsLatlonCropRect = ::GetLatlonCropRect(opt_bounds);
  }

  if (::GetGridOptions(theCmdLine, theGribFilterOptions.itsGridSettings) == false) return 9;

  if (theCmdLine.isOption('v')) theGribFilterOptions.fVerbose = true;

  if (theCmdLine.isOption('1')) theGribFilterOptions.fTreatSingleLevelsAsSurfaceData = true;

  return 0;  // 0 on ok paluuarvo
}

static std::string GetDirectory(const std::string &theFileFilter)
{
  NFmiFileString fileStr(theFileFilter);
  NFmiString str;
  str += fileStr.Device();
  str += fileStr.Path();

  return static_cast<char *>(str);
}

static std::vector<std::string> MakeFullPathFileList(list<string> &theFileNameList,
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

static vector<string> GetDataFiles(const string &theFileOrPatternOrDirectory)
{
  string directory = theFileOrPatternOrDirectory;
  if (NFmiFileSystem::DirectoryExists(directory))
  {  // jos oli hakemisto, luetaan tieedostolista hakemistosta
    list<string> dirList = NFmiFileSystem::DirectoryFiles(directory);
    return ::MakeFullPathFileList(dirList, directory);
  }

  string filePattern = theFileOrPatternOrDirectory;  // Huom! Myˆs yhden tiedoston tarkkaa nime‰
                                                     // voidaan pit‰‰ patternina.
  list<string> patternList = NFmiFileSystem::PatternFiles(filePattern);
  directory = ::GetDirectory(filePattern);
  return ::MakeFullPathFileList(patternList, directory);
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
            .second++;  // jos kyseinen hila oli jo mapissa, kasvatetaan counterin arvo yhdell‰
    }
  }

  void AddTimes(const NFmiTimeDescriptor &theTimeDesc)
  {
    NFmiTimeDescriptor &timeDesc = const_cast<NFmiTimeDescriptor &>(
        theTimeDesc);  // rumaa mutta pit‰‰ kikkailla const:ien kanssa
    for (timeDesc.Reset(); timeDesc.Next();)
      itsValidTimes.insert(timeDesc.Time());
    itsOrigTimes.insert(timeDesc.OriginTime());
  }

  void AddLevels(const NFmiVPlaceDescriptor &theVPlaceDesc)
  {
    NFmiVPlaceDescriptor &vplaceDesc = const_cast<NFmiVPlaceDescriptor &>(
        theVPlaceDesc);  // rumaa mutta pit‰‰ kikkailla const:ien kanssa
    for (vplaceDesc.Reset(); vplaceDesc.Next();)
      itsLevels.insert(*(vplaceDesc.Level()));
  }

  void AddParams(const NFmiParamDescriptor &theParamDesc)
  {
    NFmiParamDescriptor &paramDesc = const_cast<NFmiParamDescriptor &>(
        theParamDesc);  // rumaa mutta pit‰‰ kikkailla const:ien kanssa
    for (paramDesc.Reset(); paramDesc.Next();)
    {
      itsParams.insert(*(paramDesc.Param().GetParam()));
      itsWantedProducers.insert(*(paramDesc.Param().GetProducer()));
    }
  }

  NFmiHPlaceDescriptor GetGrid(void)
  {
    int maxCount = -1;
    MyGrid mostPopularGrid;  // t‰h‰ etsit‰‰n 'suosituin' hila, ja sit‰ k‰ytet‰‰n datan pohjana
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

  NFmiTimeDescriptor GetTimes(void)
  {
    NFmiTimeList times;
    for (set<NFmiMetTime>::iterator it = itsValidTimes.begin(); it != itsValidTimes.end(); ++it)
    {
      times.Add(new NFmiMetTime(*it));
    }
    set<NFmiMetTime>::iterator origTimeIt =
        itsOrigTimes.begin();  // otetaan vain 1. origtime, en jaksa nyt mietti‰ mik‰ voisi olla
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
        itsWantedProducers.begin();  // otetaan vain 1. producer, en jaksa nyt mietti‰ mik‰ voisi
                                     // olla sopivin, jos on useita vaihtoehtoja
    NFmiParamBag params;
    for (set<NFmiParam>::iterator it = itsParams.begin(); it != itsParams.end(); ++it)
    {
      params.Add(NFmiDataIdent(*it, *prodIt));
    }
    return NFmiParamDescriptor(params);
  }

 private:
  map<MyGrid, int> itsGridCounts;  // t‰h‰n lasketaan t‰m‰n level tyypin eri hilat ja kuinka monta
                                   // kertaa kukin hila on esiintynyt datoissa
  set<NFmiMetTime> itsValidTimes;
  set<NFmiMetTime> itsOrigTimes;
  set<NFmiLevel> itsLevels;
  set<NFmiParam> itsParams;
  set<NFmiProducer> itsWantedProducers;
};

static set<long> FindAllLevelTypes(TotalQDataCollector &theTotalQDataCollector)
{
  set<long> levelTypes;
  TotalQDataCollector::Container &qDatas = theTotalQDataCollector.TotalGeneratedQDatas();
  for (TotalQDataCollector::Container::iterator it = qDatas.begin(); it != qDatas.end(); ++it)
  {
    vector<boost::shared_ptr<NFmiQueryData> > &qDataVector = *it;
    for (size_t i = 0; i < qDataVector.size(); i++)
    {
      levelTypes.insert(::GetLevelType(qDataVector[i]));
    }
  }
  return levelTypes;
}

static void FillStructureData(CombineDataStructureSearcher &theStructureSearcher,
                              boost::shared_ptr<NFmiQueryData> &theQData)
{
  theStructureSearcher.AddGrid(theQData->Info()->HPlaceDescriptor());
  theStructureSearcher.AddTimes(theQData->Info()->TimeDescriptor());
  theStructureSearcher.AddLevels(theQData->Info()->VPlaceDescriptor());
  theStructureSearcher.AddParams(theQData->Info()->ParamDescriptor());
}

static void SearchForDataStructures(TotalQDataCollector &theTotalQDataCollector,
                                    map<long, CombineDataStructureSearcher> &theLevelTypeStructures)
{
  TotalQDataCollector::Container &qDatas = theTotalQDataCollector.TotalGeneratedQDatas();
  for (TotalQDataCollector::Container::iterator it = qDatas.begin(); it != qDatas.end(); ++it)
  {
    vector<boost::shared_ptr<NFmiQueryData> > &qDataVector = *it;
    for (size_t i = 0; i < qDataVector.size(); i++)
    {
      long levelType = ::GetLevelType(qDataVector[i]);
      map<long, CombineDataStructureSearcher>::iterator it2 =
          theLevelTypeStructures.find(levelType);
      if (it2 != theLevelTypeStructures.end())
      {
        ::FillStructureData((*it2).second, qDataVector[i]);
      }
    }
  }
}

static vector<boost::shared_ptr<NFmiQueryData> > CreateEmptyQDataVector(
    map<long, CombineDataStructureSearcher> &theLevelTypeStructures)
{
  vector<boost::shared_ptr<NFmiQueryData> > generatedQDatas;
  for (map<long, CombineDataStructureSearcher>::iterator it = theLevelTypeStructures.begin();
       it != theLevelTypeStructures.end();
       ++it)
  {
    NFmiQueryInfo info((*it).second.GetParams(),
                       (*it).second.GetTimes(),
                       (*it).second.GetGrid(),
                       (*it).second.GetLevels());
    generatedQDatas.push_back(
        boost::shared_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(info)));
  }
  return generatedQDatas;
}

static bool HasValidData(const NFmiDataMatrix<float> &theValues)
{
  for (size_t j = 0; j < theValues.NY(); j++)
    for (size_t i = 0; i < theValues.NX(); i++)
      if (theValues[i][j] != kFloatMissing) return true;
  return false;
}

static void FillQData(boost::shared_ptr<NFmiQueryData> &theQData,
                      TotalQDataCollector &theTotalQDataCollector)
{
  if (theQData)
  {
    long levelType = ::GetLevelType(theQData);
    NFmiFastQueryInfo destInfo(theQData.get());

    TotalQDataCollector::Container &qDatas = theTotalQDataCollector.TotalGeneratedQDatas();
    for (TotalQDataCollector::Container::iterator it = qDatas.begin(); it != qDatas.end(); ++it)
    {
      vector<boost::shared_ptr<NFmiQueryData> > &qDataVector = *it;
      for (size_t i = 0; i < qDataVector.size(); i++)
      {
        if (levelType == GetLevelType(qDataVector[i]))
        {
          NFmiFastQueryInfo sourceInfo(qDataVector[i].get());
          bool locationInterpolationNeeded =
              (destInfo.HPlaceDescriptor() == sourceInfo.HPlaceDescriptor()) == false;
          for (sourceInfo.ResetParam(); sourceInfo.NextParam();)
          {
            if (destInfo.Param(static_cast<FmiParameterName>(sourceInfo.Param().GetParamIdent())))
            {
              for (sourceInfo.ResetLevel(); sourceInfo.NextLevel();)
              {
                if (destInfo.Level(*(sourceInfo.Level())))
                {
                  for (sourceInfo.ResetTime(); sourceInfo.NextTime();)
                  {
                    if (destInfo.Time(sourceInfo.Time()))
                    {
                      if (locationInterpolationNeeded)
                      {
                        for (destInfo.ResetLocation(); destInfo.NextLocation();)
                        {
                          float value = sourceInfo.InterpolatedValue(destInfo.LatLon());
                          if (value != kFloatMissing) destInfo.FloatValue(value);
                        }
                      }
                      else
                      {
                        NFmiDataMatrix<float> values;
                        sourceInfo.Values(values);
                        if (HasValidData(values)) destInfo.SetValues(values);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

static void MakeTotalCombineQDatas(TotalQDataCollector &theTotalQDataCollector,
                                   GribFilterOptions &theGribFilterOptionsOut)
{
  if (theTotalQDataCollector.AddCount() == 0)
    throw runtime_error("Error unable to create any data.");
  else if (theTotalQDataCollector.AddCount() == 1)
  {
    TotalQDataCollector::Container::iterator it =
        theTotalQDataCollector.TotalGeneratedQDatas().begin();
    theGribFilterOptionsOut.itsGeneratedDatas =
        *it;  // Jos vain yhdest‰ grib-tiedostosta dataa, laitetaan ne sellaisenaan output-datoiksi
  }
  else
  {  // Jos useita grib-l‰hteit‰, yritet‰‰n muodostaan t‰ss‰ niist‰ kokooma-qdatoja.
    // T‰ss‰ luodaan luultavasti aikayhdistelmi‰, jos l‰hde gribit ovat olleet per aika-askel
    // tyyliin.
    // 0. Etsi kaikki mahdolliset levelTypet
    set<long> levelTypes = ::FindAllLevelTypes(theTotalQDataCollector);
    // 0.1 Tee levelType pohjainen struktuuri pohjadatoille
    map<long, CombineDataStructureSearcher> levelTypeStructures;
    for (set<long>::iterator it = levelTypes.begin(); it != levelTypes.end(); ++it)
      levelTypeStructures.insert(make_pair(*it, CombineDataStructureSearcher()));
    // 1. K‰y l‰pi datat ja etsi pohjat (hila+alue, parametrit, levelit, ajat) kaikille eri
    // level-tyypeille (esim. pinta, painepinta ja mallipinta).
    ::SearchForDataStructures(theTotalQDataCollector, levelTypeStructures);
    // 2. Luo eri level-tyypeille tarvittavat descriptorit -> innerInfo -> queryData-pohja
    vector<boost::shared_ptr<NFmiQueryData> > generatedQDatas =
        ::CreateEmptyQDataVector(levelTypeStructures);
    // 3. Tee fastInfot datoille ja t‰yt‰ eri datat
    for (size_t i = 0; i < generatedQDatas.size(); i++)
      ::FillQData(generatedQDatas[i], theTotalQDataCollector);
    theGribFilterOptionsOut.itsGeneratedDatas = generatedQDatas;
  }
}

static void DoErrorReporting(const std::string &theBaseString,
                             const std::string &theSecondString,
                             const std::string &theFileName,
                             std::exception *theException = 0)
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
  cerr << errStr << std::endl;
}

static void ConvertSingleGribFile(const GribFilterOptions &theGribFilterOptionsIn,
                                  const string &theGribFileName)
{
  GribFilterOptions gribFilterOptionsLocal = theGribFilterOptionsIn;
  gribFilterOptionsLocal.itsInputFileNameStr = theGribFileName;
  if ((gribFilterOptionsLocal.itsInputFile =
           ::fopen(gribFilterOptionsLocal.itsInputFileNameStr.c_str(), "rb")) == nullptr)
  {
    cerr << "could not open input file: " << gribFilterOptionsLocal.itsInputFileNameStr << endl;
    return;
  }

  try
  {
    ::ConvertGrib2QData(gribFilterOptionsLocal);
    gTotalQDataCollector.AddData(gribFilterOptionsLocal.itsGeneratedDatas);
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

static int BuildAndStoreAllDatas(vector<string> &theFileList,
                                 GribFilterOptions &theGribFilterOptions)
{
  size_t fileCount = theFileList.size();
  for (size_t i = 0; i < fileCount; i++)
    ::ConvertSingleGribFile(theGribFilterOptions, theFileList[i]);

  /* // Multi-threaddaava versio ei toimi, koska grib_api ei ilmeisesti toimi, jos k‰yd‰‰n l‰pi
     samaan aikaan useita grib-tiedostoja
          for(size_t i=0; i < fileCount; )
          {
                  boost::thread_group calcFiles;
                  calcFiles.add_thread(new boost::thread(::ConvertSingleGribFile,
     theGribFilterOptions, theFileList[i++]));
                  if(i < fileCount)
                          calcFiles.add_thread(new boost::thread(::ConvertSingleGribFile,
     theGribFilterOptions, theFileList[i++]));
                  if(i < fileCount)
                          calcFiles.add_thread(new boost::thread(::ConvertSingleGribFile,
     theGribFilterOptions, theFileList[i++]));
                  if(i < fileCount)
                          calcFiles.add_thread(new boost::thread(::ConvertSingleGribFile,
     theGribFilterOptions, theFileList[i++]));
                  calcFiles.join_all(); // odotetaan ett‰ threadit lopettavat
          }
  */
  ::MakeTotalCombineQDatas(gTotalQDataCollector, theGribFilterOptions);
  ::StoreQueryDatas(theGribFilterOptions);

  return theGribFilterOptions.itsReturnStatus;
}

int Run(int argc, const char **argv, bool &fReportExecutionTime)
{
  // Optiot:
  GribFilterOptions gribFilterOptions;

  NFmiCmdLine cmdline(argc, argv, "o!m!l!g!p!fnL!G!c!dvP!D!tH!r!1");

  // Tarkistetaan optioiden oikeus:
  std::string filePatternOrDirectory;  // ohjelman 1. argumentti sis‰lt‰‰ joko tiedoston nimen,
                                       // patternin tai hakemiston.
  // Jonkin n‰ist‰ avulla muodostetaan lista, jossa voi olla 0-n kpl tiedoston nimi‰.
  if (::DoCommandLineCheck(cmdline, filePatternOrDirectory) == false) return 1;

  if (cmdline.isOption('t')) fReportExecutionTime = true;
  if (::GribDefinitionPath(cmdline) == false) return 1;
  int status = ::GetOptions(cmdline, gribFilterOptions);
  if (status != 0) return status;

  vector<string> fileList = ::GetDataFiles(filePatternOrDirectory);
  if (fileList.empty())
    throw runtime_error(string("Error there were no matching grib files or given directory was "
                               "empty, see parameter:\n") +
                        filePatternOrDirectory);

  return ::BuildAndStoreAllDatas(fileList, gribFilterOptions);
}

int main(int argc, const char **argv)
{
  int returnStatus = 0;  // 0 = ok
  bool reportExecutionTime = false;
  NFmiMilliSecondTimer timer;
  timer.StartTimer();

  try
  {
    returnStatus = ::Run(argc, argv, reportExecutionTime);
  }
  catch (exception &e)
  {
    cerr << "Error in program's execution:\n" << e.what() << endl;
    returnStatus = 1;
  }
  catch (...)
  {
    cerr << "Unknown error: error in program or data? Stopping the execution..." << endl;
    returnStatus = 1;
  }

  timer.StopTimer();
  if (reportExecutionTime) cerr << "Execution run time: " << timer.EasyTimeDiffStr() << endl;

  return returnStatus;
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cout << "Usage: grib2qd [options] inputgribfile  > outputqdata" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-m <max-data-sizeMB>\tMax size of generated data, default = 1024 MB" << endl
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
       << "\t-f don't-do-global-'fix' If data is in latlon projection and covers" << endl
       << "\t\tthe globe 0->360, SmartMet cannot display data and data must" << endl
       << "\t\tbe converted so its longitudes goes from -180 to 180. This" << endl
       << "\t\toption will disable this function." << endl
       << "\t-n   Names output files by level type. E.g. output.sqd_levelType_100" << endl
       << "\t-t   Reports run-time to the stderr at the end of execution" << endl
       << "\t-v   verbose mode" << endl
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

static bool AcceptThisLevelType(GridRecordData *data, vector<FmiLevelType> &theAcceptOnlyLevelTypes)
{
  if (theAcceptOnlyLevelTypes.size() > 0)
  {
    for (unsigned int i = 0; i < theAcceptOnlyLevelTypes.size(); i++)
    {
      if (theAcceptOnlyLevelTypes[i] == data->itsLevel.LevelType())
        return true;  // skipataan jokerin valuen yhteydess‰ kaikki kyseisen level tyypin kent‰t
    }
    return false;  // jos oli accept lista mutta ei lˆytynyt leveltype‰ siit‰, hylk‰‰
  }
  return true;
}

static bool IgnoreThisLevel(GridRecordData *data, NFmiLevelBag &theIgnoredLevelList)
{
  if (theIgnoredLevelList.GetSize() > 0)
  {
    for (theIgnoredLevelList.Reset(); theIgnoredLevelList.Next();)
    {
      if (theIgnoredLevelList.Level()->LevelValue() == gMissLevelValue &&
          theIgnoredLevelList.Level()->LevelType() == data->itsLevel.LevelType())
        return true;  // skipataan jokerin valuen yhteydess‰ kaikki kyseisen level tyypin kent‰t
      if (*(theIgnoredLevelList.Level()) == data->itsLevel) return true;
    }
  }
  return false;
}

static void FreeDatas(vector<GridRecordData *> &theGribRecordDatas)
{
  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
    delete *it;
}

void FixPacificLongitude(NFmiPoint &lonLat)
{
  if (lonLat.X() < 0)
  {
    NFmiLongitude lon(lonLat.X(), true);
    lonLat.X(lon.Value());
  }
}

static NFmiArea *CreateLatlonArea(grib_handle *theGribHandle, bool doAtlanticFix)
{
  double La1 = 0;
  int status1 = grib_get_double(theGribHandle, "latitudeOfFirstGridPointInDegrees", &La1);
  double Lo1 = 0;
  int status2 = grib_get_double(theGribHandle, "longitudeOfFirstGridPointInDegrees", &Lo1);
  double La2 = 0;
  int status3 = grib_get_double(theGribHandle, "latitudeOfLastGridPointInDegrees", &La2);
  double Lo2 = 0;
  int status4 = grib_get_double(theGribHandle, "longitudeOfLastGridPointInDegrees", &Lo2);

  if (status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0)
  {
    // We ignore the status of the version check intentionally and assume V2
    long version = 2;
    grib_get_long(theGribHandle, "editionNumber", &version);

    long iScansNegatively = 0;
    int iScansNegativelyStatus =
        grib_get_long(theGribHandle, "iScansNegatively", &iScansNegatively);

    // Not needed:
    // check_jscan_direction(theGribHandle);

    if (doAtlanticFix && Lo1 == 0 && (Lo2 < 0 || Lo2 > 350))
    {
      Lo1 = -180;
      Lo2 = 180;
    }
    else if (iScansNegativelyStatus == 0 && !iScansNegatively)
    {
      // ei swapata longitude arvoja, mutta voidaan fiksailla niit‰, jos annettu Lo1 on suurempi
      // kuin annettu Lo2
      if (Lo1 > Lo2 && Lo1 >= 180)
        Lo1 -= 360;  // fiksataan vasenmman reunan pacific arvo atlantiseksi
    }
    else if (Lo1 > Lo2)
      std::swap(Lo1, Lo2);

    if (La1 > La2) std::swap(La1, La2);

    NFmiPoint bl(Lo1, La1);
    NFmiPoint tr(Lo2, La2);
    bool usePacificView = NFmiArea::IsPacificView(bl, tr);
    if (usePacificView) ::FixPacificLongitude(tr);

    return new NFmiLatLonArea(bl, tr, NFmiPoint(0, 0), NFmiPoint(1, 1), usePacificView);
  }
  else
    throw runtime_error("Error: Unable to retrieve latlon-projection information from grib.");
}

/*
static NFmiArea* CreateLatlonArea(grib_handle *theGribHandle, bool &doGlobeFix)
{
        double La1 = 0;
        int status1 = grib_get_double(theGribHandle, "latitudeOfFirstGridPointInDegrees", &La1);
        double Lo1 = 0;
        int status2 = grib_get_double(theGribHandle, "longitudeOfFirstGridPointInDegrees", &Lo1);
        double La2 = 0;
        int status3 = grib_get_double(theGribHandle, "latitudeOfLastGridPointInDegrees", &La2);
        double Lo2 = 0;
        int status4 = grib_get_double(theGribHandle, "longitudeOfLastGridPointInDegrees", &Lo2);

        if(status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0)
        {
                if(doGlobeFix && Lo1 == 0 && (Lo2 < 0 || Lo2 > 350))
                {
                        Lo1 = -180;
                        Lo2 = 180;
                }
                else
                        doGlobeFix = false;

                return new NFmiLatLonArea(NFmiPoint(Lo1, FmiMin(La1, La2)), NFmiPoint(Lo2,
FmiMax(La1, La2)));
        }
        else
                throw runtime_error("Error: Unable to retrieve latlon-projection information from
grib.");
}
*/

static NFmiArea *CreateMercatorArea(grib_handle *theGribHandle)
{
  double La1 = 0;
  int status1 = grib_get_double(theGribHandle, "latitudeOfFirstGridPointInDegrees", &La1);
  double Lo1 = 0;
  int status2 = grib_get_double(theGribHandle, "longitudeOfFirstGridPointInDegrees", &Lo1);
  double La2 = 0;
  int status3 = grib_get_double(theGribHandle, "latitudeOfLastGridPointInDegrees", &La2);
  double Lo2 = 0;
  int status4 = grib_get_double(theGribHandle, "longitudeOfLastGridPointInDegrees", &Lo2);

  if (status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0)
  {
    return new NFmiMercatorArea(NFmiPoint(Lo1, FmiMin(La1, La2)), NFmiPoint(Lo2, FmiMax(La1, La2)));
  }

  else if (status1 == 0 && status2 == 0)
  {
    // Not needed:
    // check_jscan_direction(theGribHandle);

    long nx = 0;
    long ny = 0;
    int status9 = ::grib_get_long(theGribHandle, "numberOfPointsAlongAParallel", &nx);
    if (status9 != 0) status9 = ::grib_get_long(theGribHandle, "numberOfPointsAlongXAxis", &nx);
    int status6 = ::grib_get_long(theGribHandle, "numberOfPointsAlongAMeridian", &ny);
    if (status6 != 0) status6 = ::grib_get_long(theGribHandle, "numberOfPointsAlongYAxis", &ny);

    double dx = 0;
    double dy = 0;
    int status7 = grib_get_double(theGribHandle, "xDirectionGridLength", &dx);
    int status8 = grib_get_double(theGribHandle, "yDirectionGridLength", &dy);

    if (status9 == 0 && status6 == 0 && status7 == 0 && status8 == 0)
    {
      NFmiPoint bottomLeft(Lo1, La1);
      NFmiPoint dummyTopRight(Lo1 + 5, La1 + 5);
      NFmiMercatorArea dummyArea(bottomLeft, dummyTopRight);
      NFmiPoint xyBottomLeft = dummyArea.LatLonToWorldXY(dummyArea.BottomLeftLatLon());
      NFmiPoint xyTopRight(xyBottomLeft);
      xyTopRight.X(xyTopRight.X() + (nx - 1) * dx / 1000.);
      xyTopRight.Y(xyTopRight.Y() + (ny - 1) * dy / 1000.);

      NFmiPoint topRight(dummyArea.WorldXYToLatLon(xyTopRight));
      NFmiArea *area = new NFmiMercatorArea(bottomLeft, topRight);

      return area;
    }
  }
  throw runtime_error("Error: Unable to retrieve mercator-projection information from grib.");
}

static NFmiArea *CreatePolarStereographicArea(grib_handle *theGribHandle)
{
  double La1 = 0, Lo1 = 0, Lov = 0, Lad = 0, Lad2 = 0;
  int badLa1 = grib_get_double(theGribHandle, "latitudeOfFirstGridPointInDegrees", &La1);
  int badLo1 = grib_get_double(theGribHandle, "longitudeOfFirstGridPointInDegrees", &Lo1);
  int badLov = grib_get_double(theGribHandle, "orientationOfTheGridInDegrees", &Lov);
  int badLad = grib_get_double(theGribHandle, "LaDInDegrees", &Lad);
  int badLad2 = grib_get_double(theGribHandle, "latitudeWhereDxAndDyAreSpecifiedInDegrees", &Lad2);
  double usedLad = (badLad == 0) ? Lad : Lad2;
  int usedBadLad = (badLad == 0) ? badLad : badLad2;

  long pcentre = 0;
  int badPcentre = grib_get_long(theGribHandle, "projectionCentreFlag", &pcentre);

  if (!badPcentre && pcentre != 0)
    throw runtime_error("Error: South pole not supported for polster-projection");

  long nx = 0, ny = 0;
  int badNx = ::grib_get_long(theGribHandle, "numberOfPointsAlongXAxis", &nx);
  int badNy = ::grib_get_long(theGribHandle, "numberOfPointsAlongYAxis", &ny);

  double dx = 0, dy = 0;
  int badDx = grib_get_double(theGribHandle, "xDirectionGridLength", &dx);
  int badDy = grib_get_double(theGribHandle, "yDirectionGridLength", &dy);
  double dxInM = 0, dyInM = 0;
  int badDxInM = grib_get_double(theGribHandle, "xDirectionGridLengthInMetres", &dxInM);
  int badDyInM = grib_get_double(theGribHandle, "yDirectionGridLengthInMetres", &dyInM);
  if ((badDx || badDy) && (badDxInM == 0 && badDyInM == 0))
  {  // jos jompi kumpi oli error koodissa, yritet‰‰n laskea arvot metrisist‰ arvoista
    dx = dxInM * 1000.;
    dy = dyInM * 1000.;
    badDx = 0;
    badDy = 0;
  }

  if (!badLa1 && !badLo1 && !badLov && !usedBadLad && !badNx && !badNy && !badDx && !badDy)
  {
    // Has to be done, or corners should be recalculated:
    // check_jscan_direction(theGribHandle);

    NFmiPoint bottom_left(Lo1, La1);
    NFmiPoint top_left_xy(0, 0);
    NFmiPoint top_right_xy(1, 1);

    double width_in_meters = (nx - 1) * dx / 1000.0;
    double height_in_meters = (ny - 1) * dy / 1000.0;

    NFmiArea *area = new NFmiStereographicArea(bottom_left,
                                               width_in_meters,
                                               height_in_meters,
                                               Lov,
                                               top_left_xy,
                                               top_right_xy,
                                               90,
                                               usedLad);
    return area;
  }

  throw runtime_error("Error: Unable to retrieve polster-projection information from grib.");
}

// laske sellainen gridi, joka menee originaali hilan hilapisteikˆn mukaan, mutta peitt‰‰ sen
// alueen,
// joka on annettu itsLatlonCropRect:iss‰ siten ett‰ uusi hila menee juuri seuraaviin hilapisteisiin
// originaali hilassa ett‰ se on juuri isompi kuin croppi alue, paitsi jos croppi alue menee
// originaali alueen ulkopuolelle, t‰llˆin croppihila lasketaan todella niin ett‰ originaali hila
// jatkuisi
// alueen ulkopuolelle. Seurauksena tulosdata, jossa on sitten orig alueen ulkopuolella puuttuvaa
// dataa.
static void CalcCroppedGrid(GridRecordData *theGridRecordData)
{
  NFmiGrid grid(theGridRecordData->itsOrigGrid.itsArea,
                theGridRecordData->itsOrigGrid.itsNX,
                theGridRecordData->itsOrigGrid.itsNY);

  NFmiPoint xy1 = grid.LatLonToGrid(
      theGridRecordData->itsLatlonCropRect
          .TopLeft());  // HUOM! rect maailmassa pit‰‰ olla TopLeft, eik‰ BottomLeft!
  xy1.X(::floor(xy1.X()));
  xy1.Y(::floor(xy1.Y()));
  NFmiPoint xy2 = grid.LatLonToGrid(
      theGridRecordData->itsLatlonCropRect
          .BottomRight());  // HUOM! rect maailmassa pit‰‰ olla BottomRight, eik‰ TopRight!
  xy2.X(::ceil(xy2.X()));
  xy2.Y(::ceil(xy2.Y()));

  NFmiPoint latlon1 = grid.GridToLatLon(xy1);
  NFmiPoint latlon2 = grid.GridToLatLon(xy2);
  NFmiArea *newArea = 0;
  if (theGridRecordData->itsOrigGrid.itsArea->ClassId() == kNFmiLatLonArea)
  {
    newArea = new NFmiLatLonArea(latlon1, latlon2);
  }
  else
    throw runtime_error("Error: CalcCroppedGrid doesn't support this projection yet.");

  boost::shared_ptr<NFmiArea> newAreaPtr(newArea);
  theGridRecordData->itsGridPointCropOffset = NFmiPoint(xy1.X(), xy1.Y());
  MyGrid newGrid(
      newArea, static_cast<int>(xy2.X() - xy1.X() + 1), static_cast<int>(xy2.Y() - xy1.Y() + 1));
  theGridRecordData->itsGrid = newGrid;
  theGridRecordData->fDoProjectionConversion = true;
}

static void FillGridInfoFromGribHandle(grib_handle *theGribHandle,
                                       GridRecordData *theGridRecordData,
                                       bool &doGlobeFix,
                                       GridSettingsPackage &theGridSettings)
{
  long gridDefinitionTemplateNumber =
      0;  // t‰h‰n tulee projektio tyyppi, ks. qooglesta (grib2 Table 3.1)
  int status =
      ::grib_get_long(theGribHandle, "gridDefinitionTemplateNumber", &gridDefinitionTemplateNumber);
  if (status == 0)
  {
    NFmiArea *area = 0;

    // From: definitions/grib2/section.3.def
    //
    // "regular_ll"            = { gridDefinitionTemplateNumber=0;  PLPresent=0;  }
    // "rotated_ll"            = { gridDefinitionTemplateNumber=1;  PLPresent=0;  }
    // "mercator"              = { gridDefinitionTemplateNumber=10; PLPresent=0;  }
    // "polar_stereographic"   = { gridDefinitionTemplateNumber=20; PLPresent=0;  }

    switch (gridDefinitionTemplateNumber)
    {
      case 0:
        area = ::CreateLatlonArea(theGribHandle, doGlobeFix);
        break;
      case 10:
        doGlobeFix = false;  // T‰m‰ doGlobeFix -systeemi on rakennettu v‰‰rin. Pit‰isi olla
                             // allowGlobeFix-optio ja t‰m‰ doGlobeFix olisi muuten false, paitsi
                             // jos allowGlobeFix=true ja latlon area, jossa 0-360-maailma.
        area = ::CreateMercatorArea(theGribHandle);
        break;
      case 20:
        doGlobeFix = false;  // T‰m‰ doGlobeFix -systeemi on rakennettu v‰‰rin. Pit‰isi olla
                             // allowGlobeFix-optio ja t‰m‰ doGlobeFix olisi muuten false, paitsi
                             // jos allowGlobeFix=true ja latlon area, jossa 0-360-maailma.
        area = ::CreatePolarStereographicArea(theGribHandle);
        break;
      default:
        throw runtime_error(
            "Error: Handling of projection found from grib is not implemented yet.");
    }

    long numberOfPointsAlongAParallel = 0;
    int status1 = ::grib_get_long(
        theGribHandle, "numberOfPointsAlongAParallel", &numberOfPointsAlongAParallel);
    if (status1 != 0)
      status1 =
          ::grib_get_long(theGribHandle, "numberOfPointsAlongXAxis", &numberOfPointsAlongAParallel);
    long numberOfPointsAlongAMeridian = 0;
    int status2 = ::grib_get_long(
        theGribHandle, "numberOfPointsAlongAMeridian", &numberOfPointsAlongAMeridian);
    if (status2 != 0)
      status2 =
          ::grib_get_long(theGribHandle, "numberOfPointsAlongYAxis", &numberOfPointsAlongAMeridian);
    if (status1 == 0 && status2 == 0)
    {
      theGridRecordData->fDoProjectionConversion = false;
      theGridRecordData->itsOrigGrid =
          MyGrid(area, numberOfPointsAlongAParallel, numberOfPointsAlongAMeridian);
      if (theGridSettings.itsBaseGrid)
      {
        MyGrid usedGrid(*theGridSettings.itsBaseGrid);
        if (theGridRecordData->itsLevel.LevelType() == kFmiHybridLevel)
        {
          usedGrid = *theGridSettings.itsHybridGrid;
        }
        else if (theGridRecordData->itsLevel.LevelType() == kFmiPressureLevel)
        {
          usedGrid = *theGridSettings.itsPressureGrid;
        }
        theGridRecordData->itsGrid = usedGrid;
        theGridRecordData->fDoProjectionConversion = true;
      }
      else if (theGridRecordData->itsLatlonCropRect == gMissingCropRect)
        theGridRecordData->itsGrid =
            theGridRecordData
                ->itsOrigGrid;  // jos ei haluttu croppia, on crop grid sama kuin datan orig grid
      else
      {
        ::CalcCroppedGrid(theGridRecordData);
      }
    }
    else
      throw runtime_error("Error: Couldn't get grid x-y sizes from given grib_handle.");
  }
  else
    throw runtime_error("Error: Couldn't get gridDefinitionTemplateNumber from given grib_handle.");
}

static void GetLevelVerticalCoordinates(grib_handle *theGribHandle,
                                        GridRecordData &theGridRecordData,
                                        map<int, pair<double, double> > &theVerticalCoordinateMap)
{
  // 1. Onko data hybrid-dataa?
  if (theGridRecordData.itsLevel.LevelType() == kFmiHybridLevel)
  {
    // 2. Onko t‰lle levelille jo olemassa kertoimia?
    int level = static_cast<int>(::round(theGridRecordData.itsLevel.LevelValue()));
    map<int, pair<double, double> >::iterator it = theVerticalCoordinateMap.find(level);
    if (it == theVerticalCoordinateMap.end())
    {
      // 3. Jos ei, hae koordinaatti taulu (jos lˆytyy siis)
      long numberOfVerticalCoordinateValues = 0;
      int numberOfVerticalCoordinateValuesOk = ::grib_get_long(
          theGribHandle, "numberOfVerticalCoordinateValues", &numberOfVerticalCoordinateValues);

      if (numberOfVerticalCoordinateValuesOk == 0 && numberOfVerticalCoordinateValues > 0)
      {
        vector<double> pv(numberOfVerticalCoordinateValues);
        size_t num_coord = pv.size();
        int pvValuesOk = ::grib_get_double_array(theGribHandle, "pv", &pv[0], &num_coord);
        if (pvValuesOk == 0)
        {
          // 4. Tapuksesta riippuen laske a- ja b-kertoimet ja talleta ne level-tauluun (ks. mallia
          // K. Niemel‰n alla olevasta koodista)
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
             // kertovat EC:st‰ ja Arome:sta
            // ELI EC ja Arome antavat kaikkien hybrid levelien kertoimet jokaisen erillisen
            // hybrid-levelin kanssa,
            // joten taulukosta pit‰‰ osata valita oikeat kohdat laskuihin.
            // TODO: Pit‰‰ kysy‰ Karilta selvennyst‰, miksi lasketaan kertoimista keskiarvo kahden
            // levelin v‰lille?!?!?
            double a = (pv[level - 1] + pv[level]) / 2.;
            double b = (pv[num_coord / 2 + level - 1] + pv[num_coord / 2 + level]) / 2.;
            theVerticalCoordinateMap.insert(make_pair(level, make_pair(a, b)));
          }
        }
      }
    }
  }
}

/*
Kari Niemel‰n antamaa koodia vertikaalikoordinaattien laskemista varten
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
// enemm‰n parametreja, poimitaan t‰m‰n pinnan parametrit
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
// enemm‰n parametreja, poimitaan t‰m‰n pinnan parametrit
//       Seuraavat ehdot on keksittyj‰: A, B, A, B, A, B,...
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

static NFmiMetTime GetOrigTime(grib_handle *theGribHandle)
{
  long year = 0;
  int status1 = grib_get_long(theGribHandle, "year", &year);
  long month = 0;
  int status2 = grib_get_long(theGribHandle, "month", &month);
  long day = 0;
  int status3 = grib_get_long(theGribHandle, "day", &day);
  long hour = 0;
  int status4 = grib_get_long(theGribHandle, "hour", &hour);
  long minute = 0;
  int status5 = grib_get_long(theGribHandle, "minute", &minute);
  //	long second = 0;
  //	int status6 = grib_get_long(theGribHandle, "second", &second);
  if (status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0 && status5 == 0)
  {
    NFmiMetTime origTime(static_cast<short>(year),
                         static_cast<short>(month),
                         static_cast<short>(day),
                         static_cast<short>(hour),
                         static_cast<short>(minute),
                         0 /* seconds */,
                         1 /* timestep round in mins */);
    return origTime;
  }
  else
    throw runtime_error("Error: Couldn't get origTime from given grib_handle.");
}

static NFmiMetTime GetValidTime(grib_handle *theGribHandle)
{
  long validityDate = 0;
  long validityTime = 0;

  // GRIB1 & GRIB2 edition independent keys

  int err1 = grib_get_long(theGribHandle, "validityDate", &validityDate);
  int err2 = grib_get_long(theGribHandle, "validityTime", &validityTime);

  if (!err1 && !err2)
  {
    short year = static_cast<short>(validityDate / 10000);
    short month = static_cast<short>((validityDate / 100) % 100);
    short day = static_cast<short>(validityDate % 100);
    short hour = static_cast<short>((validityTime / 100) % 100);
    short min = static_cast<short>(validityTime % 100);

    return NFmiMetTime(year, month, day, hour, min, 0, 1);
  }

  // Old GRIB1 (?) dated code

  long forecastTime = 0;
  NFmiMetTime validTime = ::GetOrigTime(theGribHandle);

  int status = grib_get_long(
      theGribHandle,
      "forecastTime",
      &forecastTime);  // t‰m‰ on loogisin nimi ennusteen validTime siirtym‰‰n originTimesta
  if (status != 0)
  {
    status = grib_get_long(
        theGribHandle,
        "stepRange",
        &forecastTime);  // t‰m‰ n‰ytt‰‰ olevan aina, yritet‰‰n k‰ytt‰‰ jos forecastTime:a ei lˆydy
    if (status != 0)
    {
      status = grib_get_long(theGribHandle, "P1", &forecastTime);  // P1:ss‰ n‰ytt‰‰ joskus myˆs
                                                                   // olevan ennustetunti, jos
                                                                   // edellisist‰ ei lˆydy....
    }
  }

  if (status == 0)
  {
    validTime.ChangeByHours(forecastTime);
    return validTime;
  }
  else
    throw runtime_error("Error: Couldn't get validTime from given grib_handle.");
}

static bool GetGribStringValue(grib_handle *theGribHandle,
                               const std::string &theDefinitionName,
                               std::string &theStringValueOut)
{
  static const std::string unacceptableStringValue =
      "unknown";  // t‰m‰ arvo tulee jos kyseist‰ definitiota ei ole m‰‰r‰tty gribiss‰ kun tekee
                  // grib_get_string -kutsun
  char stringValue[128] = "";
  size_t stringValueBufferSize = sizeof(stringValue);
  int getFailed = grib_get_string(
      theGribHandle, theDefinitionName.c_str(), stringValue, &stringValueBufferSize);
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

static bool GetGribLongValue(grib_handle *theGribHandle,
                             const std::string &theDefinitionName,
                             long &theLongValueOut)
{
  return grib_get_long(theGribHandle, theDefinitionName.c_str(), &theLongValueOut) == 0;
}

static bool GetGribDoubleValue(grib_handle *theGribHandle,
                               const std::string &theDefinitionName,
                               double &theDoubleValueOut)
{
  return grib_get_double(theGribHandle, theDefinitionName.c_str(), &theDoubleValueOut) == 0;
}

static std::string GetParamName(grib_handle *theGribHandle)
{
  std::string cfName;
  bool cfNameOk = ::GetGribStringValue(theGribHandle, "cfName", cfName);
  std::string name;
  bool nameOk = ::GetGribStringValue(theGribHandle, "name", name);
  std::string shortName;
  bool shortNameOk = ::GetGribStringValue(theGribHandle, "shortName", shortName);

  std::string usedParameterName;
  if (nameOk)
    usedParameterName = name;
  else if (shortNameOk)
    usedParameterName = shortName;
  if (usedParameterName.empty() == false && cfNameOk) usedParameterName += " - ";
  if (cfNameOk) usedParameterName += cfName;

  // haetaan viel‰ mahdollinen yksikkˆ stringi ja liitet‰‰n se parametrin nimeen
  std::string unitsStr;
  if (::GetGribStringValue(theGribHandle, "units", unitsStr))
  {  // jos onnistui, liitet‰‰n yksikkˆ hakasuluissa parametrin nimen per‰‰n
    if (usedParameterName.empty() == false) usedParameterName += " ";
    usedParameterName += "[";
    usedParameterName += unitsStr;
    usedParameterName += "]";
  }

  return usedParameterName;
}

static long GetUsedParamId(grib_handle *theGribHandle)
{
  // 1. Katsotaan onko indicatorOfParameter -id k‰ytˆss‰, jos on, k‰ytet‰‰n sit‰.
  long indicatorOfParameter = 0;
  bool indicatorOfParameterOk =
      ::GetGribLongValue(theGribHandle, "indicatorOfParameter", indicatorOfParameter);
  if (indicatorOfParameterOk) return indicatorOfParameter;

  // 2. kokeillaan parameterCategory + parameterNumber yhdistelm‰‰, jossa lopullinen arvo saadaan
  // ((paramCategory * 1000) + paramNumber)
  long parameterCategory = 0;
  bool parameterCategoryOk =
      ::GetGribLongValue(theGribHandle, "parameterCategory", parameterCategory);
  long parameterNumber = 0;
  bool parameterNumberOk = ::GetGribLongValue(theGribHandle, "parameterNumber", parameterNumber);
  if (parameterCategoryOk && parameterNumberOk) return (parameterCategory * 1000) + parameterNumber;

  // 3. Kokeillaan lˆytyykˆ parameter -hakusanalla
  long parameter = 0;
  bool parameterOk = ::GetGribLongValue(theGribHandle, "parameter", parameter);
  if (parameterOk) return parameter;

  // 4. Kokeillaan lˆytyykˆ paramId -hakusanalla
  long paramId = 0;
  bool paramIdOk = ::GetGribLongValue(theGribHandle, "paramId", paramId);
  if (paramIdOk) return paramId;

  throw runtime_error("Error: Couldn't get parameter-id from given grib_handle.");
}

static NFmiDataIdent GetParam(grib_handle *theGribHandle, const NFmiProducer &theWantedProducer)
{
  // TODO producerin voisi p‰‰tell‰ originating center numeroista

  std::string usedParameterName = ::GetParamName(theGribHandle);
  long usedParId = GetUsedParamId(theGribHandle);
  if (usedParameterName.empty()) usedParameterName = NFmiStringTools::Convert(usedParId);
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

static long GetUsedLevelType(grib_handle *theGribHandle)
{
  long indicatorOfTypeOfLevel = 0;
  bool indicatorOfTypeOk =
      ::GetGribLongValue(theGribHandle, "indicatorOfTypeOfLevel", indicatorOfTypeOfLevel);
  long typeOfFirstFixedSurface = 0;
  bool typeOfFirstOk =
      ::GetGribLongValue(theGribHandle, "typeOfFirstFixedSurface", typeOfFirstFixedSurface);
  std::string typeOfLevelValueStr;
  bool levelValueStrOk = ::GetGribStringValue(theGribHandle, "typeOfLevel", typeOfLevelValueStr);

  if (indicatorOfTypeOk == false && typeOfFirstOk == false && levelValueStrOk == false)
    throw runtime_error("Error: Couldn't get levelType from given grib_handle.");

  long usedLevelType = 0;
  if (indicatorOfTypeOk)
    usedLevelType = indicatorOfTypeOfLevel;
  else if (typeOfFirstOk)
    usedLevelType = typeOfFirstFixedSurface;

  if (levelValueStrOk && typeOfLevelValueStr == "hybrid" && usedLevelType != kFmiHybridLevel)
    usedLevelType = kFmiHybridLevel;  // t‰m‰ on ik‰v‰‰ koodia, mutta en keksi muuta keinoa Latvian
                                      // EC-datan kanssa. Muuten hybrid-datan leveltyypiksi tulee
                                      // 105 eli heightType

  return usedLevelType;
}

static NFmiLevel GetLevel(grib_handle *theGribHandle)
{
  long usedLevelType = ::GetUsedLevelType(theGribHandle);

  double levelValue = 0;
  bool levelValueOk = ::GetGribDoubleValue(theGribHandle, "level", levelValue);

  if (levelValueOk)
    return NFmiLevel(
        usedLevelType, NFmiStringTools::Convert(levelValue), static_cast<float>(levelValue));
  else
    throw runtime_error("Error: Couldn't get level from given grib_handle.");
}

static double GetMissingValue(grib_handle *theGribHandle)
{
  double missingValue = 0;
  int status = grib_get_double(theGribHandle, "missingValue", &missingValue);
  if (status == 0)
    return missingValue;
  else
    throw runtime_error("Error: Couldn't get missingValue from given grib_handle.");
}

static void MakeParameterConversions(GridRecordData *theGridRecordData,
                                     vector<ParamChangeItem> &theParamChangeTable)
{
  if (theParamChangeTable.size() > 0)
  {  // tehd‰‰n tarvittaessa parametrille base+scale muunnos
    for (unsigned int p = 0; p < theParamChangeTable.size(); p++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[p];
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

static void DoGlobalFix(NFmiDataMatrix<float> &theOrigValues, bool fDoGlobeFix, bool verbose)
{
  if (fDoGlobeFix)
  {
    // Nyt on siis tilanne ett‰ halutaan 'korjata' globaali data editoria varten.
    // Latlon-area peitt‰‰ maapallon longitudeissa 0-360, se pit‰‰ muuttaa editoria varten
    // longitudeihin -180 - 180.
    // Eli matriisissa olevia arvoja pit‰‰ siirt‰‰ niin ett‰ vasemmalla puoliskolla olevat laitetaan
    // oikealle
    // puolelle ja toisin p‰in.
    if (verbose) cerr << " f";
    int nx = static_cast<int>(theOrigValues.NX());
    int ny = static_cast<int>(theOrigValues.NY());
    for (int j = 0; j < ny; j++)
      for (int i = 0; i < nx / 2; i++)
        std::swap(theOrigValues[i][j], theOrigValues[i + nx / 2][j]);
  }
}

static void CropData(GridRecordData *theGridRecordData,
                     NFmiDataMatrix<float> &theOrigValues,
                     bool verbose)
{
  // t‰ss‰ raaka hila croppaus
  if (verbose) cerr << " c";
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
      if ((x1 + i < 0) || (x1 + i >= origSizeX)) break;
      if ((y1 + j < 0) || (y1 + j >= origSizeY)) break;
      theGridRecordData->itsGridData[i][j] = theOrigValues[x1 + i][y1 + j];
    }
  }
}

static std::string MakeGridStr(const NFmiGrid &theGrid)
{
  std::string str;
  str += theGrid.Area()->AreaStr();
  str += ":";
  str += NFmiStringTools::Convert(theGrid.XNumber());
  str += ",";
  str += NFmiStringTools::Convert(theGrid.YNumber());
  return str;
}

static void ProjectData(GridRecordData *theGridRecordData,
                        NFmiDataMatrix<float> &theOrigValues,
                        bool verbose)
{
  static std::map<std::string, NFmiDataMatrix<NFmiLocationCache> > locationCacheMap;

  if (verbose) cerr << " p";

  NFmiGrid targetGrid(theGridRecordData->itsGrid.itsArea,
                      theGridRecordData->itsGrid.itsNX,
                      theGridRecordData->itsGrid.itsNY);
  std::string targetStr = ::MakeGridStr(targetGrid);
  NFmiGrid sourceGrid(theGridRecordData->itsOrigGrid.itsArea,
                      theGridRecordData->itsOrigGrid.itsNX,
                      theGridRecordData->itsOrigGrid.itsNY);
  std::string sourceStr = ::MakeGridStr(sourceGrid);

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
  for (targetGrid.Reset(); targetGrid.Next(); counter++)
  {
    NFmiLocationCache &locCache =
        locationCacheMatrix[targetGrid.Index() % targetXSize][targetGrid.Index() / targetXSize];
    int destX = counter % theGridRecordData->itsGrid.itsNX;
    int destY = counter / theGridRecordData->itsGrid.itsNX;
    theGridRecordData->itsGridData[destX][destY] = DataMatrixUtils::InterpolatedValue(
        theOrigValues, locCache.itsGridPoint, relativeRect, param, true);
  }
}

static void DoAreaManipulations(GridRecordData *theGridRecordData,
                                NFmiDataMatrix<float> &theOrigValues,
                                bool verbose)
{
  // 2. Kun orig matriisi on saatu t‰ytetty‰, katsotaan pit‰‰kˆ viel‰ t‰ytt‰‰ cropattu alue, vai
  // k‰ytet‰‰nkˆ originaali dataa suoraan.
  if (theGridRecordData->fDoProjectionConversion == false)
    theGridRecordData->itsGridData = theOrigValues;
  else if (theGridRecordData->fDoProjectionConversion == true &&
           theGridRecordData->itsLatlonCropRect == gMissingCropRect)
    ::ProjectData(theGridRecordData, theOrigValues, verbose);
  else
    ::CropData(theGridRecordData, theOrigValues, verbose);
}

static void FillGridData(grib_handle *theGribHandle,
                         GridRecordData *theGridRecordData,
                         bool doGlobeFix,
                         vector<ParamChangeItem> &theParamChangeTable,
                         bool verbose)
{
  // 1. T‰ytet‰‰n ensin origGridin kokoinen matriisi, koska pit‰‰ pysty‰ tekem‰‰n mm. global fix
  size_t values_length = 0;
  int status1 = grib_get_size(theGribHandle, "values", &values_length);
  vector<double> doubleValues(values_length);
  int status2 = grib_get_double_array(theGribHandle, "values", &doubleValues[0], &values_length);
  int gridXSize = theGridRecordData->itsOrigGrid.itsNX;
  int gridYSize = theGridRecordData->itsOrigGrid.itsNY;
  NFmiDataMatrix<float> origValues(gridXSize, gridYSize);
  if (status1 == 0 && status2 == 0)
  {
    long scanningMode = 0;
    int status4 = grib_get_long(theGribHandle, "scanningMode", &scanningMode);

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
      else  // sitten kun tulee lis‰‰ ceissej‰, lis‰t‰‰n eri t‰yttˆ variaatioita
      {
        throw runtime_error("Error: Scanning mode " + boost::lexical_cast<string>(scanningMode) +
                            " not yet implemented.");
      }
    }

    ::DoGlobalFix(origValues, doGlobeFix, verbose);
    ::DoAreaManipulations(theGridRecordData, origValues, verbose);
  }
  else
    throw runtime_error("Error: Couldn't get values-data from given grib_handle.");

  // 3. Tarkista viel‰, jos lˆytyy paramChangeTablesta parametrille muunnos kaavat jotka pit‰‰ tehd‰
  ::MakeParameterConversions(theGridRecordData, theParamChangeTable);
}

static void ChangeParamSettingsIfNeeded(vector<ParamChangeItem> &theParamChangeTable,
                                        GridRecordData *theGribData,
                                        bool verbose)
{
  if (theParamChangeTable.size() > 0)
  {  // muutetaan tarvittaessa parametrin nime‰ ja id:t‰
    for (unsigned int i = 0; i < theParamChangeTable.size(); i++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[i];

      if (paramChangeItem.itsOriginalParamId ==
              static_cast<long>(theGribData->itsParam.GetParamIdent()) &&
          paramChangeItem.itsLevel && (*paramChangeItem.itsLevel) == theGribData->itsLevel)
      {
        if (verbose)
        {
          cerr << paramChangeItem.itsOriginalParamId << " changed to "
               << theGribData->itsParam.GetParamIdent() << " "
               << theGribData->itsParam.GetParamName().CharPtr();
        }

        theGribData->ChangeParam(paramChangeItem.itsWantedParam);
        theGribData->itsLevel =
            NFmiLevel(1, "sfc", 0);  // tarkista ett‰ t‰st‰ tulee pinta level dataa
        if (verbose) cerr << " level -> sfc";
        break;
      }
      else if (paramChangeItem.itsOriginalParamId ==
                   static_cast<long>(theGribData->itsParam.GetParamIdent()) &&
               paramChangeItem.itsLevel == nullptr)
      {
        if (verbose)
        {
          cerr << paramChangeItem.itsOriginalParamId << " changed to "
               << theGribData->itsParam.GetParamIdent() << " "
               << theGribData->itsParam.GetParamName().CharPtr();
        }
        theGribData->ChangeParam(paramChangeItem.itsWantedParam);
        break;
      }
    }
  }
}

static bool CropParam(GridRecordData *gribData,
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

static string MakeParamString(const NFmiParam &theParam)
{
  string str("id: ");
  str += NFmiStringTools::Convert(theParam.GetIdent());
  str += " name: ";
  str += theParam.GetName();
  return str;
}

static void DoParamConflictError(const NFmiParam &theChangedParam,
                                 const NFmiParam &theOrigChangedParam,
                                 const NFmiParam &theUnChangedParam)
{
  string errorStr;
  errorStr += "\nError - conflict with changed param and non-changed param.";
  errorStr += "\nChanged param:\n";
  errorStr += ::MakeParamString(theChangedParam);
  errorStr += "\nChanged param originally:\n";
  errorStr += ::MakeParamString(theOrigChangedParam);
  errorStr += "\nand unchanged param:\n";
  errorStr += ::MakeParamString(theUnChangedParam);
  errorStr += "\n\nYou must either change the changed param id to some non-conflicting\n";
  errorStr += " value or you must also change the non-changed param id to some other value.\n";
  errorStr += "Program execution stops here because of the conflict.";
  throw runtime_error(errorStr);
}

static void DoParamChecking(GridRecordData &theData,
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
      ::DoParamConflictError(*(theData.itsParam.GetParam()), theData.itsOrigParam, (*it).second);
    }
  }
  else
  {
    theUnchangedParams.insert(make_pair(keyValue, *(theData.itsParam.GetParam())));

    map<unsigned long, pair<NFmiParam, NFmiParam> >::iterator it = theChangedParams.find(keyValue);
    if (it != theChangedParams.end())
    {
      fExecutionStoppingError = true;
      ::DoParamConflictError(
          (*it).second.first, (*it).second.second, *(theData.itsParam.GetParam()));
    }
  }
}

void ConvertGrib2QData(GribFilterOptions &theGribFilterOptions)
{
  vector<GridRecordData *> gribRecordDatas;
  bool executionStoppingError = false;
  map<int, pair<double, double> > verticalCoordinateMap;

  try
  {
    grib_handle *gribHandle = nullptr;
    grib_context *gribContext = grib_context_get_default();

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
      if (theGribFilterOptions.fVerbose) cerr << counter << " ";
      GridRecordData *tmpData = new GridRecordData;
      tmpData->itsLatlonCropRect = theGribFilterOptions.itsLatlonCropRect;
      try
      {
        // param ja level tiedot pit‰‰ hanskata ennen hilan koon m‰‰rityst‰
        tmpData->itsParam = ::GetParam(gribHandle, theGribFilterOptions.itsWantedSurfaceProducer);
        tmpData->itsLevel = ::GetLevel(gribHandle);
        ::FillGridInfoFromGribHandle(gribHandle,
                                     tmpData,
                                     theGribFilterOptions.fDoGlobeFix,
                                     theGribFilterOptions.itsGridSettings);
        tmpData->itsOrigTime = ::GetOrigTime(gribHandle);
        tmpData->itsValidTime = ::GetValidTime(gribHandle);
        tmpData->itsMissingValue = ::GetMissingValue(gribHandle);
        ::GetLevelVerticalCoordinates(gribHandle, *tmpData, verticalCoordinateMap);

        if (theGribFilterOptions.fVerbose)
        {
          cerr << tmpData->itsValidTime.ToStr("YYYYMMDDHHmm", kEnglish).CharPtr() << ";";
          cerr << tmpData->itsParam.GetParamName().CharPtr() << ";";
          cerr << tmpData->itsLevel.GetIdent() << ";";
          cerr << tmpData->itsLevel.LevelValue() << ";";
        }
        ::ChangeParamSettingsIfNeeded(
            theGribFilterOptions.itsParamChangeTable, tmpData, theGribFilterOptions.fVerbose);

        ::DoParamChecking(*tmpData, changedParams, unchangedParams, executionStoppingError);

        // filtteri j‰tt‰‰ huomiotta ns. kontrolli hilan, joka on ainakin hirlam datassa 1.. se on
        // muista poikkeava 2x2 hila latlon-area.
        // Aiheuttaisi turhia ongelmia jatkossa monessakin paikassa.
        bool gribFieldUsed = false;
        if (!(tmpData->itsOrigGrid.itsNX <= 2 && tmpData->itsOrigGrid.itsNY <= 2))
        {
          if (::IgnoreThisLevel(tmpData, theGribFilterOptions.itsIgnoredLevelList) == false)
          {
            if (::AcceptThisLevelType(tmpData, theGribFilterOptions.itsAcceptOnlyLevelTypes))
            {
              if (::CropParam(tmpData,
                              theGribFilterOptions.fCropParamsNotMensionedInTable,
                              theGribFilterOptions.itsParamChangeTable) == false)
              {
                ::FillGridData(gribHandle,
                               tmpData,
                               theGribFilterOptions.fDoGlobeFix,
                               theGribFilterOptions.itsParamChangeTable,
                               theGribFilterOptions.fVerbose);
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
          if (theGribFilterOptions.fVerbose) cerr << endl;
        }
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
    ::CreateQueryDatas(gribRecordDatas, theGribFilterOptions, verticalCoordinateMap);

    if (err) throw runtime_error(grib_get_error_message(err));
  }
  catch (...)
  {
    ::FreeDatas(gribRecordDatas);
    throw;
  }

  ::FreeDatas(gribRecordDatas);
}

struct LevelLessThan
{
  bool operator()(const NFmiLevel &l1, const NFmiLevel &l2) const
  {
    if (l1.LevelType() < l2.LevelType())
      return true;
    else if (l1.LevelType() == l2.LevelType())
    {
      if (l1.LevelValue() < l2.LevelValue()) return true;
    }
    return false;
  }
};

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

// tehd‰‰n levelbagi kaikista eri tyyppisist‰ leveleist‰.
vector<NFmiVPlaceDescriptor> GetAllVPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     GribFilterOptions &theGribFilterOptions)
{
  // 1. etsit‰‰n kaikki erilaiset levelit set:in avulla
  set<NFmiLevel, LevelLessThan> levelSet;
  map<int, int> levelTypeCounter;

  vector<GridRecordData *>::iterator it = theGribRecordDatas.begin();
  vector<GridRecordData *>::iterator endIter = theGribRecordDatas.end();
  for (; it != endIter; ++it)
  {
    GridRecordData *bs = *it;
    levelSet.insert(bs->itsLevel);
    levelTypeCounter[bs->itsLevel.LevelType()]++;  // kikka vitonen: t‰m‰ laskee erityyppiset
                                                   // levelit
  }

  vector<NFmiVPlaceDescriptor> vPlaces;

  if (theGribFilterOptions.fUseOutputFile)
  {
    map<int, int>::iterator lt = levelTypeCounter.begin();
    for (; lt != levelTypeCounter.end(); ++lt)
    {
      NFmiVPlaceDescriptor vDesc = ::MakeVPlaceDescriptor(theGribRecordDatas, lt->first);
      if (vDesc.Size() > 0) vPlaces.push_back(vDesc);
    }
  }
  else
  {  // tulostus tehd‰‰n cout:iin, ja tehd‰‰n vain yksi data, toivottavasti se on se merkitsevin,
     // eli otetaan se mink‰ tyyppisi‰ esiintyi eniten
    map<int, int>::iterator lt = FindHighesLevelType(levelTypeCounter);
    if (lt != levelTypeCounter.end())
    {
      NFmiVPlaceDescriptor vDesc = ::MakeVPlaceDescriptor(theGribRecordDatas, lt->first);
      if (vDesc.Size() > 0) vPlaces.push_back(vDesc);
    }
  }
  return vPlaces;
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
  {  // tulostus tehd‰‰n cout:iin, ja tehd‰‰n vain yksi data, toivottavasti se on se merkitsevin,
     // t‰ss‰ yritet‰‰n j‰tt‰‰ huomiotta ainakin
    // hirlam gribeiss‰ esiintyv‰ 2x2 kokoinen yksi hila
    MyGrid &tmpGrid = theGribRecordDatas[theGribRecordDatas.size() > 1 ? 1 : 0]->itsGrid;
    NFmiGrid grid(tmpGrid.itsArea, tmpGrid.itsNX, tmpGrid.itsNY);
    hPlaces.push_back(NFmiHPlaceDescriptor(grid));
  }
  return hPlaces;
}

static boost::shared_ptr<NFmiQueryData> GetSurfaceData(
    vector<boost::shared_ptr<NFmiQueryData> > &theQdatas,
    FmiParameterName thePressureAtStationParId)
{
  boost::shared_ptr<NFmiQueryData> surfaceData;
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

static boost::shared_ptr<NFmiQueryData> GethybridData(
    vector<boost::shared_ptr<NFmiQueryData> > &theQdatas, FmiParameterName pressureId)
{
  boost::shared_ptr<NFmiQueryData> hybridData;
  for (size_t i = 0; i < theQdatas.size(); i++)
  {
    if (theQdatas[i]->Info()->SizeLevels() > 1)  // pit‰‰ olla useita leveleit‰
    {
      theQdatas[i]->Info()->FirstLevel();
      if (theQdatas[i]->Info()->Level()->LevelType() ==
          kFmiHybridLevel)  // pit‰‰ olla hybrid tyyppi‰
      {
        if (theQdatas[i]->Info()->Param(pressureId))  // pit‰‰ lˆyty‰ paine parametri
        {
          hybridData = theQdatas[i];
          break;
        }
      }
    }
  }
  return hybridData;
}

// Jos surfacePressure tulee hPa:na, pit‰‰ se laskuissa se pit‰‰ muuttaa Pa:ksi.
// Palautetaan kuitenkin aina laskettu paine hPa-yksikˆss‰.
static float CalcHybridPressure(double a, double b, float surfacePressure)
{
  if (surfacePressure == kFloatMissing)
    return kFloatMissing;
  else
  {
    double pressureScale = 1.;
    if (surfacePressure < 1500)  // T‰m‰ on karkea arvio, onko pintapaine hPa vai Pa yksikˆss‰, jos
                                 // pintapaine oli hPa-yksikˆss‰, pit‰‰ tehd‰ muunnoksia
      pressureScale = 100.;
    double value = a + b * surfacePressure * pressureScale;
    return static_cast<float>(value / 100.);  // paluu aina hPa:na
  }
}

static void CalcHybridPressureData(vector<boost::shared_ptr<NFmiQueryData> > &theQdatas,
                                   map<int, pair<double, double> > &theVerticalCoordinateMap,
                                   const GeneratedHybridParamInfo &theHybridPressureInfo)
{
  if (theHybridPressureInfo.fCalcHybridParam)
  {
    // 1. lasketaan hybridi-dataan paine parametri jos lˆytyy pinta hybridi data listasta
    FmiParameterName pressureAtStationParId = theHybridPressureInfo.itsHelpParamId;
    FmiParameterName hybridPressureId =
        static_cast<FmiParameterName>(theHybridPressureInfo.itsGeneratedHybridParam.GetIdent());
    boost::shared_ptr<NFmiQueryData> surfaceData =
        ::GetSurfaceData(theQdatas, pressureAtStationParId);
    boost::shared_ptr<NFmiQueryData> hybridData = ::GethybridData(theQdatas, hybridPressureId);
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
              int level = static_cast<int>(::round(hybridInfo.Level()->LevelValue()));
              map<int, pair<double, double> >::iterator it = theVerticalCoordinateMap.find(level);
              if (it != theVerticalCoordinateMap.end())
              {
                double a = (*it).second.first;
                double b = (*it).second.second;
                for (hybridInfo.ResetLocation(); hybridInfo.NextLocation();)
                {
                  float surfacePressure = surfaceInfo.InterpolatedValue(hybridInfo.LatLon());
                  if (surfacePressure != kFloatMissing)
                    hybridInfo.FloatValue(::CalcHybridPressure(a, b, surfacePressure));
                }
              }
            }
          }
        }
      }
    }
  }
}

/*
Kari niemel‰n s‰hkˆpostista suhteellisen kosteuden laskusta ominaiskosteuden avulla:

Suhteellisen kosteuden laskemiseen tarvitaan paine ja l‰mpˆtila
ominaiskosteuden lis‰ksi.

Hilakkeessa asia tehd‰‰n seuraavasti:
-- lasketaan vesihˆyryn kyll‰stysosapaine kaavalla (T on celsiuksia, ^
on potenssi)
es = 6.107 * 10 ^ (7.5 * T / (237.0 + T)) , jos l‰mpim‰mp‰‰ kuin -5 C
eli veden suhteen
es = 6.107 * 10 ^ (9.5 * T / (265.5 + T)) , jos kylmemp‰‰ kuin -5 C eli
j‰‰n suhteen
-- lasketaan RH kaavalla
RH = (P * Q / 0.622 / ES) * (P - ES) / (P - Q * P / 0.622)
RH saadaan 0...1, mutta on viel‰ tarkistettava ettei mene alle nollan
tai yli yhden
*/

// Lasketaan vesihˆyryn osapaine ES veden suhteen.
// Oletus T ei ole puuttuvaa ja on celsiuksina.
static float CalcESWater(float T)
{
  float es = 6.107f * ::pow(10.f, (7.5f * T / (237.0f + T)));
  return es;
}

// Lasketaan vesihˆyryn osapaine ES j‰‰n suhteen.
// Oletus T ei ole puuttuvaa ja on celsiuksina.
static float CalcESIce(float T)
{
  float es = 6.107f * ::pow(10.f, (9.5f * T / (265.5f + T)));
  return es;
}

// P on hPa, T on celsiuksia ja Q (specific humidity) on kg/kg
static float CalcRH(float P, float T, float Q)
{
  if (P == kFloatMissing || T == kFloatMissing || Q == kFloatMissing)
    return kFloatMissing;
  else
  {
    float ES = (T >= -5) ? ::CalcESWater(T) : ::CalcESIce(T);
    float RH = (P * Q / 0.622f / ES) * (P - ES) / (P - Q * P / 0.622f);
    if (RH > 1.f) RH = 1.f;
    if (RH < 0.f) RH = 0.f;
    return RH * 100;
  }
}

// HUOM! T‰m‰ pit‰‰ ajaa vasta jos ensin on laskettu paine parametri hybridi dataan!!!
static void CalcHybridRelativeHumidityData(
    vector<boost::shared_ptr<NFmiQueryData> > &theQdatas,
    const GeneratedHybridParamInfo &theHybridRelativeHumidityInfo,
    const GeneratedHybridParamInfo &theHybridPressureInfo)
{
  if (theHybridRelativeHumidityInfo.fCalcHybridParam)
  {
    // 1. lasketaan hybridi-dataan RH parametri jos lˆytyy ominaiskosteus parametri datasta
    FmiParameterName T_id = kFmiTemperature;
    FmiParameterName P_id =
        static_cast<FmiParameterName>(theHybridPressureInfo.itsGeneratedHybridParam.GetIdent());
    FmiParameterName SH_id = theHybridRelativeHumidityInfo.itsHelpParamId;
    FmiParameterName RH_id = static_cast<FmiParameterName>(
        theHybridRelativeHumidityInfo.itsGeneratedHybridParam.GetIdent());
    boost::shared_ptr<NFmiQueryData> hybridData = ::GethybridData(theQdatas, RH_id);
    if (hybridData)
    {
      NFmiFastQueryInfo RH_info(
          hybridData.get());  // RH info, suhteellinen kosteus, johon tulokset talletetaan
      RH_info.First();
      NFmiFastQueryInfo T_info(RH_info);  // T info, mist‰ l‰mpˆtila
      T_info.First();
      NFmiFastQueryInfo P_info(RH_info);  // P info, mist‰ paine
      P_info.First();
      NFmiFastQueryInfo SH_info(RH_info);  // SH info, mist‰ specific humidity
      SH_info.First();

      if (RH_info.Param(RH_id) && T_info.Param(T_id) && SH_info.Param(SH_id) && P_info.Param(P_id))
      {
        for (RH_info.ResetTime(); RH_info.NextTime();)
        {
          // kaikki infot ovat samasta datasta, joten niiss‰ on sama rakenne ja aika/level/location
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
              P_info.LocationIndex(RH_info.LocationIndex());
              SH_info.LocationIndex(RH_info.LocationIndex());
              T_info.LocationIndex(RH_info.LocationIndex());

              float P = P_info.FloatValue();
              float T = T_info.FloatValue();
              float SH = SH_info.FloatValue();

              float RH = ::CalcRH(P, T, SH);
              if (RH != kFloatMissing) RH_info.FloatValue(RH);
            }
          }
        }
      }
      else
        std::cerr << "Error, couldn't deduce all the parameters needed in Relative Humidity "
                     "calculations for hybrid data.";
    }
  }
}

static void ForceLevelToSurfaceType(vector<GridRecordData *> &theGribRecordDatas,
                                    const NFmiLevel &theSurfaceLevel,
                                    const NFmiLevel &theChangedLevel)
{
  for (size_t i = 0; i < theGribRecordDatas.size(); i++)
  {
    if (theGribRecordDatas[i]->itsLevel == theChangedLevel)
      theGribRecordDatas[i]->itsLevel = theSurfaceLevel;
  }
}

static void ForceOneSurfaceLevelData(vector<GridRecordData *> &theGribRecordDatas,
                                     GribFilterOptions &theGribFilterOptions)
{
  if (theGribFilterOptions.fTreatSingleLevelsAsSurfaceData)
  {
    vector<NFmiVPlaceDescriptor> vPlaceDescriptors =
        GetAllVPlaceDescriptors(theGribRecordDatas, theGribFilterOptions);
    int singleLevelTypeCount = 0;
    vector<NFmiLevel> changedLevels;
    for (size_t i = 0; i < vPlaceDescriptors.size(); i++)
    {
      if (vPlaceDescriptors[i].Size() <= 1)
      {
        singleLevelTypeCount++;
        changedLevels.push_back(*(vPlaceDescriptors[i].Level(0)));
      }
    }
    if (singleLevelTypeCount > 1)
    {
      NFmiLevel surfaceLevel(kFmiAnyLevelType, 0);
      for (size_t i = 0; i < changedLevels.size(); i++)
        ::ForceLevelToSurfaceType(theGribRecordDatas, surfaceLevel, changedLevels[i]);
    }
  }
}

void CreateQueryDatas(vector<GridRecordData *> &theGribRecordDatas,
                      GribFilterOptions &theGribFilterOptions,
                      map<int, pair<double, double> > &theVerticalCoordinateMap)
{
  cerr << "Creating querydatas" << endl;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    ::ForceOneSurfaceLevelData(theGribRecordDatas, theGribFilterOptions);
    vector<NFmiHPlaceDescriptor> hPlaceDescriptors =
        GetAllHPlaceDescriptors(theGribRecordDatas, theGribFilterOptions.fUseOutputFile);
    vector<NFmiVPlaceDescriptor> vPlaceDescriptors =
        GetAllVPlaceDescriptors(theGribRecordDatas, theGribFilterOptions);
    for (unsigned int j = 0; j < vPlaceDescriptors.size(); j++)
    {
      for (unsigned int i = 0; i < hPlaceDescriptors.size(); i++)
      {
        if (theGribFilterOptions.fVerbose)
          cerr << "L" << NFmiStringTools::Convert(j) << "H" << NFmiStringTools::Convert(i) << " ";
        boost::shared_ptr<NFmiQueryData> qdata = CreateQueryData(
            theGribRecordDatas, hPlaceDescriptors[i], vPlaceDescriptors[j], theGribFilterOptions);
        if (qdata) theGribFilterOptions.itsGeneratedDatas.push_back(qdata);
      }
    }

    ::CalcHybridPressureData(theGribFilterOptions.itsGeneratedDatas,
                             theVerticalCoordinateMap,
                             theGribFilterOptions.itsHybridPressureInfo);
    ::CalcHybridRelativeHumidityData(theGribFilterOptions.itsGeneratedDatas,
                                     theGribFilterOptions.itsHybridRelativeHumidityInfo,
                                     theGribFilterOptions.itsHybridPressureInfo);
  }
}

boost::shared_ptr<NFmiQueryData> CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                                                 NFmiHPlaceDescriptor &theHplace,
                                                 NFmiVPlaceDescriptor &theVplace,
                                                 GribFilterOptions &theGribFilterOptions)
{
  boost::shared_ptr<NFmiQueryData> qdata;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    NFmiParamDescriptor params(
        GetParamDesc(theGribRecordDatas, theHplace, theVplace, theGribFilterOptions));
    NFmiTimeDescriptor times(GetTimeDesc(theGribRecordDatas, theHplace, theVplace));
    if (params.Size() == 0 || times.Size() == 0)
      return qdata;  // turha jatkaa jos toinen n‰ist‰ on tyhj‰
    NFmiQueryInfo innerInfo(params, times, theHplace, theVplace);
    CheckInfoSize(innerInfo, theGribFilterOptions.itsMaxQDataSizeInBytes);
    qdata = boost::shared_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(innerInfo));
    bool anyDataFilled =
        FillQDataWithGribRecords(qdata, theGribRecordDatas, theGribFilterOptions.fVerbose);
    if (anyDataFilled == false)
    {
      qdata = boost::shared_ptr<NFmiQueryData>();
    }
  }
  return qdata;
}

void CheckInfoSize(const NFmiQueryInfo &theInfo, size_t theMaxQDataSizeInBytes)
{
  size_t infoSize = theInfo.Size();
  size_t infoSizeInBytes = infoSize * sizeof(float);
  if (theMaxQDataSizeInBytes < infoSizeInBytes)
  {
    stringstream ss;
    ss << "Data would be too big:" << endl;
    ss << "The result would be " << infoSizeInBytes << " bytes." << endl;
    ss << "The limit is set to " << theMaxQDataSizeInBytes << " bytes." << endl;

    unsigned long paramSize = theInfo.SizeParams();
    ss << "Number of parameters: " << paramSize << endl;
    unsigned long timeSize = theInfo.SizeTimes();
    ss << "Number of timesteps: " << timeSize << endl;
    unsigned long locSize = theInfo.SizeLocations();
    ss << "Number of points: " << locSize << endl;
    unsigned long levelSize = theInfo.SizeLevels();
    ss << "Number of levels: " << levelSize << endl;
    throw runtime_error(ss.str());
  }
}

bool FillQDataWithGribRecords(boost::shared_ptr<NFmiQueryData> &theQData,
                              vector<GridRecordData *> &theGribRecordDatas,
                              bool verbose)
{
  NFmiFastQueryInfo info(theQData.get());
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  GridRecordData *tmp = 0;
  int filledGridCount = 0;
  if (verbose) cerr << "Filling qdata grids ";
  for (int i = 0; i < gribCount; i++)
  {
    tmp = theGribRecordDatas[i];
    if (tmp->itsGrid == *info.Grid())  // vain samanlaisia hiloja laitetaan samaan qdataan
    {
      if (info.Time(tmp->itsValidTime) && info.Level(tmp->itsLevel) && info.Param(tmp->itsParam))
      {
        if (!info.SetValues(tmp->itsGridData))
          throw runtime_error("qdatan t‰yttˆ gribi datalla ep‰onnistui, lopetetaan...");
        filledGridCount++;
        if (verbose) cerr << NFmiStringTools::Convert(filledGridCount) << " ";
      }
    }
  }
  if (verbose) cerr << endl;
  return filledGridCount > 0;
}

bool ConvertTimeList2TimeBag(NFmiTimeList &theTimeList, NFmiTimeBag &theTimeBag)
{  // tutkitaan onko mahdollista tehda listasta bagi
   // eli ajat ovat per‰kk‰isi‰ ja tasav‰lisi‰
  if (theTimeList.NumberOfItems() >
      2)  // ei  tehd‰ yhdest‰ tai kahdesta ajasta bagi‰ vaikka se on mahdollista
  {
    theTimeList.First();
    theTimeList.Next();
    int resolution = theTimeList.CurrentResolution();
    for (; theTimeList.Next();)
    {
      if (resolution != theTimeList.CurrentResolution())
        return false;  // jos yhdenkin aikav‰lin resoluutio poikkeaa, ei voida tehd‰ bagia
    }
    theTimeBag = NFmiTimeBag(theTimeList.FirstTime(), theTimeList.LastTime(), resolution);
    return true;
  }
  return false;
}

// HUOM! jos datassa on 'outoja' valid-aikoja esim. 1919 jne., joita n‰ytt‰‰ tulevan esim.
// hirlamista liittyen johonkin
// kontrolli grideihin (2x2 hila ja muuta outoa). T‰ll‰iset hilat j‰tet‰‰n huomiotta.
// Timebagin rakentelussa tarkastellaan myˆs ett‰ hila ja level-type ovat halutunlaiset.
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
  NFmiMetTime dummyTime(1950, 1, 1);  // laitoin t‰ll‰isen dummytime rajoittimen, koska Pekon
                                      // antamassa datassa oli aika 1919 vuodelta ja se ja
                                      // nykyaikainen aika sekoitti mm. metkun editorin pahasti
  for (; it != timesSet.end(); ++it)
    if (*it > dummyTime) timeList.Add(new NFmiMetTime(*it));

  NFmiTimeBag timeBag;
  bool fUseTimeBag = ConvertTimeList2TimeBag(timeList, timeBag);  // jos mahd.

  // Oletus kaikki origintimet ovat samoja, en tutki niita nyt yhtaan.
  if (fUseTimeBag)
    return NFmiTimeDescriptor(theGribRecordDatas[0]->itsOrigTime, timeBag);
  else
    return NFmiTimeDescriptor(theGribRecordDatas[0]->itsOrigTime, timeList);
}

NFmiVPlaceDescriptor GetVPlaceDesc(vector<GridRecordData *> &theGribRecordDatas)
{
  NFmiLevelBag levelBag;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  for (int i = 0; i < gribCount; i++)
    levelBag.AddLevel(theGribRecordDatas[i]->itsLevel);  // AddLevel poistaa duplikaatit

  return NFmiVPlaceDescriptor(levelBag);
}

static void AddGeneratedHybridParam(NFmiParamBag &theGeneratedParamBag,
                                    const GeneratedHybridParamInfo &theGeneratedHybridParamInfo)
{
  if (theGeneratedHybridParamInfo.fCalcHybridParam)
  {
    // lis‰t‰‰n paine parametri hybridi dataan, jos sit‰ ei ole ja se lasketaan myˆhemmin
    if (theGeneratedParamBag.SetCurrent(static_cast<FmiParameterName>(
            theGeneratedHybridParamInfo.itsGeneratedHybridParam.GetIdent())) == false)
      theGeneratedParamBag.Add(theGeneratedHybridParamInfo.itsGeneratedHybridParam);
  }
}

// Etsit‰‰n ne parametrit mitk‰ lˆytyv‰t data-setist‰.
// Lis‰ksi hilan ja arean pit‰‰ olla sama kuin annetussa hplaceDescriptorissa ja level-tyypin pit‰‰
// olla
// sama kuin vplaceDescriptorissa.
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace,
                                 const GribFilterOptions &theGribFilterOptions)
{
  // Ensin pit‰‰ saada 1. levelin level-type talteen vplaceDescriptorista, sill‰ otamme vain
  // parametreja, mit‰ lˆytyy sellaisista hila kentist‰ miss‰ on t‰ll‰inen level id.
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

  if (wantedLevelType == kFmiHybridLevel)
  {
    ::AddGeneratedHybridParam(parBag, theGribFilterOptions.itsHybridPressureInfo);
    ::AddGeneratedHybridParam(parBag, theGribFilterOptions.itsHybridRelativeHumidityInfo);
  }

  if (wantedLevelType == kFmiHybridLevel)
    parBag.SetProducer(theGribFilterOptions.itsWantedHybridProducer);
  if (wantedLevelType == kFmiPressureLevel)
    parBag.SetProducer(theGribFilterOptions.itsWantedPressureProducer);

  return NFmiParamDescriptor(parBag);
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
          ::floor(ratio *
                  i);  // otetaan desimaali osa irti sijainnista niin saadaan interpolointi kerroin
      float value1 = theSourceValues[lowerIndex];
      float value2 = theSourceValues[lowerIndex + 1];
      double interpolatedValue = NFmiInterpolation::Linear(relativePosRemains, value1, value2);
      theDestValues[i] = static_cast<float>(interpolatedValue);
    }
    theDestValues[theDestValues.size() - 1] = theSourceValues[theSourceValues.size() - 1];
  }
}

// TODO ei osaa viel‰ hanskata scanmodeja
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
    // T‰ytet‰‰n ensin vaihtuva pituisen rivin data ja niiden suhteelliset sijainnit.
    long rowLength = theVariableLengthRows[row];
    if (rowLength == 0)
      throw runtime_error("Zero division in FillGridDataWithVariableLengthData-function.");

    rowValues.resize(rowLength);
    for (long i = 0; i < rowLength; i++)
    {
      rowValues[i] = theArray[totalArrayCounter];
      totalArrayCounter++;
    }
    // laske eri pituisen datan rivista matriisiin rivi
    InterpolateRowData(rowValues, matrixRowValues);
    // talleta rivi tulos matriisiin.
    for (unsigned int j = 0; j < matrixRowValues.size(); j++)
      gridData[j][row] = matrixRowValues[j];
  }
}

int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption)
{
  NFmiValueString valStr(theCmdline.OptionValue(theOption));
  if (valStr.IsInt()) return static_cast<int>(valStr);
  throw runtime_error(string("Error: '") + theOption +
                      "' option value must be integer, exiting...");
}
