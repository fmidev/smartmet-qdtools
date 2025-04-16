#include "FmiNetCdfQueryData.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeTraits.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiAreaTools.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <ncDim.h>
#include <ncFile.h>
#include <ncVar.h>
#include <algorithm>
#include <numeric>

namespace
{

netCDF::NcVarAtt ncvar_get_attr(const netCDF::NcVar& var, const char *name, bool silent)
try
{
  return var.getAtt(name);
}
catch (const netCDF::exceptions::NcException&)
{
  if (!silent)
    std::cout << "NetCDF: attribute '" << name << "' not found" << std::endl;
  return netCDF::NcVarAtt();
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

template <typename ReturnType>
typename std::enable_if<Fmi::is_numeric<ReturnType>::value, ReturnType>::type
get_att_value(const netCDF::NcAtt& att, std::size_t index)
{
  try
  {
    const auto length = att.getAttLength();
    if (att.getAttLength() < index + 1)
      throw std::runtime_error("The attribute doesn not have element " + Fmi::to_string(index));

    std::vector<ReturnType> values(length);
    att.getValues(values.data());
    return values.at(index);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_att_string_value(const netCDF::NcAtt& att)
{
  try
  {
    using namespace netCDF;
    const netCDF::NcType type = att.getType();
    if (type != NcType::nc_BYTE && type != NcType::nc_CHAR && type != NcType::nc_STRING)
      throw std::runtime_error("The attribute " +  att.getName() + " must be of string or byte type");

    if (type == NcType::nc_BYTE || type == NcType::nc_CHAR)
    {
      std::vector<char> bytes(att.getAttLength());
      att.getValues(bytes.data());
      const std::string result = std::string(bytes.data(), bytes.size());
      return result;
    }
    else
    {
      throw Fmi::Exception(BCP, "FIXME: implementation missing");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


template <typename ReturnType>
typename std::enable_if<
    Fmi::is_numeric<ReturnType>::value || std::is_same<ReturnType, std::string>::value,
    std::vector<ReturnType>>::type
get_values(const netCDF::NcVar& var)
{
  try
  {
    const auto dims = var.getDims();
    if (dims.empty())
      return std::vector<ReturnType>();

    const std::size_t length = std::accumulate(dims.begin(), dims.end(), 1,
                                 [](std::size_t a, const netCDF::NcDim& b) { return a * b.getSize(); });
    std::vector<ReturnType> values(length);
    var.getVar(values.data());
    return values;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

} // anonymous namespace

void FmiTDimVarInfo::CalcTimeList(void)
{
  itsTimeList.Clear(true);
  for (size_t i = 0; i < itsOffsetValues.size(); i++)
  {
    NFmiMetTime aTime = itsEpochTime;
    long changeByMinutesValue = itsOffsetValues[i];
    if (itsTimeOffsetType == kFmiNcSeconds)
      changeByMinutesValue /= 60;
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

FmiNcMetaData::~FmiNcMetaData(void)
{
  delete itsMetaInfo;
}
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
            0)  // jos leveleitä on yksi tai useita, tehdään niistä sitten oikea levelDescriptor
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
static NFmiQueryData *MakeQueryData(const netCDF::NcFile &theNcFile,
                                    NFmiQueryInfo &theMetaInfo,
                                    std::vector<FmiVarInfo> &theVarInfos)
{
  NFmiQueryData *qData = NFmiQueryDataUtil::CreateEmptyData(theMetaInfo);
  NFmiFastQueryInfo fInfo(qData);

  for (size_t i = 0; i < theVarInfos.size(); i++)
  {
    if (fInfo.Param(static_cast<FmiParameterName>(theVarInfos[i].itsParId)))  // theVarInfos:issa on
                                                                              // kaikki parametrit
                                                                              // surface ja level
                                                                              // paramit, joten
                                                                              // pitää tarkistaa,
                                                                              // löytyykö tästä
                                                                              // datasta erikseen
    {
      const netCDF::NcVar var = theNcFile.getVar(theVarInfos[i].itsVarName);
      if (!var.isNull())  // Pitäisi löytyä!!
      {
        // NetCDF conventioiden mukaan juoksu järjestys on:
        // aika, level, y-dim, x-dim
        for (fInfo.ResetTime(); fInfo.NextTime(); )  // juoksutetaan aika dimensiota
        {
          std::vector<float> values = get_values<float>(var);
          long counter = 0;
          for (fInfo.ResetLevel(); fInfo.NextLevel();)  // juoksutetaan level dimensiota
          {
            for (fInfo.ResetLocation(); fInfo.NextLocation();)
            {
              float value = values.at(counter);
              // jos ei ole fill-value, laitetaan arvo queryDataan, jos oli, jätetään qDatan missing
              // arvo voimaan (data luodan alustettuna puuttuvilla arvoilla)
              if (value != theVarInfos[i].itsFillValue)
                fInfo.FloatValue(value);
              counter++;
            }
          }
        }
      }
    }
  }
  return qData;
}

static void AddToVector(std::vector<NFmiQueryData *> &theQDataVec, NFmiQueryData *theQData)
{
  if (theQData)
    theQDataVec.push_back(theQData);
}

std::vector<NFmiQueryData *> FmiNetCdfQueryData::CreateQueryDatas(const std::string &theNcFileName)
{
  std::vector<NFmiQueryData *> qDatas;
  try
  {
    netCDF::NcFile ncFile(theNcFileName, netCDF::NcFile::read);
    if (!ncFile.isNull())
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
  // tässä on listattu tunnettuja parametrien standardi nimiä ja niiden vastineet FMI
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
  if (itsGrid.Area() != 0)
    return;  // jos lat-lon projektiosta poikkeava, hila on jo rakennettu
  size_t xSize = itsXInfo.itsValues.size();
  size_t ySize = itsYInfo.itsValues.size();
  if (xSize >= 2 && ySize >= 2)
  {
    // aluksi tämä hanskaa vain latlon-areat, ja hilat pakotetaan tasavälisiksi
    NFmiPoint bottomLeft(itsXInfo.itsValues[0], itsYInfo.itsValues[0]);
    NFmiPoint topRight(itsXInfo.itsValues[xSize - 1], itsYInfo.itsValues[ySize - 1]);
    NFmiArea *area = NFmiAreaTools::CreateLegacyLatLonArea(bottomLeft, topRight);
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
// Huom! Viimeinen sana (esim. -6:00) eli siirtymä aika UTC aikaan voi puuttua, jolloin
// annettu aika on 0-timezonessa (esim. "seconds since 1992-10-8 15:15:42.5").
// HUOM! timezoned ignoroidaan ja meitä kiinnostaa oikeasti vain utc-aika.
// Huom! aika voi olla suluissa (esim. "seconds since (1992-10-8 15:15:42.5)")
// Huom! vielä löytyi tälläinen aika muoto "2010-06-04T00:00:00Z", eli T erottimena ja Z lopussa.
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
            wordVec[2], '(');  // pitää trimmata, koska aika voi olla sulkujen sisällä
        std::vector<short> dateVec = NFmiStringTools::Split<std::vector<short> >(dateStr, "-");
        if (dateVec.size() == 3)
        {
          std::string timeStr = NFmiStringTools::Trim(
              wordVec[3], ')');  // pitää trimmata, koska aika voi olla sulkujen sisällä
          timeStr = NFmiStringTools::Trim(timeStr, 'Z');  // pitää trimmata mahdollinen Z pois
          // pakko ottaa tämä float:eina, koska sekunnit voidaan antaa desimaalin kera ja sitä
          // konversio suoraan short:iksi ei kestä
          std::vector<float> timeVec = NFmiStringTools::Split<std::vector<float> >(timeStr, ":");
          if (dateVec.size() == 3)
          {
            NFmiMetTime thisTime(dateVec[0],
                                 dateVec[1],
                                 dateVec[2],
                                 static_cast<short>(::round(timeVec[0])),
                                 static_cast<short>(::round(timeVec[1])),
                                 static_cast<short>(::round(timeVec[2])));
            // HUOM! jätän mahdollisen timezonen huomiotta (wordVec[4] -sisältö), koska meitä
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

void FmiNetCdfQueryData::CalcTimeList(void)
{
  itsTInfo.CalcTimeList();
}
void FmiNetCdfQueryData::InitTimeDim(
    const netCDF::NcVar &theVar,
    const std::string &theVarName,
    int theIndex)
{
  if (itsTInfo.itsTimeList.NumberOfItems() > 0)
    return;  // joskus aika määreitä voi olla useita (samoja), otetaan vain ensimmäinen niistä,
             // muuten tulee samoja aikoja useita listaan
  if (theVar.getDimCount() != 1)
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::InitTimeDim - dimensions for variable was not 1.");
  itsTInfo.itsVarName = theVarName;
  itsTInfo.itsIndex = theIndex;

  const netCDF::NcVarAtt epochAttribute = ncvar_get_attr(theVar, "units", false);
  if (epochAttribute.isNull())
    throw std::runtime_error("Error in FmiNetCdfQueryData::InitTimeDim - no epoch attribute");

  //NcValues *epochTimeValueStr = epochAttribute->values();
  //itsTInfo.itsEpochTimeStr = epochTimeValueStr->as_string(0);
  //delete epochTimeValueStr;
  const std::string epochTimeValueStr = get_att_string_value(epochAttribute);

  //NcValues *vals = theVar.values();
  //if (vals == 0)
  //  throw std::runtime_error("Error in FmiNetCdfQueryData::InitTimeDim - no offset values");
  //for (long i = 0; i < vals->num(); i++)
  //  itsTInfo.itsOffsetValues.push_back(vals->as_long(i));
  //delete vals;
  //delete epochAttribute;
  const std::vector<long> vals = get_values<long>(theVar);

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

void InitXYDim(FmiXYDimVarInfo &theInfo,
    const netCDF::NcVar &theVar,
    const std::string &theVarName,
    int theIndex)
{
  theInfo.itsValues.clear();
  theInfo.itsVarName = theVarName;
  theInfo.itsIndex = theIndex;
  //NcValues *vals = theVar.values();
  //if (vals == 0)
  //  throw std::runtime_error("Error in InitXYDim - no lat/lon or X/Y values found.");
  //for (long i = 0; i < vals->num(); i++)
  //  theInfo.itsValues.push_back(vals->as_float(i));
  //delete vals;
  const std::vector<float> vals = get_values<float>(theVar);
  theInfo.itsValues = vals;

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

// Jos netCdf tiedosto ei määrittele erikseen surface -dimensiota,
// on se pääteltävä erikseen, ja tietyt alustukset on tehtävä metadataan.
void FmiNetCdfQueryData::MakesureSurfaceMetaDataIsInitialized()
{
  if (itsSurfaceMetaData.itsSurfaceLevelInfo.itsNcLevelType == kFmiNcNoLevelType)
    itsSurfaceMetaData.itsSurfaceLevelInfo.itsNcLevelType = kFmiNcSurface;
}

void FmiNetCdfQueryData::InitZDim(const netCDF::NcVar &theVar,
                                  const std::string &theVarName,
                                  int theIndex,
                                  FmiNcLevelType theLevelType)
{
  if (theLevelType == kFmiNcSurface)
  {
    FmiVarInfo &usedLevelInfo = itsSurfaceMetaData.itsSurfaceLevelInfo;
    if (theVar.getDimCount() != 1)
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - dimensions for variable was not 1.");
    usedLevelInfo.itsVarName = theVarName;
    const netCDF::NcDim dim = theVar.getDim(0);
    if (!dim.isNull())
      usedLevelInfo.itsDimName = dim.getName();
    usedLevelInfo.itsIndex = theIndex;
    usedLevelInfo.itsNcLevelType = theLevelType;
  }
  else  // if(theLevelType == kFmiNcHeight)
  {
    FmiZDimVarInfo &usedLevelInfo = GetLevelInfo(theLevelType);
    if (theVar.getDimCount() != 1)
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - dimensions for variable was not 1.");
    usedLevelInfo.itsValues.clear();
    usedLevelInfo.itsVarName = theVarName;
    const netCDF::NcDim dim = theVar.getDim(0);
    if (dim.isNull())
      usedLevelInfo.itsDimName = dim.getName();
    usedLevelInfo.itsIndex = theIndex;
    usedLevelInfo.itsNcLevelType = theLevelType;

    const netCDF::NcVarAtt levelUnitAttribute = ncvar_get_attr(theVar, "units", true);
    if (levelUnitAttribute.isNull())
      throw std::runtime_error(
          "Error in FmiNetCdfQueryData::InitZDim - no level type unit attribute");
    //NcValues *unitStr = levelUnitAttribute->values();
    //usedLevelInfo.itsLevelUnitName = unitStr->as_string(0);
    //NcValues *vals = theVar.values();
    //if (vals == 0)
    //  throw std::runtime_error("Error in FmiNetCdfQueryData::InitZDim - no level values found.");
    //for (long i = 0; i < vals->num(); i++)
    //  usedLevelInfo.itsValues.push_back(vals->as_float(i));
    //delete levelUnitAttribute;
    //delete vals;
    const std::string levelUnitStr = get_att_string_value(levelUnitAttribute);
    usedLevelInfo.itsLevelUnitName = levelUnitStr;
    const std::vector<float> vals = get_values<float>(theVar);
    usedLevelInfo.itsValues = vals;
    if (usedLevelInfo.itsValues.size() == 0)
      throw std::runtime_error("Error in FmiNetCdfQueryData::InitZDim - no level values found.");

    ::MakeZDimLevels(usedLevelInfo);
  }
}

FmiParameterName FmiNetCdfQueryData::GetParameterName(const netCDF::NcVar &theVar,
                                                      FmiParameterName theDefaultParName)
{
  const netCDF::NcVarAtt stdNameAttr = ncvar_get_attr(theVar, "standard_name", false);
  if (not stdNameAttr.isNull())
  {
    //NcValues *vals = stdNameAttr->values();
    //std::string stdParName = vals->as_string(0);
    //delete vals;
    //delete stdNameAttr;
    const std::string stdParName = get_att_string_value(stdNameAttr);
    std::map<std::string, FmiParameterName>::iterator it = itsKnownParameterMap.find(stdParName);
    if (it != itsKnownParameterMap.end())
      return (*it).second;
  }
  // jos ei löytynyt mitään, palautetaan defaultti par-name
  return theDefaultParName;
}

static float GetMissingValue(const netCDF::NcVar &theVar)
{
  float missingValue = kFloatMissing;
  const netCDF::NcVarAtt fillValueAttribute = ncvar_get_attr(theVar, "_FillValue", false);
  if (not fillValueAttribute.isNull())
  {
    //NcValues *vals = fillValueAttribute->values();
    //missingValue = vals->as_float(0);
    //delete fillValueAttribute;
    missingValue = get_att_value<float>(fillValueAttribute, 0);
  }
  return missingValue;
}

// tarkistaa onko kyseisellä muuttujalla halutun niminen dimensio.
static bool CheckDimName(const netCDF::NcVar &theVar, const std::string &theDimName)
{
  for (int d = 0; d < theVar.getDimCount(); d++)
  {
    const netCDF::NcDim dim = theVar.getDim(d);
    if (dim.getName() == theDimName)
      return true;
  }
  return false;
}

// Tarkistaa että muuttujalla on kolme dimensiota, ja ne ovat x, y ja time
bool FmiNetCdfQueryData::IsSurfaceVariable(const netCDF::NcVar &theVar)
{
  if (theVar.getDimCount() == 3)
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

void FmiNetCdfQueryData::InitNormalVar(const netCDF::NcVar &theVar, const std::string &theVarName, int theIndex)
{
  static long defaultParamId = 2301;
  if (theVar.getDimCount() >= 3)
  {  // pitää olla tietty määrä dimensioita, että muuttuja otetaan ns. normaali muuttujaksi
    FmiVarInfo varInfo;
    varInfo.itsIndex = theIndex;
    varInfo.itsParId = GetParameterName(theVar, static_cast<FmiParameterName>(defaultParamId));
    varInfo.itsFillValue = ::GetMissingValue(theVar);
    if (varInfo.itsParId == defaultParamId)
      defaultParamId++;  // jos defaultti parId otettiin käyttöön, muutetaan sitä yhdellä isommaksi,
                         // ettei tule samoja par-ideitä samaan paramBAgiin
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

static std::string GetAttributeStringValue(const netCDF::NcVar &theVar, const std::string &theAttrName)
{
  std::string attrValue;
  const netCDF::NcVarAtt attr = ncvar_get_attr(theVar, theAttrName.c_str(), false);
  if (not attr.isNull())
  {
    //NcValues *vals = attr->values();
    //attrValue = vals->as_string(0);
    //delete vals;
    //delete attr;
    attrValue = get_att_string_value(attr);
  }
  return attrValue;
}

// tarkistaa onko kyseisellä muuttujalla halutun niminen attribuutti ja onko se arvo myös haluttu.
static bool CheckAttribute(const netCDF::NcVar &theVar,
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
      if (pos != std::string::npos)
        return true;
    }
    else
    {
      if (attrValue == theAttrValue)
        return true;
    }
  }
  return false;
}

static FmiNcLevelType GetLevelTypeByUnits(const netCDF::NcVar &theVar, const std::string &theUnitStr)
{
  FmiNcLevelType leveltype = kFmiNcNoLevelType;
  if (::CheckAttribute(theVar, theUnitStr, "m"))
    leveltype = kFmiNcHeight;
  else if (::CheckAttribute(theVar, theUnitStr, "metres"))
    leveltype = kFmiNcHeight;
  else if (::CheckAttribute(theVar,
                            theUnitStr,
                            "hybrid"))  // en tiedä mitä käytetään mallipinta-levelin yksikkönä
    leveltype = kFmiNcHybrid;
  else if (::CheckAttribute(theVar, theUnitStr, "hectopascals"))  // myös mbars, pascals ?!?!?
    leveltype = kFmiNcPressureLevel;
  else if (::CheckAttribute(theVar, theUnitStr, "level"))
    leveltype = kFmiNcSurface;
  return leveltype;
}

static bool IsLevelVariable(const netCDF::NcVar &theVar, FmiNcLevelType &theLevelTypeOut)
{
  FmiNcLevelType suggestedLevelType = kFmiNcNoLevelType;
  bool status = false;
  if (theVar.getDimCount() == 1)
  {
    if (::CheckAttribute(theVar, "axis", "Z"))
      status = true;
    if (status == false && std::string(theVar.getName()) == "level")
      status = true;
    if (status == false && std::string(theVar.getName()) == "hybrid")
    {
      suggestedLevelType = kFmiNcHybrid;
      status = true;
    }
  }

  if (status)
  {
    // FIXME: is the type correct? (not used in earlier code)
    const std::vector<float> vals = get_values<float>(theVar);
    if (not vals.empty())
    {
      long levelCount = vals.size();
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

static bool IsTimeVariable(const netCDF::NcVar &theVar)
{
  if (theVar.getDimCount() == 1)
  {
    if (::CheckAttribute(theVar, "standard_name", "time"))
      return true;
    if (::CheckAttribute(theVar, "units", "seconds since", true))
      return true;
    if (::CheckAttribute(theVar, "units", "minutes since", true))
      return true;
  }
  return false;
}

static bool IsXVariable(const netCDF::NcVar &theVar)
{
  if (::CheckAttribute(theVar, "axis", "X"))
  {
    if (theVar.getDimCount() == 1)
      return true;
  }
  return false;
}

static bool IsYVariable(const netCDF::NcVar &theVar)
{
  if (::CheckAttribute(theVar, "axis", "Y"))
  {
    if (theVar.getDimCount() == 1)
      return true;
  }
  return false;
}

static long GetVarLongValue(const netCDF::NcVar &theVar, long theIndex = 0)
{
  // There is API for getting the value of a variable as specified type, but this form
  // of call wuold only work is getDimCount() == 1. Is that correct?
  // Try to get entire data to avoid this possible problem
  const std::vector<long> vals = get_values<long>(theVar);
  return vals.at(theIndex);
  //return theVar.as_long(theIndex);
}

static float GetVarFloatValue(const netCDF::NcVar &theVar, long theIndex = 0)
{
  //return theVar.as_float(theIndex);
  // There is API for getting the value of a variable as specified type, but this form
  // of call wuold only work is getDimCount() == 1. Is that correct?
  // Try to get entire data to avoid this possible problem
  const std::vector<float> vals = get_values<float>(theVar);
  return vals.at(theIndex);
}

static FmiNcProjectionType GetProjectionType(const netCDF::NcVar &theVar)
{
  //std::string projTypeStr = theVar.as_string(0);
  std::vector<std::string> projTypeStrVec = get_values<std::string>(theVar);
  std::string projTypeStr = projTypeStrVec.at(0);
  size_t pos = projTypeStr.find("polar stereographic");
  if (pos != std::string::npos)
    return kFmiNcStreographic;

  throw std::runtime_error(std::string("Error in GetProjectionType - ") + projTypeStr +
                           " is not supported yet.");
}

void FmiNetCdfQueryData::InitializeStreographicGrid(void)
{
  if (itsProjectionInfo.La1 == kFloatMissing || itsProjectionInfo.Lo1 == kFloatMissing ||
      itsProjectionInfo.LoV == kFloatMissing || itsProjectionInfo.Dx == kFloatMissing ||
      itsProjectionInfo.Dy == kFloatMissing || itsProjectionInfo.Nx <= 1 ||
      itsProjectionInfo.Ny <= 1)
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::InitializeStreographicGrid - unable to make grid or "
        "projection "
        "for data.");

  const double clat = (itsProjectionInfo.Latin2 != kFloatMissing) ? itsProjectionInfo.Latin2 : 90;
  const double tlat = (itsProjectionInfo.Latin1 != kFloatMissing) ? itsProjectionInfo.Latin1 : 60;
  const double clon = itsProjectionInfo.LoV;

  NFmiPoint bottomLeftLatlon(itsProjectionInfo.Lo1, itsProjectionInfo.La1);

  if (itsProjectionInfo.Nx > 1 && itsProjectionInfo.Ny > 1)
  {
    double gridWidth = (itsProjectionInfo.Nx - 1) * itsProjectionInfo.Dx;
    double gridHeight = (itsProjectionInfo.Ny - 1) * itsProjectionInfo.Dy;

    auto proj = fmt::format(
        "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +R={:.0f} "
        "+units=m +wktext +towgs84=0,0,0 +no_defs",
        clat,
        tlat,
        clon,
        kRearth);

    auto *area =
        NFmiArea::CreateFromCornerAndSize(proj, "FMI", bottomLeftLatlon, gridWidth, gridHeight);

    itsGrid = NFmiGrid(area, itsProjectionInfo.Nx, itsProjectionInfo.Ny);
  }
  else
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::InitializeStreographicGrid - unable to make grid or "
        "projection "
        "for data.");
}

// Jos ei löytynyt lat-lon asetuksia, pitää etsiä, löytyykö muita projektio määrityksiä.
// Jos ei löydy, heitetään poikkeus eli ei saa kutsua, jos latlon-projektiolle löytyi jo
// määritykset!!!
void FmiNetCdfQueryData::SeekProjectionInfo(const netCDF::NcFile &theNcFile)
{
  std::string name;
  netCDF::NcVar var;
  // Käydään ensin läpi vain yksi-ulotteiset muuttujat ja etsitään tiettyjä muutujia ja niiden
  // arvoja.
  const std::multimap< std::string, netCDF::NcVar> vars = theNcFile.getVars();
  for (const auto& item : vars)
  {
    const std::string varNameStr = item.first;
    const netCDF::NcVar &var = item.second;
    if (var.getDimCount() == 1)
    {
      if (varNameStr == "Nx")
        itsProjectionInfo.Nx = ::GetVarLongValue(var);
      else if (varNameStr == "Ny")
        itsProjectionInfo.Ny = ::GetVarLongValue(var);
      else if (varNameStr == "Dx")
        itsProjectionInfo.Dx = ::GetVarFloatValue(var);
      else if (varNameStr == "Dy")
        itsProjectionInfo.Dy = ::GetVarFloatValue(var);
      else if (varNameStr == "La1")
        itsProjectionInfo.La1 = ::GetVarFloatValue(var);
      else if (varNameStr == "Lo1")
        itsProjectionInfo.Lo1 = ::GetVarFloatValue(var);
      else if (varNameStr == "LoV")
        itsProjectionInfo.LoV = ::GetVarFloatValue(var);
      else if (varNameStr == "Latin1")
        itsProjectionInfo.Latin1 = ::GetVarFloatValue(var);
      else if (varNameStr == "Latin2")
        itsProjectionInfo.Latin2 = ::GetVarFloatValue(var);
    }
    else if (var.getType() == netCDF::NcType::nc_CHAR)
    {
      if (varNameStr == "grid_type")
        itsProjectionInfo.itsProjectionType = ::GetProjectionType(var);
    }
  }
  if (itsProjectionInfo.itsProjectionType == kFmiNcStreographic)
    InitializeStreographicGrid();
  else
    throw std::runtime_error(
        "Error in FmiNetCdfQueryData::SeekProjectionInfo - No known type found from nc-data.");
}

void FmiNetCdfQueryData::InitMetaInfo(const netCDF::NcFile &theNcFile)
{
  Clear();
  InitKnownParamMap();
  if (not theNcFile.isNull())
  {
    // Käydään ensin läpi vain yksi-ulotteisen muuttuja, että saamme kokoon kaaiken tarvittavan
    // tiedon
    // moni ulotteisten muuttujien määritykseen.
    std::size_t counter = 0;
    const std::multimap< std::string, netCDF::NcVar> vars = theNcFile.getVars();
    for (const auto& item : vars)
    {
      const std::size_t index = counter++;
      const std::string varName = item.first;
      const netCDF::NcVar &var = item.second;
      if (var.getDimCount() == 1)
      {
        FmiNcLevelType levelType = kFmiNcNoLevelType;
        std::string varName = var.getName();
        if (::IsTimeVariable(var))
          InitTimeDim(var, varName, index);
        else if (::IsXVariable(var))
          ::InitXYDim(itsXInfo, var, varName, index);
        else if (::IsYVariable(var))
          ::InitXYDim(itsYInfo, var, varName, index);
        else if (::IsLevelVariable(var, levelType))
          InitZDim(var, varName, index, levelType);
      }
    }
    MakesureSurfaceMetaDataIsInitialized();

    if (itsXInfo.IsEmpty() || itsYInfo.IsEmpty())
      SeekProjectionInfo(theNcFile);  // jos ei löytynyt x- ja y-dimensioille määrityksiä, pitää
                                      // etsiä löytyykä jotain muuta projektiota
                                      // huom! jos ei löydy, poikkeus lentää.

    // Sitten käydään läpi ns. normaalit moniulotteisen parametrit
    counter = 0;
    for (const auto& item : vars)
    {
      const std::size_t n = counter++;
      const std::string varName = item.first;
      const netCDF::NcVar &var = item.second;
      if (var.getDimCount() >= 3)
      {
        if (var.getType() != netCDF::NcType::nc_CHAR)
        {  // ei oteta huomioon char tyyppisiä muuttujia (ainakaan vielä)
          InitNormalVar(var, varName, n);
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
