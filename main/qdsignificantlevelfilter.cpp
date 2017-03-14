#include <NFmiQueryData.h>
#include <NFmiCmdLine.h>
#include <NFmiFastQueryInfo.h>
#include <NFmiStreamQueryData.h>
#include <NFmiFileSystem.h>
#include <NFmiQueryDataUtil.h>
#include <NFmiTotalWind.h>
#include <NFmiTimeList.h>

#include <fstream>

using namespace std;

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------
void Usage(void)
{
    cout << "Usage: qdsignificantlevelfilter [options] qdin qdout" << endl;
    cout << "qdin must have station data and has multiple levels and has " << endl;
    cout << "parameter kFmiVerticalSoundingSignificance = 708" << endl;
    cout << "then all but non-significant levels will be removed from data." << endl;
    cout << "Used with new high resolution sounding data with thousands of levels." << endl;
    cout << "-c combinedData\tCombine filtered data with given data so that." << endl;
    cout << "\tthe filtered has priority over other data cause it's high resolution." << endl;
}

static void MakeErrorAndUsageMessages(const string &errorMessage)
{
    cout << errorMessage << endl;
    ::Usage();
    // t‰ss‰ piti ensin tulostaa cout:iin tavaraa ja sitten vasta Usage,
    // joten en voinut laittaa virheviesti poikkeuksen mukana.
    throw runtime_error("");
}

static void MakeCheckDataError(const string &filePath, const string &baseErrorString)
{
    string errorString = baseErrorString;
    errorString += "\nFile was: ";
    errorString += filePath;
    ::MakeErrorAndUsageMessages(errorString);
}

static void CheckData(NFmiFastQueryInfo &info, const string &filePath, bool needToHaveSignificantParam)
{
    if(info.IsGrid())
        MakeCheckDataError(filePath, "Error: input data was grid data, can't remove non-significant levels with it.");
    if(info.SizeLevels() < 2)
        MakeCheckDataError(filePath, "Error: input data was grid data, can't remove non-significant levels with it.");
    if(needToHaveSignificantParam && !info.Param(kFmiVerticalSoundingSignificance))
        MakeCheckDataError(filePath, "Error: input data was grid data, can't remove non-significant levels with it.");
}

// map key is location-index and time-index pair
using LevelMapKey = pair<unsigned long, unsigned long>;
using SignificantLevelsMap = map<LevelMapKey, NFmiQueryDataUtil::SignificantSoundingLevels>;

// Calculate all siginicant level indices from every station and for every time
static void FillSignificantLevelMap(NFmiFastQueryInfo &info, SignificantLevelsMap &levelsMap)
{
    info.First();
    if(info.Param(kFmiVerticalSoundingSignificance))
    {
        for(info.ResetLocation(); info.NextLocation(); )
        {
            for(info.ResetTime(); info.NextTime(); )
            {
                LevelMapKey mapKey(info.LocationIndex(), info.TimeIndex());
                levelsMap[mapKey] = NFmiQueryDataUtil::GetSignificantSoundingLevelIndices(info);
            }
        }
    }
}

static unsigned long FindMaxLevelCount(const SignificantLevelsMap &levelsMap)
{
    unsigned long maxLevelCount = 0;
    for(const auto &levels : levelsMap)
    {
        if(levels.second)
        {
            unsigned long tmpLevelCount = static_cast<unsigned long>(levels.second->size());
            if(maxLevelCount < tmpLevelCount)
                maxLevelCount = tmpLevelCount;
        }
    }
    return maxLevelCount;
}

static NFmiQueryInfo MakeNewMetaInfo(NFmiFastQueryInfo &info, unsigned long newLevelSize)
{
    // Let's take only those levels into  new levelBag that is needed
    NFmiLevelBag levelBag(info.VPlaceDescriptor().LevelBag().Level(0), newLevelSize);
    NFmiVPlaceDescriptor levelDescriptor(levelBag);

    return NFmiQueryInfo(info.ParamDescriptor(), info.TimeDescriptor(), info.HPlaceDescriptor(), levelDescriptor);
}

static unique_ptr<NFmiQueryData> CreateNewEmtyData(NFmiFastQueryInfo &info, const SignificantLevelsMap &levelsMap)
{
    auto maxLevels = ::FindMaxLevelCount(levelsMap);
    auto newMetaInfo = ::MakeNewMetaInfo(info, maxLevels);
    return unique_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(newMetaInfo));
}

static void FillSignificantLevelData(NFmiFastQueryInfo &sourceInfo, NFmiFastQueryInfo &destInfo, const NFmiQueryDataUtil::SignificantSoundingLevels &levelIndicesPtr)
{
    if(levelIndicesPtr)
    {
        const auto &levelIndices = *levelIndicesPtr;
        destInfo.FirstLevel();
        for(auto levelIndex : levelIndices)
        {
            sourceInfo.LevelIndex(levelIndex);
            destInfo.FloatValue(sourceInfo.FloatValue());
            destInfo.NextLevel();
        }
    }
}

static void FillNewData(const unique_ptr<NFmiQueryData> &newData, NFmiFastQueryInfo &sourceInfo, SignificantLevelsMap &levelsMap)
{
    NFmiFastQueryInfo destInfo(newData.get());
    sourceInfo.First();
    for(destInfo.ResetParam(); destInfo.NextParam(); )
    {
        sourceInfo.ParamIndex(destInfo.ParamIndex());
        for(destInfo.ResetLocation(); destInfo.NextLocation(); )
        {
            sourceInfo.LocationIndex(destInfo.LocationIndex());
            for(destInfo.ResetTime(); destInfo.NextTime(); )
            {
                sourceInfo.TimeIndex(destInfo.TimeIndex());
                LevelMapKey levelMapKey(destInfo.LocationIndex(), destInfo.TimeIndex());
                ::FillSignificantLevelData(sourceInfo, destInfo, levelsMap[levelMapKey]);
            }
        }
    }
}

static unique_ptr<NFmiQueryData> CreateFilteredData(NFmiFastQueryInfo &info)
{
    SignificantLevelsMap levelsMap;
    ::FillSignificantLevelMap(info, levelsMap);
    unique_ptr<NFmiQueryData> newData = ::CreateNewEmtyData(info, levelsMap);
    ::FillNewData(newData, info, levelsMap);
    return newData;
}

static void WriteQueryData(unique_ptr<NFmiQueryData> &data, const string &filePath)
{
    if(!data)
        ::MakeErrorAndUsageMessages("Error: unable to create filtered data.");
    else
    {
        if(!NFmiFileSystem::CreateDirectory(NFmiFileSystem::PathFromPattern(filePath)))
        {
            string errorMessage = "Error: unable to create target directory in:\n";
            errorMessage += NFmiFileSystem::PathFromPattern(filePath);
            ::MakeErrorAndUsageMessages(errorMessage);
        }
        NFmiStreamQueryData sqData;
        if(!sqData.WriteData(filePath, data.get(), static_cast<long>(data->InfoVersion())))
        {
            string errorMessage = "Error: unable to write data to file:\n";
            errorMessage += filePath;
            ::MakeErrorAndUsageMessages(errorMessage);
        }
    }
}

// Oletus datoissa on aina locationit, ei hilaa.
// T‰m‰ funktio piti tehd‰, koska NFmiHPlaceDescriptor:in Combine ei tarkastele mielest‰ni oikein asemien 
// samanlaisuutta. Se tutkii vain onko asema saman niminen ja onko sill‰ tarkalleen samat koordinaatit.
// Min‰ taas haen sit‰ ett‰ lˆytyykˆ asemilta sama id numero (nimet on saatettu hakea erilaisilla par-tiedotoilla
// ja latlon pisteiden koordinaatit on saatettu hakea eri tarkkuuksilla tms.)
// Huom! Tarkotuksella kopiot, koska muuten pit‰‰ alkaa kikkailemaan const_cast:eilla...
static NFmiHPlaceDescriptor MakeCombinedLocationsByIdent(NFmiHPlaceDescriptor locations1, NFmiHPlaceDescriptor locations2)
{
    auto lessThanLocation = [](const NFmiLocation *location1, const NFmiLocation *location2) { return location1->GetIdent() < location2->GetIdent(); };

    // Huom! t‰ss‰ set:iss‰ olevat location oliot ovat vain lainassa, niit‰ ei saa tuhota.
    set<const NFmiLocation*, decltype(lessThanLocation)> uniqueLocations(lessThanLocation);
    for(locations1.Reset(); locations1.Next(); )
        uniqueLocations.insert(locations1.Location());
    for(locations2.Reset(); locations2.Next(); )
        uniqueLocations.insert(locations2.Location());

    NFmiLocationBag combinedLocations;
    for(auto location : uniqueLocations)
    {
        combinedLocations.AddLocation(*location);
    }
    return NFmiHPlaceDescriptor(combinedLocations);
}

static NFmiDataIdent GetUsedWindParameter(NFmiParamDescriptor params1, NFmiParamDescriptor params2)
{
    if(params1.Param(kFmiTotalWindMS))
        return params1.Param();
    else if(params2.Param(kFmiTotalWindMS))
        return params2.Param();
    else
    {
        params1.Index(0);
        NFmiTotalWind wind;
        unique_ptr<NFmiDataIdent> windPtr(wind.CreateParam(*params1.Param().GetProducer()));
        return *windPtr;
    }
}

static bool IsNotTotalWindSubParam(const NFmiDataIdent &dataIdent)
{
    auto paramIdent = dataIdent.GetParamIdent();
    if(paramIdent != kFmiWindDirection && paramIdent != kFmiWindSpeedMS && paramIdent != kFmiWindUMS && paramIdent != kFmiWindVMS)
        return true;
    else
        return false;
}

template<typename Container>
static void RemoveSecondaryDewPointParam(Container &uniqueParams)
{
    auto equalParam = [](const NFmiDataIdent &dataIdent1, const NFmiDataIdent &dataIdent2) { return dataIdent1.GetParamIdent() == dataIdent2.GetParamIdent(); };
    NFmiDataIdent Td1(NFmiParam(kFmiDewPoint, "Td"));
    bool containsTd1 = uniqueParams.find(Td1) != uniqueParams.end();
    NFmiDataIdent Td2(NFmiParam(kFmiDewPoint2M, "Td2"));
    bool containsTd2 = uniqueParams.find(Td2) != uniqueParams.end();
    if(containsTd1 && containsTd2)
        uniqueParams.erase(Td2);
}

// Sama juttu parametrien kanssa kuin locationien.
// Tulos dataan laitetaan aina TotalWind parametri ja siksi siit‰ on poistettava sen aliparametrit p‰‰tasolta.
// Tulosdataan laitetaan 
static NFmiParamDescriptor MakeCombinedParamsByIdent(NFmiParamDescriptor params1, NFmiParamDescriptor params2)
{
    auto lessThanParam = [](const NFmiDataIdent &dataIdent1, const NFmiDataIdent &dataIdent2) { return dataIdent1.GetParamIdent() < dataIdent2.GetParamIdent(); };

    // Huom! t‰ss‰ set:iss‰ olevat location oliot ovat vain lainassa, niit‰ ei saa tuhota.
    set<NFmiDataIdent, decltype(lessThanParam)> uniqueParams(lessThanParam);
    for(params1.Reset(); params1.Next(); )
        uniqueParams.insert(params1.Param());
    for(params2.Reset(); params2.Next(); )
        uniqueParams.insert(params2.Param());
    // Varmistetaan ett‰ parametrien joukossa on totalwind
    uniqueParams.insert(::GetUsedWindParameter(params1, params2));
    ::RemoveSecondaryDewPointParam(uniqueParams);
    NFmiParamBag combinedParams;
    for(const auto &param : uniqueParams)
    {
        if(::IsNotTotalWindSubParam(param))
        {
            combinedParams.Add(param);
        }
    }
    return NFmiParamDescriptor(combinedParams);
}

static void AddTimesToSet(NFmiFastQueryInfo &info, set<NFmiMetTime> &uniqueTimes)
{
    for(info.ResetTime(); info.NextTime(); )
    {
        uniqueTimes.insert(info.Time());
    }
}

static bool HasAnyDataAtThatTime(NFmiFastQueryInfo &info, const NFmiMetTime &time)
{
    if(info.Time(time))
    {
        // Riitt‰‰ kun tutkitaan ett‰ lˆytyykˆ mit‰‰n arvoja paina parametrista milt‰‰n asemalta milt‰‰n levelilt‰
        if(info.Param(kFmiPressure))
        {
            for(info.ResetLocation(); info.NextLocation(); )
            {
                for(info.ResetLevel(); info.NextLevel(); )
                {
                    if(info.FloatValue() != kFloatMissing)
                        return true;
                }
            }
        }
    }
    return false;
}

// Otetaan molemmista datoista kaikki ajat uniikki aika set:iin.
// K‰yd‰‰n jokainen aika l‰pi molemmista datoista ja jos yht‰t‰‰n luotaus dataa ei lˆydy kummastakaan datasta
// millek‰‰n asemalle, poistetaan kyseinen aika listasta.
static NFmiTimeDescriptor MakeCombinedTimesWithRealData(NFmiQueryData &filteredData, NFmiQueryData &lowResolutionData)
{
    set<NFmiMetTime> uniqueTimes;
    NFmiFastQueryInfo filteredInfo(&filteredData);
    ::AddTimesToSet(filteredInfo, uniqueTimes);
    NFmiFastQueryInfo lowResolutionInfo(&lowResolutionData);
    ::AddTimesToSet(lowResolutionInfo, uniqueTimes);
    NFmiTimeList timesWithData;
    for(auto &time : uniqueTimes)
    {
        if(::HasAnyDataAtThatTime(filteredInfo, time) || ::HasAnyDataAtThatTime(lowResolutionInfo, time))
            timesWithData.Add(new NFmiMetTime(time), false, true);
    }
    return NFmiTimeDescriptor(filteredInfo.OriginTime(), timesWithData);
}

static NFmiQueryInfo MakeCombinedMetaInfo(NFmiQueryData &filteredData, NFmiQueryData &lowResolutionData)
{
    auto &filteredInfo = *filteredData.Info();
    auto &lowResolutionInfo = *lowResolutionData.Info();

    NFmiParamDescriptor combinedParams = ::MakeCombinedParamsByIdent(filteredInfo.ParamDescriptor(), lowResolutionInfo.ParamDescriptor());
    NFmiHPlaceDescriptor combinedLocations = ::MakeCombinedLocationsByIdent(filteredInfo.HPlaceDescriptor(), lowResolutionInfo.HPlaceDescriptor());
    NFmiTimeDescriptor combinedTimes = ::MakeCombinedTimesWithRealData(filteredData, lowResolutionData);
    // Kummassa datassa on enemm‰n leveleit‰, otetaan pohjaksi sellaisenaan (sounding levelit ovat vain j‰rjestysnumeroita).
    NFmiVPlaceDescriptor combinedLevels = (filteredInfo.SizeLevels() >= lowResolutionInfo.SizeLevels()) ? filteredInfo.VPlaceDescriptor() : lowResolutionInfo.VPlaceDescriptor();
    return NFmiQueryInfo(combinedParams, combinedTimes, combinedLocations, combinedLevels);
}

static void SetValue(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo, bool &dataUsed)
{
    float value = sourceInfo.FloatValue();
    if(value != kFloatMissing)
    {
        dataUsed = true;
        combinedInfo.FloatValue(value);
    }
}

// Huom! datat pit‰‰ laittaa samaan nousevaan/laskevaan j‰rjestykseen. Eli jos 
// infoissa on eri 'nousu' j‰rjestys, pit‰‰ k‰ytt‰‰ reversed funktiota k‰‰nt‰m‰‰n datan j‰rjestys.
static bool ReverseFillAllLevels(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo)
{
    bool anyDataFilled = false;
    for(combinedInfo.ResetLevel(), sourceInfo.LastLevel(); combinedInfo.NextLevel(); )
    {
        ::SetValue(combinedInfo, sourceInfo, anyDataFilled);
        sourceInfo.PreviousLevel();
    }
    return anyDataFilled;
}

static bool FillAllLevels(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo)
{
    bool anyDataFilled = false;
    for(combinedInfo.ResetLevel(), sourceInfo.ResetLevel(); combinedInfo.NextLevel() && sourceInfo.NextLevel(); )
    {
        ::SetValue(combinedInfo, sourceInfo, anyDataFilled);
    }
    return anyDataFilled;
}

static bool FillActualSoundingData2(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo, bool reverseLevels)
{
    if(reverseLevels)
        return ReverseFillAllLevels(combinedInfo, sourceInfo);
    else
        return FillAllLevels(combinedInfo, sourceInfo);
}

static bool TrySpecialDewPointCase(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo, bool reverseLevels)
{
    if(combinedInfo.Param().GetParamIdent() == kFmiDewPoint && sourceInfo.Param(kFmiDewPoint2M))
        return ::FillActualSoundingData2(combinedInfo, sourceInfo, reverseLevels);
    else
        return false;
}

static bool TrySpecialWindCase(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo, bool reverseLevels)
{
    bool anyDataFilled = false;
    if(combinedInfo.Param().GetParamIdent() == kFmiTotalWindMS && !sourceInfo.Param(kFmiTotalWindMS) && sourceInfo.Param(kFmiWindSpeedMS))
    {
        combinedInfo.Param(kFmiWindSpeedMS);
        anyDataFilled |= ::FillActualSoundingData2(combinedInfo, sourceInfo, reverseLevels);
        sourceInfo.Param(kFmiWindDirection);
        combinedInfo.Param(kFmiWindDirection);
        anyDataFilled |= ::FillActualSoundingData2(combinedInfo, sourceInfo, reverseLevels);
        combinedInfo.Param(kFmiTotalWindMS);
    }
    return anyDataFilled;
}

// Oletus, source ja combined infot on kohdillaan ja nyt vain siirret‰‰n data jokaiselta parametrilta ja levelilt‰.
// Jos yksik‰‰n asetettu data ei ollut missing, palautetaan true, muuten false.
static bool FillActualSoundingData(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo, bool reverseLevels)
{
    bool anyDataFilled = false;
    for(combinedInfo.ResetParam(); combinedInfo.NextParam(); )
    {
        if(sourceInfo.Param(static_cast<FmiParameterName>(combinedInfo.Param().GetParamIdent())))
        {
            anyDataFilled |= ::FillActualSoundingData2(combinedInfo, sourceInfo, reverseLevels);
        }
        else if(::TrySpecialDewPointCase(combinedInfo, sourceInfo, reverseLevels))
        {
            anyDataFilled = true;
        }
        else if(::TrySpecialWindCase(combinedInfo, sourceInfo, reverseLevels))
        {
            anyDataFilled = true;
        }
    }
    return anyDataFilled;
}

// Luotausdatat pit‰‰ yhdist‰‰ kokonaisia yhdest‰ l‰hteest‰ ja vain yhdest‰ l‰hteest‰ (params+levels).
// Kaikki parametrit ja levelit yritet‰‰n ott‰‰ ensin high-res datasta ja sitten jos ei lˆytynyt
// siihen dataa, yritet‰‰n low-res dataa.
static bool FillSoundingData(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &sourceInfo)
{
    if(sourceInfo.Location(combinedInfo.Location()->GetIdent()))
    {
        if(sourceInfo.Time(combinedInfo.Time()))
        {
            bool reverseLevels = combinedInfo.PressureParamIsRising() != sourceInfo.PressureParamIsRising();
            return FillActualSoundingData(combinedInfo, sourceInfo, reverseLevels);
        }
    }
    return false;
}

static void FillCombinedData(NFmiFastQueryInfo &combinedInfo, NFmiFastQueryInfo &highResolutionInfo, NFmiFastQueryInfo &lowResolutionInfo)
{
    for(combinedInfo.ResetLocation(); combinedInfo.NextLocation(); )
    {
        for(combinedInfo.ResetTime(); combinedInfo.NextTime(); )
        {
            if(!FillSoundingData(combinedInfo, highResolutionInfo))
                FillSoundingData(combinedInfo, lowResolutionInfo);
        }
    }
}

// Yhdist‰‰ filterˆydyn eli high-resoluutio datan toiseen dataan niin, ett‰ filtterˆyty‰ data
// on priorisoitu. Eli jos siit‰ ja toisesta lˆytyy luotauksia samalle asemalle samalla hetkell‰, 
// otetaan luotaus sellaisenaan filtterˆydyst‰ datasta.
// Tarkoitus on yhdist‰‰ high-res data, jossa on v‰hemm‰n asemia ja aikoja normaaliin dataan, miss‰ on kaikkea enemm‰n.
static unique_ptr<NFmiQueryData> CombineData(unique_ptr<NFmiQueryData> &&highResolutionData, const string &lowResolutionDataPath)
{
    if(!highResolutionData)
        throw runtime_error("Creating filtered data has failed for unknown reason, error in program?");

    NFmiQueryData lowResolutionData(lowResolutionDataPath);
    NFmiFastQueryInfo lowResolutionInfo(&lowResolutionData);
    ::CheckData(lowResolutionInfo, lowResolutionDataPath, false);
    NFmiQueryInfo combinedMetaData = ::MakeCombinedMetaInfo(*highResolutionData, lowResolutionData);
    unique_ptr<NFmiQueryData> combinedData(NFmiQueryDataUtil::CreateEmptyData(combinedMetaData));
    NFmiFastQueryInfo combinedInfo(combinedData.get());
    NFmiFastQueryInfo highResolutionInfo(highResolutionData.get());
    ::FillCombinedData(combinedInfo, highResolutionInfo, lowResolutionInfo);
    return combinedData;
}

void Domain(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "c!");

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
      string errorMessage("Error: Invalid command line:\n");
      errorMessage += cmdline.Status().ErrorLog();
      ::MakeErrorAndUsageMessages(errorMessage);
  }
  if(cmdline.NumberofParameters() != 2)
  {
      ::MakeErrorAndUsageMessages("Error: Invalid command line argument count, must have 2 parameters (qdin qdout)");
  }
  string fileNameIn = cmdline.Parameter(1);
  string fileNameOut = cmdline.Parameter(2);
  string lowResolutionDataPath;
  if(cmdline.isOption('c'))
  {
      lowResolutionDataPath = cmdline.OptionValue('c');
  }
  NFmiQueryData qData(fileNameIn);
  NFmiFastQueryInfo info(&qData);
  ::CheckData(info, fileNameIn, true);
  unique_ptr<NFmiQueryData> newData = CreateFilteredData(info);
  if(!lowResolutionDataPath.empty())
  {
      newData = ::CombineData(move(newData), lowResolutionDataPath);
  }
  ::WriteQueryData(newData, fileNameOut);
}

int main(int argc, const char *argv[])
{
    try
    {
        ::Domain(argc, argv);
    }
    catch(exception &e)
    {
        cout << e.what() << endl;
        return 1;
    }
    return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
