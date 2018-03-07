/*!
 *  \file qdcheck.cpp
 *  Tekijä: Marko (19.1.2001) \par
 *  Ohjelmaa käytetään tarkistamaan, ettei editoitu data ole korruptoitunutta tai
 *  että sen data on suht. järkevää.
 *  Tämä ohjelma lukee stdin:in annetun qdatan, ja tarkistaa, että siitä ei puutu
 *  liikaa dataa, eikä data ole liian 'suoraa' (lähinnä parametrit kuten lämpötila).\par
 *  Lisäksi tarkistetaan, että data on tarpeeksi lähellä nykyhetkeä (ettei) vahingossa
 *  pääse läpi vaikka eilistä dataa. Dataa on tarpeeksi ajallisesti ja siinä on
 *  oikea aikaresoluutio.
 */

#include "NFmiParamCheckData.h"
#include "NFmiQueryDataChecker.h"
#include <newbase/NFmiCommentStripper.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiValueString.h>

#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <sstream>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool check_tdew;      // --check-tdew
  std::string config;   // arg 1
  std::string infile;   // arg 2
  std::string outfile;  // arg 3
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default command line options
 */
// ----------------------------------------------------------------------

Options::Options() : check_tdew(false), config(), infile(), outfile() {}
// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 *
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool ParseOptions(int argc, char *argv[], Options &options)
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "check-dewpoint-difference",
      po::bool_switch(&options.check_tdew),
      "enable temperature >= dew point check")(
      "config", po::value(&options.config), "configuration file")(
      "infile", po::value(&options.infile), "input querydata")(
      "outfile", po::value(&options.outfile), "output report filename");

  po::positional_options_description p;
  p.add("config", 1);
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdcheck v1.1 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: qdcheck [options] configfile infile outfile" << std::endl
              << std::endl
              << "Checks input querydata for outliers based on the given" << std::endl
              << "configuration file" << std::endl
              << std::endl
              << desc << std::endl;
    return false;
  }

  if (opt.count("config") == 0)
    throw std::runtime_error("Expecting configuration file as parameter 1");

  if (opt.count("infile") == 0) throw std::runtime_error("Expecting input file as parameter 2");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 3");

  if (!fs::exists(options.config))
    throw std::runtime_error("Config file '" + options.config + "' does not exist");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input file '" + options.infile + "' does not exist");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Format an error report on the allowed value range for the parameter
 */
// ----------------------------------------------------------------------

string GetParamMinMaxString(const NFmiParamCheckData &theParamCheckData)
{
  string str("(min = ");
  str += NFmiValueString::GetStringWithMaxDecimalsSmartWay(
             theParamCheckData.itsCheckedParamMinValue, 2)
             .CharPtr();
  str += " max = ";
  str += NFmiValueString::GetStringWithMaxDecimalsSmartWay(
             theParamCheckData.itsCheckedParamMaxValue, 2)
             .CharPtr();
  str += ")";
  return str;
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void SetErrorStatus(int &theCurrentErrorLevel, int theOccuredErrorCode)
{
  if (theCurrentErrorLevel == 0 || theCurrentErrorLevel > theOccuredErrorCode)
    theCurrentErrorLevel = theOccuredErrorCode;
}

// ----------------------------------------------------------------------
/*!
 * \brief
 *
 * Saa parametrina fatal, error ja warning limitit (tässä
 * järjestyksessä), vertaa nittä annettuun theCalculatedValue arvoon. Jos
 * jokin raja ylittyy, on kyseessä jonkinlainen virhetilanne. Tällöin
 * täyttää sopivan virheilmoituksen theText-parametriin. Lisäksi
 * päivittää errorStatus-parametria seuraavissa tapauksissa:
 * errorStatus= 0 tai jos errorStatus > nyt löytynyt virhe
 * (0=ok, 1=fatal, 2=error, 3=warning)
 */
// ----------------------------------------------------------------------

bool CheckErrorLevelAndProduceStatusTextAndUpdateErrorCode(const std::vector<float> &theErrorLimits,
                                                           float theCalculatedValue,
                                                           int &errorStatus,
                                                           NFmiString &theText)
{
  int size = theErrorLimits.size();
  int i;
  for (i = 0; i < size; i++)
  {
    if (theErrorLimits[i] < theCalculatedValue)
    {
      SetErrorStatus(errorStatus, i + 1);  // i+1 ==> vektorin indeksi alkaa 0:sta, mutta errorit
                                           // menevät (1=fatal, 2=error, 3=warning)
      switch (i + 1)
      {
        case 1:  // FATAL
        {
          theText = "###FATAL ERROR###: raja oli ";
          NFmiValueString valueStr(theErrorLimits[i], "%0.1f");
          theText += valueStr;
          theText += "%";
        }
        break;
        case 2:  // ERROR
        {
          theText = "#ERROR#: raja oli ";
          NFmiValueString valueStr(theErrorLimits[i], "%0.1f");
          theText += valueStr;
          theText += "%";
        }
        break;
        case 3:  // WARNING
        {
          theText = "Warning: raja oli ";
          NFmiValueString valueStr(theErrorLimits[i], "%0.1f");
          theText += valueStr;
          theText += "%";
        }
        break;
      }
      break;
    }
  }
  if (i >= size) theText = "Ok.";

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the configuration file
 */
// ----------------------------------------------------------------------

bool ReadOhjausTiedosto(const NFmiString &theFileName, NFmiOhjausData &theOhjausData)
{
  string fileName(theFileName);
  NFmiCommentStripper stripComments;
  if (stripComments.ReadFile(fileName))
  {
    stripComments.Strip();
    stringstream strippedControlFile(stripComments.GetString());

    strippedControlFile >> theOhjausData.itsLocationCheckingType;
    strippedControlFile >> theOhjausData.itsRandomPointCount;
    int checkedParamCount = 0;
    strippedControlFile >> checkedParamCount;
    NFmiParamCheckData data;
    int i;
    for (i = 0; i < checkedParamCount; i++)
    {
      strippedControlFile >> data;
      theOhjausData.itsParamIdCheckList.push_back(data);
    }
    int errorLevelCount = 0;
    strippedControlFile >> errorLevelCount;
    theOhjausData.itsMinDataLengthInHours.resize(errorLevelCount);
    theOhjausData.itsMaxDataStartHourDifferenceBackwardInHours.resize(errorLevelCount);
    theOhjausData.itsMaxDataStartHourDifferenceForwardInHours.resize(errorLevelCount);
    for (i = 0; i < errorLevelCount; i++)
      strippedControlFile >> theOhjausData.itsMinDataLengthInHours[i];
    for (i = 0; i < errorLevelCount; i++)
      strippedControlFile >> theOhjausData.itsMaxDataStartHourDifferenceBackwardInHours[i];
    for (i = 0; i < errorLevelCount; i++)
      strippedControlFile >> theOhjausData.itsMaxDataStartHourDifferenceForwardInHours[i];
    strippedControlFile >> theOhjausData.itsWantedTimeStepInMinutes;
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Verify that dew point <= temperature at all points
 */
// ----------------------------------------------------------------------

bool IsDewPointOK(NFmiFastQueryInfo &info)
{
  info.First();

  if (!info.Param(kFmiTemperature)) return true;
  if (!info.Param(kFmiDewPoint)) return true;

  for (info.ResetLocation(); info.NextLocation();)
    for (info.ResetLevel(); info.NextLevel();)
      for (info.ResetTime(); info.NextTime();)
      {
        info.Param(kFmiTemperature);
        float t2m = info.FloatValue();
        info.Param(kFmiDewPoint);
        float tdew = info.FloatValue();
        if (t2m != kFloatMissing && tdew != kFloatMissing && tdew > t2m) return false;
      }
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception trapping
 */
// ----------------------------------------------------------------------

int run(int argc, char *argv[])
{
  if (!ParseOptions(argc, argv, options)) return 0;

  NFmiOhjausData ohjausData;
  NFmiString ohjausfileName;
  NFmiString datafileName;
  NFmiString tulosfileName;
  int returnValue = 0;

  ohjausfileName = options.config;
  tulosfileName = options.outfile;

  ofstream out(tulosfileName);
  if (!out)
  {
    cout << "Resultfile: " << static_cast<char *>(tulosfileName) << " could not be opened." << endl;
    return 2;
  }
  NFmiMetTime currentTime(1);
  out << "Data check begins: " << currentTime << " UTC." << endl;
  out << "Data file: " << endl << options.infile << endl << endl;

  bool status = ReadOhjausTiedosto(ohjausfileName, ohjausData);
  if (!status)
  {
    cout << "#ERROR#: control file: " << static_cast<char *>(ohjausfileName)
         << " could not be opened." << endl;
    out << "#ERROR#: control file: " << static_cast<char *>(ohjausfileName)
        << " could not be opened." << endl;
    out.close();
    return 2;
  }

  NFmiQueryData data(options.infile);

  NFmiQueryDataChecker dataChecker(&ohjausData);
  dataChecker.Data(&data);
  NFmiParamBag params(*dataChecker.DatasParamBag());
  params.SetActivities(false);
  for (unsigned int i = 0; i < ohjausData.itsParamIdCheckList.size(); i++)
  {
    if (params.SetCurrent(ohjausData.itsParamIdCheckList[i].itsParamId, false))
      params.Current(false)->SetActive(true);
  }
  dataChecker.ParamBag(params);
  dataChecker.CheckOnlyWantedTimes(false);
  dataChecker.CheckList(1 + 2 + 4);  // 1=missing data check ja 2=suoraa dataa
  if (ohjausData.itsLocationCheckingType == 2)
    dataChecker.RandomlyCheckedLocationCount(ohjausData.itsRandomPointCount);
  else                                 // tarkistetaan tässä vaiheessa muuten vain kaikki pisteet
    dataChecker.LocationCheckType(0);  // 0= tarkista kaikki pisteet

  int stat = dataChecker.DoTotalCheck() ? 0 : 2;
  SetErrorStatus(returnValue, stat);

  if (out)
  {
    NFmiFastQueryInfo *info = dataChecker.Info();
    int parCount = ohjausData.itsParamIdCheckList.size();
    for (int i = 0; i < parCount; i++)
    {
      out << "Param ID: " << ohjausData.itsParamIdCheckList[i].itsParamId;
      if (info && info->Param(FmiParameterName(ohjausData.itsParamIdCheckList[i].itsParamId)))
        out << " '" << static_cast<char *>(info->Param().GetParamName()) << "'" << endl;
      else
      {
        out << " #ERROR#: (the parameter is not in the querydata?)" << endl;
        SetErrorStatus(returnValue, 2);
      }
      if (ohjausData.itsParamIdCheckList[i].itsCheckedParamMissingDataMaxProcent == kFloatMissing)
        out << "Dataa missing: No inspection." << endl;
      else
      {
        out << "Dataa missing: "
            << ohjausData.itsParamIdCheckList[i].itsCheckedParamMissingDataMaxProcent << " %. ";
        NFmiString errorText;
        static_cast<void>(CheckErrorLevelAndProduceStatusTextAndUpdateErrorCode(
            ohjausData.itsParamIdCheckList[i].itsParamMissingDataMaxProcent,
            ohjausData.itsParamIdCheckList[i].itsCheckedParamMissingDataMaxProcent,
            returnValue,
            errorText));
        out << static_cast<char *>(errorText) << endl;
      }

      if (ohjausData.itsParamIdCheckList[i].itsCheckedParamStraightDataMaxProcent == kFloatMissing)
        out << "Data was straight. No inspection." << endl;
      else
      {
        out << "Data was straight: "
            << ohjausData.itsParamIdCheckList[i].itsCheckedParamStraightDataMaxProcent << " %. ";
        NFmiString errorText;
        static_cast<void>(CheckErrorLevelAndProduceStatusTextAndUpdateErrorCode(
            ohjausData.itsParamIdCheckList[i].itsParamStraightDataMaxProcent,
            ohjausData.itsParamIdCheckList[i].itsCheckedParamStraightDataMaxProcent,
            returnValue,
            errorText));
        out << static_cast<char *>(errorText) << endl;
      }

      if (ohjausData.itsParamIdCheckList[i].itsCheckedParamOutOfLimitError)
      {
        out << "#ERROR#: Data containd values exceeding the set min/max values: "
            << ohjausData.itsParamIdCheckList[i].itsParamLowerLimits[1] << " - "
            << ohjausData.itsParamIdCheckList[i].itsParamUpperLimits[1] << " "
            << GetParamMinMaxString(ohjausData.itsParamIdCheckList[i]) << endl;
        SetErrorStatus(returnValue, 2);
      }
      else if (ohjausData.itsParamIdCheckList[i].itsCheckedParamOutOfLimitWarning)
      {
        out << "Warning: Data contained values exceeding the set min/max warning limits: "
            << ohjausData.itsParamIdCheckList[i].itsParamLowerLimits[2] << " - "
            << ohjausData.itsParamIdCheckList[i].itsParamUpperLimits[2] << " "
            << GetParamMinMaxString(ohjausData.itsParamIdCheckList[i]) << endl;
        SetErrorStatus(returnValue, 3);
      }
      else
      {
        out << "Data was within the set warning limits "
            << GetParamMinMaxString(ohjausData.itsParamIdCheckList[i]) << "." << endl;
      }
      out << endl;
    }

    //			NFmiFastQueryInfo* info = dataChecker.Info();
    if (info)
    {
      if (options.check_tdew)
      {
        if (!IsDewPointOK(*info))
        {
          out << "#ERROR#: Dew point > Temperature at some points in the data" << endl;
          SetErrorStatus(returnValue, 2);
        }
        else
          out << "OK: Dew point <= Temperature at all points" << endl;
        out << endl;
      }

      info->First();

      NFmiMetTime time1(info->Time());
      info->LastTime();
      info->PreviousTime();
      NFmiMetTime time2(info->Time());
      int diffInHours = time2.DifferenceInHours(time1);
      if (ohjausData.itsMinDataLengthInHours[0] != -1 &&
          diffInHours < ohjausData.itsMinDataLengthInHours[0])
      {
        out << "###FATAL ERROR###: Data does not cover a sufficient long time interval. The "
               "limit: "
            << ohjausData.itsMinDataLengthInHours[0] << "h, file covered only " << diffInHours
            << "h" << endl;
        SetErrorStatus(returnValue, 1);
      }
      else if (ohjausData.itsMinDataLengthInHours[1] != -1 &&
               diffInHours < ohjausData.itsMinDataLengthInHours[1])
      {
        out << "#ERROR#: Data does not cover a sufficiently long time interval. The limit: "
            << ohjausData.itsMinDataLengthInHours[1] << "h, file covered only " << diffInHours
            << "h" << endl;
        SetErrorStatus(returnValue, 2);
      }
      else if (ohjausData.itsMinDataLengthInHours[2] != -1 &&
               diffInHours < ohjausData.itsMinDataLengthInHours[2])
      {
        out << "Warning: Data does not cover a sufficiently long time interval. The limit: "
            << ohjausData.itsMinDataLengthInHours[2] << "h, file covered only " << diffInHours
            << "h" << endl;
        SetErrorStatus(returnValue, 3);
      }
      else
        out << "OK: File covered " << diffInHours << " hours." << endl;

      int diffToCurrentHour = currentTime.DifferenceInHours(time1);
      if (ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[0] != -1 &&
          diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[0])
      {
        out << "###FATAL ERROR###: Data begins too early. The limit: "
            << ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[0]
            << "h before present, the data begins " << diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 1);
      }
      else if (ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[1] != -1 &&
               diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[1])
      {
        out << "#ERROR#: Data begins too early. The limit:: "
            << ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[1]
            << "h before present, the data begins " << diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 2);
      }
      else if (ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[2] != -1 &&
               diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[2])
      {
        out << "Warning: Data begins too early. The limit: "
            << ohjausData.itsMaxDataStartHourDifferenceBackwardInHours[2]
            << "h before present, the data begins " << diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 3);
      }
      else if (diffToCurrentHour >= 0)
        out << "OK: Data begins " << diffToCurrentHour << " before present." << endl;

      if (ohjausData.itsMaxDataStartHourDifferenceForwardInHours[0] != -1 &&
          -diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceForwardInHours[0])
      {
        out << "###FATAL ERROR###: Data begins too late. The limit: "
            << ohjausData.itsMaxDataStartHourDifferenceForwardInHours[0]
            << "h before present, the data begins " << -diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 1);
      }
      else if (ohjausData.itsMaxDataStartHourDifferenceForwardInHours[1] != -1 &&
               -diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceForwardInHours[1])
      {
        out << "#ERROR#: Data begins too late. The limit: "
            << ohjausData.itsMaxDataStartHourDifferenceForwardInHours[1]
            << "h before present, the data begins " << -diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 2);
      }
      else if (ohjausData.itsMaxDataStartHourDifferenceForwardInHours[2] != -1 &&
               -diffToCurrentHour > ohjausData.itsMaxDataStartHourDifferenceForwardInHours[2])
      {
        out << "Warning: Data begins too late. The limit: "
            << ohjausData.itsMaxDataStartHourDifferenceForwardInHours[2]
            << "h before present, the data begins " << -diffToCurrentHour << "h before present."
            << endl;
        SetErrorStatus(returnValue, 3);
      }
      else if (-diffToCurrentHour > 0)
        out << "OK: Data begins " << -diffToCurrentHour << "h before present." << endl;

      // aika-askeleen tarkistus
      int timeStep = info->TimeResolution();
      if (ohjausData.itsWantedTimeStepInMinutes != -1 &&
          timeStep != ohjausData.itsWantedTimeStepInMinutes)
      {
        out << "#ERROR#: Data time resolution of " << timeStep
            << " minutes is incorrect, the desired value is "
            << ohjausData.itsWantedTimeStepInMinutes << " minutes." << endl;
        SetErrorStatus(returnValue, 2);
      }
      else
        out << "OK: Data time resolution " << timeStep << " minutes is correct." << endl;
    }
    else
    {
      out << "#ERROR#: Could not extract information for time validation." << endl;
      SetErrorStatus(returnValue, 2);
    }
  }

  out.close();
  return returnValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (std::exception &e)
  {
    cerr << "ERROR when executing " << argv[0] << endl;
    cerr << e.what() << endl;
    cerr << "Stopping the program." << endl;
  }
  catch (char *str)
  {
    cerr << "ERROR when executing " << argv[0] << endl;
    cerr << str << endl;
    cerr << "Stopping the program." << endl;
  }
  catch (...)
  {
    cerr << "Unknown ERROR when executing " << argv[0] << " \nStopping the program." << endl;
  }
  return 1;  // palautetaan 1 errorin merkiksi. Tähän tullaan catch-blokkien kautta
}
