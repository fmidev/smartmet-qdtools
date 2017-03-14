// FmiNetCdfQueryData.cpp

#include "FmiNetCdfQueryData.h"
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStereographicArea.h>

#include <netcdfcpp.h>

#include <boost/algorithm/string/case_conv.hpp>

void FmiTDimVarInfo::CalcTimeList(void)
{
  itsTimeList.Clear(true);
  for (size_t i = 0; i < itsOffsetValues.size(); i++)
  {
    NFmiMetTime aTime = itsEpochTime;
    long changeByMinutesValue = itsOffsetValues[i];
    if (itsTimeOffsetType == kFmiNcSeconds) changeByMinutesValue /= 60;
    aTime.ChangeByMinutes(changeByMinutesValue);
    itsTimeList.Add(new NFmiMetTime(aTime));
  }
  if (itsTimeList.NumberOfItems() == 0)
    throw std::runtime_error("Error in FmiTDimVarInfo::CalcTimeList - no levels were found.");
}

FmiNcMetaData::FmiNcMetaData(bool useSurfaceInfo)
    : fUseSurfaceInfo(useSurfaceInfo),
      itsMultiLevelInfo(),
      itsSurfaceLevelInfo(),
      itsParams(),
      itsMetaInfo(0)
{
}

FmiNcMetaData::~FmiNcMetaData(void) { delete itsMetaInfo; }
void FmiNcMetaData::MakeMetaInfo(FmiTDimVarInfo &theTimeInfo, NFmiGrid &theGrid)
{
  delete itsMetaInfo;
  itsMetaInfo = 0;
  if (itsParams.GetSize())
  {
    if (itsMultiLevelInfo.itsNcLevelType != kFmiNcNoLevelType ||
        itsSurfaceLevelInfo.itsNcLevelType != kFmiNcNoLevelType)
    {
      bool doSurfaceInfo = (itsSurfaceLevelInfo.itsNcLevelType != kFmiNcNoLevelType);

      NFmiParamDescriptor parDesc(itsParams);
      theTimeInfo.itsTimeList.First();
      NFmiMetTime origTime = *(theTimeInfo.itsTimeList.Current());
      NFmiTimeDescriptor timeDesc(origTime, theTimeInfo.itsTimeList);
      NFmiHPlaceDescriptor hplaceDesc(theGrid);
      NFmiVPlaceDescriptor vplaceDesc;  // defaulttina normaali 'pinta' data descriptor
      if (doSurfaceInfo == false)
      {
        if (itsMultiLevelInfo.itsLevels.GetSize() >
            0)  // jos leveleit‰ on yksi tai useita, tehd‰‰n niist‰ sitten oikea levelDescriptor
          vplaceDesc = NFmiVPlaceDescriptor(itsMultiLevelInfo.itsLevels);
        else
          return;  // ei ollut oikeasti level tietoa
      }
      itsMetaInfo = new NFmiQueryInfo(parDesc, timeDesc, hplaceDesc, vplaceDesc);
    }
  }
}

std::string FmiNcMetaData::GetLevelDimName(void)
{
  if (fUseSurfaceInfo)
    return itsSurfaceLevelInfo.itsDimName;
  else
    return itsMultiLevelInfo.itsDimName;
}

FmiNcLevelType FmiNcMetaData::GetLevelType(void)
{
  if (fUseSurfaceInfo)
    return itsSurfaceLevelInfo.itsNcLevelType;
  else
    return itsMultiLevelInfo.itsNcLevelType;
}

FmiNetCdfQueryData::FmiNetCdfQueryData(void)
    : fDataOk(false),
      itsTInfo(),
      itsXInfo(),
      itsYInfo(),
      itsProjectionInfo(),
      itsNormalParameters(),
      itsGrid(),
      itsProducer(123, "NetCdf-Prod"),
      itsSurfaceMetaData(true),
      itsHeightLevelMetaData(false),
      itsPressureLevelMetaData(false),
      itsHybridLevelMetaData(false)
{
}

FmiNetCdfQueryData::~FmiNetCdfQueryData(void) {}
static NFmiQueryData *MakeQueryData(NcFile &theNcFile,
                                    NFmiQueryInfo &theMetaInfo,
                                    std::vector<FmiVarInfo> &theVarInfos)
{
  if (&theMetaInfo == 0) return 0;

  NFmiQueryData *qData = NFmiQueryDataUtil::CreateEmptyData(theMetaInfo);
  NFmiFastQueryInfo fInfo(qData);

  for (size_t i = 0; i < theVarInfos.size(); i++)
  {
    if (fInfo.Param(static_cast<FmiParameterName>(theVarInfos[i].itsParId)))  // theVarInfos:issa on
                                                                              // kaikki parametrit
                                                                              // surface ja level
                                                                              // paramit, joten
                                                                              // pit‰‰ tarkistaa,
                                                                              // lˆytyykˆ t‰st‰
                                                                              // datasta erikseen
    {
      NcVar *varPtr = theNcFile.get_var(theVarInfos[i].itsIndex);
      if (varPtr)  // Pit‰isi lˆyty‰!!
      {
        // NetCDF conventioiden mukaan juoksu j‰rjestys on:
        // aika, level, y-dim, x-dim
        int timeInd = 0;
        for (fInfo.ResetTime(); fInfo.NextTime(); timeInd++)  // juoksutetaan aika dimensiota
        {
          NcValues *vals = varPtr->get_rec(timeInd);
          long counter = 0;
          for (fInfo.ResetLevel(); fInfo.NextLevel();)  // juoksutetaan level dimensiota
          {
            for (fInfo.ResetLocation(); fInfo.NextLocation();)
            {
              float value = vals->as_float(counter);
              // jos ei ole fill-value, laitetaan arvo queryDataan, jos oli, j‰tet‰‰n qDatan missing
              // arvo voimaan (data luodan alustettuna puuttuvilla arvoilla)
              if (value != theVarInfos[i].itsFillValue) fInfo.FloatValue(value);
              counter++;
            }
          }
          delete vals;
        }
      }
    }
  }
  return qData;
}

static void AddToVector(std::vector<NFmiQueryData *> &theQDataVec, NFmiQueryData *theQData)
{
  if (theQData) theQDataVec.push_back(theQData);
}

std::vector<NFmiQueryData *> FmiNetCdfQueryData::CreateQueryDatas(const std::string &theNcFileName)
{
  std::vector<NFmiQueryData *> qDatas;
  try
  {
    NcError tmpErrorSetting(NcError::silent_nonfatal);  // pit‰‰ laittaa pois defaultti tilasta,
                                                        // koska defaultti vain tekee exit:in ilman
                                                        // sen kummempaa selittely‰ tietyiss‰ (ei
                                                        // niin fataaleissa tilanteissa)
    NcFile ncFile(theNcFileName.c_str(), NcFile::ReadOnly);
    if (ncFile.is_valid())
    {
      fDataOk = true;
      InitMetaInfo(ncFile);
      ::AddToVector(
          qDatas, ::MakeQueryData(ncFile, *(itsSurfaceMetaData.itsMetaInfo), itsNormalParameters));
      ::AddToVector(
          qDatas,
          ::MakeQueryData(ncFile, *(itsHeightLevelMetaData.itsMetaInfo), itsNormalParameters));
      ::AddToVector(
          qDatas,
          ::MakeQueryData(ncFile, *(itsPressureLevelMetaData.itsMetaInfo), itsNormalParameters));
      ::AddToVector(
          qDatas,
          ::MakeQueryData(ncFile, *(itsHybridLevelMetaData.itsMetaInfo), itsNormalParameters));
    }
    else
      throw std::runtime_error(std::string("Error nc-file is not valid netCdf: ") + theNcFileName);
  }
  catch (std::exception &e)
  {
    fDataOk = false;
    itsErrorMessage = e.what();
  }
  catch (...)
  {
    fDataOk = false;
    itsErrorMessage = "Unknown error while converting nc -> qd.";
  }
  return qDatas;
}

void FmiNetCdfQueryData::MakeAllMetaInfos(void)
{
  itsSurfaceMetaData.MakeMetaInfo(itsTInfo, itsGrid);
  itsHeightLevelMetaData.MakeMetaInfo(itsTInfo, itsGrid);
  itsPressureLevelMetaData.MakeMetaInfo(itsTInfo, itsGrid);
  itsHybridLevelMetaData.MakeMetaInfo(itsTInfo, itsGrid);
}

void FmiNetCdfQueryData::InitKnownParamMap(void)
{
  itsKnownParameterMap.clear();
  // t‰ss‰ on listattu tunnettuja parametrien standardi nimi‰ ja niiden vastineet FMI
  // parId-maailmassa
  itsKnownParameterMap.insert(std::make_pair("sea_water_temperature", kFmiTemperatureSea));
}

void FmiNetCdfQueryData::MakeWantedParamBag(void)
{
  if (itsNormalParameters.size())
  {
    for (size_t i = 0; i < itsNormalParameters.size(); i++)
    {
      NFmiParam param(itsNormalParameters[i].itsParId,
                      itsNormalParameters[i].itsVarName,
                      kFloatMissing,
                      kFloatMissing,
                      1,
                      0,
                      "%.1f",
                      kLinearly);
      NFmiDataIdent dataIdent(param, itsProducer);
      if (itsNormalParameters[i].itsNcLevelType == kFmiNcSurface)
        itsSurfaceMetaData.itsParams.Add(dataIdent);
      else if (itsNormalParameters[i].itsNcLevelType == kFmiNcHeight)
        itsHeightLevelMetaData.itsParams.Add(dataIdent);
      else if (itsNormalParameters[i].itsNcLevelType == kFmiNcPressureLevel)
        itsPressureLevelMetaData.itsParams.Add(dataIdent);
      else if (itsNormalParameters[i].itsNcLevelType == kFmiNcHybrid)
        itsHybridLevelMetaData.itsParams.Add(dataIdent);
    }
  }
  else
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::MakeWantedParamBag - parameters were missing.");
}

void FmiNetCdfQueryData::MakeWantedGrid(void)
{
  if (itsGrid.Area() != 0) return;  // jos lat-lon projektiosta poikkeava, hila on jo rakennettu
  size_t xSize = itsXInfo.itsValues.size();
  size_t ySize = itsYInfo.itsValues.size();
  if (xSize >= 2 && ySize >= 2)
  {
    // aluksi t‰m‰ hanskaa vain latlon-areat, ja hilat pakotetaan tasav‰lisiksi
    NFmiPoint bottomLeft(itsXInfo.itsValues[0], itsYInfo.itsValues[0]);
    NFmiPoint topRight(itsXInfo.itsValues[xSize - 1], itsYInfo.itsValues[ySize - 1]);
    NFmiArea *area = new NFmiLatLonArea(bottomLeft, topRight);
    itsGrid = NFmiGrid(area, static_cast<unsigned long>(xSize), static_cast<unsigned long>(ySize));
  }
  else
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::MakeWantedGrid - lat-lon values were missing.");
}

static FmiNcTimeOffsetType GetOffsetType(const std::string &theEpochTimeStr)
{
  FmiNcTimeOffsetType timeOffsetType = kFmiNcNoTimeType;
  if (theEpochTimeStr.empty() == false)
  {
    std::vector<std::string> wordVec = NFmiStringTools::Split(theEpochTimeStr, " ");
    if (wordVec.size() >= 3)
    {
      std::string word1 = boost::algorithm::to_lower_copy(wordVec[0]);
      if (word1 == "seconds")
        timeOffsetType = kFmiNcSeconds;
      else if (word1 == "minutes")
        timeOffsetType = kFmiNcMinutes;
    }
  }
  return timeOffsetType;
}

// Annettu epokki aika stringi on muotoa:
// "seconds since 1992-10-8 15:15:42.5 -6:00" tai "minutes since 1992-10-8 15:15:42.5 -6:00"
// Huom! Viimeinen sana (esim. -6:00) eli siirtym‰ aika UTC aikaan voi puuttua, jolloin
// annettu aika on 0-timezonessa (esim. "seconds since 1992-10-8 15:15:42.5").
// HUOM! timezoned ignoroidaan ja meit‰ kiinnostaa oikeasti vain utc-aika.
// Huom! aika voi olla suluissa (esim. "seconds since (1992-10-8 15:15:42.5)")
// Huom! viel‰ lˆytyi t‰ll‰inen aika muoto "2010-06-04T00:00:00Z", eli T erottimena ja Z lopussa.
static NFmiMetTime GetEpochTime(const std::string &theEpochTimeStr)
{
  NFmiMetTime aTime = NFmiMetTime::gMissingTime;
  if (theEpochTimeStr.empty() == false)
  {
    std::vector<std::string> wordVec = NFmiStringTools::Split(theEpochTimeStr, " ");
    if (wordVec.size() == 3)
    {
      std::vector<std::string> wordVec2 = NFmiStringTools::Split(wordVec[2], "T");
      if (wordVec2.size() == 2)
      {
        wordVec[2] = wordVec2[0];
        wordVec.push_back(wordVec2[1]);
      }
      else
        throw std::runtime_error("Error in GetEpochTime - unknown error.");
    }

    if (wordVec.size() == 4 || wordVec.size() == 5)
    {
      std::string word1 = boost::algorithm::to_lower_copy(wordVec[0]);
      std::string word2 = boost::algorithm::to_lower_copy(wordVec[1]);

      if ((word1 == "seconds" || word1 == "minutes") && word2 == "since")
      {
        std::string dateStr = NFmiStringTools::Trim(
            wordVec[2], '(');  // pit‰‰ trimmata, koska aika voi olla sulkujen sis‰ll‰
        std::vector<short> dateVec = NFmiStringTools::Split<std::vector<short> >(dateStr, "-");
        if (dateVec.size() == 3)
        {
          std::string timeStr = NFmiStringTools::Trim(
              wordVec[3], ')');  // pit‰‰ trimmata, koska aika voi olla sulkujen sis‰ll‰
          timeStr = NFmiStringTools::Trim(timeStr, 'Z');  // pit‰‰ trimmata mahdollinen Z pois
          // pakko ottaa t‰m‰ float:eina, koska sekunnit voidaan antaa desimaalin kera ja sit‰
          // konversio suoraan short:iksi ei kest‰
          std::vector<float> timeVec = NFmiStringTools::Split<std::vector<float> >(timeStr, ":");
          if (dateVec.size() == 3)
          {
            NFmiMetTime thisTime(dateVec[0],
                                 dateVec[1],
                                 dateVec[2],
                                 static_cast<short>(::round(timeVec[0])),
                                 static_cast<short>(::round(timeVec[1])),
                                 static_cast<short>(::round(timeVec[2])));
            // HUOM! j‰t‰n mahdollisen timezonen huomiotta (wordVec[4] -sis‰ltˆ), koska meit‰
            // kiinnostaa vain utc-ajat
            aTime = thisTime;
            return aTime;
          }
        }
      }
    }
  }
  throw std::runtime_error("Error in GetEpochTime - unknown error.");
}

void FmiNetCdfQueryData::CalcTimeList(void) { itsTInfo.CalcTimeList(); }
void FmiNetCdfQueryData::InitTimeDim(NcVar &theVar, const std::string &theVarName, int theIndex)
{
  if (itsTInfo.itsTimeList.NumberOfItems() > 0)
    return;  // joskus aika m‰‰reit‰ voi olla useita (samoja), otetaan vain ensimm‰inen niist‰,
             // muuten tulee samoja aikoja useita listaan
  if (theVar.num_dims() != 1)
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::InitTimeDim - dimensions for variable was not 1.");
  itsTInfo.itsVarName = theVarName;
  itsTInfo.itsIndex = theIndex;
  NcAtt *epochAttribute = theVar.get_att("units");
  if (epochAttribute == 0)
    throw std::runtime_error("Error in FmiNetCdfQueryData::InitTimeDim - no epoch attribute");
  NcValues *epochTimeValueStr = epochAttribute->values();
  itsTInfo.itsEpochTimeStr = epochTimeValueStr->as_string(0);
  delete epochTimeValueStr;
  NcValues *vals = theVar.values();
  if (vals == 0)
    throw std::runtime_error("Error in FmiNetCdfQueryData::InitTimeDim - no offset values");
  for (long i = 0; i < vals->num(); i++)
    itsTInfo.itsOffsetValues.push_back(vals->as_long(i));
  delete vals;
  delete epochAttribute;
  itsTInfo.itsEpochTime = ::GetEpochTime(itsTInfo.itsEpochTimeStr);
  if (itsTInfo.itsEpochTime == NFmiMetTime::gMissingTime)
    throw std::runtime_error("Error in FmiNetCdfQueryData::InitTimeDim - no epoch time found.");
  itsTInfo.itsTimeOffsetType = ::GetOffsetType(itsTInfo.itsEpochTimeStr);
  if (itsTInfo.itsTimeOffsetType == kFmiNcNoTimeType)
    throw std::runtime_error(
        std::string("Error in FmiNetCdfQueryData::InitTimeDim - unknown time type found: ") +
        itsTInfo.itsEpochTimeStr);
  CalcTimeList();
}

void InitXYDim(FmiXYDimVarInfo &theInfo, NcVar &theVar, const std::string &theVarName, int theIndex)
{
  theInfo.itsValues.clear();
  theInfo.itsVarName = theVarName;
  theInfo.itsIndex = theIndex;
  NcValues *vals = theVar.values();
  if (vals == 0) throw std::runtime_error("Error in InitXYDim - no lat/lon or X/Y values found.");
  for (long i = 0; i < vals->num(); i++)
    theInfo.itsValues.push_back(vals->as_float(i));
  delete vals;
  if (theInfo.itsValues.size() == 0)
    throw std::runtime_error("Error in InitXYDim - no lat/lon or X/Y values found.");
}

static void MakeZDimLevels(FmiZDimVarInfo &theZVarInfo)
{
  if (theZVarInfo.itsNcLevelType == kFmiNcHeight)
    theZVarInfo.itsLevelType = kFmiHeight;
  else if (theZVarInfo.itsNcLevelType == kFmiNcHybrid)
    theZVarInfo.itsLevelType = kFmiHybridLevel;
  else if (theZVarInfo.itsNcLevelType == kFmiNcPressureLevel)
    theZVarInfo.itsLevelType = kFmiPressureLevel;
  else
    throw std::runtime_error("Error in MakeZDimLevels - unknown level type unit attribute");

  for (size_t i = 0; i < theZVarInfo.itsValues.size(); i++)
  {
    float levValue = theZVarInfo.itsValues[i];
    theZVarInfo.itsLevels.AddLevel(
        NFmiLevel(theZVarInfo.itsLevelType, NFmiStringTools::Convert(levValue), levValue));
  }

  if (theZVarInfo.itsLevels.GetSize() == 0)
    throw std::runtime_error("Error in MakeZDimLevels - no level values found.");
}

FmiZDimVarInfo &FmiNetCdfQueryData::GetLevelInfo(FmiNcLevelType theLevelType)
{
  switch (theLevelType)
  {
    case kFmiNcHeight:
      return itsHeightLevelMetaData.itsMultiLevelInfo;
    case kFmiNcPressureLevel:
      return itsPressureLevelMetaData.itsMultiLevelInfo;
    case kFmiNcHybrid:
      return itsHybridLevelMetaData.itsMultiLevelInfo;
    default:
      throw std::runtime_error("Error in FmiNetCdfQueryData::GetLevelInfo - unknown level type.");
  }
}

// Jos netCdf tiedosto ei m‰‰rittele erikseen surface -dimensiota,
// on se p‰‰telt‰v‰ erikseen, ja tietyt alustukset on teht‰v‰ metadataan.
void FmiNetCdfQueryData::MakesureSurfaceMetaDataIsInitialized()
{
  if (itsSurfaceMetaData.itsSurfaceLevelInfo.itsNcLevelType == kFmiNcNoLevelType)
    itsSurfaceMetaData.itsSurfaceLevelInfo.itsNcLevelType = kFmiNcSurface;
}

void FmiNetCdfQueryData::InitZDim(NcVar &theVar,
                                  const std::string &theVarName,
                                  int theIndex,
                                  FmiNcLevelType theLevelType)
{
  if (theLevelType == kFmiNcSurface)
  {
    FmiVarInfo &usedLevelInfo = itsSurfaceMetaData.itsSurfaceLevelInfo;
    if (theVar.num_dims() != 1)
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - dimensions for variable was not 1.");
    usedLevelInfo.itsVarName = theVarName;
    NcDim *dim = theVar.get_dim(0);
    if (dim) usedLevelInfo.itsDimName = dim->name();
    usedLevelInfo.itsIndex = theIndex;
    usedLevelInfo.itsNcLevelType = theLevelType;
  }
  else  // if(theLevelType == kFmiNcHeight)
  {
    FmiZDimVarInfo &usedLevelInfo = GetLevelInfo(theLevelType);
    if (theVar.num_dims() != 1)
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - dimensions for variable was not 1.");
    usedLevelInfo.itsValues.clear();
    usedLevelInfo.itsVarName = theVarName;
    NcDim *dim = theVar.get_dim(0);
    if (dim) usedLevelInfo.itsDimName = dim->name();
    usedLevelInfo.itsIndex = theIndex;
    usedLevelInfo.itsNcLevelType = theLevelType;

    NcAtt *levelUnitAttribute = theVar.get_att("units");
    if (levelUnitAttribute == 0)
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - no level type unit attribute");
    NcValues *unitStr = levelUnitAttribute->values();
    usedLevelInfo.itsLevelUnitName = unitStr->as_string(0);
    NcValues *vals = theVar.values();
    if (vals == 0)
      throw std::runtime_error("Error in FmiNetCdfQueryData::InitZDim - no level values found.");
    for (long i = 0; i < vals->num(); i++)
      usedLevelInfo.itsValues.push_back(vals->as_float(i));
    delete levelUnitAttribute;
    delete vals;
    ::MakeZDimLevels(usedLevelInfo);
  }
}

FmiParameterName FmiNetCdfQueryData::GetParameterName(NcVar &theVar,
                                                      FmiParameterName theDefaultParName)
{
  NcAtt *stdNameAttr = theVar.get_att("standard_name");
  if (stdNameAttr)
  {
    NcValues *vals = stdNameAttr->values();
    std::string stdParName = vals->as_string(0);
    delete vals;
    delete stdNameAttr;
    std::map<std::string, FmiParameterName>::iterator it = itsKnownParameterMap.find(stdParName);
    if (it != itsKnownParameterMap.end()) return (*it).second;
  }
  // jos ei lˆytynyt mit‰‰n, palautetaan defaultti par-name
  return theDefaultParName;
}

static float GetMissingValue(NcVar &theVar)
{
  float missingValue = kFloatMissing;
  NcAtt *fillValueAttribute = theVar.get_att("_FillValue");
  if (fillValueAttribute)
  {
    NcValues *vals = fillValueAttribute->values();
    missingValue = vals->as_float(0);
    delete fillValueAttribute;
  }
  return missingValue;
}

// tarkistaa onko kyseisell‰ muuttujalla halutun niminen dimensio.
static bool CheckDimName(NcVar &theVar, const std::string &theDimName)
{
  for (int d = 0; d < theVar.num_dims(); d++)
  {
    NcDim *dim = theVar.get_dim(d);
    if (dim->name() == theDimName) return true;
  }
  return false;
}

// Tarkistaa ett‰ muuttujalla on kolme dimensiota, ja ne ovat x, y ja time
bool FmiNetCdfQueryData::IsSurfaceVariable(NcVar &theVar)
{
  if (theVar.num_dims() == 3)
  {
    if (::CheckDimName(theVar, itsTInfo.itsVarName))
    {
      if (::CheckDimName(theVar, itsXInfo.itsVarName))
      {
        if (::CheckDimName(theVar, itsYInfo.itsVarName))
        {
          return true;
        }
      }
    }
  }
  return false;
}

void FmiNetCdfQueryData::InitNormalVar(NcVar &theVar, const std::string &theVarName, int theIndex)
{
  static long defaultParamId = 2301;
  if (theVar.num_dims() >= 3)
  {  // pit‰‰ olla tietty m‰‰r‰ dimensioita, ett‰ muuttuja otetaan ns. normaali muuttujaksi
    FmiVarInfo varInfo;
    varInfo.itsIndex = theIndex;
    varInfo.itsParId = GetParameterName(theVar, static_cast<FmiParameterName>(defaultParamId));
    varInfo.itsFillValue = ::GetMissingValue(theVar);
    if (varInfo.itsParId == defaultParamId)
      defaultParamId++;  // jos defaultti parId otettiin k‰yttˆˆn, muutetaan sit‰ yhdell‰ isommaksi,
                         // ettei tule samoja par-ideit‰ samaan paramBAgiin
    varInfo.itsVarName = theVarName;
    if (::CheckDimName(theVar, itsSurfaceMetaData.GetLevelDimName()))
      varInfo.itsNcLevelType = itsSurfaceMetaData.GetLevelType();
    else if (::CheckDimName(theVar, itsHeightLevelMetaData.GetLevelDimName()))
      varInfo.itsNcLevelType = itsHeightLevelMetaData.GetLevelType();
    else if (::CheckDimName(theVar, itsPressureLevelMetaData.GetLevelDimName()))
      varInfo.itsNcLevelType = itsPressureLevelMetaData.GetLevelType();
    else if (::CheckDimName(theVar, itsHybridLevelMetaData.GetLevelDimName()))
      varInfo.itsNcLevelType = itsHybridLevelMetaData.GetLevelType();
    else if (IsSurfaceVariable(theVar))
      varInfo.itsNcLevelType = kFmiNcSurface;
    itsNormalParameters.push_back(varInfo);
  }
}

static std::string GetAttributeStringValue(NcVar &theVar, const std::string &theAttrName)
{
  std::string attrValue;
  NcAtt *attr = theVar.get_att(theAttrName.c_str());
  if (attr)
  {
    NcValues *vals = attr->values();
    attrValue = vals->as_string(0);
    delete vals;
    delete attr;
  }
  return attrValue;
}

// tarkistaa onko kyseisell‰ muuttujalla halutun niminen attribuutti ja onko se arvo myˆs haluttu.
static bool CheckAttribute(NcVar &theVar,
                           const std::string &theAttrName,
                           const std::string &theAttrValue,
                           bool fJustContains = false)
{
  std::string attrValue = GetAttributeStringValue(theVar, theAttrName);

  if (attrValue.empty() == false)
  {
    if (fJustContains)
    {
      size_t pos = attrValue.find(theAttrValue);
      if (pos != std::string::npos) return true;
    }
    else
    {
      if (attrValue == theAttrValue) return true;
    }
  }
  return false;
}

static FmiNcLevelType GetLevelTypeByUnits(NcVar &theVar, const std::string &theUnitStr)
{
  FmiNcLevelType leveltype = kFmiNcNoLevelType;
  if (::CheckAttribute(theVar, theUnitStr, "m"))
    leveltype = kFmiNcHeight;
  else if (::CheckAttribute(theVar, theUnitStr, "metres"))
    leveltype = kFmiNcHeight;
  else if (::CheckAttribute(theVar,
                            theUnitStr,
                            "hybrid"))  // en tied‰ mit‰ k‰ytet‰‰n mallipinta-levelin yksikkˆn‰
    leveltype = kFmiNcHybrid;
  else if (::CheckAttribute(theVar, theUnitStr, "hectopascals"))  // myˆs mbars, pascals ?!?!?
    leveltype = kFmiNcPressureLevel;
  else if (::CheckAttribute(theVar, theUnitStr, "level"))
    leveltype = kFmiNcSurface;
  return leveltype;
}

static bool IsLevelVariable(NcVar &theVar, FmiNcLevelType &theLevelTypeOut)
{
  FmiNcLevelType suggestedLevelType = kFmiNcNoLevelType;
  bool status = false;
  if (theVar.num_dims() == 1)
  {
    if (::CheckAttribute(theVar, "axis", "Z")) status = true;
    if (status == false && std::string(theVar.name()) == "level") status = true;
    if (status == false && std::string(theVar.name()) == "hybrid")
    {
      suggestedLevelType = kFmiNcHybrid;
      status = true;
    }
  }

  if (status)
  {
    NcValues *vals = theVar.values();
    if (vals)
    {
      long levelCount = vals->num();
      delete vals;
      std::string unitStr("units");
      theLevelTypeOut = ::GetLevelTypeByUnits(theVar, unitStr);
      if (theLevelTypeOut == kFmiNcNoLevelType)
      {
        if (suggestedLevelType != kFmiNcNoLevelType)
          theLevelTypeOut = suggestedLevelType;
        else if (levelCount == 1)
          theLevelTypeOut = kFmiNcSurface;
        else
        {
          std::string attrValue = GetAttributeStringValue(theVar, unitStr);
          throw std::runtime_error(
              std::string("Error in IsLevelVariable - Unknown unit with levels: ") + attrValue);
        }
      }
    }
  }
  return status;
}

static bool IsTimeVariable(NcVar &theVar)
{
  if (theVar.num_dims() == 1)
  {
    if (::CheckAttribute(theVar, "standard_name", "time")) return true;
    if (::CheckAttribute(theVar, "units", "seconds since", true)) return true;
    if (::CheckAttribute(theVar, "units", "minutes since", true)) return true;
  }
  return false;
}

static bool IsXVariable(NcVar &theVar)
{
  if (::CheckAttribute(theVar, "axis", "X"))
  {
    if (theVar.num_dims() == 1) return true;
  }
  return false;
}

static bool IsYVariable(NcVar &theVar)
{
  if (::CheckAttribute(theVar, "axis", "Y"))
  {
    if (theVar.num_dims() == 1) return true;
  }
  return false;
}

static long GetVarLongValue(NcVar &theVar, long theIndex = 0) { return theVar.as_long(theIndex); }
static float GetVarFloatValue(NcVar &theVar, long theIndex = 0)
{
  return theVar.as_float(theIndex);
}

static FmiNcProjectionType GetProjectionType(NcVar &theVar)
{
  std::string projTypeStr = theVar.as_string(0);
  size_t pos = projTypeStr.find("polar stereographic");
  if (pos != std::string::npos) return kFmiNcStreographic;

  throw std::runtime_error(std::string("Error in GetProjectionType - ") + projTypeStr +
                           " is not supported yet.");
}

void FmiNetCdfQueryData::InitializeStreographicGrid(void)
{
  if (itsProjectionInfo.La1 != kFloatMissing && itsProjectionInfo.Lo1 != kFloatMissing &&
      itsProjectionInfo.LoV != kFloatMissing && itsProjectionInfo.Dx != kFloatMissing &&
      itsProjectionInfo.Dy != kFloatMissing)
  {
    NFmiPoint bottomLeftLatlon(itsProjectionInfo.Lo1, itsProjectionInfo.La1);
    NFmiPoint topRightLatlon(itsProjectionInfo.Lo1 + 1,
                             itsProjectionInfo.La1 + 1);  // pit‰‰ tehd‰ v‰liaikainen feikki
                                                          // top-right piste, ett‰ voimme laskea
                                                          // oikean top-right-kulman
    double usedCentralLatitude =
        (itsProjectionInfo.Latin2 != kFloatMissing) ? itsProjectionInfo.Latin2 : 90;
    double usedTrueLatitude =
        (itsProjectionInfo.Latin1 != kFloatMissing) ? itsProjectionInfo.Latin1 : 60;
    NFmiStereographicArea tmpArea1(bottomLeftLatlon,
                                   topRightLatlon,
                                   itsProjectionInfo.LoV,
                                   NFmiPoint(0, 0),
                                   NFmiPoint(1, 1),
                                   usedCentralLatitude,
                                   usedTrueLatitude);
    if (itsProjectionInfo.Nx > 1 && itsProjectionInfo.Ny > 1)
    {
      double gridWidth = (itsProjectionInfo.Nx - 1) * itsProjectionInfo.Dx;
      double gridHeight = (itsProjectionInfo.Ny - 1) * itsProjectionInfo.Dy;
      NFmiPoint worldXyBottomLeft = tmpArea1.WorldXYPlace();
      NFmiPoint worldXyTopRight(worldXyBottomLeft);
      worldXyTopRight.X(worldXyTopRight.X() + gridWidth);
      worldXyTopRight.Y(worldXyTopRight.Y() + gridHeight);
      NFmiPoint realTopRightLatlon = tmpArea1.WorldXYToLatLon(worldXyTopRight);
      NFmiStereographicArea realArea(bottomLeftLatlon,
                                     realTopRightLatlon,
                                     itsProjectionInfo.LoV,
                                     NFmiPoint(0, 0),
                                     NFmiPoint(1, 1),
                                     usedCentralLatitude,
                                     usedTrueLatitude);
      itsGrid = NFmiGrid(&realArea, itsProjectionInfo.Nx, itsProjectionInfo.Ny);
      return;
    }
  }
  throw std::runtime_error(
      "Error in FmiNetCdfQueryData::InitializeStreographicGrid - unable to make grid or projection "
      "for data.");
}

// Jos ei lˆytynyt lat-lon asetuksia, pit‰‰ etsi‰, lˆytyykˆ muita projektio m‰‰rityksi‰.
// Jos ei lˆydy, heitet‰‰n poikkeus eli ei saa kutsua, jos latlon-projektiolle lˆytyi jo
// m‰‰ritykset!!!
void FmiNetCdfQueryData::SeekProjectionInfo(NcFile &theNcFile)
{
  NcVar *varPtr = 0;
  // K‰yd‰‰n ensin l‰pi vain yksi-ulotteiset muuttujat ja etsit‰‰n tiettyj‰ muutujia ja niiden
  // arvoja.
  for (int n = 0; (varPtr = theNcFile.get_var(n)) != 0; n++)
  {
    if (varPtr->num_dims() == 1)
    {
      std::string varNameStr = varPtr->name();
      if (varNameStr == "Nx")
        itsProjectionInfo.Nx = ::GetVarLongValue(*varPtr);
      else if (varNameStr == "Ny")
        itsProjectionInfo.Ny = ::GetVarLongValue(*varPtr);
      else if (varNameStr == "Dx")
        itsProjectionInfo.Dx = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "Dy")
        itsProjectionInfo.Dy = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "La1")
        itsProjectionInfo.La1 = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "Lo1")
        itsProjectionInfo.Lo1 = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "LoV")
        itsProjectionInfo.LoV = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "Latin1")
        itsProjectionInfo.Latin1 = ::GetVarFloatValue(*varPtr);
      else if (varNameStr == "Latin2")
        itsProjectionInfo.Latin2 = ::GetVarFloatValue(*varPtr);
    }
    else if (varPtr->type() == NC_CHAR)
    {
      std::string varNameStr = varPtr->name();
      if (varNameStr == "grid_type")
        itsProjectionInfo.itsProjectionType = ::GetProjectionType(*varPtr);
    }
  }
  if (itsProjectionInfo.itsProjectionType == kFmiNcStreographic)
    InitializeStreographicGrid();
  else
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::SeekProjectionInfo - No known type found from nc-data.");
}

void FmiNetCdfQueryData::InitMetaInfo(NcFile &theNcFile)
{
  Clear();
  InitKnownParamMap();
  if (theNcFile.is_valid())
  {
    NcVar *varPtr = 0;
    // K‰yd‰‰n ensin l‰pi vain yksi-ulotteisen muuttuja, ett‰ saamme kokoon kaaiken tarvittavan
    // tiedon
    // moni ulotteisten muuttujien m‰‰ritykseen.
    for (int n = 0; (varPtr = theNcFile.get_var(n)) != 0; n++)
    {
      if (varPtr->num_dims() == 1)
      {
        FmiNcLevelType levelType = kFmiNcNoLevelType;
        std::string varName = varPtr->name();
        if (::IsTimeVariable(*varPtr))
          InitTimeDim(*varPtr, varName, n);
        else if (::IsXVariable(*varPtr))
          ::InitXYDim(itsXInfo, *varPtr, varName, n);
        else if (::IsYVariable(*varPtr))
          ::InitXYDim(itsYInfo, *varPtr, varName, n);
        else if (::IsLevelVariable(*varPtr, levelType))
          InitZDim(*varPtr, varName, n, levelType);
      }
    }
    MakesureSurfaceMetaDataIsInitialized();

    if (itsXInfo.IsEmpty() || itsYInfo.IsEmpty())
      SeekProjectionInfo(theNcFile);  // jos ei lˆytynyt x- ja y-dimensioille m‰‰rityksi‰, pit‰‰
                                      // etsi‰ lˆytyyk‰ jotain muuta projektiota
                                      // huom! jos ei lˆydy, poikkeus lent‰‰.

    // Sitten k‰yd‰‰n l‰pi ns. normaalit moniulotteisen parametrit
    for (int n = 0; (varPtr = theNcFile.get_var(n)) != 0; n++)
    {
      if (varPtr->num_dims() >= 3)
      {
        if (varPtr->type() != NC_CHAR)
        {  // ei oteta huomioon char tyyppisi‰ muuttujia (ainakaan viel‰)
          std::string varName = varPtr->name();
          InitNormalVar(*varPtr, varName, n);
        }
      }
    }

    MakeWantedParamBag();
    MakeWantedGrid();
    MakeAllMetaInfos();
  }
}

void FmiNetCdfQueryData::Clear(void)
{
  itsTInfo = FmiTDimVarInfo();
  itsXInfo = FmiXYDimVarInfo();
  itsYInfo = FmiXYDimVarInfo();
  itsProjectionInfo = FmiProjectionInfo();
  itsSurfaceMetaData = FmiNcMetaData(true);
  itsHeightLevelMetaData = FmiNcMetaData(false);
  itsPressureLevelMetaData = FmiNcMetaData(false);
  itsHybridLevelMetaData = FmiNcMetaData(false);
  itsNormalParameters.clear();
}
