// temp2qd.cpp Tekijä Marko 16.11.2005
// Lukee halutut tiedostot/tiedostonimi-filtterit, joista
// löytyvät TEMP-luotaus koodit tulkitaan ja niistä muodostetaan
// querydata, missä on yhdistettynä kaikki tulkitut luotaukset.

#include <iostream>

#include "NFmiCmdLine.h"
#include "NFmiAreaFactory.h"
#include "NFmiStringTools.h"
#include "NFmiStreamQueryData.h"
#include "NFmiQueryDataUtil.h"
#include "NFmiFileSystem.h"
#include "NFmiTimeList.h"
#include "NFmiArea.h"
#include "NFmiGrid.h"
#include "NFmiTEMPCode.h"
#include "NFmiFileString.h"
#include "NFmiAviationStationInfoSystem.h"

using namespace std;

const char *default_stations_file = "/usr/share/smartmet/stations.csv";

void Domain(int argc, const char *argv[]);
void Usage(void);

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

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  //	cout << "Usage: temp2qd [options] fileFilter1[,fileFilter2,...] > output" << endl
  cerr << "Usage: temp2qd fileFilter1[,fileFilter2,...] > output" << endl
       << endl
       << "Program reads all the files/filefilter files given as arguments" << endl
       << "and tries to find TEMP sounding messages from them." << endl
       << "All the found soundings are combined in one output querydata file." << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-s stationInfoFile\t File that contains different sounding station infos (default="
       << default_stations_file << ")" << endl
       << "\t-p <1005,UAIR>\tMake result datas producer id and name as wanted." << endl
       << "\t-t \tPut sounding times to nearest synoptic times." << endl
       << endl;
}

struct PointerDestroyer
{
  template <typename T>
  void operator()(T *thePtr)
  {
    delete thePtr;
  }
};

static void FillCombinedSoundingData(vector<NFmiQueryData *> &theDataList,
                                     NFmiFastQueryInfo &theInfo)
{
  for (unsigned int i = 0; i < theDataList.size(); i++)
  {
    NFmiFastQueryInfo sourceInfo(theDataList[i]);
    for (sourceInfo.ResetTime(); sourceInfo.NextTime();)
    {
      if (theInfo.Time(sourceInfo.Time()))
      {
        for (sourceInfo.ResetParam(); sourceInfo.NextParam();)
        {
          if (theInfo.Param(*(sourceInfo.Param().GetParam())))
          {
            for (sourceInfo.ResetLevel(), theInfo.ResetLevel();
                 sourceInfo.NextLevel() && theInfo.NextLevel();)
            {
              for (sourceInfo.ResetLocation(); sourceInfo.NextLocation();)
              {
                if (theInfo.Location(*sourceInfo.Location()))
                {
                  theInfo.FloatValue(sourceInfo.FloatValue());
                }
              }
            }
          }
        }
      }
    }
  }
}

static NFmiQueryInfo MakeCombinedInnerInfo(vector<NFmiQueryData *> &theDataList,
                                           const NFmiProducer &theWantedProducer)
{
  if (theDataList.size() == 0)
    throw runtime_error("Error in MakeCombinedInnerInfo: no query datas in list.");

  if (theDataList.size() == 1) return *(theDataList[0]->Info());

  set<NFmiMetTime> allTimes;
  set<NFmiStation> allStations;
  unsigned int maxLevelSize = 0;

  const NFmiVPlaceDescriptor *maxLevelVPlaceDesc =
      0;  // otetaan talteen sen datan vplaceDesc, jossa eniten leveleitä

  NFmiMetTime originTime;  // otetaan vain currentti aika origin timeksi
  for (unsigned int i = 0; i < theDataList.size(); i++)
  {
    NFmiFastQueryInfo info(theDataList[i]);

    for (info.ResetTime(); info.NextTime();)
      allTimes.insert(info.Time());
    for (info.ResetLocation(); info.NextLocation();)
      allStations.insert(*(static_cast<const NFmiStation *>(info.Location())));

    if (maxLevelSize < info.SizeLevels())
    {
      maxLevelSize = info.SizeLevels();
      maxLevelVPlaceDesc = &(theDataList[i]->Info()->VPlaceDescriptor());
    }
  }

  // Tehdään kaikkia datoja yhdistävä timeDescriptor
  NFmiTimeList timeList;
  for (set<NFmiMetTime>::iterator it1 = allTimes.begin(); it1 != allTimes.end(); ++it1)
    timeList.Add(new NFmiMetTime(*it1));
  NFmiTimeDescriptor timeDesc(originTime, timeList);

  // Otetaan 1. datasta paramdesc, koska se on sama kaikille
  NFmiParamBag paramBag;
  NFmiParamDescriptor paramDesc(theDataList[0]->Info()->ParamDescriptor());
  paramDesc.SetProducer(theWantedProducer);

  NFmiLocationBag locationBag;
  for (set<NFmiStation>::iterator it2 = allStations.begin(); it2 != allStations.end(); ++it2)
    locationBag.AddLocation(*it2, false);
  NFmiHPlaceDescriptor hplaceDesc(locationBag);

  NFmiQueryInfo info(paramDesc, timeDesc, hplaceDesc, *maxLevelVPlaceDesc);
  return info;
}

static NFmiQueryData *CombineQueryDatas(vector<NFmiQueryData *> &theDataList,
                                        const NFmiProducer &theWantedProducer)
{
  NFmiQueryInfo innerInfo(MakeCombinedInnerInfo(theDataList, theWantedProducer));
  NFmiQueryData *data = NFmiQueryDataUtil::CreateEmptyData(innerInfo);
  if (data)
  {
    NFmiFastQueryInfo info(data);
    ::FillCombinedSoundingData(theDataList, info);
    return data;
  }

  throw runtime_error("Error in CombineQueryDatas: couldn't create query data.");
}

void Domain(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "s!p!t");

  // Tarkistetaan optioiden oikeus:

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  int numOfParams = cmdline.NumberofParameters();
  if (numOfParams < 1)
  {
    cerr << "Error: Atleast 1 parameter expected, 'fileFilter1 [fileFilter2 ...]'\n\n";
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  NFmiAviationStationInfoSystem stationInfoSystem(true, false);
  if (cmdline.isOption('s'))
  {
    string stationInfoFile = cmdline.OptionValue('s');
    stationInfoSystem.InitFromMasterTableCsv(stationInfoFile);
  }
  else
    stationInfoSystem.InitFromMasterTableCsv(default_stations_file);

  NFmiProducer wantedProducer(1005, "UAIR");
  if (cmdline.isOption('p'))
  {
    std::vector<std::string> strVector = NFmiStringTools::Split(cmdline.OptionValue('p'), ",");
    if (strVector.size() != 2)
      throw runtime_error(
          "Error: with p-option 2 comma separated parameters expected (e.g. 1005,UAIR)");

    unsigned long prodId = NFmiStringTools::Convert<unsigned long>(strVector[0]);
    wantedProducer = NFmiProducer(prodId, strVector[1]);
  }

  bool roundTimesToNearestSynopticTimes = false;
  if (cmdline.isOption('t')) roundTimesToNearestSynopticTimes = true;

  //	1. Lue n kpl filefiltereitä listaan
  vector<string> fileFilterList;
  for (int i = 1; i <= numOfParams; i++)
  {
    fileFilterList.push_back(cmdline.Parameter(i));
  }

  NFmiPoint startingLatlonForUnknownStation = NFmiPoint::gMissingLatlon;
  bool foundAnyFiles = false;
  bool couldDecodeSoundings = false;
  vector<NFmiQueryData *> qDataList;
  for (unsigned int j = 0; j < fileFilterList.size(); j++)
  {
    //	2. Hae jokaista filefilteriä vastaavat tiedostonimet omaan listaan
    std::string filePatternStr = fileFilterList[j];
    std::string usedPath = NFmiFileSystem::PathFromPattern(filePatternStr);
    list<string> fileList = NFmiFileSystem::PatternFiles(filePatternStr);
    for (list<string>::iterator it = fileList.begin(); it != fileList.end(); ++it)
    {
      //	3. Lue listan tiedostot vuorollaan sisään ja tulkitse siitä mahdolliset TEMPit
      //querydataksi
      std::string finalFileName = usedPath + *it;
      foundAnyFiles = true;
      string tempFileContent;
      //			if(NFmiFileSystem::ReadFile2String(wantedPath + fileListVec[k],
      //tempFileContent))
      if (NFmiFileSystem::ReadFile2String(finalFileName, tempFileContent))
      {
        //	4. tulkitse siitä mahdolliset TEMPit querydataksi
        string errorStr;
        NFmiQueryData *data = DecodeTEMP::MakeNewDataFromTEMPStr(tempFileContent,
                                                                 errorStr,
                                                                 stationInfoSystem,
                                                                 startingLatlonForUnknownStation,
                                                                 wantedProducer,
                                                                 roundTimesToNearestSynopticTimes);
        //	5. talleta syntyneet querydatat listaan
        if (data)
        {
          qDataList.push_back(data);
          couldDecodeSoundings = true;
        }
      }
    }
  }
  if (foundAnyFiles == false) throw runtime_error("Error: Didn't find any files to read.");
  if (couldDecodeSoundings == false)
    throw runtime_error("Error: Couldn't decode any soundings from any files.");
  //	6. yhdistä lopuksi querydata yhdeksi kokonaisuudeksi
  NFmiQueryData *bigQData = ::CombineQueryDatas(qDataList, wantedProducer);

  //	7. talleta querydata output:iin
  if (bigQData)
  {
    //		auto_ptr<NFmiQueryData> bigQDataPtr(bigQData); // tuhoaa datan automaattisesti
    //lopuksi
    NFmiStreamQueryData sQOutData(bigQData);
    if (!sQOutData.WriteCout())
      throw runtime_error("Error: Couldn't write combined qdata to stdout.");
  }
  else
    throw runtime_error("Error CombineQueryDatas: coudn't combine datas.");
}
