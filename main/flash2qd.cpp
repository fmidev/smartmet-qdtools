/*!
 *  Ohjelma lukee salama-dataa tiedostosta ja muodostaa siit‰ queryDatan
 *  ja tulostaa sen stdout:iin.
 *
 *  Annettu salama data pit‰‰ olla muodossa:
 *  YYYYMMDDHHmmss lon lat power multiplicity accuracy
 *
 *  eli esim.
 *
 *  20040505144234	19.984	53.199	-28	1	43.5
 *  20040505144925	19.457	53.563	-30	1	29.5
 *  20040505150731	19.246	53.853	23	1	39.2
 *  20040505151128	29.938	63.529	-3	1	10.4
 *  jne.
 *
 *  Kyseist‰ dataa voi hakea vaikka tietokannasta seuraavalla SQL-kyselyll‰:
 *
 *  SELECT
 *  DATE_FORMAT(date,'%Y%m%d%H%i%S'),lon,lat,power,multiplicity,accuracy
 *  FROM `havainnot`
 *  WHERE date>=DATE_SUB(NOW(), INTERVAL 24 HOUR)
 *  ORDER BY date
 *
 */

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiValueString.h>

#include <fstream>
#include <stdexcept>

bool ReadFlashFile(const std::string &theFileName,
                   int theSkipLines,
                   std::vector<std::string> &theFlashStrings);
NFmiQueryData *CreateFlashQueryData(std::vector<std::string> &theFlashStrings,
                                    bool fMakeLocal2UtcTimeConversion);
void Usage(void);
void Domain(int argc, const char *argv[]);
int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption);

using namespace std;  // t‰t‰ ei saa sitten laittaa headeriin, eik‰ ennen includeja!!!!

int main(int argc, const char *argv[])
{
  try
  {
    Domain(argc, argv);
  }
  catch (exception &e)
  {
    cerr << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

void Domain(int argc, const char *argv[])
{
  string flashFileName;
  int skipLines = 0;
  bool makeLocal2UtcTimeConversion = false;

  NFmiCmdLine cmdline(argc, argv, "s!t");

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;

    Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() != 1)
  {
    cerr << "Error: 1 parameter expected, 'flashdata'\n\n";
    Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }
  else
    flashFileName = cmdline.Parameter(1);

  if (cmdline.isOption('s')) skipLines = GetIntegerOptionValue(cmdline, 's');
  if (cmdline.isOption('t')) makeLocal2UtcTimeConversion = true;

  std::vector<std::string> flashStrings;
  if (!ReadFlashFile(flashFileName, skipLines, flashStrings))
    throw runtime_error(std::string("salamadata-tiedostoa ") + flashFileName +
                        std::string(" ei saatu avattua"));

  NFmiQueryData *newData = CreateFlashQueryData(flashStrings, makeLocal2UtcTimeConversion);
  unique_ptr<NFmiQueryData> dataPtr(newData);  // t‰m‰ tuhoaa dynaamisesti luodun datan
                                             // automaattisesti (vaikka return paikkoja olisi kuinka
                                             // monta)
  if (newData)
  {
    NFmiStreamQueryData streamQDataTulos;
    streamQDataTulos.WriteCout(newData);
  }
  else
    throw runtime_error("Salama QueryDataa ei saatu luotua.");
}

struct FlashData
{
  NFmiMetTime itsTime;
  float lon;
  float lat;
  float power;
  float multiplicity;
  float accuracy;
};

// data stringi siis muotoa:
// YYYYMMDDHHmmss lon lat power multiplicity accuracy
// eli esim.
// 20040505144234	19.984	53.199	-28	1	43.5
bool ParseFlashDataLine(const std::string &theLineStr, FlashData &theData)
{
  if (!theLineStr.empty())
  {
    std::vector<std::string> words = NFmiStringTools::Split(theLineStr, "	");
    if (words.size() == 6)
    {
      theData.itsTime.FromStr(words[0]);
      theData.lon = NFmiStringTools::Convert<float>(words[1]);
      theData.lat = NFmiStringTools::Convert<float>(words[2]);
      theData.power = NFmiStringTools::Convert<float>(words[3]);
      theData.multiplicity = NFmiStringTools::Convert<float>(words[4]);
      theData.accuracy = NFmiStringTools::Convert<float>(words[5]);
      return true;
    }
    else
      throw runtime_error(std::string("Data rivill‰ oli v‰‰r‰ m‰‰r‰ arvoja: \n") + theLineStr);
  }
  return false;
}

NFmiHPlaceDescriptor MakeHPlaceDescriptor(void)
{
  NFmiLocationBag locs;
  locs.AddLocation(NFmiLocation());
  return NFmiHPlaceDescriptor(locs);
}

NFmiParamDescriptor MakeParamDescriptor(void)
{
  NFmiProducer prod(1012, "flash");
  NFmiParamBag params;
  params.Add(NFmiDataIdent(NFmiParam(kFmiLongitude, "Longitude"), prod));
  params.Add(NFmiDataIdent(NFmiParam(kFmiLatitude, "Latitude"), prod));
  params.Add(NFmiDataIdent(NFmiParam(kFmiFlashStrength, "Strength"), prod));
  params.Add(NFmiDataIdent(NFmiParam(kFmiFlashMultiplicity, "Multiplicity"), prod));
  params.Add(NFmiDataIdent(NFmiParam(kFmiFlashAccuracy, "Accuracy"), prod));
  return NFmiParamDescriptor(params);
}

NFmiQueryData *CreateQueryData(const NFmiTimeList &theTimes)
{
  NFmiParamDescriptor paramDesc = MakeParamDescriptor();
  NFmiVPlaceDescriptor levelDesc;
  NFmiMetTime tim;
  NFmiTimeDescriptor timeDesc(tim, theTimes);
  NFmiHPlaceDescriptor hPlaceDesc = MakeHPlaceDescriptor();
  NFmiQueryInfo innerInfo(paramDesc, timeDesc, hPlaceDesc, levelDesc);
  return NFmiQueryDataUtil::CreateEmptyData(innerInfo);
}

NFmiQueryData *CreateFlashQueryData(std::vector<std::string> &theFlashStrings,
                                    bool fMakeLocal2UtcTimeConversion)
{
  NFmiQueryData *data = 0;
  if (!theFlashStrings.empty())
  {
    NFmiTimeList times;
    std::vector<float> lats(theFlashStrings.size(), kFloatMissing);
    std::vector<float> lons(theFlashStrings.size(), kFloatMissing);
    std::vector<float> powers(theFlashStrings.size(), kFloatMissing);
    std::vector<float> multiplicitys(theFlashStrings.size(), kFloatMissing);
    std::vector<float> accuracys(theFlashStrings.size(), kFloatMissing);

    std::vector<std::string>::size_type ssize = theFlashStrings.size();
    FlashData tmp;
    int counter = 0;  // kuinka monta juttua on saatu parseroitua todella
    for (unsigned int i = 0; i < ssize; i++)
    {
      if (ParseFlashDataLine(theFlashStrings[i], tmp))
      {
        if (fMakeLocal2UtcTimeConversion) tmp.itsTime = NFmiMetTime(tmp.itsTime.UTCTime(), 1);

        times.Add(new NFmiMetTime(tmp.itsTime), true, true);
        lons[counter] = tmp.lon;
        lats[counter] = tmp.lat;
        powers[counter] = tmp.power;
        multiplicitys[counter] = tmp.multiplicity;
        accuracys[counter] = tmp.accuracy;
        counter++;
      }
    }
    data = CreateQueryData(times);
    if (data)
    {
      NFmiFastQueryInfo info(data);
      info.First();
      // T‰ytet‰‰n lopuksi eri parametrit infoon/dataan
      if (info.Param(kFmiLongitude))
        for (info.ResetTime(); info.NextTime();)
          info.FloatValue(lons[info.TimeIndex()]);
      if (info.Param(kFmiLatitude))
        for (info.ResetTime(); info.NextTime();)
          info.FloatValue(lats[info.TimeIndex()]);
      if (info.Param(kFmiFlashStrength))
        for (info.ResetTime(); info.NextTime();)
          info.FloatValue(powers[info.TimeIndex()]);
      if (info.Param(kFmiFlashMultiplicity))
        for (info.ResetTime(); info.NextTime();)
          info.FloatValue(multiplicitys[info.TimeIndex()]);
      if (info.Param(kFmiFlashAccuracy))
        for (info.ResetTime(); info.NextTime();)
          info.FloatValue(accuracys[info.TimeIndex()]);
    }
  }
  return data;
}

bool ReadFlashFile(const std::string &theFileName,
                   int theSkipLines,
                   std::vector<std::string> &theFlashStrings)
{
  if (!theFileName.empty())
  {
    ifstream in(theFileName.c_str());
    if (in)
    {
      string rowbuffer;

      // luetaan tiedostoa rivi kerrallaan ja talletetaan vektoriin
      int counter = 0;
      while (std::getline(in, rowbuffer))
      {
        counter++;
        if (theSkipLines >= counter) continue;
        if (!rowbuffer.empty())
        {
          theFlashStrings.push_back(rowbuffer);
        }
      }
      in.close();
      return true;
    }
  }
  return false;
}

void Usage(void)
{
  cout << "Usage: flash2qd [-s lineCount] [-t] flashData > flash.sqd" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-s <lineCount>\tLines to skip from start of file, default = 0" << endl
       << "\t-t\tMake time conversion from local to utc, default = no conversion" << endl
       << "\tExample usage: flash2qd -s 1 myflashdata.txt > flash.sqd" << endl
       << endl;
}

int GetIntegerOptionValue(const NFmiCmdLine &theCmdline, char theOption)
{
  NFmiValueString valStr(theCmdline.OptionValue(theOption));
  if (valStr.IsInt()) return static_cast<int>(valStr);
  throw runtime_error(string("Error: '") + theOption +
                      "' option value must be integer, exiting...");
}
