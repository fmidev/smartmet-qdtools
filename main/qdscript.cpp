#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiSettings.h>
#include <smarttools/NFmiSmartToolUtil.h>
#include <fstream>

bool ReadScriptFile(const std::string &theFileName, std::string *theScript);
void Usage(const std::string &theExecutableName);
void Run(int argc, const char *argv[]);
NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo &qi, const std::string &opts);

using namespace std;  // tätä ei saa sitten laittaa headeriin, eikä ennen includeja!!!!

int main(int argc, const char *argv[])
{
  try
  {
    Run(argc, argv);
  }
  catch (exception &e)
  {
    cerr << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

void Run(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "a!d!sli!");

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    Usage(argv[0]);
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() < 1)
  {
    cerr << "Error: atleast 1 parameter expected, 'scriptFile'\n\n";
    Usage(argv[0]);
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  string scriptFileName = cmdline.Parameter(1);

  std::vector<string> helperFileNameList;
  if (cmdline.NumberofParameters() > 1)
  {
    for (int i = 2; i <= cmdline.NumberofParameters(); i++)
      helperFileNameList.push_back(cmdline.Parameter(i));
  }

#ifndef UNIX
  string dictionaryFile("dictionary.conf");
#else
  string dictionaryFile("/usr/share/smartmet/dictionaries/en.conf");
#endif
  if (cmdline.isOption('d')) dictionaryFile = cmdline.OptionValue('d');

  bool makeStaticIfOneTimeStepData = false;
  if (cmdline.isOption('s')) makeStaticIfOneTimeStepData = true;

  bool goThroughAllLevels = true;
  if (cmdline.isOption('l')) goThroughAllLevels = false;

  if (NFmiSettings::Read(dictionaryFile) == false)
    cerr << "Warning: dictionary string was not found, continuing..." << endl;

  string smartToolScript;
  if (!ReadScriptFile(scriptFileName, &smartToolScript))
  {
    throw runtime_error(string("Error: Could not read script-file ") + scriptFileName);
  }

  string infile = "-";
  if (cmdline.isOption('i')) infile = cmdline.OptionValue('i');

  NFmiQueryData *qd = new NFmiQueryData(infile);
  NFmiQueryData *newData = 0;
  if (!cmdline.isOption('a'))
  {
    // true = vapautetaan querydata, silla sen omistus siirtyy NF
    newData = NFmiSmartToolUtil::ModifyData(smartToolScript,
                                            qd,
                                            &helperFileNameList,
                                            false,
                                            goThroughAllLevels,
                                            makeStaticIfOneTimeStepData);
  }
  else
  {
    // Create new data

    NFmiFastQueryInfo qi(qd);

    NFmiFastQueryInfo qitmp(MakeParamDescriptor(qi, cmdline.OptionValue('a')),
                            qi.TimeDescriptor(),
                            qi.HPlaceDescriptor(),
                            qi.VPlaceDescriptor(),
                            qi.InfoVersion());

    NFmiQueryData *qd2(NFmiQueryDataUtil::CreateEmptyData(qitmp));
    NFmiFastQueryInfo qi2(qd2);

    // Copy original data

    for (qi.ResetLevel(), qi2.ResetLevel(); qi.NextLevel() && qi2.NextLevel();)
      for (qi.ResetParam(); qi.NextParam();)
      {
        if (qi2.Param(qi.Param()))
        {
          for (qi.ResetTime(), qi2.ResetTime(); qi.NextTime() && qi2.NextTime();)
            for (qi.ResetLocation(), qi2.ResetLocation(); qi.NextLocation() && qi2.NextLocation();)
            {
              qi2.FloatValue(qi.FloatValue());
            }
        }
        else
          throw runtime_error("Internal error when copying querydata");
      }

    newData = NFmiSmartToolUtil::ModifyData(smartToolScript,
                                            qd2,
                                            &helperFileNameList,
                                            false,
                                            goThroughAllLevels,
                                            makeStaticIfOneTimeStepData);  // true = vapautetaan
                                                                           // querydata, silla sen
                                                                           // omistus siirtyy NF
                                                                           // }
  }
  if (newData)
    newData->Write();
  else
    throw runtime_error("Unable to create new data, stopping...");
}

bool ReadScriptFile(const std::string &theFileName, std::string *theScript)
{
  if (theScript)
  {
    ifstream in(theFileName.c_str());
    if (in)
    {
      string rowbuffer, bigstring;

      // luetaan tiedostoa rivi kerrallaan ja testataan löytyykö yhden rivin kommentteja

      while (std::getline(in, rowbuffer))
      {
        if (!rowbuffer.empty())
        {
          if (rowbuffer[rowbuffer.size() - 1] == '\r') rowbuffer.resize(rowbuffer.size() - 1);
        }
        bigstring += rowbuffer + "\n";
      }
      *theScript = bigstring;
      in.close();
    }
    else
      return false;

    return true;
  }

  return false;
}

void Usage(const std::string &theExecutableName)
{
  NFmiFileString fileNameStr(theExecutableName);
  std::string usedFileName(
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimestä
  cerr << "Usage: " << endl
       << usedFileName.c_str()
       << " [options] macroFile [qdata1 qdata2 ...] < inputdata > outputData" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-i infile\tinput querydata filename, default is to read standard input" << endl
       << "\t-a p1,p2,...\tlist of parameters to add to the data" << endl
       << "\t-d dictionary-file\tTranslations for messages." << endl
       << "\t-s \tUse all one-time-step datas as stationary (default = false)." << endl
       << "\t-l \tDon't go through all levels separately, smarttool-skript is" << endl
       << "\t\thandling specific levels (default = go through all levels)." << endl
       << "Example usage: " << usedFileName.c_str()
       << " MySkript.st < myData.sqd > modifiedData.sqd" << endl
       << "Example usage: " << usedFileName.c_str()
       << " MySkript.st helpData1 helpData2 < myData.sqd > modifiedData.sqd" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter description
 */
// ----------------------------------------------------------------------

FmiParameterName parse_param(const string &theName)
{
  // Try ascii name

  static NFmiEnumConverter converter;
  FmiParameterName paramnum = FmiParameterName(converter.ToEnum(theName));
  if (paramnum != kFmiBadParameter) return paramnum;

  // Try numerical value

  try
  {
    int value = NFmiStringTools::Convert<int>(theName);
    return FmiParameterName(value);
  }
  catch (...)
  {
    throw runtime_error("Parameter '" + theName + "' is not known to newbase");
  }
}

NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo &qi, const std::string &opts)
{
  NFmiParamBag pbag = qi.ParamBag();

  vector<string> names = NFmiStringTools::Split<vector<string> >(opts);

  const NFmiProducer &producer = qi.FirstParamProducer();
  for (vector<string>::const_iterator it = names.begin(); it != names.end(); ++it)
  {
    FmiParameterName paramnum = parse_param(*it);
    // If the parameter is already in the data - we simply keep it
    if (!qi.Param(paramnum))
    {
      NFmiParam param(paramnum,
                      *it,
                      kFloatMissing,
                      kFloatMissing,
                      kFloatMissing,
                      kFloatMissing,
                      "%.1f",
                      kLinearly);
      pbag.Add(NFmiDataIdent(param, producer));
    }
  }
  return NFmiParamDescriptor(pbag);
}
