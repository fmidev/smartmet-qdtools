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
NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo &qi,
                                        const std::vector<NFmiDataIdent> &addedParameters);
std::vector<NFmiDataIdent> GetAddedParameters(NFmiCmdLine &cmdline, NFmiQueryData *baseQueryData);
std::unique_ptr<NFmiQueryData> CreateNewBaseData(NFmiQueryData &originalData,
                                            const std::vector<NFmiDataIdent> &addedParameters);

using namespace std;  // t�t� ei saa sitten laittaa headeriin, eik� ennen includeja!!!!

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
  NFmiCmdLine cmdline(argc, argv, "a!d!sli!o!A!");

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    Usage(argv[0]);
    throw runtime_error("");  // t�ss� piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() < 1)
  {
    cerr << "Error: atleast 1 parameter expected, 'scriptFile'\n\n";
    Usage(argv[0]);
    throw runtime_error("");  // t�ss� piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
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
  string outfile;
  if (cmdline.isOption('o')) outfile = cmdline.OptionValue('o');
  // querydata pit�� lukea kokonaisuudessaan muistiin (= ei memory-mapped),
  // ett� dataa voi muokata smarttools skriptill�, siksi 2. parametri false.
  std::unique_ptr<NFmiQueryData> querydataPtr(new NFmiQueryData(infile, false));
  if (!querydataPtr)
  {
    throw runtime_error(string("Error: working queryData file could not be open correctly: ") + infile);
  }

  auto possibleAddedParameters = GetAddedParameters(cmdline, querydataPtr.get());

  std::unique_ptr<NFmiQueryData> newDataPtr;
  if (possibleAddedParameters.empty())
  {
    // Tehd��n querydataPtr.release(), koska querydata olio menee NFmiSmartToolUtil::ModifyData
    // funktiossa NFmiInfoOrganizer luokan olion hallintaan.
    newDataPtr.reset(NFmiSmartToolUtil::ModifyData(smartToolScript,
                                                   querydataPtr.release(),
                                                   &helperFileNameList,
                                                   false,
                                                   goThroughAllLevels,
                                                   makeStaticIfOneTimeStepData));
  }
  else
  {
    // Create new base data
    unique_ptr<NFmiQueryData> qd2Ptr(CreateNewBaseData(*querydataPtr, possibleAddedParameters));
    // Tehd��n qd2Ptr.release(), koska querydata olio menee NFmiSmartToolUtil::ModifyData
    // funktiossa NFmiInfoOrganizer luokan olion hallintaan.
    newDataPtr.reset(NFmiSmartToolUtil::ModifyData(smartToolScript,
                                                   qd2Ptr.release(),
                                                   &helperFileNameList,
                                                   false,
                                                   goThroughAllLevels,
                                                   makeStaticIfOneTimeStepData));
  }
  if (newDataPtr)
  {
    if (outfile.empty())
      newDataPtr->Write();
    else
      newDataPtr->Write(outfile);
  }
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

      // luetaan tiedostoa rivi kerrallaan ja testataan l�ytyyk� yhden rivin kommentteja

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
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimest�
  cerr << "Usage: " << endl
       << usedFileName.c_str()
       << " [options] macroFile [qdata1 qdata2 ...] < inputdata > outputData" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-i infile\tinput querydata filename, default is to read standard input" << endl
       << "\t-o outfile\toutput querydata filename, default is to write standard output" << endl
       << "\t-a p1,p2,...\tlist of parameters to add to the data" << endl
       << "\t-A id1,name1,id2,name2,...\tlist of parameter id and name pairs to add to the data" << endl
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

void AddNonExistingParamToVector(FmiParameterName paramId,
                                 const string &name,
                                 NFmiFastQueryInfo &fastInfo,
                                 std::vector<NFmiDataIdent> &paramVectorInOut)
{
  // If the parameter is already in the data - do nothing
  if (!fastInfo.Param(paramId))
  {
    NFmiParam param(paramId,
                    name,
                    kFloatMissing,
                    kFloatMissing,
                    kFloatMissing,
                    kFloatMissing,
                    "%.1f",
                    kLinearly);
    paramVectorInOut.push_back(NFmiDataIdent(param, fastInfo.FirstParamProducer()));
  }
}

// Oletus: baseQueryData ei ole nullptr
std::vector<NFmiDataIdent> GetAddedParameters(NFmiCmdLine &cmdline, NFmiQueryData *baseQueryData)
{
  NFmiFastQueryInfo fastInfo(baseQueryData);
  std::vector<NFmiDataIdent> possibleAddedParameters;
  if (cmdline.isOption('a'))
  {
    // Annettu lista parametrin newbase enum nimi� tai numeroita, joista samasta
    // string-arvosta muodostetaan par-id ja nimi
    vector<string> names = NFmiStringTools::Split<vector<string>>(cmdline.OptionValue('a'));
    for (const string &name : names)
    {
      FmiParameterName paramId = parse_param(name);
      AddNonExistingParamToVector(paramId, name, fastInfo, possibleAddedParameters);
    }
  }
  else if (cmdline.isOption('A'))
  {
    // Annettu lista parametrin id,nimi pareja, id on aina numero, eik� saa olla newbase enum
    // juttuun liittyv� stringi
    vector<string> idNamePairs = NFmiStringTools::Split<vector<string>>(cmdline.OptionValue('A'));
    if (idNamePairs.size() >= 2)
    {
      for (size_t index = 0; index < idNamePairs.size() - 1; index += 2)
      {
        try
        {
          FmiParameterName paramId = static_cast<FmiParameterName>(stoi(idNamePairs[index]));
          auto name = idNamePairs[index + 1];
          AddNonExistingParamToVector(paramId, name, fastInfo, possibleAddedParameters);
        }
        catch (exception &e)
        {
          throw runtime_error(
              string("Error when trying to parse par-id,par-name pair with values: ") +
              idNamePairs[index] + "," + idNamePairs[index + 1] + ", with error: " + e.what());
        }
      }
    }
  }
  return possibleAddedParameters;
}

NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo &qi,
                                        const std::vector<NFmiDataIdent> &addedParameters)
{
  NFmiParamBag pbag = qi.ParamBag();
  for (const NFmiDataIdent &dataIdent : addedParameters)
  {
    // Tehd��n viel� tarkistus ett� parametria ei ole jo datassa
    if (!qi.Param(*dataIdent.GetParam()))
    {
      pbag.Add(dataIdent);
    }
  }
  return NFmiParamDescriptor(pbag);
}

unique_ptr<NFmiQueryData> CreateNewBaseData(NFmiQueryData &originalData,
                                            const std::vector<NFmiDataIdent> &addedParameters)
{
  NFmiFastQueryInfo qi(&originalData);

  NFmiFastQueryInfo qitmp(MakeParamDescriptor(qi, addedParameters),
                          qi.TimeDescriptor(),
                          qi.HPlaceDescriptor(),
                          qi.VPlaceDescriptor(),
                          qi.InfoVersion());

  std::unique_ptr<NFmiQueryData> qd2Ptr(NFmiQueryDataUtil::CreateEmptyData(qitmp));
  NFmiFastQueryInfo qi2(qd2Ptr.get());

  // Copy original data
  for (qi.ResetLevel(), qi2.ResetLevel(); qi.NextLevel() && qi2.NextLevel();)
  {
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
  }
  return qd2Ptr;
}