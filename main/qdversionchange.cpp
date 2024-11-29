/*!
 *  \file QDVersionFilter.cpp
 *  (18.2.2002/Marko)
 *  Tekee tietokantahaulla tehdystä fqd-tiedostosta Meteorologin editoriin
 *  soveltuvan datatiedoston (SQD), jossa on koottuja parametreja totalwind
 *  ja weatherandcloudiness. \par
 *
 *  UusiFiltteri saa stdin:istä lähtödatan ja tulostaa stdout:iin tulosdatan.
 *  Ohjelma se tutkii lähtödatan ja konvertoi sen sopivasti niin, että tulos-
 *  datassa on yhdistelmä parametrit mukana ja toisaalta siitä on poistettu
 *  lähtödatassa olevat näiden 'rakennusparametrit'. Paitsi jos halutaan,
 *  että 328=kFmiCloudSymbol jätetään, annetaan toisena parametrina arvo 1(true).
 *  Oletus arvolla 0(false) tämäkin poistetaan tulos parametrilistasta. \par
 *
 *  Jos lähtö datassa on esim. parametrit T, P, WD, WS, tulosdatassa on T, P ja
 *  totalwind parametrit. WD ja WS parametreja on käytetty rakentamaan totalwind
 *  parametria ja ne on sitten poistettu lopullisesta datasta. \par
 *
 *  Muutoksia vanhaan versioon: \par
 *  \li Enää ei tarvitse antaa tulosdatan parambagia ohjelman argumenttina,
 *  koska ohjelma analysoi datan itse.
 *  \li UusiFiltteri-ohjelma toimii putkena, joten sille annetaan lähde
 *  data stdin:iin ja tulosdata menee stdout:iin. Eli esim. komento rivillä: \par
 *  \code
 *  type sourceData.fqd | UusiFiltteri.exe 6 > resultData.sqd
 *  \endcode
 *  \li Ohjelmalle voi antaa fqd infoversion 1. argumenttina (oletus 7)
 *  \li Ohjelmalle voi antaa tiedon pidetäänkö kFmiCloudSymbol-parametir mukana (oletus ei)
 */

// Usage: type inputTiedosto | QDVersionFilter.exe [infoversion=7] [keepCloudSymbol=0=false] >
// outputTiedosto

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiMilliSecondTimer.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiValueString.h>

#include <stdexcept>

using namespace std;

void Usage(void);
void run(int argc, const char *argv[]);
int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption);

int main(int argc, const char *argv[])
{
  try
  {
    run(argc, argv);
  }
  catch (exception &e)
  {
    cerr << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

static std::vector<int> GetOptionalParamIdList(const NFmiCmdLine &cmdline, char option)
{
  std::vector<int> paramIdVector;
  if (cmdline.isOption(option))
  {
    std::string precipFromIdListStr = cmdline.OptionValue(option);
    try
    {
      paramIdVector = NFmiStringTools::Split<std::vector<int> >(precipFromIdListStr);
    }
    catch (...)
    {
      std::string errorString("-");
      errorString += option;
      errorString +=
          " was not correctly formatted, there should be an integer or list\nof integers "
          "separated by commas, e.g.\"123\" or \"123,-28\", given option was:\n";
      errorString += precipFromIdListStr;
      throw std::runtime_error(errorString);
    }
  }
  return paramIdVector;
}

// Transform N octas to %:
// one octa is 12.5 % (result will be rounded *later* to nearest 10 %, due to WeatherAndCloudiness
// limitation)
//    E.g. 0 -> 0 %, 4 -> 50 %, 8 -> 100 %
//    9 -> 100 % (9 = can't observe cloudines due e.g. fog, how should we really put this, missing?
//    ) all other values -> missing
static float convertOctaToProcent(float octas)
{
  if (octas >= 0 && octas <= 8)
    return octas * 12.5f;
  else if (octas == 9)
    return 100.f;
  else
    return kFloatMissing;
}

static void ConvertNfromOctasToProcent(std::unique_ptr<NFmiQueryData> &data,
                                       NFmiFastQueryInfo &sourceInfo)
{
  NFmiFastQueryInfo info(data.get());
  if (info.Param(kFmiTotalCloudCover) && sourceInfo.Param(kFmiTotalCloudCover))
  {
    for (info.ResetLocation(), sourceInfo.ResetLocation();
         info.NextLocation() && sourceInfo.NextLocation();)
    {
      for (info.ResetLevel(), sourceInfo.ResetLevel(); info.NextLevel() && sourceInfo.NextLevel();)
      {
        for (info.ResetTime(), sourceInfo.ResetTime(); info.NextTime() && sourceInfo.NextTime();)
        {
          info.FloatValue(::convertOctaToProcent(sourceInfo.FloatValue()));
        }
      }
    }
  }
}

void run(int argc, const char *argv[])
{
  string inputfile = "-";
  bool doTotalWind = true;
  bool doWeatherAndCloudiness = true;
  double infoVersion = 7.;
  bool keepCloudSymbolParameter = false;
  bool buildTimeBag = false;
  bool allowLessParamsWhenCreatingWeather = false;  // halutessa weatherAndCloudiness parametri
                                                    // voidaan rakentaa pelkästään rr ja N
                                                    // parametrien avulla
  FmiParameterName windGustParId =
      kFmiHourlyMaximumGust;  // täytä totalWind-parametrin windGust osio tällä parametrilla.
                              // Ota pois löytynyt windGust param päätason parametreista.

  int maxUsedThreadCount = 0;  // kuinko monta worker-threadia tekee töitä, < 1 -arvot tarkoittaa,
                               // että otetaan kaikki koneen threadit käyttöön
  bool convertNfromOctasToProcent = false;

  NFmiCmdLine cmdline(argc, argv, "t!w!g!ahi!m!pbf!F!P!N");
  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;

    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() > 2)
  {
    cerr << "Error: 0-2 parameter expected, '[qdversion] [keepCloudSymbol]'\n\n";
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.isOption('h'))
  {
    Usage();
    return;
  }

  if (cmdline.isOption('t'))
    doTotalWind = GetIntegerOptionValue(cmdline, 't') != 0;
  if (cmdline.isOption('w'))
    doWeatherAndCloudiness = GetIntegerOptionValue(cmdline, 'w') != 0;
  if (cmdline.isOption('g'))
    windGustParId = static_cast<FmiParameterName>(GetIntegerOptionValue(cmdline, 'g'));
  if (cmdline.isOption('a'))
    allowLessParamsWhenCreatingWeather = true;
  if (cmdline.isOption('i'))
    inputfile = cmdline.OptionValue('i');
  if (cmdline.isOption('m'))
    maxUsedThreadCount = static_cast<FmiParameterName>(GetIntegerOptionValue(cmdline, 'm'));
  if (cmdline.isOption('b'))
    buildTimeBag = true;

  bool doAccuratePrecip = false;
  if (cmdline.isOption('p'))
    doAccuratePrecip = true;
  if (cmdline.isOption('N'))
    convertNfromOctasToProcent = true;

  // -f optiolla voidaan antaa lista parId:tä, joita käytetään Weather-parametrin
  // precipForm -aliparametrin täyttämisessä.
  // Parametrit annetaan pilkulla eroteltuina ja ne ovat prioriteetti järjestyksessä. Jos 1. löytyy
  // arvo johonkin aikaan ja paikkaan, sitä käytetään, jos 1. arvo on puuttuvaa, käytetään 2. arvoa
  // jne. Jos parId on positiivinen, kyseinen parametri poistetaan tulosdatan päätason
  // parametrilistasta, jos se on negatiivinen, jätetään se sinne.
  std::vector<int> precipFormParIds = ::GetOptionalParamIdList(cmdline, 'f');
  std::vector<int> fogParIds = ::GetOptionalParamIdList(cmdline, 'F');
  std::vector<int> potParIds = ::GetOptionalParamIdList(cmdline, 'P');

  if (cmdline.NumberofParameters() >= 1)
    infoVersion = NFmiStringTools::Convert<float>(cmdline.Parameter(1));
  if (cmdline.NumberofParameters() >= 2)
    keepCloudSymbolParameter = NFmiStringTools::Convert<int>(cmdline.Parameter(2)) != 0;

  NFmiQueryData qd(inputfile);
  NFmiFastQueryInfo sourceInfo(&qd);

  std::unique_ptr<NFmiQueryData> uusiData(
      NFmiQueryDataUtil::MakeCombineParams(sourceInfo,
                                           infoVersion,
                                           keepCloudSymbolParameter,
                                           doTotalWind,
                                           doWeatherAndCloudiness,
                                           windGustParId,
                                           precipFormParIds,
                                           fogParIds,
                                           potParIds,
                                           allowLessParamsWhenCreatingWeather,
                                           maxUsedThreadCount,
                                           doAccuratePrecip,
                                           buildTimeBag));
  if (convertNfromOctasToProcent)
    ::ConvertNfromOctasToProcent(uusiData, sourceInfo);
  uusiData->Write();
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cout << "Usage: qdversionchange [options] [qdVersion=7 keepCloudSymbol=0] < inputdata" << endl
       << " > outputData" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-i <inputfile>\tSpecify input filename. Enables memory mapping of input" << endl
       << "\t-t <0|1>\tDo TotalWind combinationParam, default = 1" << endl
       << "\t-w <0|1>\tDo weatherAndCloudiness combinationParam, default = 1" << endl
       << "\t-h \tPrint help." << endl
       << "\t-g windGust-parId Add windGust param to totalWind using param" << endl
       << "\t\twith given parId." << endl
       << "\t-f precipForm-parId-list <id1[,id2,...]> Add precipForm param values to Weather using "
          "param(s)"
       << endl
       << "\t-F fog-parId-list <id1[,id2,...]> Add fog param values to Weather using param(s)"
       << endl
       << "\t-P pot-parId-list <id1[,id2,...]> Add pot param (probability of thunder) values "
          "to Weather using param(s)"
       << endl
       << "\t\twith given parId(s). If parId is positive, param will be removed from result data."
       << endl
       << "\t\tIf parId is negative, param will be left in result data." << endl
       << "\t-a \tAllow WeatherAndCloudinessParam to be created with minimum" << endl
       << "\t\tN and rr(1h|3h|6h) parameters." << endl
       << "\t-m <thread-count>\tMax used worker threads, default all" << endl
       << "\t-p  add accurate precip param and calcs snowFall param per 1h" << endl
       << "\t-b  build time bag instead of time list if possible (required by TAF editor)" << endl
       << "\t-N  Convert (observed) N parameter from octas to %, default = not" << endl
       << "\tExample usage: qdversionchange -t 1 -w 0 7 < input > output" << endl
       << endl;
}

int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption)
{
  NFmiValueString valStr(theCmdline.OptionValue(theOption));
  if (valStr.IsInt())
    return static_cast<int>(valStr);
  throw runtime_error(string("Error: '") + theOption +
                      "' option value must be integer, exiting...");
}
