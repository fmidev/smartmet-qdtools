/*!
 *  \file QDVersionFilter.cpp
 *  (18.2.2002/Marko)
 *  Tekee tietokantahaulla tehdyst‰ fqd-tiedostosta Meteorologin editoriin
 *  soveltuvan datatiedoston (SQD), jossa on koottuja parametreja totalwind
 *  ja weatherandcloudiness. \par
 *
 *  UusiFiltteri saa stdin:ist‰ l‰htˆdatan ja tulostaa stdout:iin tulosdatan.
 *  Ohjelma se tutkii l‰htˆdatan ja konvertoi sen sopivasti niin, ett‰ tulos-
 *  datassa on yhdistelm‰ parametrit mukana ja toisaalta siit‰ on poistettu
 *  l‰htˆdatassa olevat n‰iden 'rakennusparametrit'. Paitsi jos halutaan,
 *  ett‰ 328=kFmiCloudSymbol j‰tet‰‰n, annetaan toisena parametrina arvo 1(true).
 *  Oletus arvolla 0(false) t‰m‰kin poistetaan tulos parametrilistasta. \par
 *
 *  Jos l‰htˆ datassa on esim. parametrit T, P, WD, WS, tulosdatassa on T, P ja
 *  totalwind parametrit. WD ja WS parametreja on k‰ytetty rakentamaan totalwind
 *  parametria ja ne on sitten poistettu lopullisesta datasta. \par
 *
 *  Muutoksia vanhaan versioon: \par
 *  \li En‰‰ ei tarvitse antaa tulosdatan parambagia ohjelman argumenttina,
 *  koska ohjelma analysoi datan itse.
 *  \li UusiFiltteri-ohjelma toimii putkena, joten sille annetaan l‰hde
 *  data stdin:iin ja tulosdata menee stdout:iin. Eli esim. komento rivill‰: \par
 *  \code
 *  type sourceData.fqd | UusiFiltteri.exe 6 > resultData.sqd
 *  \endcode
 *  \li Ohjelmalle voi antaa fqd infoversion 1. argumenttina (oletus 7)
 *  \li Ohjelmalle voi antaa tiedon pidet‰‰nkˆ kFmiCloudSymbol-parametir mukana (oletus ei)
 */

// Usage: type inputTiedosto | QDVersionFilter.exe [infoversion=7] [keepCloudSymbol=0=false] >
// outputTiedosto

#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiValueString.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiMilliSecondTimer.h>

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

void run(int argc, const char *argv[])
{
  string inputfile = "-";
  bool doTotalWind = true;
  bool doWeatherAndCloudiness = true;
  double infoVersion = 7.;
  bool keepCloudSymbolParameter = false;
  bool buildTimeBag = false;
  bool allowLessParamsWhenCreatingWeather = false;  // halutessa weatherAndCloudiness parametri
                                                    // voidaan rakentaa pelk‰st‰‰n rr ja N
                                                    // parametrien avulla
  FmiParameterName windGustParId =
      kFmiHourlyMaximumGust;  // t‰yt‰ totalWind-parametrin windGust osio t‰ll‰ parametrilla.
                              // Ota pois lˆytynyt windGust param p‰‰tason parametreista.
  std::vector<int> precipFormParIds;  // -f optiolla voidaan antaa lista parId:t‰, joita k‰ytet‰‰n
  // Weather-parametrin precipForm -aliparametrin t‰ytt‰misess‰.
  // Parametrit annetaan pilkulla eroteltuina ja ne ovat prioriteetti j‰rjestyksess‰. Jos 1. lˆytyy
  // arvo johonkin aikaan
  // ja paikkaan, sit‰ k‰ytet‰‰n, jos 1. arvo on puuttuvaa, k‰ytet‰‰n 2. arvoa jne.
  // Jos parId on positiivinen, kyseinen parametri poistetaan tulosdatan p‰‰tason parametrilistasta,
  // jos se on negatiivinen, j‰tet‰‰n se sinne.
  int maxUsedThreadCount = 0;  // kuinko monta worker-threadia tekee tˆit‰, < 1 -arvot tarkoittaa,
                               // ett‰ otetaan kaikki koneen threadit k‰yttˆˆn

  NFmiCmdLine cmdline(argc, argv, "t!w!g!ahi!m!pbf!");
  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;

    Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() > 2)
  {
    cerr << "Error: 0-2 parameter expected, '[qdversion] [keepCloudSymbol]'\n\n";
    Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.isOption('h'))
  {
    Usage();
    return;
  }

  if (cmdline.isOption('t')) doTotalWind = GetIntegerOptionValue(cmdline, 't') != 0;
  if (cmdline.isOption('w')) doWeatherAndCloudiness = GetIntegerOptionValue(cmdline, 'w') != 0;
  if (cmdline.isOption('g'))
    windGustParId = static_cast<FmiParameterName>(GetIntegerOptionValue(cmdline, 'g'));
  if (cmdline.isOption('a')) allowLessParamsWhenCreatingWeather = true;
  if (cmdline.isOption('i')) inputfile = cmdline.OptionValue('i');
  if (cmdline.isOption('m'))
    maxUsedThreadCount = static_cast<FmiParameterName>(GetIntegerOptionValue(cmdline, 'm'));
  if (cmdline.isOption('b')) buildTimeBag = true;

  bool doAccuratePrecip = false;
  if (cmdline.isOption('p')) doAccuratePrecip = true;
  if (cmdline.isOption('f'))
  {
    std::string precipFromIdListStr = cmdline.OptionValue('f');
    try
    {
      precipFormParIds = NFmiStringTools::Split<std::vector<int> >(precipFromIdListStr);
    }
    catch (...)
    {
      throw std::runtime_error(
          "-f option was not correctly formatted, there should be an integer or list\nof integers "
          "separated by commas, e.g.\"123\" or \"123,-28\", given option was:\n" +
          precipFromIdListStr);
    }
  }

  if (cmdline.NumberofParameters() >= 1)
    infoVersion = NFmiStringTools::Convert<float>(cmdline.Parameter(1));
  if (cmdline.NumberofParameters() >= 2)
    keepCloudSymbolParameter = NFmiStringTools::Convert<int>(cmdline.Parameter(2)) != 0;

  NFmiQueryData qd(inputfile);
  NFmiFastQueryInfo sourceInfo(&qd);

  NFmiQueryData *uusiData = NFmiQueryDataUtil::MakeCombineParams(sourceInfo,
                                                                 infoVersion,
                                                                 keepCloudSymbolParameter,
                                                                 doTotalWind,
                                                                 doWeatherAndCloudiness,
                                                                 windGustParId,
                                                                 precipFormParIds,
                                                                 allowLessParamsWhenCreatingWeather,
                                                                 maxUsedThreadCount,
                                                                 doAccuratePrecip,
                                                                 buildTimeBag);

  uusiData->Write();
  delete uusiData;
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
       << "\t\twith given parId(s). If parId is positive, param will be removed from result data."
       << endl
       << "\t\tIf parId is negative, param will be left in result data." << endl
       << "\t-a \tAllow WeatherAndCloudinessParam to be created with minimum" << endl
       << "\t\tN and rr(1h|3h|6h) parameters." << endl
       << "\t-m <thread-count>\tMax used worker threads, default all" << endl
       << "\t-p  add accurate precip param and calcs snowFall param per 1h" << endl
       << "\t-b  build time bag instead of time list if possible (required by TAF editor)" << endl
       << "\tExample usage: qdversionchange -t 1 -w 0 7 < input > output" << endl
       << endl;
}

int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption)
{
  NFmiValueString valStr(theCmdline.OptionValue(theOption));
  if (valStr.IsInt()) return static_cast<int>(valStr);
  throw runtime_error(string("Error: '") + theOption +
                      "' option value must be integer, exiting...");
}
