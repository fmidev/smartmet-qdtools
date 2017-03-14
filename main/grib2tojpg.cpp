/*!
 */

#ifdef _MSC_VER
#pragma warning(disable : 4786 4996)  // poistaa n kpl VC++ k‰‰nt‰j‰n varoitusta (liian pitk‰ nimi
                                      // >255 merkki‰
                                      // joka johtuu 'puretuista' STL-template nimist‰)
#endif

#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiStereographicArea.h>
#include <newbase/NFmiRotatedLatLonArea.h>
#include <newbase/NFmiMercatorArea.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiValueString.h>
#include <newbase/NFmiTotalWind.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiInterpolation.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiCommentStripper.h>
#include <newbase/NFmiAreaFactory.h>

#include <grib_api.h>
//#include "grib_api_internal.h"

extern "C" {
#include <jpeglib.h>
}

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <set>

using namespace std;

// Varoitus: Rikos ihmiskuntaa kohtaan!
//
// N‰m‰ kulkevat toistaiseksi globaaleissa muuttujissa,
// koska muuten koodia pit‰isi refaktoroida runsaasti.
//
// T‰m‰ koodi on menossa refaktoroitavaksi myˆhemmin,
// joten siit‰ pit‰‰ ottaa ko. asiat huomioon

float globalGamma = 0.75;
bool globalInvert = true;
std::string globalFilenamePattern = "%Y%m%d%H%M";  // e.g. 200812312359
std::string globalFilenamePrefix = "GRIB_";

// template<typename T>
struct PointerDestroyer
{
  template <typename T>
  void operator()(T *thePtr)
  {
    delete thePtr;
  }
};

struct ParamChangeItem
{
  ParamChangeItem(void)
      : itsOriginalParamId(0),
        itsWantedParam(0,
                       "",
                       kFloatMissing,
                       kFloatMissing,
                       kFloatMissing,
                       kFloatMissing,
                       "%.1f",
                       kLinearly)  // laitetaan lineaarinen interpolointi p‰‰lle
        ,
        itsConversionBase(0),
        itsConversionScale(1.f),
        itsLevel(0)
  {
  }

  ParamChangeItem(const ParamChangeItem &theOther)
      : itsOriginalParamId(theOther.itsOriginalParamId),
        itsWantedParam(theOther.itsWantedParam),
        itsConversionBase(theOther.itsConversionBase),
        itsConversionScale(theOther.itsConversionScale),
        itsLevel(theOther.itsLevel ? new NFmiLevel(*theOther.itsLevel) : 0)
  {
  }

  ~ParamChangeItem(void) { delete itsLevel; }
  ParamChangeItem &operator=(const ParamChangeItem &theOther)
  {
    if (this != &theOther)
    {
      itsOriginalParamId = theOther.itsOriginalParamId;
      itsWantedParam = theOther.itsWantedParam;
      itsConversionBase = theOther.itsConversionBase;
      itsConversionScale = theOther.itsConversionScale;
      itsLevel = theOther.itsLevel ? new NFmiLevel(*theOther.itsLevel) : 0;
    }
    return *this;
  }

  void Reset(void)
  {
    itsOriginalParamId = 0;
    itsWantedParam = NFmiParam(0,
                               "",
                               kFloatMissing,
                               kFloatMissing,
                               kFloatMissing,
                               kFloatMissing,
                               "%.1f",
                               kLinearly);  // laitetaan lineaarinen interpolointi p‰‰lle
    itsConversionBase = 0;
    itsConversionScale = 1.f;
    if (itsLevel) delete itsLevel;
    itsLevel = 0;
  }

  unsigned long itsOriginalParamId;
  NFmiParam itsWantedParam;
  float itsConversionBase;  // jos ei 0 ja scale ei 1, tehd‰‰n parametrille muunnos konversio
  float itsConversionScale;
  NFmiLevel *itsLevel;  // jos ei 0-pointer, tehd‰‰n t‰st‰ levelist‰ pintaparametri
};

void Usage(void);
vector<NFmiQueryData *> ConvertGrib2QData(FILE *theInput,
                                          int theMaxQDataSizeInBytes,
                                          bool useOutputFile,
                                          NFmiLevelBag &theIgnoredLevelList,
                                          int wantedGridInfoPrintCount,
                                          const NFmiProducer &theWantedProducer,
                                          bool doGlobeFix,
                                          vector<FmiLevelType> &theAcceptOnlyLevelTypes,
                                          int &theDifferentAreaCount,
                                          const NFmiRect &theLatlonCropRect,
                                          vector<ParamChangeItem> &theParamChangeTable,
                                          bool fCropParamsNotMensionedInTable,
                                          bool verbose,
                                          NFmiGrid *theWantedGrid,
                                          const NFmiPoint &thePressureDataGridSize,
                                          const NFmiPoint &theHybridDataGridSize);
vector<NFmiQueryData *> CreateQueryDatas(vector<GridRecordData *> &theGribRecordDatas,
                                         int theMaxQDataSizeInBytes,
                                         bool useOutputFile,
                                         int &theDifferentAreaCount);
NFmiQueryData *CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                               NFmiHPlaceDescriptor &theHplace,
                               NFmiVPlaceDescriptor &theVplace,
                               int theMaxQDataSizeInBytes);
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace);
const NFmiDataIdent &FindFirstParam(int theParId, vector<GridRecordData *> &theGribRecordDatas);
NFmiVPlaceDescriptor GetVPlaceDesc(vector<GridRecordData *> &theGribRecordDatas);
const NFmiLevel &FindFirstLevel(int theLevelValue, vector<GridRecordData *> &theGribRecordDatas);
NFmiTimeDescriptor GetTimeDesc(vector<GridRecordData *> &theGribRecordDatas,
                               NFmiHPlaceDescriptor &theHplace,
                               NFmiVPlaceDescriptor &theVplace);
bool FillQDataWithGribRecords(vector<GridRecordData *> &theGribRecordDatas);
int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption);
void CheckInfoSize(const NFmiQueryInfo &theInfo, int theMaxQDataSizeInBytes);
vector<NFmiHPlaceDescriptor> GetAllHPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile);
vector<NFmiVPlaceDescriptor> GetAllVPlaceDescriptors(vector<GridRecordData *> &theGribRecordDatas,
                                                     bool useOutputFile);

static const unsigned long gMissLevelValue =
    9999999;  // t‰ll‰ ignoorataan kaikki tietyn level tyypin hilat

#if 0
static string GetFileNameAreaStr(const NFmiArea *theArea)
{
	string str("_area_");
	str += theArea->AreaStr();
	return str;
}

static void ReplaceChar(string &theFileName, char replaceThis, char toThis)
{
	for(string::size_type i=0; i<theFileName.size(); i++)
	{
		if(theFileName[i] == replaceThis)
			theFileName[i] = toThis;
	}
}
#endif

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

static NFmiGrid *CreateWantedGrid(const std::string &theWantedGridStr)
{
  if (theWantedGridStr.empty() == false)
  {
    boost::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(theWantedGridStr);
    NFmiPoint gridSize = area->XYArea().Size();
    if (gridSize.X() == 1 && gridSize.Y() == 1)
    {
      gridSize = NFmiPoint(50, 50);
      cerr << "Warning: no grid size given with -P option, using 50x50 projected grid size.";
    }
    else
    {
      area->SetXYArea(NFmiRect(0, 0, 1, 1));
    }
    NFmiGrid *aGrid = new NFmiGrid(area->Clone(),
                                   static_cast<unsigned long>(gridSize.X()),
                                   static_cast<unsigned long>(gridSize.Y()));
    return aGrid;
  }
  throw runtime_error(
      string("Error in CreateWantedGrid-function, projection conversion wanted, but empty "
             "projection string given, error in application?"));
}

const NFmiPoint gMissingGridSize(kFloatMissing, kFloatMissing);

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
static void HandleProjectionString(const string &theProjStr,
                                   NFmiGrid &theWantedGrid,
                                   NFmiPoint &thePressureDataGridSize,
                                   NFmiPoint &theHybridDataGridSize)
{
  string projStringUsed = theProjStr;
  int gridSizeFunction = 0;  // 0 = error, 1 = proj. ok, ei hila kokoa, 2= proj+1-hilakoko annettu,
                             // 3= proj+2-hilakokoa, 4=proj+3-hilakokoa annettu
  string::size_type pos = theProjStr.find(":");
  if (pos != string::npos)
  {
    gridSizeFunction = 1;
    pos = theProjStr.find(":", pos + 1);
    if (pos != string::npos)
    {  // eli lˆytyi 2. ':' -merkki, joka merkitsee ett‰ annetaan halutun hilan koko
      // nyt pit‰‰ tutkia, onko annettu useita hilakokoja eli onko annettu myˆs
      // painepinta ja mallipintadatoille omat hila koot.
      string gridSizesStr(theProjStr.begin() + pos + 1, theProjStr.end());
      vector<double> gridSizesList = NFmiStringTools::Split<vector<double> >(gridSizesStr, ",");
      if (gridSizesList.size() < 2)
        throw std::runtime_error(
            "Error with -P option, the grid size options were not correct, minimum one x and y "
            "size must be given ('x,y').");
      else if (gridSizesList.size() == 2)  // kaikki hila koot on samaa
        gridSizeFunction = 2;
      else if (gridSizesList.size() == 4 || gridSizesList.size() == 6)
      {  // nyt on annettu yksi ylim‰‰r‰inen hila koko
        gridSizeFunction =
            0;  // laitetaan error koodi p‰‰lle ja korjataan se myˆhemmin pois jos homma onnistui
        string::size_type pos2 = theProjStr.find(",", pos + 1);
        if (pos2 != string::npos)
        {
          pos2 = theProjStr.find(",", pos2 + 1);
          if (pos2 != string::npos)
          {  // nyt lˆytyi toinen pilkku, josta alkaa ylim‰‰r‰isten hilakokojen m‰‰ritys. T‰m‰ loppu
            // pit‰‰ saada
            // pois hilan m‰‰ritys stringist‰
            projStringUsed = std::string(theProjStr.begin(), theProjStr.begin() + pos2);
            if (gridSizesList.size() == 4)
            {
              theHybridDataGridSize = thePressureDataGridSize =
                  NFmiPoint(gridSizesList[2], gridSizesList[3]);
              gridSizeFunction = 3;
            }
            else
            {
              thePressureDataGridSize = NFmiPoint(gridSizesList[2], gridSizesList[3]);
              theHybridDataGridSize = NFmiPoint(gridSizesList[4], gridSizesList[5]);
              gridSizeFunction = 4;
            }
          }
          if (gridSizeFunction == 0)
            throw std::runtime_error(
                "Error with -P option in HandleProjectionString-function, error with extra grid "
                "sizes.");
        }
      }
    }
  }
  NFmiGrid *wantedGrid = ::CreateWantedGrid(projStringUsed);
  std::auto_ptr<NFmiGrid> wantedGridPtr(wantedGrid);  // tuhoaa lopuksi dynaamisen datan
  if (wantedGrid == 0)
    throw std::runtime_error(
        "Error with -P option in HandleProjectionString-function, unable to create the wanted "
        "projection grid.");
  theWantedGrid = *wantedGrid;
  if (gridSizeFunction <= 2)
    theHybridDataGridSize = thePressureDataGridSize =
        NFmiPoint(theWantedGrid.XNumber(), theWantedGrid.YNumber());
}

int main(int argc, const char **argv)
{
  FILE *input = 0;

  // Optiot:
  string inputFileName;   // annetaan pakollisena argumenttina
  string outputFileName;  // -o optio tai sitten tulostetann cout:iin
  bool fUseOutputFile = false;
  const int MBsize = 1024 * 1024;
  int maxQDataSizeInBytes = 200 * MBsize;  // eli default max koko 200 MB
  int returnStatus = 0;                    // 0 = ok
  NFmiLevelBag
      ignoredLevelList;  // lista miss‰ yksitt‰isi‰ leveleit‰, mitk‰ halutaan j‰tt‰‰ pois laskuista
  vector<NFmiQueryData *> datas;
  vector<FmiLevelType> acceptOnlyLevelTypes;  // lista jossa ainoat hyv‰ksytt‰v‰t level typet
  int gridInfoPrintCount = 0;
  vector<ParamChangeItem> paramChangeTable;

  try
  {
    NFmiCmdLine cmdline(argc, argv, "O!T!I");

    // Tarkistetaan optioiden oikeus:

    if (cmdline.Status().IsError())
    {
      cerr << "Error: Invalid command line:" << endl
           << cmdline.Status().ErrorLog().CharPtr() << endl;

      Usage();
      return 1;
    }

    if (cmdline.NumberofParameters() != 1)
    {
      cerr << "Error: 1 parameter expected, 'inputfile'\n\n";
      Usage();
      return 1;
    }
    else
      inputFileName = cmdline.Parameter(1);

    if (cmdline.isOption('O'))
    {
      globalFilenamePrefix = cmdline.OptionValue('O');
    }
    if (cmdline.isOption('T'))
    {
      globalFilenamePattern = cmdline.OptionValue('T');
    }
    if (cmdline.isOption('I'))
    {
      globalInvert = false;
    }

    bool cropParamsNotMensionedInTable = false;
    bool doGlobeFix = true;
    if (cmdline.isOption('f')) doGlobeFix = false;

#if 0
	  bool useLevelTypeFileNaming = false; // n optiolla voidaan laittaa output tiedostojen nimen per‰‰n esim. _levelType_100
	  if(cmdline.isOption('n'))
		  useLevelTypeFileNaming = true;
#endif

    NFmiProducer wantedProducer;

    NFmiRect latlonCropRect = gMissingCropRect;
    if (cmdline.isOption('G'))
    {
      string opt_bounds = cmdline.OptionValue('G');
      latlonCropRect = ::GetLatlonCropRect(opt_bounds);
    }
    NFmiGrid *wantedGrid = 0;
    NFmiPoint pressureDataGridSize = gMissingGridSize;
    NFmiPoint hybridDataGridSize = gMissingGridSize;
    if (cmdline.isOption('P'))
    {
      string opt_wantedGrid = cmdline.OptionValue('P');
      try
      {
        wantedGrid = new NFmiGrid;
        HandleProjectionString(
            opt_wantedGrid, *wantedGrid, pressureDataGridSize, hybridDataGridSize);
      }
      catch (std::exception &e)
      {
        cerr << e.what() << endl;
        return 9;
      }
    }
    boost::shared_ptr<NFmiGrid> wantedGridPtr(wantedGrid);  // tuhoaa lopuksi dynaamisen datan

    bool verbose = false;

    if ((input = fopen(inputFileName.c_str(), "rb")) == NULL)
    {
      cerr << "could not open input file: " << inputFileName << endl;
      return 7;
    }

    int differentAreaCount = 0;
    datas = ConvertGrib2QData(input,
                              maxQDataSizeInBytes,
                              fUseOutputFile,
                              ignoredLevelList,
                              gridInfoPrintCount,
                              wantedProducer,
                              doGlobeFix,
                              acceptOnlyLevelTypes,
                              differentAreaCount,
                              latlonCropRect,
                              paramChangeTable,
                              cropParamsNotMensionedInTable,
                              verbose,
                              wantedGrid,
                              pressureDataGridSize,
                              hybridDataGridSize);
//		auto_ptr<NFmiQueryData> dataPtr(data);

#if 0
		if(!datas.empty())
		{
			bool useDifferentFileNamesOnDifferentGrids = ::IsDifferentGridFileNamesUsed(datas);
			int ssize = static_cast<int>(datas.size());
			for(int i=0; i<ssize; i++)
			{
				NFmiStreamQueryData streamData;
				if(fUseOutputFile)
				{
					string usedFileName(outputFileName);
					if(useLevelTypeFileNaming)
					{
						datas[i]->Info()->FirstLevel();
						usedFileName += "_levelType_";
						usedFileName += NFmiStringTools::Convert(datas[i]->Info()->Level()->GetIdent());
						// TƒMƒ PITƒƒ viel‰ korjata, jos saman level tyypill‰ on erilaisia hila/area m‰‰rityksi‰, pit‰‰ ne nimet‰!!!
						if(useDifferentFileNamesOnDifferentGrids)
						{
							string areaStr = GetFileNameAreaStr(datas[i]->Info()->Area());
							::ReplaceChar(areaStr, ':', '_');
							usedFileName += areaStr;
						}
					}
					else
					{
						if(i>0)
						{
							usedFileName += "_";
							usedFileName += NFmiStringTools::Convert(i);
						}
					}
					if(!streamData.WriteData(usedFileName, datas[i]))
					{
						cerr << "could not open qd-file to write: " << outputFileName << endl;
						returnStatus |= 0;
					}
				}
				else if(ssize > 1) // jos qdatoja syntyy enemm‰n kuin 1, pit‰‰ antaa output-tiedoston nimi, ett‰ muut tiedostonimet voidaan generoida
				{
					cerr << "GRIB:ist‰ syntyy useita eri sqd-data tiedostoja." << endl;
					cerr << "K‰yt‰ o-optiota antamaan yhden tiedoston nimi (muut nimet" << endl;
					cerr << "annetaan automaattisesti)\nLopetetaan..." << endl;
					returnStatus = 1;
				}
				else
				{
					if(!streamData.WriteCout(datas[i]))
					{
						cerr << "could not open qd-file to stdout" << endl;
						returnStatus = 1;
					}
				}
			}
		}
		else
		{
			cerr << "SQL Dataa ei saatu luotua." << endl;
			returnStatus = 1;
		}
#endif  // #if 0
  }
  catch (exception &e)
  {
    cerr << e.what() << endl;
    returnStatus = 1;
  }
  catch (...)
  {
    cerr << "Tuntematon poikkeus: virhe ohjelmassa tai datassa?, lopetetaan..." << endl;
    returnStatus = 1;
  }
  if (input) fclose(input);  // suljetaan tiedosto, josta gribi luettu

  std::for_each(
      datas.begin(), datas.end(), PointerDestroyer());  // tuhotaan dynaamisesti luodut datat

  return returnStatus;
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cout << "Usage: grib2jpg [options] inputgribfile " << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-O <output prefix>\tPrefix for the output (path, prefix, ..)" << endl
       << "\t-T <strftime pattern>\tSpecify the timestamp part of output file" << endl
       << "\t-I \t\tDo NOT invert the luminance information" << endl

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
/*
static void printAccessorNames(grib_section* s)
{
        grib_accessor* a = s ? s->block->first : NULL;
//	grib_accessor* b;

        while(a)
        {
                cerr << a->name << endl;
                a = a->next;
        }
}
*/

static NFmiArea *CreateLatlonArea(grib_handle *theGribHandle, bool &doGlobeFix)
{
  double La1 = 0;
  int status1 = grib_get_double(theGribHandle, "La1", &La1);
  double Lo1 = 0;
  int status2 = grib_get_double(theGribHandle, "Lo1", &Lo1);
  double La2 = 0;
  int status3 = grib_get_double(theGribHandle, "La2", &La2);
  double Lo2 = 0;
  int status4 = grib_get_double(theGribHandle, "Lo2", &Lo2);
  double grib1divider = 1000000;
  grib_get_double(
      theGribHandle,
      "grib1divider",
      &grib1divider);  // katsotaan jos lˆytyy grib-divider, muuten k‰ytet‰‰n defaultti arvoa

  if (status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0)
  {
    La1 /= grib1divider;
    Lo1 /= grib1divider;
    La2 /= grib1divider;
    Lo2 /= grib1divider;

    if (doGlobeFix && Lo1 == 0 && (Lo2 < 0 || Lo2 > 350))
    {
      Lo1 = -180;
      Lo2 = 180;
    }
    else
      doGlobeFix = false;

    return new NFmiLatLonArea(NFmiPoint(Lo1, FmiMin(La1, La2)), NFmiPoint(Lo2, FmiMax(La1, La2)));
  }
  else
    throw runtime_error("Error: Unable to retrieve latlon-projection information from grib.");
}

static NFmiArea *CreateMercatorArea(grib_handle *theGribHandle)
{
  double La1 = 0;
  int status1 = grib_get_double(theGribHandle, "La1", &La1);
  double Lo1 = 0;
  int status2 = grib_get_double(theGribHandle, "Lo1", &Lo1);
  double La2 = 0;
  int status3 = grib_get_double(theGribHandle, "La2", &La2);
  double Lo2 = 0;
  int status4 = grib_get_double(theGribHandle, "Lo2", &Lo2);
  double grib1divider = 1;
  int status5 = grib_get_double(theGribHandle, "grib1divider", &grib1divider);

  if (status1 == 0 && status2 == 0 && status3 == 0 && status4 == 0 && status5 == 0)
  {
    La1 /= grib1divider;
    Lo1 /= grib1divider;
    La2 /= grib1divider;
    Lo2 /= grib1divider;

    return new NFmiMercatorArea(NFmiPoint(Lo1, FmiMin(La1, La2)), NFmiPoint(Lo2, FmiMax(La1, La2)));
  }
  else if (status1 == 0 && status2 == 0 && status5 == 0)
  {
    La1 /= grib1divider;
    Lo1 /= grib1divider;

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
      //			double w = area->WorldXYWidth();
      //			double h = area->WorldXYHeight();
      return area;
    }
  }
  throw runtime_error("Error: Unable to retrieve mercator-projection information from grib.");
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
                                       NFmiGrid *theWantedGrid,
                                       const NFmiPoint &thePressureDataGridSize,
                                       const NFmiPoint &theHybridDataGridSize)
{
  long gridDefinitionTemplateNumber =
      0;  // t‰h‰n tulee projektio tyyppi, ks. qooglesta (grib2 Table 3.1)
  int status =
      ::grib_get_long(theGribHandle, "gridDefinitionTemplateNumber", &gridDefinitionTemplateNumber);
  if (status == 0)
  {
    NFmiArea *area = 0;
    switch (gridDefinitionTemplateNumber)
    {
      case 0:  // 0 = latlonArea
        area = ::CreateLatlonArea(theGribHandle, doGlobeFix);
        break;
      case 20:  // 0 = mercatorArea
        area = ::CreateMercatorArea(theGribHandle);
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
      if (theWantedGrid)
      {
        MyGrid usedGrid(*theWantedGrid);
        if (theGridRecordData->itsLevel.LevelType() == kFmiHybridLevel &&
            theHybridDataGridSize != gMissingGridSize)
        {
          usedGrid.itsNX = static_cast<int>(theHybridDataGridSize.X());
          usedGrid.itsNY = static_cast<int>(theHybridDataGridSize.Y());
        }
        else if (theGridRecordData->itsLevel.LevelType() == kFmiPressureLevel &&
                 thePressureDataGridSize != gMissingGridSize)
        {
          usedGrid.itsNX = static_cast<int>(thePressureDataGridSize.X());
          usedGrid.itsNY = static_cast<int>(thePressureDataGridSize.Y());
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

/*
static MyGrid MakeGridFromGribHandle(grib_handle *theGribHandle, bool &doGlobeFix)
{
        long gridDefinitionTemplateNumber = 0; // t‰h‰n tulee projektio tyyppi, ks. qooglesta (grib2
Table 3.1)
        int status = grib_get_long(theGribHandle, "gridDefinitionTemplateNumber",
&gridDefinitionTemplateNumber);
        if(status == 0)
        {
                NFmiArea *area = 0;
                switch(gridDefinitionTemplateNumber)
                {
                case 0: // 0 = latlonArea
                        area = CreateLatlonArea(theGribHandle, doGlobeFix);
                        break;
                default:
                        throw runtime_error("Error: Handling of projection found from grib is not
implemented yet.");
                }

                long numberOfPointsAlongAParallel = 0;
                int status1 = grib_get_long(theGribHandle, "numberOfPointsAlongAParallel",
&numberOfPointsAlongAParallel);
                long numberOfPointsAlongAMeridian = 0;
                int status2 = grib_get_long(theGribHandle, "numberOfPointsAlongAMeridian",
&numberOfPointsAlongAMeridian);
                if(status1 == 0 && status2 == 0)
                        return MyGrid(area, numberOfPointsAlongAParallel,
numberOfPointsAlongAMeridian);
                else
                        throw runtime_error("Error: Couldn't get grid x-y sizes from given
grib_handle.");
        }
        else
                throw runtime_error("Error: Couldn't get gridDefinitionTemplateNumber from given
grib_handle.");
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
  NFmiMetTime validTime = ::GetOrigTime(theGribHandle);
  long forecastTime = 0;
  int status = grib_get_long(theGribHandle, "forecastTime", &forecastTime);
  if (status == 0)
  {
    validTime.ChangeByHours(forecastTime);
    return validTime;
  }
  else
    throw runtime_error("Error: Couldn't get validTime from given grib_handle.");
}

static NFmiDataIdent GetParam(grib_handle *theGribHandle, const NFmiProducer &theWantedProducer)
{
  /* // k‰ytet‰‰nkin aina ktegoria+paramnumber kikkaa
          long parameter = 0;
          int status1 = grib_get_long(theGribHandle, "parameter", &parameter);

          size_t stringLength = 1000;
          char tigge_name[1000];
          int status2 = grib_get_string(theGribHandle, "tigge_name", tigge_name, &stringLength);

          // TODO producerin voisi p‰‰tell‰ originating center numeroista

          if(status1 == 0 && status2 == 0)
                  return NFmiDataIdent(NFmiParam(parameter, tigge_name, kFloatMissing,
     kFloatMissing, kFloatMissing, kFloatMissing, "%.1f", kLinearly), theWantedProducer);
          else
  */
  {  // jos ei lˆytynyt normaalia reitti‰ parametri arvoja, kokeillaan parameterCategory +
     // parameterNumber yhdistelm‰‰
    long parameterCategory = 0;
    int status3 = grib_get_long(theGribHandle, "parameterCategory", &parameterCategory);

    long parameterNumber = 0;
    int status4 = grib_get_long(theGribHandle, "parameterNumber", &parameterNumber);
    if (status3 == 0 && status4 == 0)
    {  // tehd‰‰n parametri numeroksi categorian ja numberin yhdistelm‰ niin ett‰ eri categorioissa
      // olevat
      // parametrit eiv‰t menisi p‰‰llekk‰in eli parId = 1000*categoria+number
      unsigned int parId = parameterCategory * 1000 + parameterNumber;
      string name = NFmiStringTools::Convert(parId);
      return NFmiDataIdent(NFmiParam(parId,
                                     name,
                                     kFloatMissing,
                                     kFloatMissing,
                                     kFloatMissing,
                                     kFloatMissing,
                                     "%.1f",
                                     kLinearly),
                           theWantedProducer);
    }
    else
    {
      long parameter = 0;
      int status1 = grib_get_long(theGribHandle, "parameter", &parameter);
      if (status1 == 0 || status1 == -4)
      {
        string name = NFmiStringTools::Convert(parameter);
        return NFmiDataIdent(NFmiParam(parameter,
                                       name,
                                       kFloatMissing,
                                       kFloatMissing,
                                       kFloatMissing,
                                       kFloatMissing,
                                       "%.1f",
                                       kLinearly),
                             theWantedProducer);
      }
      else
        throw runtime_error("Error: Couldn't get parameter from given grib_handle.");
    }
  }
}

static NFmiLevel GetLevel(grib_handle *theGribHandle)
{
  long indicatorOfTypeOfLevel = 0;
  int status1a = grib_get_long(theGribHandle, "indicatorOfTypeOfLevel", &indicatorOfTypeOfLevel);
  long typeOfFirstFixedSurface = 0;
  int status1b = grib_get_long(theGribHandle, "typeOfFirstFixedSurface", &typeOfFirstFixedSurface);
  long level = 0;
  int status2 = grib_get_long(theGribHandle, "level", &level);

  if ((status1a == 0 || status1b == 0) && status2 == 0)
    return NFmiLevel(typeOfFirstFixedSurface, NFmiStringTools::Convert(level), level);
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

static void FillGridData(grib_handle *theGribHandle,
                         GridRecordData *theGridRecordData,
                         bool doGlobeFix,
                         vector<ParamChangeItem> &theParamChangeTable)
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
    //		long resolutionAndComponentFlags = 0;
    //		int status3 = grib_get_long(theGribHandle, "resolutionAndComponentFlags",
    //&resolutionAndComponentFlags);
    long scanningMode = 0;
    int status4 = grib_get_long(theGribHandle, "scanningMode", &scanningMode);

    if (status4 == 0)
    {
      if (scanningMode == 0)  // scanningMode 1. bit 0 -> +i +x | scanningMode 2. bit 0 -> -j -y
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
      else if (scanningMode == 80)  // en tied‰ mik‰ t‰m‰ moodi on, grib2 speksi ei m‰‰r‰‰ 5. tai 7.
                                    // bitin merkityst‰
      // jouduin vain kokeilemaan LAPS datan juoksutuksen k‰sipelill‰
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
      else  // sitten kun tulee lis‰‰ ceissej‰, lis‰t‰‰n eri t‰yttˆ variaatioita
      {
        throw runtime_error("Error: Found scanning mode not yet implemented.");
        //			for(size_t i = 0; i<values_length; i++)
        //				theValues[i%gridXSize][i/gridXSize] =
        // static_cast<float>(doubleValues[i]);
      }
    }

    if (doGlobeFix)
    {
      // Nyt on siis tilanne ett‰ halutaan 'korjata' globaali data editoria varten.
      // Latlon-area peitt‰‰ maapallon longitudeissa 0-360, se pit‰‰ muuttaa editoria varten
      // longitudeihin -180 - 180.
      // Eli matriisissa olevia arvoja pit‰‰ siirt‰‰ niin ett‰ vasemmalla puoliskolla olevat
      // laitetaan oikealle
      // puolelle ja toisin p‰in.
      int nx = static_cast<int>(origValues.NX());
      int ny = static_cast<int>(origValues.NY());
      for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx / 2; i++)
          std::swap(origValues[i][j], origValues[i + nx / 2][j]);
    }

    // 2. Kun orig matriisi on saatu t‰ytetty‰, katsotaan pit‰‰kˆ viel‰ t‰ytt‰‰ cropattu alue, vai
    // k‰ytet‰‰nkˆ originaali dataa suoraan.
    if (theGridRecordData->fDoProjectionConversion == false)
      theGridRecordData->itsGridData = origValues;
    else if (theGridRecordData->fDoProjectionConversion == true &&
             theGridRecordData->itsLatlonCropRect == gMissingCropRect)
    {  // t‰ss‰ tehd‰‰n latlon projisointia
      int destSizeX = theGridRecordData->itsGrid.itsNX;
      int destSizeY = theGridRecordData->itsGrid.itsNY;
      theGridRecordData->itsGridData.Resize(destSizeX, destSizeY);
      NFmiGrid destGrid(theGridRecordData->itsGrid.itsArea,
                        theGridRecordData->itsGrid.itsNX,
                        theGridRecordData->itsGrid.itsNY);
      int counter = 0;

      FmiParameterName param = FmiParameterName(theGridRecordData->itsParam.GetParam()->GetIdent());
      for (destGrid.Reset(); destGrid.Next(); counter++)
      {
        int destX = counter % theGridRecordData->itsGrid.itsNX;
        int destY = counter / theGridRecordData->itsGrid.itsNX;
        theGridRecordData->itsGridData[destX][destY] = origValues.InterpolatedValue(
            theGridRecordData->itsOrigGrid.itsArea->ToXY(destGrid.LatLon()), param);
      }
    }
    else
    {  // t‰ss‰ raaka hila croppaus
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
          theGridRecordData->itsGridData[i][j] = origValues[x1 + i][y1 + j];
        }
      }
    }
  }
  else
    throw runtime_error("Error: Couldn't get values-data from given grib_handle.");

  // 3. Tarkista viel‰, jos lˆytyy paramChangeTablesta parametrille muunnos kaavat jotka pit‰‰ tehd‰
  ::MakeParameterConversions(theGridRecordData, theParamChangeTable);
}

/*
static void FillGridData(grib_handle *theGribHandle, NFmiDataMatrix<float> &theValues, const MyGrid
&theGrid, bool doGlobeFix)
{
        size_t values_length = 0;
        int status1 = grib_get_size(theGribHandle, "values", &values_length);
        vector<double> doubleValues(values_length);
        int status2 = grib_get_double_array(theGribHandle, "values", &doubleValues[0],
&values_length);
        int gridXSize = theGrid.itsNX;
        int gridYSize = theGrid.itsNY;
        theValues.Resize(theGrid.itsNX, theGrid.itsNY);
        if(status1 == 0 && status2 == 0)
        {

//		long resolutionAndComponentFlags = 0;
//		int status3 = grib_get_long(theGribHandle, "resolutionAndComponentFlags",
&resolutionAndComponentFlags);
                long scanningMode = 0;
                int status4 = grib_get_long(theGribHandle, "scanningMode", &scanningMode);

                if(status4 == 0 && scanningMode == 0) // scanningMode 1. bit 0 -> +i +x |
scanningMode 2. bit 0 -> -j -y
                {
                        for(size_t i = 0; i<values_length; i++)
                                theValues[i%gridXSize][gridYSize - (i/gridXSize) - 1] =
static_cast<float>(doubleValues[i]);
                }
                else // sitten kun tulee lis‰‰ ceissej‰, lis‰t‰‰n eri t‰yttˆ variaatioita
                {
                        throw runtime_error("Error: Found scanning mode not yet implemented.");
//			for(size_t i = 0; i<values_length; i++)
//				theValues[i%gridXSize][i/gridXSize] =
static_cast<float>(doubleValues[i]);
                }

                if(doGlobeFix)
                {
                        // Nyt on siis tilanne ett‰ halutaan 'korjata' globaali data editoria
varten.
                        // Latlon-area peitt‰‰ maapallon longitudeissa 0-360, se pit‰‰ muuttaa
editoria varten longitudeihin -180 - 180.
                        // Eli matriisissa olevia arvoja pit‰‰ siirt‰‰ niin ett‰ vasemmalla
puoliskolla olevat laitetaan oikealle
                        // puolelle ja toisin p‰in.
                        int nx = static_cast<int>(theValues.NX());
                        int ny = static_cast<int>(theValues.NY());
                        for(int j=0; j < ny; j++)
                                for(int i=0; i < nx/2; i++)
                                        std::swap(theValues[i][j], theValues[i+nx/2][j]);

                }

        }
        else
                throw runtime_error("Error: Couldn't get values-data from given grib_handle.");
}
*/

static void ChangeParamSettingsIfNeeded(vector<ParamChangeItem> &theParamChangeTable,
                                        GridRecordData *theGribData,
                                        bool verbose)
{
  if (theParamChangeTable.size() > 0)
  {  // muutetaan tarvittaessa parametrin nime‰ ja id:t‰
    for (unsigned int i = 0; i < theParamChangeTable.size(); i++)
    {
      ParamChangeItem &paramChangeItem = theParamChangeTable[i];
      if (paramChangeItem.itsOriginalParamId == theGribData->itsParam.GetParamIdent() &&
          paramChangeItem.itsLevel && (*paramChangeItem.itsLevel) == theGribData->itsLevel)
      {
        theGribData->itsParam.SetParam(paramChangeItem.itsWantedParam);
        if (verbose)
        {
          cerr << " changed to ";
          cerr << theGribData->itsParam.GetParamIdent() << " "
               << theGribData->itsParam.GetParamName().CharPtr();
        }
        theGribData->itsLevel =
            NFmiLevel(1, "sfc", 0);  // tarkista ett‰ t‰st‰ tulee pinta level dataa
        if (verbose) cerr << " level -> sfc";
        break;
      }
      else if (paramChangeItem.itsOriginalParamId == theGribData->itsParam.GetParamIdent() &&
               paramChangeItem.itsLevel == 0)
      {
        theGribData->itsParam.SetParam(paramChangeItem.itsWantedParam);
        if (verbose)
        {
          cerr << " changed to ";
          cerr << theGribData->itsParam.GetParamIdent() << " "
               << theGribData->itsParam.GetParamName().CharPtr();
        }
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

vector<NFmiQueryData *> ConvertGrib2QData(FILE *theInput,
                                          int theMaxQDataSizeInBytes,
                                          bool useOutputFile,
                                          NFmiLevelBag &theIgnoredLevelList,
                                          int /* wantedGridInfoPrintCount */,
                                          const NFmiProducer &theWantedProducer,
                                          bool doGlobeFix,
                                          vector<FmiLevelType> &theAcceptOnlyLevelTypes,
                                          int &theDifferentAreaCount,
                                          const NFmiRect &theLatlonCropRect,
                                          vector<ParamChangeItem> &theParamChangeTable,
                                          bool fCropParamsNotMensionedInTable,
                                          bool verbose,
                                          NFmiGrid *theWantedGrid,
                                          const NFmiPoint &thePressureDataGridSize,
                                          const NFmiPoint &theHybridDataGridSize)
{
  vector<NFmiQueryData *> datas;
  vector<GridRecordData *> gribRecordDatas;

  try
  {
    grib_handle *gribHandle = NULL;
    grib_context *gribContext = grib_context_get_default();
    int err = 0;
    //		char *mode = "serialize";
    //		int option_flags = 0;
    //		int status = 0;
    int counter = 0;
    NFmiMetTime firstValidTime;
    cerr << endl << "Reading and intepreting grib fields." << endl;
    //		while((gribHandle = grib_handle_new_from_file(0, theInput, &err)) != NULL)
    while ((gribHandle = grib_handle_new_from_file(gribContext, theInput, &err)) != NULL ||
           err != GRIB_SUCCESS)
    {
      counter++;
      cerr << counter << " ";
      //			if(counter < 25 || counter > 70)
      //				continue; // ***** POISTA TƒMƒ, TESTI VAIHEESSA PURETAAN
      // VAIN max 10 kentt‰‰!!!!  *******
      //			if(counter % 30 != 0)
      //				continue;
      GridRecordData *tmpData = new GridRecordData;
      tmpData->itsLatlonCropRect = theLatlonCropRect;
      try
      {
        // param ja level tiedot pit‰‰ hanskata ennen hilan koon m‰‰rityst‰
        tmpData->itsParam = ::GetParam(gribHandle, theWantedProducer);
        tmpData->itsLevel = ::GetLevel(gribHandle);
        ::FillGridInfoFromGribHandle(gribHandle,
                                     tmpData,
                                     doGlobeFix,
                                     theWantedGrid,
                                     thePressureDataGridSize,
                                     theHybridDataGridSize);
        tmpData->itsOrigTime = ::GetOrigTime(gribHandle);
        tmpData->itsValidTime = ::GetValidTime(gribHandle);
        tmpData->itsMissingValue = ::GetMissingValue(gribHandle);

        if (verbose)
        {
          cerr << tmpData->itsValidTime.ToStr("YYYYMMDDHHmm", kEnglish).CharPtr() << ";";
          cerr << tmpData->itsParam.GetParamName().CharPtr() << ";";
          cerr << tmpData->itsLevel.GetIdent() << ";";
          cerr << tmpData->itsLevel.LevelValue() << ";";
        }
        /*
                                        if(counter == 1)
                                                firstValidTime = tmpData->itsValidTime; // T‰m‰
           validtime juttu oli vain testaamista varten gfs grib2 datasta, koska siell‰ oli joku
           kentt‰ toisessa ajassa kuin muut (kun tiedostossa pit‰isi olla vain yhden aika-askeleen
           dataa)
                                        if(firstValidTime != tmpData->itsValidTime)
                                        {
                                                cerr << "\nValidTime was different than the
           first:\n";
                                                cerr << "ValidTime: " << tmpData->itsValidTime <<
           endl;
                                                cerr << "Param: " << tmpData->itsParam << endl;
                                                cerr << "Level: " << tmpData->itsLevel << endl;
                                                cerr << "Area+Grid: " <<
           tmpData->itsOrigGrid.itsArea->AreaStr() << " X: " << tmpData->itsOrigGrid.itsNX << " Y: "
           << tmpData->itsOrigGrid.itsNY << endl;
                                        }
        */
        ::ChangeParamSettingsIfNeeded(theParamChangeTable, tmpData, verbose);

        /*
                                        if(theParamChangeTable.size() > 0)
                                        { // muutetaan tarvittaessa parametrin nime‰ ja id:t‰
                                                for(unsigned int i = 0;
           i<theParamChangeTable.size(); i++)
                                                {
                                                        if(theParamChangeTable[i].first ==
           tmpData->itsParam.GetParamIdent())
                                                        {
                                                                tmpData->itsParam.SetParam(theParamChangeTable[i].second);
                                                                break;
                                                        }
                                                }
                                        }
        */
        // filtteri j‰tt‰‰ huomiotta ns. kontrolli hilan, joka on ainakin hirlam datassa 1.. se on
        // muista poikkeava 2x2 hila latlon-area.
        // Aiheuttaisi turhia ongelmia jatkossa monessakin paikassa.
        bool gribFieldUsed = false;
        if (!(tmpData->itsOrigGrid.itsNX <= 2 && tmpData->itsOrigGrid.itsNY <= 2))
        {
          if (::IgnoreThisLevel(tmpData, theIgnoredLevelList) == false)
          {
            if (::AcceptThisLevelType(tmpData, theAcceptOnlyLevelTypes))
            {
              if (::CropParam(tmpData, fCropParamsNotMensionedInTable, theParamChangeTable) ==
                  false)
              {
                //				::FillGridData(gribHandle, tmpData->itsGridData,
                // tmpData->itsGrid, doGlobeFix);
                ::FillGridData(gribHandle, tmpData, doGlobeFix, theParamChangeTable);
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
          if (verbose)
          {
            cerr << " (skipped)" << endl;
          }
        }
        else
        {
          if (verbose) cerr << endl;
        }
      }
      catch (exception &e)
      {
        cerr << "\nProblem with grib filed #" << NFmiStringTools::Convert(counter)
             << " continuing..." << endl;
        cerr << e.what() << endl;
        delete tmpData;
      }
      catch (...)
      {
        cerr << "\nUnknown problem with grib filed #" << NFmiStringTools::Convert(counter)
             << " continuing..." << endl;
        delete tmpData;
      }
      grib_handle_delete(gribHandle);
    }  // for-loop

    datas = CreateQueryDatas(
        gribRecordDatas, theMaxQDataSizeInBytes, useOutputFile, theDifferentAreaCount);

    if (err) throw runtime_error(grib_get_error_message(err));
  }
  catch (...)
  {
    ::FreeDatas(gribRecordDatas);
    throw;
  }

  ::FreeDatas(gribRecordDatas);

  return datas;
}

struct LevelLessThan
{
  bool operator()(const NFmiLevel &l1, const NFmiLevel &l2)
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
                                                     bool useOutputFile)
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

  if (useOutputFile)
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

vector<NFmiQueryData *> CreateQueryDatas(vector<GridRecordData *> &theGribRecordDatas,
                                         int theMaxQDataSizeInBytes,
                                         bool useOutputFile,
                                         int &theDifferentAreaCount)
{
  cerr << endl << "Converting grids to images" << endl;
  vector<NFmiQueryData *> qdatas;
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    vector<NFmiHPlaceDescriptor> hPlaceDescriptors =
        GetAllHPlaceDescriptors(theGribRecordDatas, useOutputFile);
    theDifferentAreaCount = static_cast<int>(hPlaceDescriptors.size());
    vector<NFmiVPlaceDescriptor> vPlaceDescriptors =
        GetAllVPlaceDescriptors(theGribRecordDatas, useOutputFile);
    for (unsigned int j = 0; j < vPlaceDescriptors.size(); j++)
    {
      for (unsigned int i = 0; i < hPlaceDescriptors.size(); i++)
      {
        cerr << "L" << NFmiStringTools::Convert(j) << "H" << NFmiStringTools::Convert(i) << " ";
        NFmiQueryData *qdata = CreateQueryData(
            theGribRecordDatas, hPlaceDescriptors[i], vPlaceDescriptors[j], theMaxQDataSizeInBytes);
        if (qdata) qdatas.push_back(qdata);
      }
    }
    /*
                    NFmiParamDescriptor params(GetParamDesc(theGribRecordDatas));
                    NFmiVPlaceDescriptor levels(GetVPlaceDesc(theGribRecordDatas));
                    NFmiTimeDescriptor times(GetTimeDesc(theGribRecordDatas));
                    NFmiQueryInfo innerInfo(params, times, hplace, levels);
                    CheckInfoSize(innerInfo, theMaxQDataSizeInBytes);
                    qdata = NFmiQueryDataUtil::CreateEmptyData(innerInfo);
                    FillQDataWithGribRecords(*qdata, theGribRecordDatas);
    */
  }
  return qdatas;
}

NFmiQueryData *CreateQueryData(vector<GridRecordData *> &theGribRecordDatas,
                               NFmiHPlaceDescriptor &theHplace,
                               NFmiVPlaceDescriptor &theVplace,
                               int theMaxQDataSizeInBytes)
{
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  if (gribCount > 0)
  {
    FillQDataWithGribRecords(theGribRecordDatas);
  }
  return 0;
}

void CheckInfoSize(const NFmiQueryInfo &theInfo, int theMaxQDataSizeInBytes)
{
  unsigned long infoSize = theInfo.Size();
  unsigned long infoSizeInBytes = infoSize * sizeof(float);
  if (theMaxQDataSizeInBytes < static_cast<int>(infoSizeInBytes))
  {
    stringstream ss;
    ss << "Datasta tulisi liian iso:" << endl;
    ss << "QDatan kooksi tulisi nyt " << infoSizeInBytes << " tavua." << endl;
    ss << "Rajaksi on asetettu " << theMaxQDataSizeInBytes << " tavua." << endl;

    unsigned long paramSize = theInfo.SizeParams();
    ss << "Parametreja on " << paramSize << " kpl." << endl;
    unsigned long timeSize = theInfo.SizeTimes();
    ss << "Aikoja on " << timeSize << " kpl." << endl;
    unsigned long locSize = theInfo.SizeLocations();
    ss << "Hilapisteit‰/asemia on " << locSize << " kpl." << endl;
    unsigned long levelSize = theInfo.SizeLevels();
    ss << "Leveleit‰ on " << levelSize << " kpl." << endl;
    throw runtime_error(ss.str());
  }
}

/* The following code taken from pnmgamma and editor to support our needs */
#define MIN(a, b) (a < b ? a : b)
static void buildPowGamma(unsigned char table[],
                          unsigned char const maxval,
                          unsigned char const newMaxval,
                          double const gamma)
{
  /*----------------------------------------------------------------------------
    Build a gamma table of size maxval+1 for the given gamma value.

    This function depends on pow(3m).  If you don't have it, you can
    simulate it with '#define pow(x,y) exp((y)*log(x))' provided that
    you have the exponential function exp(3m) and the natural logarithm
    function log(3m).  I can't believe I actually remembered my log
    identities.
    -----------------------------------------------------------------------------*/
  unsigned int i;
  double const oneOverGamma = 1.0 / gamma;

  for (i = 0; i <= maxval; ++i)
  {
    double const normalized = static_cast<double>(i) / maxval;
    /* sample value normalized to 0..1 */

    double const v = pow(normalized, oneOverGamma);
    table[i] = MIN(static_cast<unsigned char>(v * newMaxval + 0.5), newMaxval);
    /* denormalize, round and clip */
  }
}
/* pnmgamma code ends */

bool FillQDataWithGribRecords(vector<GridRecordData *> &theGribRecordDatas)
{
  int gribCount = static_cast<int>(theGribRecordDatas.size());
  GridRecordData *tmp = 0;
  int filledGridCount = 0;

  cerr << "Exporting JPEGs ";

  // LUMINANCE XOR DEPENDS ON THE INVERT FLAG!
  unsigned char luminance_xor = (globalInvert == true ? 255 : 0);

  // Create a luminance transformation table ==>
  cerr << "[Luminance transformation table: ";
  unsigned char transformationTable[256];
  for (int x = 0; x < 255; x++)
    transformationTable[x] = x;

  // Apply gamma correction for luminance transformation table
  cerr << "gamma=" << globalGamma;
  buildPowGamma(transformationTable, 255, 255, globalGamma);

  cerr << "] " << std::endl;
  // <== Done with luminance transformation table

  // Export the grids
  for (int k = 0; k < gribCount; k++)
  {
    tmp = theGribRecordDatas[k];

    // Generate filename based on pattern
    char pattern[255], jpegFilename[255];
    struct tm *time_table;
    const time_t itsValidTime = tmp->itsValidTime.EpochTime();
    time_table = gmtime(&itsValidTime);
    strftime(pattern, 254, globalFilenamePattern.c_str(), time_table);
    snprintf(jpegFilename, 254, "%s%s.jpg", globalFilenamePrefix.c_str(), pattern);

    // Initialize data matrix
    NFmiDataMatrix<float> gridValues = tmp->itsGridData;

    // Initialize JPEG creation
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *jpeg;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((jpeg = fopen(jpegFilename, "wb")) == NULL)
    {
      std::cerr << "Unable to create output file " << jpegFilename << std::endl;
      return filledGridCount > 0;
    }
    jpeg_stdio_dest(&cinfo, jpeg);

    cinfo.image_width = gridValues.NX(); /* image width and height, in pixels */
    cinfo.image_height = gridValues.NY();
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 85, TRUE /* limit to baseline-JPEG values */);

    // Construct the scanlines for JPEG
    jpeg_start_compress(&cinfo, TRUE);

    // Allocate RGB buffer for single RGB8 row
    JSAMPLE *image_buffer = reinterpret_cast<JSAMPLE *>(malloc(3 * gridValues.NX()));
    JSAMPLE luminance;
    unsigned int byte_offset;

    for (int j = gridValues.NY() - 1; j >= 0; j--)
    {
      for (unsigned int i = 0; i < gridValues.NX(); i++)
      {
        // Read the luminance information and apply transformation
        luminance =
            transformationTable[(static_cast<unsigned char>(gridValues[i][j]) ^ luminance_xor)];

        // Calculate the byte offsets in RGB buffer
        byte_offset = i * 3;

        // RED channel
        image_buffer[byte_offset] = luminance;
        // GREEN channel
        image_buffer[byte_offset + 1] = luminance;
        // BLUE channel
        image_buffer[byte_offset + 2] = luminance;
      }
      // Output the scanline (one row)
      (void)jpeg_write_scanlines(&cinfo, &image_buffer, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(jpeg);

    // Grid exported
    filledGridCount++;
  }

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

// Etsit‰‰n ne parametrit mitk‰ lˆytyv‰t data-setist‰.
// Lis‰ksi hilan ja arean pit‰‰ olla sama kuin annetussa hplaceDescriptorissa ja level-tyypin pit‰‰
// olla
// sama kuin vplaceDescriptorissa.
NFmiParamDescriptor GetParamDesc(vector<GridRecordData *> &theGribRecordDatas,
                                 NFmiHPlaceDescriptor &theHplace,
                                 NFmiVPlaceDescriptor &theVplace)
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
