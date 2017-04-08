/**
 * metar2qd-filtteri lukee viestikoneelta saatavia metar/speci viestejä,
 * ja muuttaa ne querydataksi ja sitten tallettaa sen haluttuun tiedostoon.
 * Seuraavassa näyte metar-tiedostosta.
 */
/*
SAJP31 KWBC 242030
METAR
RJAA 242030Z VRB01KT 2800 -RA BR FEW001 BKN007 13/12 Q1008 BECMG
     BKN004=
RJAA 242030Z VRB01KT 2800 -RA BR FEW001 BKN007 13/12 Q1008 BECMG
     BKN004 RMK 1ST001 7ST007 A2978=
RJCC 242030Z 17010KT 9999 FEW010 SCT015 BKN020 08/06 Q1008=
RJCC 242030Z 17010KT 9999 FEW010 SCT015 BKN020 08/06 Q1008 RMK
     1ST010 4ST015 7CU020 A2978=
RJTT 242030Z 36010KT 6000 -RA FEW007 SCT010 BKN015 12/11 Q1008=
RJTT 242030Z 36010KT 6000 -RA FEW007 SCT010 BKN015 12/11 Q1008 RMK
     2ST007 4ST010 7SC015 A2978=
*/

#ifdef _MSC_VER
#pragma warning(disable : 4109 4068 4996)  // Disables many warnings that MSVC++ 7.1 generates
#endif

#include <fstream>
#include <iostream>
#include <map>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiProducerName.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiTimeList.h>
#include <smarttools/NFmiAviationStationInfoSystem.h>

extern "C" {
#include "metar_structs.h"
void print_decoded_metar(Decoded_METAR *);
int decode_metar(char *, Decoded_METAR *);
}

using namespace std;

static void run(int argc, const char *argv[]);

static bool fVerboseMode = false;
static bool fIgnoreBadStations = false;

// ----------------------------------------------------------------------
/*!
 * \brief Report a problem with coordinates
 */
// ----------------------------------------------------------------------

static void ThrowLatLonProblemException(const std::string &theFunctionNameStr,
                                        const std::string &theProblemStr,
                                        const std::string &theLatLonStr,
                                        const std::string &theLineStr,
                                        const std::string &theInitFileName)
{
  std::string errStr;
  errStr += theFunctionNameStr;
  errStr += " - ";
  errStr += theProblemStr;
  errStr += ": ";
  errStr += theLatLonStr;
  errStr += "\non line:\n";
  errStr += theLineStr;
  errStr += "\nin file: ";
  errStr += theInitFileName;
  throw std::runtime_error(errStr);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new parameter to the given parameter bag
 */
// ----------------------------------------------------------------------

static void MakeParamThings(unsigned long parId,
                            const string &name,
                            NFmiDataIdent &dataIdent,
                            NFmiParamBag &parBag)
{
  NFmiParam *param = dataIdent.GetParam();
  param->SetIdent(parId);
  param->SetName(name);
  parBag.Add(dataIdent);
}

// ----------------------------------------------------------------------
/*!
 * \brief Build the default parameter descriptor
 *
 * Includes desired parameters only
 */
// ----------------------------------------------------------------------

static void MakeDefaultParamDesc(NFmiParamDescriptor &params)
{
  NFmiProducer prod(kFmiMETAR, "METAR");
  NFmiParamBag parBag;
  NFmiParam param(kFmiTemperature,
                  "T",
                  kFloatMissing,
                  kFloatMissing,
                  kFloatMissing,
                  kFloatMissing,
                  "%.1f",
                  kLinearly);
  NFmiDataIdent dataIdent(param, prod);
  parBag.Add(NFmiDataIdent(dataIdent));

  ::MakeParamThings(kFmiDewPoint, "Td", dataIdent, parBag);
  ::MakeParamThings(kFmiWindSpeedMS, "WS", dataIdent, parBag);
  ::MakeParamThings(kFmiWindDirection, "WD", dataIdent, parBag);
  ::MakeParamThings(kFmiWindGust, "WGust", dataIdent, parBag);
  ::MakeParamThings(kFmiPressure, "P", dataIdent, parBag);
  ::MakeParamThings(kFmiAviationVisibility, "VIS", dataIdent, parBag);
  ::MakeParamThings(kFmiVerticalVisibility, "VV[m]", dataIdent, parBag);
  ::MakeParamThings(kFmiAviationWeather1, "ww1", dataIdent, parBag);
  ::MakeParamThings(kFmiAviationWeather2, "ww2", dataIdent, parBag);
  ::MakeParamThings(kFmiAviationWeather3, "ww3", dataIdent, parBag);
  ::MakeParamThings(kFmi1CloudCover, "ClCov1", dataIdent, parBag);
  ::MakeParamThings(kFmi1CloudBase, "ClBase1", dataIdent, parBag);
  ::MakeParamThings(kFmi1CloudType, "ClType1", dataIdent, parBag);
  ::MakeParamThings(kFmi2CloudCover, "ClCov2", dataIdent, parBag);
  ::MakeParamThings(kFmi2CloudBase, "ClBase2", dataIdent, parBag);
  ::MakeParamThings(kFmi2CloudType, "ClType2", dataIdent, parBag);
  ::MakeParamThings(kFmi3CloudCover, "ClCov3", dataIdent, parBag);
  ::MakeParamThings(kFmi3CloudBase, "ClBase3", dataIdent, parBag);
  ::MakeParamThings(kFmi3CloudType, "ClType3", dataIdent, parBag);
  ::MakeParamThings(kFmi4CloudCover, "ClCov4", dataIdent, parBag);
  ::MakeParamThings(kFmi4CloudBase, "ClBase4", dataIdent, parBag);
  ::MakeParamThings(kFmi4CloudType, "ClType4", dataIdent, parBag);

  params = NFmiParamDescriptor(parBag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Data for one station, time and level
 */
// ----------------------------------------------------------------------

struct MetarData
{
  static const size_t itsTemperatureIndex = 0;
  static const size_t itsDewpointIndex = 1;
  static const size_t itsWSIndex = 2;
  static const size_t itsWDIndex = 3;
  static const size_t itsWGustIndex = 4;
  static const size_t itsPressureIndex = 5;
  static const size_t itsVisibilityIndex = 6;
  static const size_t itsVerticalVisibilityIndex = 7;
  static const size_t itsWW1Index = 8;
  static const size_t itsWW2Index = 9;
  static const size_t itsWW3Index = 10;
  static const size_t itsClCover1Index = 11;
  static const size_t itsClBase1Index = 12;
  static const size_t itsClType1Index = 13;
  static const size_t itsClCover2Index = 14;
  static const size_t itsClBase2Index = 15;
  static const size_t itsClType2Index = 16;
  static const size_t itsClCover3Index = 17;
  static const size_t itsClBase3Index = 18;
  static const size_t itsClType3Index = 19;
  static const size_t itsClCover4Index = 20;
  static const size_t itsClBase4Index = 21;
  static const size_t itsClType4Index = 22;
  static const size_t itsParamVectorSize = 23;

  MetarData(void)
      : itsValues(),
        itsParamIds(),
        itsIcaoName("xxxx"),
        itsTime(),
        itsOriginalStr("xxxxxxx"),
        itsStationId(99999999),
        fIsCorrected(false)
  {
    itsValues.resize(itsParamVectorSize, kFloatMissing);
    itsParamIds.resize(itsParamVectorSize, kFmiBadParameter);

    // HUOM! ks. MakeParamThings-funktiota edellä, että tiedät mitä parametreja pitää alustaa.
    // Miten tämän alustuksen voisi tehdä käyttämällä olemassa olevaa Paramdescriptoria?!?!
    itsParamIds[itsTemperatureIndex] = kFmiTemperature;
    itsParamIds[itsDewpointIndex] = kFmiDewPoint;
    itsParamIds[itsWSIndex] = kFmiWindSpeedMS;
    itsParamIds[itsWDIndex] = kFmiWindDirection;
    itsParamIds[itsWGustIndex] = kFmiWindGust;
    itsParamIds[itsPressureIndex] = kFmiPressure;
    itsParamIds[itsVisibilityIndex] = kFmiAviationVisibility;
    itsParamIds[itsVerticalVisibilityIndex] = kFmiVerticalVisibility;
    itsParamIds[itsWW1Index] = kFmiAviationWeather1;
    itsParamIds[itsWW2Index] = kFmiAviationWeather2;
    itsParamIds[itsWW3Index] = kFmiAviationWeather3;
    itsParamIds[itsClCover1Index] = kFmi1CloudCover;
    itsParamIds[itsClBase1Index] = kFmi1CloudBase;
    itsParamIds[itsClType1Index] = kFmi1CloudType;
    itsParamIds[itsClCover2Index] = kFmi2CloudCover;
    itsParamIds[itsClBase2Index] = kFmi2CloudBase;
    itsParamIds[itsClType2Index] = kFmi2CloudType;
    itsParamIds[itsClCover3Index] = kFmi3CloudCover;
    itsParamIds[itsClBase3Index] = kFmi3CloudBase;
    itsParamIds[itsClType3Index] = kFmi3CloudType;
    itsParamIds[itsClCover4Index] = kFmi4CloudCover;
    itsParamIds[itsClBase4Index] = kFmi4CloudBase;
    itsParamIds[itsClType4Index] = kFmi4CloudType;
  }

  checkedVector<float> itsValues;
  checkedVector<FmiParameterName> itsParamIds;
  string itsIcaoName;
  NFmiMetTime itsTime;
  string itsOriginalStr;
  unsigned long itsStationId;  // tähän talletetaan löydetyn aseman wmoid, jota käytetään kun
                               // querydataa täytetään näillä otuksilla
  bool fIsCorrected;
};

// ----------------------------------------------------------------------
/*!
 * \brief Extract time information
 *
 * tämä on siis muotoa 250500Z eli DDHHmmZ
 * joten kuukausi on tuntematon joten jos päiväys muuten menisi tulevaisuuteen,
 * siirretäänkin se viime kuukauteen.
 */
// ----------------------------------------------------------------------

static NFmiMetTime GetTime(const std::string &timeStr,
                           const string &theMetarStr,
                           const string &theMetarFileName,
                           bool no_Z_ending,
                           int theTimeRoundingResolution)
{
  if (timeStr.size() == static_cast<size_t>(no_Z_ending ? 6 : 7))
  {
    if (no_Z_ending || ::toupper(timeStr[timeStr.size() - 1]) == 'Z')
    {
      NFmiMetTime currentTime(theTimeRoundingResolution);
      NFmiMetTime aTime(currentTime);
      short aDay = NFmiStringTools::Convert<short>(string(timeStr.begin(), timeStr.begin() + 2));
      short aHour =
          NFmiStringTools::Convert<short>(string(timeStr.begin() + 2, timeStr.begin() + 4));
      short aMinute =
          NFmiStringTools::Convert<short>(string(timeStr.begin() + 4, timeStr.begin() + 6));
      aTime.SetDay(aDay);
      aTime.SetHour(aHour);
      aTime.SetMin(aMinute);
      aTime.SetTimeStep(static_cast<short>(theTimeRoundingResolution));
      // aTime.FromStr(NFmiString(timeStr), kDDHHMM); // bugi FromStr-metodissa, ei toimi
      if (aTime > currentTime)
        aTime.PreviousMetTime(NFmiTimePerioid(0, 1, 0, 0, 0, 0));  // siirretään kuukausi
      // taaksepäin, jos saatu aika oli
      // tulevaisuudessa
      return aTime;
    }
    else
      ::ThrowLatLonProblemException(
          "GetTime", "Time string didn't have ending 'Z'", timeStr, theMetarStr, theMetarFileName);
  }
  else
    ::ThrowLatLonProblemException(
        "GetTime", "Time string was illegal", timeStr, theMetarStr, theMetarFileName);

  return NFmiMetTime();  // ei mene tähän, koska edellä olevat ThrowLatLonProblemException-funktiot
                         // heittävät poikkeuksen
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert time list to time bag if possible
 *
 * Timebags are more space efficient and possibly also faster
 */
// ----------------------------------------------------------------------

static bool ConvertTimeList2TimeBag(NFmiTimeList &theTimeList, NFmiTimeBag &theTimeBag)
{
  // tutkitaan onko mahdollista tehda listasta bagi
  // eli ajat ovat peräkkäisiä ja tasavälisiä
  if (theTimeList.NumberOfItems() >
      2)  // ei  tehdä yhdestä tai kahdesta ajasta bagiä vaikka se on mahdollista
  {
    theTimeList.First();
    theTimeList.Next();
    int resolution = theTimeList.CurrentResolution();
    int maxMissedSteps = static_cast<int>(3. * 60. / resolution);  // tässä lasketaan montako
    // steppiä on 3 tunnissa, mikä on
    // hyväksyttävä aukko datassa
    int currentRes = 0;
    for (; theTimeList.Next();)
    {
      currentRes = theTimeList.CurrentResolution();
      if (resolution != currentRes)
      {
        // sallitaan pienet poikkeavuudet, koska yksi tai kaksi aika-askelta saattaa puuttua
        // listasta,
        // mutta kannattaa tehdä timebagi kuitenkin
        // ELI jos kurrentti res on isompi kuin default ja niiden jakojäännös on 0 ja ei jää reilua
        // vuorokautta suurempi aukko, niin todetaan ok:si
        if ((currentRes > resolution) && (currentRes % resolution == 0) &&
            (currentRes / resolution < maxMissedSteps))
          continue;
        return false;  // jos yhdenkin aikavälin resoluutio poikkeaa, ei voida tehdä bagia
      }
    }
    theTimeBag = NFmiTimeBag(theTimeList.FirstTime(), theTimeList.LastTime(), resolution);
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build the time descriptor
 */
// ----------------------------------------------------------------------

static NFmiTimeDescriptor MakeTimeDesc(const vector<MetarData> &dataBlocks)
{
  set<NFmiMetTime> times;  // kerää eri ajat settiin ensin, sitten tee timelist jne.
  for (size_t i = 0; i < dataBlocks.size(); i++)
    times.insert(dataBlocks[i].itsTime);

  // Tehdaan aluksi timelist, koska se on helpompi,
  NFmiTimeList timeList;
  set<NFmiMetTime>::iterator it = times.begin();
  if (it != times.end())
  {
    NFmiMetTime origTime(*it);
    for (; it != times.end(); ++it)
      timeList.Add(new NFmiMetTime(*it));

    NFmiTimeBag timeBag;
    bool fUseTimeBag = ::ConvertTimeList2TimeBag(timeList, timeBag);  // jos mahd.

    // Oletus kaikki origintimet ovat samoja, en tutki niita nyt yhtaan.
    if (fUseTimeBag)
      return NFmiTimeDescriptor(origTime, timeBag);
    else
    {
      cerr << "Warning: Cannot make timelist from data, continuing..." << endl;
      return NFmiTimeDescriptor(origTime, timeList);
    }
  }
  throw runtime_error("Error: MakeTimeDesc-function - No times were found, stopping program...");
}

// ----------------------------------------------------------------------
/*!
 * \brief Make HPlaceDescriptor
 */
// ----------------------------------------------------------------------

static NFmiHPlaceDescriptor MakeHPlaceDesc(NFmiAviationStationInfoSystem &theStationInfoSystem,
                                           const vector<MetarData> &dataBlocks)
{
  set<string> icaoStrSet;  // kerää eri icao tunnukset settiin ensin, sitten tee locationbag jne.
  for (size_t i = 0; i < dataBlocks.size(); i++)
    icaoStrSet.insert(dataBlocks[i].itsIcaoName);

  NFmiLocationBag locations;
  for (set<string>::iterator it = icaoStrSet.begin(); it != icaoStrSet.end(); ++it)
  {
    NFmiAviationStation *aviationStation = theStationInfoSystem.FindStation(*it);
    if (aviationStation)
    {
      NFmiStation station(*aviationStation);
      if (theStationInfoSystem.WmoStationsWanted() == false)
      {
        // Tehdään lopullisesta asema nimesta ICAO-id + oikea nimi suluissa
        string usedStationName(*it);
        usedStationName += " (";
        usedStationName += station.GetName();
        usedStationName += ")";
        station.SetName(usedStationName);
      }
      locations.AddLocation(station);  // pakko tehdä tässä station-kopio, koska muuten tuolla
                                       // sisällä tehtäisiin aviationstationille clone
    }
    else
      cerr << "Warning: Unknown ICAO station ID found: " << (*it).c_str()
           << ", ignoring it and continuing..." << endl;
  }

  NFmiHPlaceDescriptor hplace(locations);
  return hplace;
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy METAR data into empty querydata
 */
// ----------------------------------------------------------------------

static void FillInfoFromMetarData(NFmiFastQueryInfo &info, const vector<MetarData> &datas)
{
  int timeErrorCount = 0;
  int paramErrorCount = 0;
  int stationErrorCount = 0;
  info.First();
  for (size_t i = 0; i < datas.size(); i++)
  {
    const MetarData &data = datas[i];
    bool correctedOverRide = data.fIsCorrected;
    if (info.Time(data.itsTime))
    {
      if (info.Location(data.itsStationId))
      {
        for (size_t j = 0; j < data.itsParamIds.size(); j++)
        {
          if (info.Param(data.itsParamIds[j]))
          {
            if (correctedOverRide)
            {
              if (data.itsValues[j] != kFloatMissing)
                info.FloatValue(data.itsValues[j]);  // corrected datan kanssa tutkitaan onko
                                                     // correctedin datan parametri ei puuttuvaa ja
                                                     // tällöin laitetaan päälle
            }
            else
            {
              if (info.FloatValue() == kFloatMissing)  // ei vedetä dataa päälle, jos siellä on jo
                                                       // jotain (samaan aikaan samalle asemalle
                                                       // voidaan saada useita sanomia, missä
                                                       // joissain on puuttuvaa)
                info.FloatValue(data.itsValues[j]);
            }
          }
          else
            paramErrorCount++;
        }
      }
      else
        stationErrorCount++;
    }
    else
      timeErrorCount++;
  }

  if (timeErrorCount || paramErrorCount ||
      stationErrorCount)  // jos jossain oli yksikin virhe, raportoidaan
  {
    cerr << "Some level/time/station/param were not found in FillInfoFromBlocks-function\n";
    cerr << "timeErrorCount: " << NFmiStringTools::Convert(timeErrorCount);
    cerr << "\nstationErrorCount: " << NFmiStringTools::Convert(stationErrorCount);
    cerr << "\nparamErrorCount: " << NFmiStringTools::Convert(paramErrorCount);
    cerr << "\nContinuing execution...";
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Build empty queryinfo from METAR info
 */
// ----------------------------------------------------------------------

static NFmiQueryInfo MakeQueryInfo(NFmiParamDescriptor &params,
                                   NFmiAviationStationInfoSystem &theStationInfoSystem,
                                   const vector<MetarData> &dataBlocks)
{
  NFmiTimeDescriptor times(::MakeTimeDesc(dataBlocks));
  NFmiHPlaceDescriptor hplace(::MakeHPlaceDesc(theStationInfoSystem, dataBlocks));
  NFmiQueryInfo info(params, times, hplace, NFmiVPlaceDescriptor());
  return info;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build empty querydata from METAR info
 */
// ----------------------------------------------------------------------

static NFmiQueryData *MakeQueryDataFromBlocks(NFmiParamDescriptor &params,
                                              NFmiAviationStationInfoSystem &theStationInfoSystem,
                                              const vector<MetarData> &dataBlocks)
{
  NFmiQueryInfo info = ::MakeQueryInfo(params, theStationInfoSystem, dataBlocks);
  NFmiQueryData *data = NFmiQueryDataUtil::CreateEmptyData(info);
  if (!data)
    throw runtime_error(
        "Error: Unable to create querydata in MakeQueryDataFromBlocks, stopping program...");

  NFmiFastQueryInfo fastInfo(data);
  ::FillInfoFromMetarData(fastInfo, dataBlocks);
  return data;
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(const std::string &theExecutableName)
{
  NFmiFileString fileNameStr(theExecutableName);
  std::string usedFileName(
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimestä

  cerr << "Usage: " << usedFileName.c_str() << " [options] fileFilter1[,fileFilter2,...] > output"
       << endl
       << endl
       << "Program reads all the files/filefilter files given as arguments" << endl
       << "and tries to find METAR (and SPECI) messages from them." << endl
       << "All the found synops are combined in one output querydata file." << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-s station-info-file\tGive station info from in csv-format which is converted " << endl
       << "from file "
          "http://www.weathergraphics.com/identifiers/master-location-identifier-database.xls"
       << endl
       << "and which is provided now in file master-location-identifier-database.csv." << endl
       << "\t-v\tUse verbose warning and error reporting." << endl
       << "\t-F\tIgnore bad station descriptions in station info." << endl
       << "\t-r <round_time_in_minutes>\tUse messages time rounding, default value is 30 minutes."
       << endl
       << "\t-n <NOAA-format=false>\tTry reading NOAA metar format files." << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract next METAR token
 */
// ----------------------------------------------------------------------

static std::string GetStringAndAdvance(boost::sregex_token_iterator &it, bool mustExist)
{
  boost::sregex_token_iterator end;
  if (it != end)
  {
    std::string str;
    do
    {
      str = *it;
      ++it;  // advance iterator
    } while (str.empty() &&
             it != end);  // joskus tulee tyhjiä stringejä ennen loppua, jotka pitää ohittaa
    return str;
  }
  if (mustExist)
    throw runtime_error(
        "Error: The header-section was incomplete in input data, stopping program...");
  return string();  // muutoin palauta tyhjä stringi
}

// ----------------------------------------------------------------------
/*!
 * \brief This is some platform specific logging system
 */
// ----------------------------------------------------------------------

class StationErrorStrings : public set<string>
{
 public:
  ~StationErrorStrings(void)
  {
    ofstream out("d://data//11caribia//missing_icao.txt");
    if (out)
    {
      out << endl << "Here is list of all unknown ICAO ids:" << endl;
      StationErrorStrings::iterator it = begin();
      for (; it != end(); ++it)
        out << *it << ", ";
      out << endl << endl;
    }
  }
};

// ----------------------------------------------------------------------
/*!
 * \brief Dummy time to indicate missing time information
 */
// ----------------------------------------------------------------------

static const NFmiMetTime missingTime(1900, 1, 1, 0);

// ----------------------------------------------------------------------
/*!
 * \brief Identify an empty METAR
 */
// ----------------------------------------------------------------------

static bool IsMetarNilOrEmptyReport(const string &str)
{
  boost::regex reg(
      "\\s+");  // splittaa yksittäiset sanat, oli niissä yksi tai useampia spaceja välissä
  boost::sregex_token_iterator it(str.begin(), str.end(), reg, -1);
  boost::sregex_token_iterator end;
  for (int i = 0; it != end; ++it, i++)
  {
    if (i > 3)  // jos ollaan menossa jo 4 sanaan, voidaan tarkastelu lopettaa
      break;
    else
    {
      string aWord = *it;
      NFmiStringTools::UpperCase(aWord);
      if (aWord == "NIL") return true;
    }
  }
  if (it != end)
    return false;
  else
    return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief mdsplib constants
 */
// ----------------------------------------------------------------------

static const int MDSP_missing_int = MAXINT;
static const float MDSP_missing_float = static_cast<float>(MAXINT);

// ----------------------------------------------------------------------
/*!
 * \brief Extract time
 */
// ----------------------------------------------------------------------

static NFmiMetTime GetTime2(const Decoded_METAR &theMetarStruct,
                            const NFmiMetTime &theHeaderTime,
                            int theTimeRoundingResolution)
{
  NFmiMetTime aTime(theHeaderTime);  // otetaan pohjat tästä ja loput metarStructista. Periaatteessa
                                     // headertimen pitäisi olla jo oikea aika
  if (theMetarStruct.ob_date != MDSP_missing_int)
    aTime.SetDay(static_cast<short>(theMetarStruct.ob_date));
  if (theMetarStruct.ob_hour != MDSP_missing_int)
    aTime.SetHour(static_cast<short>(theMetarStruct.ob_hour));
  if (theMetarStruct.ob_minute != MDSP_missing_int)
    aTime.SetMin(static_cast<short>(theMetarStruct.ob_minute));
  aTime.SetTimeStep(static_cast<short>(theTimeRoundingResolution));

  return aTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a translation table for weather codes
 */
// ----------------------------------------------------------------------

static void InitWWSymbols(map<string, float> &ww_symbols)
{
  ww_symbols.clear();

  // Nämä METAR ww konversiot -> synop code on saatu Viljo Kangasniemen dokumentista:
  // "METAReiden purkamisen kuvaus lyhyt versio.doc"

  ww_symbols.insert(make_pair(string("BCFG"), 11.f));
  ww_symbols.insert(make_pair(string("PRFG"), 41.f));
  ww_symbols.insert(make_pair(string("VCFG"), 40.f));
  ww_symbols.insert(make_pair(string("FG"), 49.f));
  ww_symbols.insert(make_pair(string("+FG"), 49.f));
  ww_symbols.insert(make_pair(string("FZFG"), 49.f));
  ww_symbols.insert(make_pair(string("-DRRASN"), 68.f));
  ww_symbols.insert(make_pair(string("DRRASN"), 68.f));
  ww_symbols.insert(make_pair(string("+DRRASN"), 68.f));

  ww_symbols.insert(make_pair(string("+SHRA"), 82.f));
  ww_symbols.insert(make_pair(string("SHGS"), 87.f));
  ww_symbols.insert(make_pair(string("SHPL"), 87.f));
  ww_symbols.insert(make_pair(string("+TSRA"), 97.f));
  ww_symbols.insert(make_pair(string("+TSGR"), 97.f));

  ww_symbols.insert(make_pair(string("FU"), 4.f));
  ww_symbols.insert(make_pair(string("HZ"), 5.f));
  ww_symbols.insert(make_pair(string("+HZ"), 5.f));
  ww_symbols.insert(make_pair(string("DU"), 6.f));
  ww_symbols.insert(make_pair(string("+DU"), 6.f));
  ww_symbols.insert(make_pair(string("VA"), 6.f));

  ww_symbols.insert(make_pair(string("SA"), 7.f));
  ww_symbols.insert(make_pair(string("DRSA"), 7.f));
  ww_symbols.insert(make_pair(string("DRDU"), 7.f));
  ww_symbols.insert(make_pair(string("BLSA"), 7.f));
  ww_symbols.insert(make_pair(string("BLDU"), 7.f));
  ww_symbols.insert(make_pair(string("PO"), 8.f));
  ww_symbols.insert(make_pair(string("VCPO"), 8.f));
  ww_symbols.insert(make_pair(string("VCDS"), 9.f));
  ww_symbols.insert(make_pair(string("VCSS"), 9.f));

  ww_symbols.insert(make_pair(string("BR"), 10.f));
  ww_symbols.insert(make_pair(string("BCFG"), 11.f));
  ww_symbols.insert(make_pair(string("MIFG"), 12.f));
  ww_symbols.insert(make_pair(string("VCTS"), 13.f));
  ww_symbols.insert(make_pair(string("TS"), 17.f));
  ww_symbols.insert(make_pair(string("-TS"), 17.f));
  ww_symbols.insert(make_pair(string("+TS"), 17.f));

  ww_symbols.insert(make_pair(string("SQ"), 18.f));
  ww_symbols.insert(make_pair(string("FC"), 19.f));
  ww_symbols.insert(make_pair(string("VCFC"), 19.f));

  ww_symbols.insert(make_pair(string("DS"), 31.f));
  ww_symbols.insert(make_pair(string("SS"), 31.f));

  ww_symbols.insert(make_pair(string("-DRSN"), 36.f));
  ww_symbols.insert(make_pair(string("DRSN"), 36.f));
  ww_symbols.insert(make_pair(string("+DRSN"), 37.f));

  ww_symbols.insert(make_pair(string("BLSN"), 38.f));
  ww_symbols.insert(make_pair(string("-BLSN"), 38.f));
  ww_symbols.insert(make_pair(string("+BLSN"), 39.f));

  ww_symbols.insert(make_pair(string("-DZ"), 51.f));
  ww_symbols.insert(make_pair(string("DZ"), 53.f));
  ww_symbols.insert(make_pair(string("+DZ"), 55.f));

  ww_symbols.insert(make_pair(string("-FZDZ"), 56.f));
  ww_symbols.insert(make_pair(string("FZDZ"), 57.f));
  ww_symbols.insert(make_pair(string("+FZDZ"), 57.f));

  ww_symbols.insert(make_pair(string("-RADZ"), 58.f));
  ww_symbols.insert(make_pair(string("-DZRA"), 58.f));

  ww_symbols.insert(make_pair(string("RADZ"), 59.f));
  ww_symbols.insert(make_pair(string("DZRA"), 59.f));
  ww_symbols.insert(make_pair(string("+RADZ"), 59.f));
  ww_symbols.insert(make_pair(string("+DZRA"), 59.f));

  ww_symbols.insert(make_pair(string("-RA"), 61.f));
  ww_symbols.insert(make_pair(string("RA"), 63.f));
  ww_symbols.insert(make_pair(string("+RA"), 63.f));

  ww_symbols.insert(make_pair(string("-FZRA"), 66.f));
  ww_symbols.insert(make_pair(string("FZRA"), 67.f));
  ww_symbols.insert(make_pair(string("+FZRA"), 67.f));

  ww_symbols.insert(make_pair(string("-RASN"), 68.f));
  ww_symbols.insert(make_pair(string("-SNRA"), 68.f));
  ww_symbols.insert(make_pair(string("-DZSN"), 68.f));
  ww_symbols.insert(make_pair(string("-SNDZ"), 68.f));
  ww_symbols.insert(make_pair(string("-DZSN"), 68.f));
  ww_symbols.insert(make_pair(string("-RADZSN"), 68.f));
  ww_symbols.insert(make_pair(string("-RASNDZ"), 68.f));
  ww_symbols.insert(make_pair(string("-DZRASN"), 68.f));
  ww_symbols.insert(make_pair(string("-DZSNRA"), 68.f));
  ww_symbols.insert(make_pair(string("-SNDZRA"), 68.f));

  ww_symbols.insert(make_pair(string("RASN"), 69.f));
  ww_symbols.insert(make_pair(string("SNRA"), 69.f));
  ww_symbols.insert(make_pair(string("DZSN"), 69.f));
  ww_symbols.insert(make_pair(string("SNDZ"), 69.f));
  ww_symbols.insert(make_pair(string("DZSN"), 69.f));
  ww_symbols.insert(make_pair(string("RADZSN"), 69.f));
  ww_symbols.insert(make_pair(string("RASNDZ"), 69.f));
  ww_symbols.insert(make_pair(string("DZRASN"), 69.f));
  ww_symbols.insert(make_pair(string("DZSNRA"), 69.f));
  ww_symbols.insert(make_pair(string("SNDZRA"), 69.f));

  ww_symbols.insert(make_pair(string("+RASN"), 69.f));
  ww_symbols.insert(make_pair(string("+SNRA"), 69.f));
  ww_symbols.insert(make_pair(string("+DZSN"), 69.f));
  ww_symbols.insert(make_pair(string("+SNDZ"), 69.f));
  ww_symbols.insert(make_pair(string("+DZSN"), 69.f));
  ww_symbols.insert(make_pair(string("+RADZSN"), 69.f));
  ww_symbols.insert(make_pair(string("+RASNDZ"), 69.f));
  ww_symbols.insert(make_pair(string("+DZRASN"), 69.f));
  ww_symbols.insert(make_pair(string("+DZSNRA"), 69.f));
  ww_symbols.insert(make_pair(string("+SNDZRA"), 69.f));

  ww_symbols.insert(make_pair(string("-SN"), 71.f));
  ww_symbols.insert(make_pair(string("SN"), 73.f));
  ww_symbols.insert(make_pair(string("+SN"), 75.f));

  ww_symbols.insert(make_pair(string("IC"), 76.f));
  ww_symbols.insert(make_pair(string("-IC"), 76.f));
  ww_symbols.insert(make_pair(string("+IC"), 76.f));

  ww_symbols.insert(make_pair(string("-SG"), 77.f));
  ww_symbols.insert(make_pair(string("SG"), 77.f));
  ww_symbols.insert(make_pair(string("+SG"), 77.f));

  ww_symbols.insert(make_pair(string("-PL"), 79.f));
  ww_symbols.insert(make_pair(string("PL"), 79.f));
  ww_symbols.insert(make_pair(string("+PL"), 79.f));
  ww_symbols.insert(make_pair(string("-PE"), 79.f));
  ww_symbols.insert(make_pair(string("PE"), 79.f));
  ww_symbols.insert(make_pair(string("+PE"), 79.f));

  ww_symbols.insert(make_pair(string("-SHRA"), 80.f));
  ww_symbols.insert(make_pair(string("SHRA"), 81.f));
  ww_symbols.insert(make_pair(string("+SHRA"), 81.f));

  ww_symbols.insert(make_pair(string("-SHRASN"), 83.f));
  ww_symbols.insert(make_pair(string("-SHSNRA"), 83.f));

  ww_symbols.insert(make_pair(string("SHRASN"), 84.f));
  ww_symbols.insert(make_pair(string("SHSNRA"), 84.f));
  ww_symbols.insert(make_pair(string("+SHRASN"), 84.f));
  ww_symbols.insert(make_pair(string("+SHSNRA"), 84.f));

  ww_symbols.insert(make_pair(string("-SHSN"), 85.f));
  ww_symbols.insert(make_pair(string("SHSN"), 86.f));
  ww_symbols.insert(make_pair(string("+SHSN"), 86.f));

  ww_symbols.insert(make_pair(string("-SHGS"), 87.f));
  ww_symbols.insert(make_pair(string("-SHGSRA"), 87.f));
  ww_symbols.insert(make_pair(string("-SHRAGS"), 87.f));
  ww_symbols.insert(make_pair(string("-SHGSRASN"), 87.f));
  ww_symbols.insert(make_pair(string("-SHGSSNRA"), 87.f));
  ww_symbols.insert(make_pair(string("-SHRASNGS"), 87.f));
  ww_symbols.insert(make_pair(string("-SHSNRAGS"), 87.f));
  ww_symbols.insert(make_pair(string("-SHRAGSSN"), 87.f));
  ww_symbols.insert(make_pair(string("-SHSNGSRA"), 87.f));

  ww_symbols.insert(make_pair(string("SHGS"), 88.f));
  ww_symbols.insert(make_pair(string("SHGSRA"), 88.f));
  ww_symbols.insert(make_pair(string("SHRAGS"), 88.f));
  ww_symbols.insert(make_pair(string("SHGSRASN"), 88.f));
  ww_symbols.insert(make_pair(string("SHGSSNRA"), 88.f));
  ww_symbols.insert(make_pair(string("SHRASNGS"), 88.f));
  ww_symbols.insert(make_pair(string("SHSNRAGS"), 88.f));
  ww_symbols.insert(make_pair(string("SHRAGSSN"), 88.f));
  ww_symbols.insert(make_pair(string("SHSNGSRA"), 88.f));

  ww_symbols.insert(make_pair(string("+SHGS"), 88.f));
  ww_symbols.insert(make_pair(string("+SHGSRA"), 88.f));
  ww_symbols.insert(make_pair(string("+SHRAGS"), 88.f));
  ww_symbols.insert(make_pair(string("+SHGSRASN"), 88.f));
  ww_symbols.insert(make_pair(string("+SHGSSNRA"), 88.f));
  ww_symbols.insert(make_pair(string("+SHRASNGS"), 88.f));
  ww_symbols.insert(make_pair(string("+SHSNRAGS"), 88.f));
  ww_symbols.insert(make_pair(string("+SHRAGSSN"), 88.f));
  ww_symbols.insert(make_pair(string("+SHSNGSRA"), 88.f));

  ww_symbols.insert(make_pair(string("-SHGR"), 89.f));
  ww_symbols.insert(make_pair(string("-SHGRRA"), 89.f));
  ww_symbols.insert(make_pair(string("-SHRAGR"), 89.f));
  ww_symbols.insert(make_pair(string("-SHGRRASN"), 89.f));
  ww_symbols.insert(make_pair(string("-SHGRSNRA"), 89.f));
  ww_symbols.insert(make_pair(string("-SHRASNGR"), 89.f));
  ww_symbols.insert(make_pair(string("-SHSNRAGR"), 89.f));
  ww_symbols.insert(make_pair(string("-SHRAGRSN"), 89.f));
  ww_symbols.insert(make_pair(string("-SHSNGRRA"), 89.f));

  ww_symbols.insert(make_pair(string("SHGR"), 90.f));
  ww_symbols.insert(make_pair(string("SHGRRA"), 90.f));
  ww_symbols.insert(make_pair(string("SHRAGR"), 90.f));
  ww_symbols.insert(make_pair(string("SHGRRASN"), 90.f));
  ww_symbols.insert(make_pair(string("SHGRSNRA"), 90.f));
  ww_symbols.insert(make_pair(string("SHRASNGR"), 90.f));
  ww_symbols.insert(make_pair(string("SHSNRAGR"), 90.f));
  ww_symbols.insert(make_pair(string("SHRAGRSN"), 90.f));
  ww_symbols.insert(make_pair(string("SHSNGRRA"), 90.f));

  ww_symbols.insert(make_pair(string("+SHGR"), 90.f));
  ww_symbols.insert(make_pair(string("+SHGRRA"), 90.f));
  ww_symbols.insert(make_pair(string("+SHRAGR"), 90.f));
  ww_symbols.insert(make_pair(string("+SHGRRASN"), 90.f));
  ww_symbols.insert(make_pair(string("+SHGRSNRA"), 90.f));
  ww_symbols.insert(make_pair(string("+SHRASNGR"), 90.f));
  ww_symbols.insert(make_pair(string("+SHSNRAGR"), 90.f));
  ww_symbols.insert(make_pair(string("+SHRAGRSN"), 90.f));
  ww_symbols.insert(make_pair(string("+SHSNGRRA"), 90.f));

  ww_symbols.insert(make_pair(string("-TSRA"), 95.f));
  ww_symbols.insert(make_pair(string("-TSRASN"), 95.f));
  ww_symbols.insert(make_pair(string("-TSSNRA"), 95.f));
  ww_symbols.insert(make_pair(string("-TSSN"), 95.f));
  ww_symbols.insert(make_pair(string("TSRA"), 95.f));
  ww_symbols.insert(make_pair(string("TSRASN"), 95.f));
  ww_symbols.insert(make_pair(string("TSSNRA"), 95.f));
  ww_symbols.insert(make_pair(string("TSSN"), 95.f));

  ww_symbols.insert(make_pair(string("-TSGR"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGRRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGRSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAGR"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNGR"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGRRASN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGRSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRASNGR"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNRAGR"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAGRSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNGRRA"), 96.f));

  ww_symbols.insert(make_pair(string("TSGR"), 96.f));
  ww_symbols.insert(make_pair(string("TSGRRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSGRSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAGR"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNGR"), 96.f));
  ww_symbols.insert(make_pair(string("TSGRRASN"), 96.f));
  ww_symbols.insert(make_pair(string("TSGRSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSRASNGR"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNRAGR"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAGRSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNGRRA"), 96.f));

  ww_symbols.insert(make_pair(string("-TSGS"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGSRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGSSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAGS"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNGS"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGSRASN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSGSSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRASNGS"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNRAGS"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAGSSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNGSRA"), 96.f));

  ww_symbols.insert(make_pair(string("TSGS"), 96.f));
  ww_symbols.insert(make_pair(string("TSGSRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSGSSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAGS"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNGS"), 96.f));
  ww_symbols.insert(make_pair(string("TSGSRASN"), 96.f));
  ww_symbols.insert(make_pair(string("TSGSSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSRASNGS"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNRAGS"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAGSSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNGSRA"), 96.f));

  ww_symbols.insert(make_pair(string("-TSPL"), 96.f));
  ww_symbols.insert(make_pair(string("-TSPLRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSPLSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAPL"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNPL"), 96.f));
  ww_symbols.insert(make_pair(string("-TSPLRASN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSPLSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRASNPL"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNRAPL"), 96.f));
  ww_symbols.insert(make_pair(string("-TSRAPLSN"), 96.f));
  ww_symbols.insert(make_pair(string("-TSSNPLRA"), 96.f));

  ww_symbols.insert(make_pair(string("TSPL"), 96.f));
  ww_symbols.insert(make_pair(string("TSPLRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSPLSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAPL"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNPL"), 96.f));
  ww_symbols.insert(make_pair(string("TSPLRASN"), 96.f));
  ww_symbols.insert(make_pair(string("TSPLSNRA"), 96.f));
  ww_symbols.insert(make_pair(string("TSRASNPL"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNRAPL"), 96.f));
  ww_symbols.insert(make_pair(string("TSRAPLSN"), 96.f));
  ww_symbols.insert(make_pair(string("TSSNPLRA"), 96.f));

  ww_symbols.insert(make_pair(string("+TSRA"), 97.f));
  ww_symbols.insert(make_pair(string("+TSRASN"), 97.f));
  ww_symbols.insert(make_pair(string("+TSSNRA"), 97.f));
  ww_symbols.insert(make_pair(string("+TSSN"), 97.f));

  ww_symbols.insert(make_pair(string("+TSGR"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGRRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGRSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAGR"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNGR"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGRRASN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGRSNRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRASNGR"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNRAGR"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAGRSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNGRRA"), 99.f));

  ww_symbols.insert(make_pair(string("+TSGS"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGSRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGSSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAGS"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNGS"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGSRASN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSGSSNRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRASNGS"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNRAGS"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAGSSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNGSRA"), 99.f));

  ww_symbols.insert(make_pair(string("+TSPL"), 99.f));
  ww_symbols.insert(make_pair(string("+TSPLRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSPLSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAPL"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNPL"), 99.f));
  ww_symbols.insert(make_pair(string("+TSPLRASN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSPLSNRA"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRASNPL"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNRAPL"), 99.f));
  ww_symbols.insert(make_pair(string("+TSRAPLSN"), 99.f));
  ww_symbols.insert(make_pair(string("+TSSNPLRA"), 99.f));

  // Markon lisäykset listaan
  ww_symbols.insert(make_pair(string("VCSH"), 16.f));  // 16 = Precipitation within sight, reaching
                                                       // the ground or the surface of the sea, near
                                                       // to, but not at the station
  ww_symbols.insert(make_pair(string("-BR"), 10.f));   // 10 = Mist
  ww_symbols.insert(make_pair(string("VCBLDU"), 7.f));
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the weather part of the METAR
 */
// ----------------------------------------------------------------------

static bool FindWeatherValue(const string &theWord,
                             map<string, float> &ww_symbols,
                             float &value,
                             bool fDoLowerBoundSearch)
{
  if (fDoLowerBoundSearch)
  {
    // Etsitään etsityn sanan lower_bound -kohta, jos sana löytyi suoraan, käytetään sen arvoa.
    // Jos ei löytynyt, katsotaan lower_bound-kohdasta löytyneellä sanalla että löytyykö se haetusta
    // sanasta edes osana.
    // Tämä siksi että joskus metareissa on jotain viilauksia ja standardi sanojen perässä voi olla
    // jotain maa kohtaisia virityksiä.
    map<string, float>::iterator lb = ww_symbols.lower_bound(theWord);
    if (lb != ww_symbols.end() && !(ww_symbols.key_comp()(theWord, lb->first)))
    {
      // avain löytyi heti, otetaan sen arvo käyttöön
      value = (*lb).second;
      return true;
    }
    else
    {
      // lower_bound palauttaa iteraattorin seuraavaan, en tiedä miten saisin suoraan edellisen
      // kohdan map:ista, joten --
      // Myös jos on etsitty sanaa joka on viimeisenä mapissa, se on end-kohdassa.
      --lb;
      if (lb != ww_symbols.end())
      {
        string::size_type pos = theWord.find((*lb).first);
        if (pos != string::npos)
        {
          value = (*lb).second;
          return true;
        }
      }
    }
  }
  else
  {
    map<string, float>::iterator it = ww_symbols.find(theWord);
    if (it != ww_symbols.end())
    {
      value = (*it).second;
      return true;
    }
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract weather section of the METAR
 */
// ----------------------------------------------------------------------

static void FillMetarDataWeatherSection2(MetarData &data,
                                         map<string, float> &ww_symbols,
                                         const string &theWWStr,
                                         size_t theParamVectorIndex,
                                         const string &theMetarStr,
                                         const string &theMetarFileName,
                                         const string &theFieldName)
{
  if (theWWStr.empty() == false)
  {
    float value = kFloatMissing;
    if (::FindWeatherValue(theWWStr, ww_symbols, value, true))
      data.itsValues[theParamVectorIndex] = value;
    else
    {
      if (fVerboseMode)
      {
        cerr << "Warning: " << theFieldName.c_str()
             << " field was given but there is no synop code conversion for it: '"
             << theWWStr.c_str() << "' from METAR:\n";
        cerr << theMetarStr.c_str();
        cerr << "\nFrom file: " << theMetarFileName.c_str();
        cerr << "\n, ignoring field and continuing..." << endl;
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract weather sections of the METAR
 */
// ----------------------------------------------------------------------

static void FillMetarDataWeatherSection(MetarData &data,
                                        const Decoded_METAR &metarStruct,
                                        const string &theMetarStr,
                                        const string &theMetarFileName)
{
  static map<string, float> ww_symbols;
  static bool ww_symbols_initialized = false;
  if (ww_symbols_initialized == false)
  {
    ww_symbols_initialized = true;
    ::InitWWSymbols(ww_symbols);
  }

  ::FillMetarDataWeatherSection2(data,
                                 ww_symbols,
                                 metarStruct.WxObstruct[0],
                                 data.itsWW1Index,
                                 theMetarStr,
                                 theMetarFileName,
                                 "w'w'1");
  ::FillMetarDataWeatherSection2(data,
                                 ww_symbols,
                                 metarStruct.WxObstruct[1],
                                 data.itsWW2Index,
                                 theMetarStr,
                                 theMetarFileName,
                                 "w'w'2");
  ::FillMetarDataWeatherSection2(data,
                                 ww_symbols,
                                 metarStruct.WxObstruct[2],
                                 data.itsWW3Index,
                                 theMetarStr,
                                 theMetarFileName,
                                 "w'w'3");
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize conversion table for cloud symbols
 */
// ----------------------------------------------------------------------

static void InitCloudSymbols(map<string, float> &cloudCover_symbols,
                             map<string, float> &cloudType_symbols)
{
  cloudCover_symbols.clear();
  cloudType_symbols.clear();

  // Nämä METAR cloudtype/cover konversiot on saatu Viljo Kangasniemen dokumentista:
  // "METAReiden purkamisen kuvaus lyhyt versio.doc"
  cloudCover_symbols.insert(make_pair(string("SKC"), 0.f));
  cloudCover_symbols.insert(make_pair(string("CLR"), 0.f));
  cloudCover_symbols.insert(make_pair(string("FEW"), 1.f));
  cloudCover_symbols.insert(make_pair(string("SCT"), 3.f));
  cloudCover_symbols.insert(make_pair(string("BKN"), 6.f));
  cloudCover_symbols.insert(make_pair(string("OVC"), 8.f));

  cloudType_symbols.insert(make_pair(string("///"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("TCU"), 2.f));
  cloudType_symbols.insert(make_pair(string("CB"), 3.f));

  // lisätään listaan myös kaikki ei standardi metar pilvi tyypit ja laitetaan niissä pilvi tyyppi
  // puuttuvaksi
  cloudType_symbols.insert(make_pair(string("SC"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("ST"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("CU"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("AC"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("AS"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("NS"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("CI"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("CC"), kFloatMissing));
  cloudType_symbols.insert(make_pair(string("CS"), kFloatMissing));
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract cloud information from a METAR
 */
// ----------------------------------------------------------------------

static void FillMetarDataCloudSection2(MetarData &data,
                                       map<string, float> &cloudCover_symbols,
                                       map<string, float> &cloudType_symbols,
                                       const Cloud_Conditions &cloud_Conditions,
                                       int clCoverIndex,
                                       int clBaseIndex,
                                       int clTypeIndex,
                                       const string &theMetarStr,
                                       const string &theMetarFileName,
                                       const string &theFieldName)
{
  string cloudCoverStr(cloud_Conditions.cloud_type);  // cloud_type on oikeasti cloud cover
  NFmiStringTools::TrimAll(
      cloudCoverStr);  // joskus stringissä on rivin vaihtoja tms, siivotaan se ensin
  if (cloudCoverStr.empty() == false &&
      cloudCoverStr !=
          "VV")  // VV eli vertical visibilty ignoorataan tässä, koska se hoidetaan toisaalla
  {
    float value = kFloatMissing;
    if (::FindWeatherValue(cloudCoverStr, cloudCover_symbols, value, true))
      data.itsValues[clCoverIndex] = value;
    else
    {
      if (fVerboseMode)
      {
        cerr << "Warning: " << theFieldName.c_str()
             << " field was given but there was unkonwn code in it: '" << cloudCoverStr.c_str()
             << "' from METAR:\n";
        cerr << theMetarStr.c_str();
        cerr << "\nFrom file: " << theMetarFileName.c_str();
        cerr << "\n, ignoring it and continuing..." << endl;
      }
    }
  }

  string cloudTypeStr(
      cloud_Conditions.other_cld_phenom);  // other_cld_phenom on oikeasti cloud type (CB tai TCU)
  NFmiStringTools::TrimAll(
      cloudTypeStr);  // joskus stringissä on rivin vaihtoja tms, siivotaan se ensin
  if (cloudTypeStr.empty() == false)
  {
    float value = kFloatMissing;
    if (::FindWeatherValue(cloudTypeStr, cloudType_symbols, value, true))
      data.itsValues[clTypeIndex] = value;
    else
    {
      if (fVerboseMode)
      {
        cerr << "Warning: " << theFieldName.c_str()
             << " field was given but there was unkonwn code in it: '" << cloudTypeStr.c_str()
             << "' from METAR:\n";
        cerr << theMetarStr.c_str();
        cerr << "\nFrom file: " << theMetarFileName.c_str();
        cerr << "\n, ignoring it and continuing..." << endl;
      }
    }
  }

  if (cloud_Conditions.cloud_hgt_meters != MDSP_missing_int)
    data.itsValues[clBaseIndex] = static_cast<float>(cloud_Conditions.cloud_hgt_meters);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract cloud information sections from a METAR
 */
// ----------------------------------------------------------------------

static void FillMetarDataCloudSection(MetarData &data,
                                      const Decoded_METAR &metarStruct,
                                      const string &theMetarStr,
                                      const string &theMetarFileName)
{
  static map<string, float> cloudCover_symbols;
  static map<string, float> cloudType_symbols;
  static bool cloud_symbols_initialized = false;
  if (cloud_symbols_initialized == false)
  {
    cloud_symbols_initialized = true;
    ::InitCloudSymbols(cloudCover_symbols, cloudType_symbols);
  }

  ::FillMetarDataCloudSection2(data,
                               cloudCover_symbols,
                               cloudType_symbols,
                               metarStruct.cloudGroup[0],
                               data.itsClCover1Index,
                               data.itsClBase1Index,
                               data.itsClType1Index,
                               theMetarStr,
                               theMetarFileName,
                               "cloud1");
  ::FillMetarDataCloudSection2(data,
                               cloudCover_symbols,
                               cloudType_symbols,
                               metarStruct.cloudGroup[1],
                               data.itsClCover2Index,
                               data.itsClBase2Index,
                               data.itsClType2Index,
                               theMetarStr,
                               theMetarFileName,
                               "cloud2");
  ::FillMetarDataCloudSection2(data,
                               cloudCover_symbols,
                               cloudType_symbols,
                               metarStruct.cloudGroup[2],
                               data.itsClCover3Index,
                               data.itsClBase3Index,
                               data.itsClType3Index,
                               theMetarStr,
                               theMetarFileName,
                               "cloud3");
  ::FillMetarDataCloudSection2(data,
                               cloudCover_symbols,
                               cloudType_symbols,
                               metarStruct.cloudGroup[3],
                               data.itsClCover4Index,
                               data.itsClBase4Index,
                               data.itsClType4Index,
                               theMetarStr,
                               theMetarFileName,
                               "cloud4");
}

// ----------------------------------------------------------------------
/*!
 * \brief Decode a METAR
 */
// ----------------------------------------------------------------------

static void DecodeMetar(NFmiAviationStationInfoSystem &theStationInfoSystem,
                        const string &theMetarStr,
                        vector<MetarData> &dataBlocks,
                        const string &theMetarFileName,
                        const NFmiMetTime &theHeaderTime,
                        int theTimeRoundingResolution,
                        std::set<std::string> &theIcaoIdUnknownSetOut)
{
  if (::IsMetarNilOrEmptyReport(theMetarStr))  // DcdMETAR-tarkastelu ei jostain syystä osaa kertoa
                                               // onko kyseessä NIL sanoma, joten teen sellaisen
                                               // tarkastelun ensin
    return;

  Decoded_METAR metarStruct;
  if (decode_metar(const_cast<char *>(theMetarStr.c_str()), &metarStruct) ==
      0)  // DcdMETAR palauttaa 0:n jos ok
  {
    string icaoStr = metarStruct.stnid;
    NFmiAviationStation *aviationStation = theStationInfoSystem.FindStation(icaoStr);
    if (aviationStation)
    {
      int intValue = MDSP_missing_int;
      float floatValue = MDSP_missing_float;
      MetarData data;
      data.itsStationId = aviationStation->GetIdent();  // laitetaan wmo-id talteen että löydetään
                                                        // myöhemmin tämä asema querydatasta
      data.itsIcaoName = icaoStr;
      data.itsTime = ::GetTime2(metarStruct, theHeaderTime, theTimeRoundingResolution);

      intValue = metarStruct.temp;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsTemperatureIndex] = static_cast<float>(intValue);
      intValue = metarStruct.dew_pt_temp;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsDewpointIndex] = static_cast<float>(intValue);
      intValue = metarStruct.winData.windSpeed;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsWSIndex] = 1852.f * intValue / 3600.0f;
      intValue = metarStruct.winData.windDir;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsWDIndex] = static_cast<float>(intValue);
      intValue = metarStruct.winData.windGust;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsWGustIndex] = 1852.f * intValue / 3600.0f;
      intValue = metarStruct.hectoPasc_altstng;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsPressureIndex] = static_cast<float>(intValue);
      floatValue = metarStruct.prevail_vsbyM;
      if (floatValue != MDSP_missing_float) data.itsValues[data.itsVisibilityIndex] = floatValue;

      intValue = metarStruct.VertVsby;
      if (intValue != MDSP_missing_int)
        data.itsValues[data.itsVerticalVisibilityIndex] = static_cast<float>(intValue);

      ::FillMetarDataWeatherSection(data, metarStruct, theMetarStr, theMetarFileName);
      ::FillMetarDataCloudSection(data, metarStruct, theMetarStr, theMetarFileName);
      data.fIsCorrected = metarStruct.COR == TRUE;

      dataBlocks.push_back(data);  // lopuksi laitetaan datablokki listaan

      bool debugLogCode = false;
      NFmiMetTime atime;
      //			if(debudLogCode && data.itsStationId == 40738)
      if (debugLogCode && data.itsTime > atime)
        cerr << "\"" << theMetarStr << "\" In file: " << theMetarFileName << endl;
    }
    else
    {
      theIcaoIdUnknownSetOut.insert(icaoStr);
    }
  }
  else  // DcdMETAR-funktio failasi
  {
    if (fVerboseMode)
      cerr << "Error in METAR: \"" << theMetarStr << "\" In file: " << theMetarFileName
           << ", Continuing execution..." << endl;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Check for an old style header
 *
 * jos sana on 10 merkkiä pitkä numeerinen arvo, on kyseessä vanhan viesti koneen
 * tekemä headerin alku osa
 */
// ----------------------------------------------------------------------

static bool CheckIsOldMessageMachineStartWord(const string &theStr)
{
  bool retValue = false;
  try
  {
    if (theStr.size() == 10)
    {
      double tmpValue = NFmiStringTools::Convert<double>(theStr);
      tmpValue++;  // tämä poistaa varoituksen että muuttujaa ei käytetä (Convert:in ainoa käyttö
                   // edellä oli tarkistaa
      // että stringi muuttuu luvuksi)
      retValue = true;
    }
  }
  catch (...)
  {
    // ei muuttunut luvuksi, ei ollut pelkästään numeerinen luku
  }
  return retValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Check header status
 *
 * jos eri metari sanomia on pakattu samaan tiedostoon ja tiedetään
 * miten headeri osio alkaa, tässä tarkistetaan sitä
 */
// ----------------------------------------------------------------------

static bool DoNewHeaderStartHere(const string &theLineStr)
{
  boost::regex reg(
      "\\s+");  // splittaa yksittäiset sanat, oli niissä yksi tai useampia spaceja välissä
  boost::sregex_token_iterator it(theLineStr.begin(), theLineStr.end(), reg, -1);
  boost::sregex_token_iterator end;

  string tmpStr = *it;
  bool isHeaderStartWord = ::CheckIsOldMessageMachineStartWord(tmpStr);
  return isHeaderStartWord;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove extra control characters from the message
 */
// ----------------------------------------------------------------------

static std::string RemoveControlCharacters(const std::string &theOrigStr)
{
  std::string strippedStr;
  for (size_t i = 0; i < theOrigStr.size(); i++)
  {
    unsigned char ch = (unsigned char)(theOrigStr[i]);
    if (isspace(ch) || iscntrl(ch) == 0) strippedStr += theOrigStr[i];
  }
  return strippedStr;
}

// ----------------------------------------------------------------------
/*!
 * \brief METAR keywords
 */
// ----------------------------------------------------------------------

const std::string gMetarWord = "METAR";
const std::string gSpeciWord = "SPECI";
const std::string gNilWord = "NIL";

const boost::regex gNoaaTimeLineReg(
    "\\d{4}/\\d{2}/\\d{2} \\d{2}:\\d{2}");  // esim.     2013/08/09 02:45

static bool IsMetarFileInNoaaFormat(std::ifstream &input)
{
  const int maxLineCheckCount = 10;
  std::string line;
  for (int i = 0; i < maxLineCheckCount; i++)
  {
    std::getline(input, line);
    if (boost::regex_match(line, gNoaaTimeLineReg)) return true;
  }
  return false;
}

static NFmiMetTime GetNoaaTime(std::string &timeStr)
{
  std::vector<std::string> to_vec;
  boost::split(to_vec, timeStr, boost::is_any_of("/ :"));
  if (to_vec.size() >= 5)
  {
    short year = boost::lexical_cast<short>(to_vec[0]);
    short month = boost::lexical_cast<short>(to_vec[1]);
    short day = boost::lexical_cast<short>(to_vec[2]);
    short hour = boost::lexical_cast<short>(to_vec[3]);
    short minute = boost::lexical_cast<short>(to_vec[4]);
    return NFmiMetTime(year, month, day, hour, minute);
  }
  else
    throw std::runtime_error(
        std::string("Error in GetNoaaTime, string was suppose to hold NOAA format time: ") +
        timeStr);
}

static NFmiMetTime GetNoaaFormatTimeLine(std::ifstream &input)
{
  std::string line;
  do
  {
    std::getline(input, line);
    if (boost::regex_match(line, gNoaaTimeLineReg))
    {
      return ::GetNoaaTime(line);
    }

  } while (input);
  return NFmiMetTime::gMissingTime;
}

static bool DoNoaaFormatRead(NFmiAviationStationInfoSystem &theStationInfoSystem,
                             vector<MetarData> &dataBlocks,
                             const string &theMetarFileName,
                             int theTimeRoundingResolution,
                             std::set<std::string> &theIcaoIdUnknownSetOut)
{
  std::ifstream input(theMetarFileName);
  if (::IsMetarFileInNoaaFormat(input))
  {
    size_t oldDataBlockSize = dataBlocks.size();
    input.seekg(std::ios_base::beg);  // aloitetaan tarkastelujen jälkeen luku taas alusta
    std::string metarLine;
    do
    {
      NFmiMetTime fullTime = ::GetNoaaFormatTimeLine(input);
      if (input && fullTime != NFmiMetTime::gMissingTime)
      {
        do
        {  // hetaan sellaista riviä, missä on muutakin kuin white spaceja, ja sen oletetaan olevan
           // metar stringi
          std::getline(input, metarLine);
          metarLine = NFmiStringTools::TrimAll(metarLine);
        } while (metarLine.size() == 0);
        ::DecodeMetar(theStationInfoSystem,
                      metarLine,
                      dataBlocks,
                      theMetarFileName,
                      fullTime,
                      theTimeRoundingResolution,
                      theIcaoIdUnknownSetOut);
      }
    } while (input);
    return oldDataBlockSize < dataBlocks.size();  // jos löytyi ainakin yksi metar, palautetaan true
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract METAR messages into a vector
 */
// ----------------------------------------------------------------------

static void MakeDataBlocks(NFmiAviationStationInfoSystem &theStationInfoSystem,
                           const string &theMetarFileStr,
                           vector<MetarData> &dataBlocks,
                           const string &theMetarFileName,
                           int theTimeRoundingResolution,
                           std::set<std::string> &theIcaoIdUnknownSetOut)
{
  // käydään data läpi rivi kerrallaan
  boost::regex reg("=");  // METARit on eroteltu =-merkillä
  boost::sregex_token_iterator it(theMetarFileStr.begin(), theMetarFileStr.end(), reg, -1);
  boost::sregex_token_iterator end;
  std::string lineStr;
  int counter = 0;
  NFmiMetTime headerTime = missingTime;
  for (; it != end; ++it)
  {
    try
    {
      counter++;
      lineStr = *it;
      lineStr = ::RemoveControlCharacters(lineStr);
      NFmiStringTools::TrimAll(lineStr, true);
      if (lineStr.empty()) continue;

      bool newHeaderStarts = (counter == 1);
      if (newHeaderStarts == false && ::DoNewHeaderStartHere(lineStr)) newHeaderStarts = true;

      if (newHeaderStarts)  // siis 1. ja jos useita metar sanomia samassa paketissa,
      // headerin alussa sanoman kohdalla pitää lukea ohi header osio
      {
        boost::regex reg2(
            "\\s+");  // splittaa yksittäiset sanat, oli niissä yksi tai useampia spaceja välissä
        boost::sregex_token_iterator it2(lineStr.begin(), lineStr.end(), reg2, -1);
        boost::sregex_token_iterator end2;

        boost::regex expression("([0-9]{6})");  // etsitään 1. sana missä täsmälleen kuusi numeroa
                                                // eli metar-sanomien aikaleima
        string tmpStr = *it2;
        int headerWordCounter = 0;
        do
        {
          string tmpStr = *it2;
          ++it2;
          boost::cmatch what;
          if (boost::regex_match(tmpStr.c_str(), what, expression))
          {  // otetaan headerissa oleva aikaleima talteen, koska jossain metareissa ei ole omaa
             // aikaleimaa
            try
            {
              headerTime =
                  ::GetTime(tmpStr, lineStr, theMetarFileName, true, theTimeRoundingResolution);
            }
            catch (...)
            {
              // ei tehdä mitään
            }
          }
          headerWordCounter++;
          NFmiStringTools::UpperCase(tmpStr);
          if (tmpStr == gMetarWord)
            break;  // aloitetaan metar sanomien purkaminen, 1. metarissa on aina mukana myös
                    // headeri osa, joka loppuu METAR sanaan
          if (tmpStr == gSpeciWord)
            return;  // Mutta ei vielä toistaiseksi oteta huomioon SPECI sanomia
          if (tmpStr == gNilWord)
            break;  // Jos NIL tulee ennen METAR/SPECI:ä, lopetetaan kanssa, kyseessä tyhjä
                    // sanomatiedosto
        } while (it2 != end2);

        if (it2 == end2)
          continue;  // joskus on virheellisiä sanoma tiedostoja, eikä METAR/SPECI/NIL sanoja löydy,
                     // ja tässä pitää silloin breakata

        // jokaisen tiedoston 1. metar pitää rakentaa tässä erikseen, koska alusta pitää skipata
        // header osio
        // loput metarit tulevat splittauksesta sellaisenaan.
        string tmpMetarStr;
        do
        {
          tmpMetarStr += *it2;
          tmpMetarStr += " ";
          ::GetStringAndAdvance(it2, false);
        } while (it2 != end2);

        lineStr = tmpMetarStr;
      }
      std::string::size_type pos = lineStr.find(gMetarWord);
      if (pos != std::string::npos)
        lineStr =
            std::string(lineStr.begin() + pos,
                        lineStr.end());  // pitää poistaa vielä mahdollisia turhia header osuuksia

      lineStr = NFmiStringTools::TrimAll(lineStr, true);

      ::DecodeMetar(theStationInfoSystem,
                    lineStr,
                    dataBlocks,
                    theMetarFileName,
                    headerTime,
                    theTimeRoundingResolution,
                    theIcaoIdUnknownSetOut);
    }  // end-of-for
    catch (exception &e)
    {
      if (fVerboseMode)
        cerr << "Error in METAR nr. " << counter << ": " << lineStr
             << " In file: " << theMetarFileName << " With error: " << e.what()
             << ", Continuing execution..." << endl;
    }
  }  // end-of-for-loop
}

// ----------------------------------------------------------------------
/*!
 * \brief Collect METAR files with matching patterns
 */
// ----------------------------------------------------------------------

list<string> CollectMetarFiles(const vector<string> &fileFilterList)
{
  list<string> files;

  for (unsigned int j = 0; j < fileFilterList.size(); j++)
  {
    if (fVerboseMode)
    {
      cerr << "Scanning pattern " << fileFilterList[j] << " ... ";
      cerr.flush();
    }

    list<string> fileList = NFmiFileSystem::PatternFiles(fileFilterList[j]);

    if (fVerboseMode) cerr << " found " << fileList.size() << " files" << endl;

    // Extract path from the pattern

    NFmiFileString fileString(fileFilterList[j]);
    NFmiString wantedPathTmp(fileString.Device());
    wantedPathTmp += fileString.Path();
    string wantedPath(wantedPathTmp.CharPtr());

    // Add found files with path into output list

    BOOST_FOREACH (const string &f, fileList)
    {
      string metarFileName = wantedPath + f;
      files.push_back(metarFileName);
    }
  }

  return files;
}

// ----------------------------------------------------------------------
/*!
 * \brief Sort METAR files into processing order
 *
 * Sometimes new messages override older ones, so processing order
 * is important. Unfortunately the time stamp conventions for the
 * messages vary, and the message itself contains incomplete information.
 *
 * Current solution: sort based on file modification time, from oldest to newest.
 *
 */
// ----------------------------------------------------------------------

list<string> SortMetarFiles(const list<string> &metarfiles)
{
  typedef multimap<time_t, string> SortedFiles;
  SortedFiles sortedfiles;

  BOOST_FOREACH (const string &filename, metarfiles)
  {
    std::time_t t = boost::filesystem::last_write_time(filename);
    sortedfiles.insert(make_pair(t, filename));
  }

  list<string> outfiles;
  BOOST_FOREACH (const SortedFiles::value_type &vt, sortedfiles)
    outfiles.push_back(vt.second);

  return outfiles;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without error catching
 */
// ----------------------------------------------------------------------

void run(int argc, const char *argv[])
{
  // HUOM!! VC++ 2012 (Update 3) -versiolla x64-debug versio toimii debuggerissa ihan oudosti,
  // ohjelman steppaus ei mene oikein (win32 debug käyttäytyy oikein).
  // Ohjelma tuottaa kuitenkin oikean tuloksen kaikilla kombinaatioilla win32/x64 + debug/release
  NFmiCmdLine cmdline(argc, argv, "s!vFr!n");

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    ::Usage(argv[0]);
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  int numOfParams = cmdline.NumberofParameters();
  if (numOfParams < 1)
  {
    cerr << "Error: Atleast 1 parameter expected, 'fileFilter1[,fileFilter2,...]'\n\n";
    ::Usage(argv[0]);
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.isOption('v')) fVerboseMode = true;

  if (cmdline.isOption('F')) fIgnoreBadStations = true;

#if 0
  bool icaoFirst = false;
  if(cmdline.isOption('i'))
	icaoFirst = true;
#endif

  bool tryNoaaFileFormat = false;
  if (cmdline.isOption('n')) tryNoaaFileFormat = true;

  NFmiAviationStationInfoSystem stationInfoSystem(false, fVerboseMode);

#ifdef UNIX
  std::string stationFile = "/usr/share/smartmet/stations.csv";
#else
  std::string stationFile = "";
#endif

  if (cmdline.isOption('s')) stationFile = cmdline.OptionValue('s');

  if (stationFile.empty())
    throw runtime_error(
        "Error: -s option (station info file) is not really an option, you must provide it (check "
        "http://weather.noaa.gov/data/nsd_bbsss.txt), stopping program...");

  stationInfoSystem.InitFromMasterTableCsv(stationFile);
  if (stationInfoSystem.InitLogMessage().empty() == false)
  {
    cerr << "\nFollowing warning came from initializing station info:" << endl;
    cerr << stationInfoSystem.InitLogMessage() << endl;
  }

  NFmiParamDescriptor params;
  ::MakeDefaultParamDesc(params);

  int timeRoundingResolution = 30;  // pyöristys arvo minuuteissa
  if (cmdline.isOption('r'))
    timeRoundingResolution = NFmiStringTools::Convert<int>(cmdline.OptionValue('r'));

  //	1. Lue n kpl filefiltereitä listaan
  vector<string> fileFilterList;
  for (int i = 1; i <= numOfParams; i++)
  {
    fileFilterList.push_back(cmdline.Parameter(i));
  }

  // Collect files and sort them into processing order

  list<string> metarfiles = CollectMetarFiles(fileFilterList);

  metarfiles = SortMetarFiles(metarfiles);

  // Process them

  std::set<std::string> icaoIdUnknownSet;  // tähän kerätään kaikki tuntemattomat icao-id:t jotka
                                           // ovat tulleet metar-sanomista
  vector<MetarData> dataBlocks;

  int debugCounter = 0;
  BOOST_FOREACH (const string &filename, metarfiles)
  {
    if (fVerboseMode)
      std::cerr << "Processing file no: " << ++debugCounter << " (" << filename << ")" << std::endl;
    if (tryNoaaFileFormat)
    {
      if (::DoNoaaFormatRead(
              stationInfoSystem, dataBlocks, filename, timeRoundingResolution, icaoIdUnknownSet))
        continue;  // jos tiedosto oli NOAA formaattia, se luettiin jo, mennään seuraavaan
                   // tiedostoon
    }
    string metarFileContent;
    if (NFmiFileSystem::ReadFile2String(filename, metarFileContent) == false)
      cerr << "Failed to read file: " << filename.c_str() << endl
           << "Continuing with other files..." << endl;
    else
    {
      ::MakeDataBlocks(stationInfoSystem,
                       metarFileContent,
                       dataBlocks,
                       filename,
                       timeRoundingResolution,
                       icaoIdUnknownSet);
    }
  }

  // Build querydata from the contents

  NFmiQueryData *newQData = ::MakeQueryDataFromBlocks(params, stationInfoSystem, dataBlocks);
  auto_ptr<NFmiQueryData> newQDataPtr(newQData);
  if (newQData == 0)
    throw runtime_error("Error: Unable to create querydata from METAR data, stopping program...");

  // tehdään dataan vielä totalwind parametri WS, WD ja WGustin avulla
  NFmiFastQueryInfo tempInfo(newQData);
  NFmiQueryData *newQDataWithTotalWind = NFmiQueryDataUtil::MakeCombineParams(
      tempInfo, 7, false, true, false, kFmiWindGust, std::vector<int>(), false, 0, false, false);
  if (newQDataWithTotalWind == 0)
    throw runtime_error(
        "Error: Unable to create querydata with totalWind-parameter, stopping program...");

  cerr << "\nStoring data to file." << endl;
  NFmiStreamQueryData sQOutData(newQDataWithTotalWind);  // tämä myös tuhoaa qdatan
  if (!sQOutData.WriteCout())
    throw runtime_error("Error: Couldn't write combined qdata to stdout.");

  if (fVerboseMode && icaoIdUnknownSet.size())
  {
    cerr << "\nWarning, there were " << icaoIdUnknownSet.size()
         << " unknown ICAO id's in given messages that were ignored:" << endl;
    for (std::set<std::string>::iterator it = icaoIdUnknownSet.begin();
         it != icaoIdUnknownSet.end();
         ++it)
    {
      cerr << *it << " ";
    }
    cerr << std::endl << std::endl;
  }

  cerr << "Success, end of execution." << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program with error catching
 */
// ----------------------------------------------------------------------

int main(int argc, const char *argv[]) try
{
  run(argc, argv);
  return 0;
}
catch (exception &e)
{
  cerr << e.what() << endl;
  return 1;
}
