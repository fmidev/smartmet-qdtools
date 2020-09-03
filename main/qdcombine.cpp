// qdcombine Tekijä Marko 30.8.2005
// Yrittää yhdistää halutusta hakemistosta löytyneet tiedostot
// aikojen/parametrien/leveleiden mukaan ja tuottaa niistä yksi
// yhteinen data tiedosto.

#include <newbase/NFmiArea.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiTotalWind.h>
#include <newbase/NFmiWeatherAndCloudiness.h>
#include <iostream>
#include <utility>

using namespace std;

static const int kDontUseForcedLevelValue = -9999999;

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cout << "Usage: qdcombine [options] directory [file2 file3...]> output" << endl
       << "       qdcombine [options] -o [outfile] directory [file2 file3...]" << endl
       << "       qdcombine [options] -O [mmapped_outfile] directory [file2 file3...]" << endl
       << endl
       << "The program combines all querydatas from given directory to one file if possible."
       << endl
       << "If multiple arguments are given, each one is processedn as a file, or as" << endl
       << "the latest file in a directory." << endl
       << endl
       << "The program makes combined times/params/level to output file." << endl
       << "Querydata in files must have the same projection and grid size" << endl
       << "unless point data is being combined." << endl
       << endl
       << "Options:" << endl
       << endl
       << "   -o outfile           Output filename (default = stdout)" << endl
       << "   -O outfile           Memory mapped output filename" << endl
       << "   -P                   Combine point data instead of grid data" << endl
       << "   -l levelType[,value] Set level type and value (e.g. 5000,0 would be normal ground "
          "data)."
       << endl
       << "   -p producer_id,name  Set producer id and name (for example 240,ecmwf)." << endl
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

static void ReadGridData(vector<string> &theDataFileNames, MyGrid &theUsedGrid)
{
  for (unsigned int i = 0; i < theDataFileNames.size(); i++)
  {
    NFmiQueryData qd(theDataFileNames[i]);

    const NFmiGrid *grid = qd.Info()->Grid();

    if (grid)
    {
      // otetaan 1. löytynyt gridi käyttöön lopulliseen dataan
      theUsedGrid = MyGrid(*grid);
      return;
    }
  }
}

static void FillCombinedData(const vector<string> &theDataFileNames,
                             NFmiFastQueryInfo &theInfo,
                             NFmiLevel *theForcedLevel)
{
  MyGrid usedGrid(*theInfo.Grid());
  for (unsigned int i = 0; i < theDataFileNames.size(); i++)
  {
    NFmiQueryData qd(theDataFileNames[i]);
    NFmiFastQueryInfo sourceInfo(&qd);

    if (sourceInfo.Grid() && usedGrid == *sourceInfo.Grid())
    {
      typedef std::pair<unsigned int, unsigned int> IndexPair;
      typedef std::vector<IndexPair> Indexes;
      Indexes params, times, levels;

      // Collect indexes for common parameters, times and levels
      for (sourceInfo.ResetParam(); sourceInfo.NextParam();)
        if (theInfo.Param(*(sourceInfo.Param().GetParam())))
          params.push_back(IndexPair(sourceInfo.ParamIndex(), theInfo.ParamIndex()));

      for (sourceInfo.ResetTime(); sourceInfo.NextTime();)
        if (theInfo.Time(sourceInfo.Time()))
          times.push_back(IndexPair(sourceInfo.TimeIndex(), theInfo.TimeIndex()));

      for (sourceInfo.ResetLevel(); sourceInfo.NextLevel();)
        if (theForcedLevel || theInfo.Level(*sourceInfo.Level()))
          levels.push_back(IndexPair(sourceInfo.LevelIndex(), theInfo.LevelIndex()));

      // We avoid unnecessary location loops with this extra test

      if (!params.empty() && !times.empty() && !levels.empty())
      {
        for (const IndexPair &param : params)
        {
          sourceInfo.ParamIndex(param.first);
          theInfo.ParamIndex(param.second);

          for (sourceInfo.ResetLocation(), theInfo.ResetLocation();
               sourceInfo.NextLocation() && theInfo.NextLocation();)
          {
            for (const IndexPair &level : levels)
            {
              sourceInfo.LevelIndex(level.first);
              theInfo.LevelIndex(level.second);

              for (const IndexPair &time : times)
              {
                sourceInfo.TimeIndex(time.first);
                theInfo.TimeIndex(time.second);

                if (theInfo.FloatValue() == kFloatMissing)
                  theInfo.FloatValue(sourceInfo.FloatValue());
              }  // times
            }    // levels
          }      // locations
        }        // params
      }          // if !empty
    }            // if same grid
  }              // for files
}

static void FillCombinedData(const vector<string> &theDataFileNames,
                             bool use_point_data,
                             NFmiFastQueryInfo &theInfo,
                             NFmiLevel *theForcedLevel)
{
  if (!use_point_data)
    FillCombinedData(theDataFileNames, theInfo, theForcedLevel);
  else
  {
    for (unsigned int i = 0; i < theDataFileNames.size(); i++)
    {
      NFmiQueryData qd(theDataFileNames[i]);
      NFmiFastQueryInfo sourceInfo(&qd);

      if (!sourceInfo.Grid())
      {
        typedef std::pair<unsigned int, unsigned int> IndexPair;
        typedef std::vector<IndexPair> Indexes;
        Indexes params, times, levels, locations;

        // Collect indexes for common parameters, times, levels and locations
        for (sourceInfo.ResetParam(); sourceInfo.NextParam();)
          if (theInfo.Param(*(sourceInfo.Param().GetParam())))
            params.push_back(IndexPair(sourceInfo.ParamIndex(), theInfo.ParamIndex()));

        for (sourceInfo.ResetTime(); sourceInfo.NextTime();)
          if (theInfo.Time(sourceInfo.Time()))
            times.push_back(IndexPair(sourceInfo.TimeIndex(), theInfo.TimeIndex()));

        for (sourceInfo.ResetLevel(); sourceInfo.NextLevel();)
          if (theForcedLevel || theInfo.Level(*sourceInfo.Level()))
            levels.push_back(IndexPair(sourceInfo.LevelIndex(), theInfo.LevelIndex()));

        for (sourceInfo.ResetLocation(); sourceInfo.NextLocation();)
          if (theInfo.Location(sourceInfo.Location()->GetIdent()))
            locations.push_back(IndexPair(sourceInfo.LocationIndex(), theInfo.LocationIndex()));

        // We avoid unnecessary location loops with this extra test

        if (!params.empty() && !times.empty() && !levels.empty() && !locations.empty())
        {
          for (const IndexPair &param : params)
          {
            sourceInfo.ParamIndex(param.first);
            theInfo.ParamIndex(param.second);

            for (const IndexPair &loc : locations)
            {
              sourceInfo.LocationIndex(loc.first);
              theInfo.LocationIndex(loc.second);

              for (const IndexPair &level : levels)
              {
                sourceInfo.LevelIndex(level.first);
                theInfo.LevelIndex(level.second);

                for (const IndexPair &time : times)
                {
                  sourceInfo.TimeIndex(time.first);
                  theInfo.TimeIndex(time.second);

                  if (theInfo.FloatValue() == kFloatMissing)
                    theInfo.FloatValue(sourceInfo.FloatValue());
                }  // times
              }    // levels
            }      // locations
          }        // params
        }          // if !empty
      }            // if same grid
    }              // for files
  }
}

struct LevelLessThan
{
  bool operator()(const NFmiLevel &l1, const NFmiLevel &l2)
  {
    if (l1.LevelType() < l2.LevelType()) return true;
    if (l1.LevelValue() < l2.LevelValue()) return true;
    return false;
  }
};

static NFmiQueryInfo MakeCombinedInnerInfo(const vector<string> &dataFileNames,
                                           bool use_point_data,
                                           MyGrid &theUsedGrid,
                                           NFmiLevel *theForcedLevel,
                                           NFmiProducer *theWantedProducer)
{
  if (dataFileNames.empty()) throw std::runtime_error("Attempting to combine zero querydatas");

  set<NFmiMetTime> allTimes;
  set<NFmiParam> allParams;  // huom! parametrin identti on ainoa mikä ratkaisee NFmiParm:in
                             // ==-operaattorissa, joten tämä toimii vaikka muut arvot esim. nimi
                             // olisivat mitä
  set<NFmiLevel, LevelLessThan> allLevels;
  if (theForcedLevel) allLevels.insert(*theForcedLevel);
  // otetaan 1. datasta tuottaja ellei ole annettu pakotettua tuottajaa

  NFmiQueryData qd(dataFileNames[0]);
  NFmiFastQueryInfo qi(&qd);

  NFmiMetTime originTime = qi.OriginTime();

  NFmiProducer usedProducer =
      theWantedProducer ? *theWantedProducer : *(qi.Producer());  // oletus pitää olla väh. 1 data
                                                                  // listassa että tullaan tähän
                                                                  // funktioon.

  NFmiParamBag firstParaBag = qi.ParamBag();
  bool isAllParamBagsIdentical = true;  // jos kaikki parambagit olivat identtisiä, kopioidaan se
  // suoraan käyttöön, koska muuten parametrien järjestys voi
  // muuttua

  std::set<NFmiStation> stations;  // for point data only

  for (unsigned int i = 0; i < dataFileNames.size(); i++)
  {
    NFmiQueryData data(dataFileNames[i]);
    NFmiFastQueryInfo info(&data);

    bool ok = false;
    if (use_point_data)
      ok = !info.Grid();
    else
      ok = (info.Grid() && theUsedGrid == *info.Grid());

    if (ok)
    {
      isAllParamBagsIdentical &= (firstParaBag == info.ParamBag());

      for (info.ResetTime(); info.NextTime();)
        allTimes.insert(info.Time());

      for (info.ResetParam(); info.NextParam();)
        allParams.insert(*(info.Param().GetParam()));

      if (!theForcedLevel)
        for (info.ResetLevel(); info.NextLevel();)
          allLevels.insert(*(info.Level()));

      if (use_point_data)
      {
        for (info.ResetLocation(); info.NextLocation();)
        {
          auto *loc = info.Location();
          // loc may by NFmiStation, NFmiLocation or NFmiRadarStation. We select NFmiStation
          NFmiStation station(
              loc->GetIdent(), loc->GetName(), loc->GetLongitude(), loc->GetLatitude());
          stations.insert(station);
        }
      }
    }
  }

  // Tehdään kaikkia datoja yhdistävä tdescriptor
  NFmiTimeList timeList;
  for (set<NFmiMetTime>::iterator it1 = allTimes.begin(); it1 != allTimes.end(); ++it1)
    timeList.Add(new NFmiMetTime(*it1));
  NFmiTimeDescriptor tdesc(originTime, timeList);

  // Tehdään kaikkia datoja yhdistävä paramDescriptor
  NFmiParamBag paramBag;
  if (isAllParamBagsIdentical)
    paramBag = firstParaBag;
  else
  {
    for (set<NFmiParam>::iterator it2 = allParams.begin(); it2 != allParams.end(); ++it2)
    {
      FmiParameterName parId = static_cast<FmiParameterName>((*it2).GetIdent());
      if (parId == kFmiTotalWindMS ||
          parId == kFmiWeatherAndCloudiness)  // HUOM! yhdistelmä parametrit totalWind ja
                                              // weatherAndCloudiness ovat erikois tapauksia!!!
      {
        NFmiTotalWind totWind;  // pitäisi muuttaa kyseisten luokkien CreateParam
                                // static-metodiksi, niin koodista tulisi fiksumpaa
        NFmiWeatherAndCloudiness weatAndC;
        NFmiDataIdent *dataIdent = (parId == kFmiTotalWindMS) ? totWind.CreateParam(usedProducer)
                                                              : weatAndC.CreateParam(usedProducer);
        paramBag.Add(*dataIdent);
        delete dataIdent;
      }
      else
        paramBag.Add(NFmiDataIdent(*it2, usedProducer));
    }
  }
  NFmiParamDescriptor pdesc(paramBag);

  // Tehdään kaikkia datoja yhdistävä vPlaceDescriptor
  NFmiLevelBag levelBag;
  for (set<NFmiLevel, LevelLessThan>::iterator it3 = allLevels.begin(); it3 != allLevels.end();
       ++it3)
    levelBag.AddLevel(*it3);
  NFmiVPlaceDescriptor vdesc(levelBag);

  if (use_point_data)
  {
    NFmiLocationBag lbag;
    for (const auto &station : stations)
      lbag.AddLocation(station);
    NFmiHPlaceDescriptor hdesc(lbag);
    NFmiQueryInfo info(pdesc, tdesc, hdesc, vdesc);
    return info;
  }
  else
  {
    NFmiGrid grid(theUsedGrid.itsArea, theUsedGrid.itsNX, theUsedGrid.itsNY);
    NFmiHPlaceDescriptor hdesc(grid);
    NFmiQueryInfo info(pdesc, tdesc, hdesc, vdesc);
    return info;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program minus exception catching
 */
// ----------------------------------------------------------------------

int Run(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "l!p!o!O!P");

  // Tarkistetaan optioiden oikeus:

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.NumberofParameters() < 1)
  {
    cerr << "Error: Atleast 1 parameter expected, 'directory'\n\n";
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  bool use_point_data = cmdline.isOption('P');

  NFmiLevel *forcedLevel = 0;
  if (cmdline.isOption('l'))
  {
    string levelForceStr = cmdline.OptionValue('l');
    vector<string> optionsList = NFmiStringTools::Split(levelForceStr, ",");
    if (optionsList.size() != 2)
      throw runtime_error(std::string("Level option invalid must be levelType,levelValue."));

    FmiLevelType forcedLevelType =
        static_cast<FmiLevelType>(NFmiStringTools::Convert<int>(optionsList[0]));
    int forcedLevelValue = NFmiStringTools::Convert<int>(optionsList[1]);
    forcedLevel = new NFmiLevel(forcedLevelType, forcedLevelValue);
  }

  NFmiProducer *wantedProducer = 0;
  if (cmdline.isOption('p'))
  {
    std::vector<std::string> strVector = NFmiStringTools::Split(cmdline.OptionValue('p'), ",");
    if (strVector.size() != 2)
      throw runtime_error(
          "Error: with p-option 2 comma separated parameters expected (e.g. 240,ecmwf)");
    unsigned long prodId = NFmiStringTools::Convert<unsigned long>(strVector[0]);
    wantedProducer = new NFmiProducer(prodId, strVector[1]);
  }
  unique_ptr<NFmiProducer> wantedProducerPtr(
      wantedProducer);  // tämä tuhoaa dynaamisen datan automaattisesti

  std::string outfile = "-";
  bool mmapped = false;

  if (cmdline.isOption('o')) outfile = cmdline.OptionValue('o');

  if (cmdline.isOption('O'))
  {
    outfile = cmdline.OptionValue('O');
    mmapped = true;
  }

  vector<string> dataFileNames;

  if (cmdline.NumberofParameters() == 1)
  {
    string dataDirectory = cmdline.Parameter(1);
    if (!NFmiFileSystem::DirectoryExists(dataDirectory))
      throw runtime_error(
          std::string("Given data directory '" + dataDirectory + "' doesn't exist, exiting...\n"));

    list<string> dataFileNamesTmp = NFmiFileSystem::DirectoryFiles(dataDirectory);
    for (list<string>::const_iterator ft = dataFileNamesTmp.begin(); ft != dataFileNamesTmp.end();
         ++ft)
    {
      string fullname = dataDirectory + "/" + *ft;
      dataFileNames.push_back(fullname);
    }
  }
  else
  {
    // Take newest file only from each directory
    for (int param = 1; param <= cmdline.NumberofParameters(); ++param)
    {
      string p = cmdline.Parameter(param);
      if (!NFmiFileSystem::DirectoryExists(p))
        dataFileNames.push_back(p);
      else
      {
        string tmp = NFmiFileSystem::NewestFile(p);
        string fullname = p + "/" + tmp;
        dataFileNames.push_back(fullname);
      }
    }
  }

  MyGrid usedGrid;
  if (!use_point_data) ::ReadGridData(dataFileNames, usedGrid);

  NFmiQueryInfo innerInfo =
      ::MakeCombinedInnerInfo(dataFileNames, use_point_data, usedGrid, forcedLevel, wantedProducer);

  if (!mmapped)
  {
    NFmiQueryData *newData = NFmiQueryDataUtil::CreateEmptyData(innerInfo);
    if (newData)
    {
      NFmiFastQueryInfo info(newData);
      ::FillCombinedData(dataFileNames, use_point_data, info, forcedLevel);
      newData->Write(outfile);
    }
    delete newData;
  }
  else
  {
    NFmiQueryData *newData = NFmiQueryDataUtil::CreateEmptyData(innerInfo, outfile, true);
    NFmiFastQueryInfo info(newData);
    ::FillCombinedData(dataFileNames, use_point_data, info, forcedLevel);
    delete newData;
  }

  return 0;
}

int main(int argc, const char *argv[]) try
{
  return Run(argc, argv);
}
catch (exception &e)
{
  cerr << e.what() << endl;
  return 1;
}
