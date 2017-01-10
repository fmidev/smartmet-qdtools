// ======================================================================
/*!
 * \brief NetCDF to querydata conversion for CF-conforming data
 *
 * http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/
 */
// ======================================================================

#include "NFmiAreaFactory.h"
#include "NFmiFastQueryInfo.h"
#include "NFmiHPlaceDescriptor.h"
#include "NFmiParamDescriptor.h"
#include "NFmiQueryData.h"
#include "NFmiQueryDataUtil.h"
#include "NFmiTimeDescriptor.h"
#include "NFmiTimeList.h"
#include "NFmiVPlaceDescriptor.h"

#include <nctools.h>

#include <netcdfcpp.h>

#include <boost/algorithm/string.hpp>

nctools::Options options;

// case insensitive search of sub string from stackoverflow
// http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find

// templated version of my_equal so it could work with both char and wchar_t
template <typename charT>
struct my_equal
{
  my_equal(const std::locale &loc) : loc_(loc) {}
  bool operator()(charT ch1, charT ch2)
  {
    return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
  }

 private:
  my_equal &operator=(const my_equal &);  // Disabloidaan sijoitus operaattori, jotta estet‰‰n VC++
                                          // 2012 k‰‰nnˆs varoitus

  const std::locale &loc_;
};

// find substring (case insensitive)
template <typename T>
int ci_find_substr(const T &str1, const T &str2, const std::locale &loc = std::locale())
{
  typename T::const_iterator it = std::search(
      str1.begin(), str1.end(), str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
  if (it != str1.end())
    return static_cast<int>(it - str1.begin());
  else
    return -1;  // not found
}

static const std::string staggeredKeyWord = "stag";
static const std::string bottomTopKeyWord = "bottom_top";

namespace WRFData
{
typedef std::map<std::string, NcDim *> DimensionMap;
struct TotalDimensionData
{
  TotalDimensionData()
      : timeDimName(),
        timeDimSize(0),
        levDimName(),
        levDimSize(0),
        levDimStaggered(false),
        xDimName(),
        xDimSize(0),
        xDimStaggered(false),
        yDimName(),
        yDimSize(0),
        yDimStaggered(false)
  {
  }

  // Tarkistaa onko kyseess‰ staggered dimensio mukana ja laittaa tarvittaessa liput p‰‰lle.
  void CheckStaggered(void)
  {
    if (ci_find_substr(xDimName, staggeredKeyWord) != -1) xDimStaggered = true;
    if (ci_find_substr(yDimName, staggeredKeyWord) != -1) yDimStaggered = true;

    // level suunnassa staggered juttu vaatii ett‰ kyse ei ole ns. soil-leveleist‰, vaan pit‰‰ olla
    // bottom_top-leveleist‰
    if (ci_find_substr(levDimName, staggeredKeyWord) != -1 &&
        ci_find_substr(levDimName, bottomTopKeyWord) != -1)
      levDimStaggered = true;
  }

  // Tarkistaa xDimName:n ja yDimName:n, jos ne ovat v‰‰rin p‰in (x <-> y), vaihda arvot ristiin
  void CheckFillOrder(void)
  {
    if (ci_find_substr(xDimName, std::string("south")) != -1 &&
        ci_find_substr(yDimName, std::string("west")) != -1)
    {
      std::swap(xDimName, yDimName);
      std::swap(xDimSize, yDimSize);  // ilan t‰t‰ ei tule oikean muotoista hilaa
      std::swap(xDimStaggered, yDimStaggered);
    }
  }

  std::string MakeDimensionName() const
  {
    std::string dimName;
    dimName += timeDimName;
    dimName += "_";
    dimName += levDimName;
    dimName += "_";
    dimName += xDimName;
    dimName += "_";
    dimName += yDimName;

    return dimName;
  }

  // Pit‰‰ olla time, x- ja y-dimensiot kunnossa, level osiolla ei ole v‰li‰.
  bool HasGoodDimensions() const
  {
    if (!timeDimName.empty() && !xDimName.empty() && !yDimName.empty())
    {
      if (timeDimSize && xDimSize && yDimSize) return true;
    }
    return false;
  }

  bool Has4DData() const
  {
    if (!timeDimName.empty() && !xDimName.empty() && !yDimName.empty() && !levDimName.empty())
    {
      if (timeDimSize && xDimSize && yDimSize && levDimSize) return true;
    }
    return false;
  }

  bool operator==(const TotalDimensionData &other) const
  {
    return (timeDimName == other.timeDimName && timeDimSize == other.timeDimSize &&
            levDimName == other.levDimName && levDimSize == other.levDimSize &&
            xDimName == other.xDimName && xDimSize == other.xDimSize &&
            yDimName == other.yDimName && yDimSize == other.yDimSize);
  }

  bool operator<(const TotalDimensionData &other) const
  {
    if (timeDimName != other.timeDimName)
      return timeDimName < other.timeDimName;
    else if (timeDimSize != other.timeDimSize)
      return timeDimSize < other.timeDimSize;
    else if (levDimName != other.levDimName)
      return levDimName < other.levDimName;
    else if (levDimSize != other.levDimSize)
      return levDimSize < other.levDimSize;
    else if (xDimName != other.xDimName)
      return xDimName < other.xDimName;
    else if (xDimSize != other.xDimSize)
      return xDimSize < other.xDimSize;
    else if (yDimName != other.yDimName)
      return yDimName < other.yDimName;
    else if (yDimSize != other.yDimSize)
      return yDimSize < other.yDimSize;
    return false;
  }

  std::string timeDimName;
  int timeDimSize;
  std::string levDimName;
  int levDimSize;
  bool levDimStaggered;
  std::string xDimName;
  int xDimSize;
  bool xDimStaggered;
  std::string yDimName;
  int yDimSize;
  bool yDimStaggered;
};

// Erilaiset dimensiopaketit ja niihin liittyvien parametrien nimet.
typedef std::map<TotalDimensionData, std::vector<std::string> > TotalDimensionDataSet;

const int MissingInt = -999999;
const double MissingDouble = -999999.;
}

static void PrintNcDimension(NcDim *dim, int indexZeroBased)
{
  std::cerr << "#" << indexZeroBased + 1 << " - ";
  std::cerr << "name: " << dim->name() << " id: " << dim->id() << " size: " << dim->size();
  std::cerr << std::endl;
}

// Haetaan kaikki dimensiot erilliseen map:iin.
static WRFData::DimensionMap GetNCDimensionMap(nctools::Options &options, const NcFile &ncFile)
{
  WRFData::DimensionMap dimMap;
  if (options.verbose) std::cerr << "dimensions:" << std::endl;
  for (int i = 0; i < ncFile.num_dims(); i++)
  {
    NcDim *dim = ncFile.get_dim(i);
    dimMap.insert(std::make_pair(dim->name(), dim));
    if (options.verbose) ::PrintNcDimension(dim, i);
  }
  return dimMap;
}

static WRFData::TotalDimensionData GetTotalDimensionDataFromParam(nctools::Options &options,
                                                                  NcVar *var)
{
  WRFData::TotalDimensionData dimData;
  // Ohitetaan sellaiset dimensiot yhdistelm‰t, miss‰ on alle 3 tai yli 4 dimensiota.
  int numOfDims = var->num_dims();
  bool fillData = (numOfDims >= 3 && numOfDims <= 4);
  bool noLevels = (numOfDims == 3);
  if (options.verbose)
  {
    std::cerr << "param(";
    if (fillData && noLevels)
      std::cerr << "3D";
    else if (fillData)
      std::cerr << "4D";
    else
      std::cerr << "NO";
    std::cerr << ") " << var->name() << " (";
  }
  for (int i = 0; i < numOfDims; i++)
  {
    NcDim *dim = var->get_dim(i);
    if (options.verbose)
    {
      if (i > 0) std::cerr << ",";
      std::cerr << dim->name();
    }
    if (fillData)
    {
      if (i == 0)
      {
        dimData.timeDimName = dim->name();
        dimData.timeDimSize = dim->size();
      }
      else if ((!noLevels && i == 1))
      {
        dimData.levDimName = dim->name();
        dimData.levDimSize = dim->size();
      }
      else if ((noLevels && i == 1) || (!noLevels && i == 2))
      {
        dimData.xDimName = dim->name();
        dimData.xDimSize = dim->size();
      }
      else if ((noLevels && i == 2) || (!noLevels && i == 3))
      {
        dimData.yDimName = dim->name();
        dimData.yDimSize = dim->size();
      }
    }
  }

  if (options.verbose) std::cerr << ")" << std::endl;

  dimData.CheckStaggered();
  dimData.CheckFillOrder();
  return dimData;
}

static void AddDimensionData(WRFData::TotalDimensionDataSet &dimDataSet,
                             const WRFData::TotalDimensionData &dimData,
                             NcVar *var)
{
  if (dimData.HasGoodDimensions())
  {
    std::string paramName = var->name();
    WRFData::TotalDimensionDataSet::iterator it = dimDataSet.find(dimData);
    if (it != dimDataSet.end())
    {  // lis‰t‰‰n olemassa olevaan listaan uusi parametri
      it->second.push_back(paramName);
    }
    else
    {  // lis‰t‰‰n uusi dimensio data uudella parametrilla
      std::vector<std::string> paramVector;
      paramVector.push_back(paramName);
      dimDataSet.insert(std::make_pair(dimData, paramVector));
    }
  }
}

// K‰yd‰‰n l‰pi kaikki variable:t datassa.
// Pyydet‰‰n jokaiselta sen dimensiot, ja tehd‰‰n yksilˆlllisi‰ dimensio paketteja.
// Lis‰ksi liitet‰‰n kaikki dimensioPaketteihin niihin liittyv‰t parametrit.
// N‰ist‰ dimensiopaketeista ja niihin liittyvist‰ parametreista voidaan tehd‰ myˆhemmin
// erillisi‰ queryInfoja eri tulos datoille.
static WRFData::TotalDimensionDataSet GetNCTotalDimensionDataSet(nctools::Options &options,
                                                                 const NcFile &ncFile)
{
  if (options.verbose) std::cerr << "params:" << std::endl;
  WRFData::TotalDimensionDataSet dimDataSet;
  for (int i = 0; i < ncFile.num_vars(); i++)
  {
    NcVar *var = ncFile.get_var(i);
    WRFData::TotalDimensionData dimData = ::GetTotalDimensionDataFromParam(options, var);
    ::AddDimensionData(dimDataSet, dimData, var);
  }
  return dimDataSet;
}

template <typename T>
static bool GetGlobalAttributeValue(const std::string &attributeName,
                                    nctools::attributesMap &globalAttributes,
                                    T &valueOut)
{
  nctools::attributesMap::iterator it = globalAttributes.find(attributeName);
  if (it != globalAttributes.end())
  {
    try
    {
      valueOut = NFmiStringTools::Convert<T>(it->second);
      return true;
    }
    catch (...)
    {
      throw std::runtime_error("Error when converting cmd-line global attribute '" + attributeName +
                               "' with value: " + it->second);
    }
  }

  return false;
}

static int GetWRFGlobalAttributeInteger(const NcFile &ncFile,
                                        const std::string &attributeName,
                                        nctools::attributesMap &globalAttributes)
{
  int returnValue = WRFData::MissingInt;
  if (!::GetGlobalAttributeValue(attributeName, globalAttributes, returnValue))
  {
    for (int i = 0; i < ncFile.num_atts(); i++)
    {
      NcAtt *attr = ncFile.get_att(i);
      if (attributeName == attr->name() && attr->num_vals() > 0) return attr->as_int(0);
    }
  }
  return returnValue;
}

static double GetWRFGlobalAttributeDouble(const NcFile &ncFile,
                                          const std::string &attributeName,
                                          nctools::attributesMap &globalAttributes)
{
  double returnValue = WRFData::MissingDouble;
  if (!::GetGlobalAttributeValue(attributeName, globalAttributes, returnValue))
  {
    for (int i = 0; i < ncFile.num_atts(); i++)
    {
      NcAtt *attr = ncFile.get_att(i);
      if (attributeName == attr->name() && attr->num_vals() > 0) return attr->as_double(0);
    }
  }
  return returnValue;
}

static std::string GetWRFGlobalAttributeString(const NcFile &ncFile,
                                               const std::string &attributeName,
                                               nctools::attributesMap &globalAttributes)
{
  std::string returnValue = "";
  if (!::GetGlobalAttributeValue(attributeName, globalAttributes, returnValue))
  {
    for (int i = 0; i < ncFile.num_atts(); i++)
    {
      NcAtt *attr = ncFile.get_att(i);
      if (attributeName == attr->name() && attr->num_vals() > 0) return attr->as_string(0);
    }
  }
  return returnValue;
}

static void PrintWRFGlobalAttributes(nctools::Options &options, const NcFile &ncFile)
{
  if (options.verbose)
  {
    std::cerr << "global attributes:" << std::endl;
    for (int i = 0; i < ncFile.num_atts(); i++)
    {
      NcAtt *attr = ncFile.get_att(i);
      if (attr->num_vals()) std::cerr << attr->name() << " = " << attr->as_string(0) << std::endl;
    }
  }
}

struct BaseGridAreaData
{
  BaseGridAreaData()
      : baseAreaString(),
        DX(0),
        DY(0),
        baseSizeX(0),
        baseSizeY(0),
        baseAreaPtr(),
        baseGrid(),
        baseHybridLevels(),
        doFinalDataProjection(false),
        finalDataGrid()
  {
  }

  bool isEmpty(void) const
  {
    if (!DX || !DY || !baseSizeX || !baseSizeY || baseAreaString.empty())
      return true;
    else
      return false;
  }

  void calcBaseGrid(void)
  {
    calcBaseArea();
    baseGrid = NFmiGrid(baseAreaPtr.get(), baseSizeX, baseSizeY);
  }

  void calcBaseArea(void)
  {
    if (isEmpty())
      throw std::runtime_error(
          "Error in BaseGridAreaData::calcBaseArea: there were one or more missing values in the "
          "data's area and/or projection information.");

    baseAreaPtr = calcArea(baseSizeX, baseSizeY);
  }

  NFmiAreaFactory::return_type calcArea(int gridSizeX, int gridSizeY) const
  {
    // K‰ytetyn alueen suuruus, eli lasketaan hilan koon ja hilapisteen koon avulla
    double totalWidthInKM = (gridSizeX - 1) * DX / 1000.;
    std::string projectionString = baseAreaString;
    projectionString += NFmiStringTools::Convert(totalWidthInKM);
    projectionString += ",";
    double totalHeightInKM = (gridSizeY - 1) * DY / 1000.;
    projectionString += NFmiStringTools::Convert(totalHeightInKM);
    return NFmiAreaFactory::Create(projectionString);
  }

  std::string baseAreaString;  // NFmiAreaFactory:lle menev‰n area-stringin perusosa, joka on
                               // koostettu netCdf:n metatiedoista
  double DX;                   // hilakoko x-suunnassa metreiss‰
  double DY;                   // hilakoko y-suunnassa metreiss‰
  int baseSizeX;               // nc-tiedoston metadatoista otettu perus hilan koko (ei staggered)
  int baseSizeY;
  NFmiAreaFactory::return_type baseAreaPtr;
  NFmiGrid baseGrid;
  NFmiVPlaceDescriptor baseHybridLevels;  // t‰h‰n talletetaan bottom_top-tyyppinen level-rakenne
  bool doFinalDataProjection;  // jos optioilla on annettu aluem‰‰ritelyt, johon data lopullisesti
                               // interpoloidaan, t‰m‰ on true
  NFmiGrid finalDataGrid;      // T‰ss‰ on lopullinen datan alue+hila, jos optioilla on sellainen
                               // haluttu tehd‰
};

// Annetun stringin pit‰‰ sis‰lt‰‰ kolme osiota eroteltuna ':' merkeill‰:
// Esim. latlon:x1,y1,x2,y2:xsize,ysize
static NFmiGrid GetGridFromProjectionStr(const std::string &theProjectionStr)
{
  std::vector<std::string> projectionPartsStr = NFmiStringTools::Split(theProjectionStr, ":");
  if (projectionPartsStr.size() < 3)
    throw std::runtime_error(
        std::string("Unable to create area and grid from given projection string: ") +
        theProjectionStr);
  else
  {
    std::string areaStr = projectionPartsStr[0] + ":" + projectionPartsStr[1];
    ;
    std::string gridStr = projectionPartsStr[2];
    gridStr = NFmiStringTools::ReplaceChars(gridStr, 'x', ',');  // on mahdollista ett‰ optiona
                                                                 // annettu hilakoko sis‰lt‰‰
    // x-merkin erottimena ja se pit‰‰
    // muuttaa ','-merkiksi

    boost::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(areaStr);
    checkedVector<double> values = NFmiStringTools::Split<checkedVector<double> >(gridStr, ",");
    if (values.size() != 2)
      throw std::runtime_error(std::string("Projection string contains GridSize that was invalid, "
                                           "has to be two numbers (e.g. x,y):\n'") +
                               gridStr + "' in string: " + theProjectionStr);
    NFmiPoint gridSize(values[0], values[1]);
    NFmiGrid grid(area->Clone(),
                  static_cast<unsigned int>(gridSize.X()),
                  static_cast<unsigned int>(gridSize.Y()));
    return grid;
  }
}

static std::string MakeGridSizeString(int xSize, int ySize)
{
  std::string str;
  str += NFmiStringTools::Convert(xSize);
  str += ",";
  str += NFmiStringTools::Convert(ySize);
  return str;
}

static std::string MakePointString(const NFmiPoint &p)
{
  std::string str;
  str += NFmiStringTools::Convert(p.X());
  str += ",";
  str += NFmiStringTools::Convert(p.Y());
  return str;
}

static std::string MakeAreaCornersString(const NFmiPoint &bottomLeft, const NFmiPoint &topRight)
{
  std::string str;
  str += MakePointString(bottomLeft);
  str += ",";
  str += MakePointString(topRight);
  return str;
}

static void SetFinalDataAreaInfo(nctools::Options &options, BaseGridAreaData &areaData)
{
  if (!options.projection.empty())
  {
    std::vector<std::string> projectionsParts = NFmiStringTools::Split(options.projection, ":");
    if (projectionsParts.size() <= 3)
    {
      std::string finalProjectionStr;
      if (projectionsParts.size() ==
          3)  // koko datan rakenne on annettu optiona, rakennetaan siit‰ haluttu hila
        finalProjectionStr = options.projection;
      else if (projectionsParts.size() == 2)
      {  // projektiotyyppi ja alue on annettu optiona, lis‰t‰‰n siihen originaali datan hilakoko
        finalProjectionStr = options.projection;
        if (finalProjectionStr[finalProjectionStr.size() - 1] != ':') finalProjectionStr += ":";
        finalProjectionStr += MakeGridSizeString(areaData.baseSizeX, areaData.baseSizeY);
      }
      else if (projectionsParts.size() == 1)
      {  // projektiotyyppi on annettu optiona, lis‰t‰‰n siihen originaali datan alue ja hilakoko
        finalProjectionStr = options.projection;
        if (finalProjectionStr[finalProjectionStr.size() - 1] != ':') finalProjectionStr += ":";
        finalProjectionStr += MakeAreaCornersString(areaData.baseGrid.Area()->BottomLeftLatLon(),
                                                    areaData.baseGrid.Area()->TopRightLatLon());
        finalProjectionStr += ":";
        finalProjectionStr += MakeGridSizeString(areaData.baseSizeX, areaData.baseSizeY);
      }
      areaData.finalDataGrid = GetGridFromProjectionStr(finalProjectionStr);
      areaData.doFinalDataProjection = true;
    }
    else
      throw std::runtime_error(
          std::string("Error with the projection options (-P), too many ':' searated sections:\n") +
          options.projection);
  }
}

// Yritet‰‰n saada projektio ulos WRF-NetCdf:st‰. Sen p‰‰ttelyss‰ yritet‰‰n k‰ytt‰‰ joitain
// seuraavista globaaleista attribuuteista (esimerkki J. Kauhaselta saadusta WRF data dumbista):
//:WEST-EAST_GRID_DIMENSION = 76 ;
//:SOUTH-NORTH_GRID_DIMENSION = 61 ;
//:DX = 8695.241f ;
//:DY = 8695.241f ;
//:GRIDTYPE = "C" ;
//:KM_OPT = 4 ;
//:DT = 30.f ;
//:CEN_LAT = 15.341f ;
//:CEN_LON = 32.068f ;
//:TRUELAT1 = 15.341f ;
//:TRUELAT2 = 15.341f ;
//:MOAD_CEN_LAT = 15.341f ;
//:STAND_LON = 32.068f ;
//:POLE_LAT = 90.f ;
//:POLE_LON = 0.f ;
//:MAP_PROJ = 6 ;

// Ohessa dokumentti jonka lˆysin google:lla, jossa on selitetty jotain WRF-netcdf asetuksia, lˆytyi
// osoitteesta:
// http://www.ncl.ucar.edu/Document/Functions/Built-in/wrf_ij_to_ll.shtml
// The opt variable can contain the following attributes, many of which are included as global
// attributes on the WRF output file. Attributes are case-insensitive:
//   MAP_PROJ - Model projection [1=Lambert, 2=polar stereographic, 3=mercator, 6=lat-lon]
//   (required)
//   TRUELAT1 - required for MAP_PROJ = 1, 2, 3 (defaults to 0 otherwise)
//   TRUELAT2 - required for MAP_PROJ = 6 (defaults to 0 otherwise)
//   STAND_LON - Standard longitude used in model projection (required)
//   REF_LON, REF_LON - A reference longitude and latitude (required)
//   KNOWNI, KNOWNJ - The I and J locations of REF_LON and REF_LAT (required)
//   POLE_LAT - optional for MAP_PROJ = 6 (defaults to 90 otherwise)
//   POLE_LAT - optional for MAP_PROJ = 6 (defaults to 0 otherwise)
//   DX, DY - required for MAP_PROJ = 1, 2, 3 (defaults to 0 otherwise)
//   LATINC, LONINC - required for MAP_PROJ = 6 (defaults to 0 otherwise)

// Rakentaa metatietojen avulla pohja stringin, johon myˆhemmin sitten voidaan
// liitt‰‰ hila koko -osio.
// DXout ja DYout -parametreihin talletetaan hilapisteen koko metreiss‰.
static BaseGridAreaData GetBaseAreaData(nctools::Options &options, const NcFile &ncFile)
{
  // metatiedoissa on staggered-hilan koko, ett‰ saadaan haluttu perushilan koko, niit‰ pit‰‰
  // v‰hent‰‰ yhdell‰
  int gridSizeX = ::GetWRFGlobalAttributeInteger(
      ncFile, "WEST-EAST_GRID_DIMENSION", options.cmdLineGlobalAttributes);
  if (gridSizeX != WRFData::MissingInt) gridSizeX--;
  int gridSizeY = ::GetWRFGlobalAttributeInteger(
      ncFile, "SOUTH-NORTH_GRID_DIMENSION", options.cmdLineGlobalAttributes);
  if (gridSizeY != WRFData::MissingInt) gridSizeY--;

  // Oletan ett‰ n‰m‰ DX/DY ovat hilapisteiden koot metreiss‰.
  double DX = ::GetWRFGlobalAttributeDouble(ncFile, "DX", options.cmdLineGlobalAttributes);
  double DY = ::GetWRFGlobalAttributeDouble(ncFile, "DY", options.cmdLineGlobalAttributes);

  // En tied‰ mit‰ t‰m‰ GRIDTYPE oikeasti on, mutta "C" voisi viitata keskipisteen olemassaoloon.
  std::string gridType =
      ::GetWRFGlobalAttributeString(ncFile, "GRIDTYPE", options.cmdLineGlobalAttributes);

  double centralLat =
      ::GetWRFGlobalAttributeDouble(ncFile, "CEN_LAT", options.cmdLineGlobalAttributes);
  double centralLon =
      ::GetWRFGlobalAttributeDouble(ncFile, "CEN_LON", options.cmdLineGlobalAttributes);

  // En tied‰ mit‰ MAP_PROJ:in arvot oikeasti ovat, mutta 6:n voisi viitata latlon-projektioon.
  int mapProj = ::GetWRFGlobalAttributeInteger(ncFile, "MAP_PROJ", options.cmdLineGlobalAttributes);

  if ((mapProj == 3 || mapProj == 6) && gridType == "C")
  {
    std::string projectionString;
    // Projektio tyyppi ja se mahdolliset lis‰tiedot
    if (mapProj == 3)
      projectionString += "mercator";
    else if (mapProj == 6)
      projectionString += "latlon";
    projectionString += ":";
    // central-point + scale => cen_lon,cen_lat,scale
    projectionString += NFmiStringTools::Convert(centralLon);
    projectionString += ",";
    projectionString += NFmiStringTools::Convert(centralLat);
    projectionString += ",";
    projectionString += "0.5";
    projectionString += ":";

    BaseGridAreaData data;
    data.baseAreaString = projectionString;
    data.DX = DX;
    data.DY = DY;
    data.baseSizeX = gridSizeX;
    data.baseSizeY = gridSizeY;
    data.calcBaseGrid();
    SetFinalDataAreaInfo(options, data);

    return data;
  }

  throw std::runtime_error(
      "Error in GetWRFArea: unable to determine the data's area and/or projection information.");
}

NFmiParamDescriptor CreateWRFParamDescriptor(
    nctools::Options &options,
    WRFData::TotalDimensionDataSet::value_type &totalDiemnsionData,
    const nctools::ParamConversions &paramconvs)
{
  NFmiParamBag pbag;

  const float minvalue = kFloatMissing;
  const float maxvalue = kFloatMissing;
  const float scale = kFloatMissing;
  const float base = kFloatMissing;
  const NFmiString precision = "%.1f";
  const FmiInterpolationMethod interpolation = kLinearly;

  std::vector<std::string> ncParamNames = totalDiemnsionData.second;
  for (size_t i = 0; i < ncParamNames.size(); i++)
  {
    const std::string &ncParamName = ncParamNames[i];
    if (nctools::is_name_in_list(
            options.excludeParams,
            ncParamName))  // kaikkia parametreja ei v‰ltt‰m‰tt‰ haluata mukaan (-x optio)
    {
      if (options.verbose) std::cerr << "Param " << ncParamName << " excluded" << std::endl;
    }
    else
    {
      // Here we need to know only the id
      nctools::ParamInfo pinfo = nctools::parse_parameter(ncParamName, paramconvs, true);
      NFmiParam param(
          pinfo.id, pinfo.name, minvalue, maxvalue, scale, base, precision, interpolation);
      NFmiDataIdent ident(param);
      pbag.Add(ident);
    }
  }

  return NFmiParamDescriptor(pbag);
}

static NcVar *GetWRFVariable(const NcFile &ncFile, const std::string &varName)
{
  for (int i = 0; i < ncFile.num_vars(); i++)
  {
    NcVar *var = ncFile.get_var(i);
    if (boost::iequals(varName, var->name())) return var;
  }
  return 0;
}

static int vcfix_isdigit(int ch) { return isdigit(static_cast<unsigned char>(ch)); }
// Jos wrf-datassa on useita aika-askelia, ei netCdf-api jostain syyst‰ osaa erotella niit‰
// erilleen, joten minun piti
// tehd‰ omaa koodia, joka osaa irroittaa tarvittaessa eri ajat.
static std::string GetTimeStr(int index, NcVar *timeVar)
{
  std::string timeStr = timeVar->as_string(index);
  NcDim *timeStringLengthDim = timeVar->get_dim(1);
  if (timeStringLengthDim)
  {
    int timeStringSize = timeStringLengthDim->size();
    if (timeStr.size() > static_cast<std::string::size_type>(timeStringSize))
    {  // timeStr:ss‰ on enemm‰n merkkej‰ kuin yhdess‰ aikaleimassa, siit‰ pit‰‰ nyt leikata haluttu
      // osio
      int startIndex = index * timeStringSize - index;
      std::string wantedTimeStr(timeStr.begin() + startIndex,
                                timeStr.begin() + startIndex + timeStringSize);
      return wantedTimeStr;
    }
  }

  // Muuten palautetaan alkuper‰inen timeStr
  return timeStr;
}

static bool IsInTimeStampFormat(const std::string &timeStr)
{
  try
  {
    if (timeStr.size() < 10)
    {
      (void)NFmiStringTools::Convert<int>(timeStr);
      return false;  // jos konversio integeriksi onnistuu, oletetaan ett‰ kyse on offset-arvoista
    }
  }
  catch (...)
  {
  }

  return true;  // oletetaan ett‰ kyse on time-stamp stringeist‰, muotoa: 2013-09-24_00:00:00
}

static NFmiTimeDescriptor CreateWRFTimeDescriptor(const NcFile &ncFile)
{
  NcVar *var = ::GetWRFVariable(ncFile, "Times");
  if (var == 0) var = ::GetWRFVariable(ncFile, "time");
  if (var)
  {
    NcDim *dim = var->get_dim(
        0);  // K‰yd‰‰n vain 1. dimensio l‰pi, toisessa dimensiossa on time-stringin pituus
    NFmiTimeList timeList;
    std::string probeTimeStr =
        ::GetTimeStr(0, var);  // tutkitaan 1. aika-stringi‰ ja tehd‰‰n siit‰ johtop‰‰tˆksi‰
    NFmiMetTime startingTime(1);
    startingTime.NearestMetTime(60);
    bool isInTimeStampFormat = ::IsInTimeStampFormat(probeTimeStr);
    for (int i = 0; i < dim->size(); i++)
    {
      std::string timeStr = ::GetTimeStr(i, var);
      if (!timeStr.empty())
      {
        if (isInTimeStampFormat)
        {
          // pit‰isi olla muotoa: 2013-09-24_00:00:00
          // Siit‰ on poistettava kaikki v‰lis‰l‰, jotta se saadaan muotoon: 20130924000000
          timeStr.erase(
              std::remove_if(timeStr.begin(), timeStr.end(), !boost::bind(::vcfix_isdigit, _1)),
              timeStr.end());
          NFmiMetTime *aTime = new NFmiMetTime(1);
          aTime->FromStr(timeStr);
          timeList.Add(aTime, false, false);
        }
        else
        {
          NFmiMetTime *aTime = new NFmiMetTime(startingTime);
          int offsetInSeconds = NFmiStringTools::Convert<int>(timeStr);
          aTime->ChangeBySeconds(offsetInSeconds);
          timeList.Add(aTime, false, false);
        }
      }
    }
    if (timeList.NumberOfItems())
    {
      timeList.First();
      NFmiMetTime origTime = *timeList.Current();
      return NFmiTimeDescriptor(origTime, timeList);
    }
  }

  return NFmiTimeDescriptor();
}

static NFmiHPlaceDescriptor CreateWRFHplaceDescriptor(
    WRFData::TotalDimensionDataSet::value_type &totalDiemnsionData,
    const BaseGridAreaData &areaData)
{
  NFmiAreaFactory::return_type areaPtr =
      areaData.calcArea(totalDiemnsionData.first.xDimSize, totalDiemnsionData.first.yDimSize);
  NFmiGrid grid(
      areaPtr.get(), totalDiemnsionData.first.xDimSize, totalDiemnsionData.first.yDimSize);
  return NFmiHPlaceDescriptor(grid);
}

static NcVar *GetWantedLevelVariable(const NcFile &ncFile,
                                     const std::string &levelDimName,
                                     FmiLevelType &usedLevelTypeOut)
{
  std::vector<std::pair<std::string, FmiLevelType> > possibleLevelVarNames;
  possibleLevelVarNames.push_back(std::make_pair("znu", kFmiHybridLevel));
  possibleLevelVarNames.push_back(std::make_pair("znw", kFmiHybridLevel));
  possibleLevelVarNames.push_back(std::make_pair("zs", kFmiHybridLevel));
  possibleLevelVarNames.push_back(std::make_pair("dzs", kFmiHybridLevel));
  possibleLevelVarNames.push_back(std::make_pair("pressure", kFmiPressureLevel));

  for (size_t i = 0; i < possibleLevelVarNames.size(); i++)
  {
    NcVar *var = ::GetWRFVariable(ncFile, possibleLevelVarNames[i].first);
    if (var)
    {
      for (int j = 0; j < var->num_dims(); j++)
      {
        if (boost::iequals(var->get_dim(j)->name(), levelDimName))
        {
          usedLevelTypeOut = possibleLevelVarNames[i].second;
          return var;
        }
      }
    }
  }

  return 0;
}

static bool IsBaseHybridLevelType(WRFData::TotalDimensionDataSet::value_type &totalDiemnsionData,
                                  FmiLevelType usedLevelType)
{
  // usedLevelType pit‰‰ olla hybrid tyyppi‰
  if (usedLevelType == kFmiHybridLevel)
  {
    // level suunnassa perus-hybrid datassa ei saa olla staggered juttua ja ei saa olla kyse ns.
    // soil-leveleist‰, vaan pit‰‰ olla bottom_top-leveleist‰
    if (ci_find_substr(totalDiemnsionData.first.levDimName, staggeredKeyWord) == -1 &&
        ci_find_substr(totalDiemnsionData.first.levDimName, bottomTopKeyWord) != -1)
      return true;
  }
  return false;
}

static NFmiVPlaceDescriptor CreateWRFVplaceDescriptor(
    nctools::Options &options,
    WRFData::TotalDimensionDataSet::value_type &totalDiemnsionData,
    const NcFile &ncFile,
    BaseGridAreaData &areaData)
{
  if (totalDiemnsionData.first.Has4DData())
  {
    FmiLevelType usedLevelType = kFmiHybridLevel;
    NcVar *levelVar =
        GetWantedLevelVariable(ncFile, totalDiemnsionData.first.levDimName, usedLevelType);
    if (levelVar)
    {
      if (options.verbose) std::cerr << "level values:" << std::endl;
      std::vector<float> pLevels;
      for (int i = 0; i < levelVar->num_vals(); i++)
      {
        if (i >= totalDiemnsionData.first.levDimSize)
          break;  // joskus level variableen on liitetty myˆs aika dimensio, joten lopetamme loopin
                  // kun jokainen eri leveli on k‰yty kerran l‰pi
        if (options.verbose) std::cerr << levelVar->as_string(i) << ", ";
        pLevels.push_back(levelVar->as_float(i));
      }
      if (options.verbose) std::cerr << std::endl;

      NFmiLevelBag levelBag;
      for (std::vector<float>::iterator it = pLevels.begin(); it != pLevels.end(); ++it)
      {
        levelBag.AddLevel(NFmiLevel(usedLevelType, NFmiStringTools::Convert(*it), *it));
      }
      NFmiVPlaceDescriptor vplaceDesc(levelBag);
      if (IsBaseHybridLevelType(totalDiemnsionData, usedLevelType))
        areaData.baseHybridLevels = vplaceDesc;

      return vplaceDesc;
    }

    return NFmiVPlaceDescriptor();
  }
  else
    return NFmiVPlaceDescriptor();
}

static NFmiQueryInfo *CreateNewInnerInfo(
    nctools::Options &options,
    WRFData::TotalDimensionDataSet::value_type &totalDimensionData,
    BaseGridAreaData &areaData,
    const NcFile &ncFile,
    const nctools::ParamConversions &paramconvs)
{
  NFmiParamDescriptor paramDesc =
      ::CreateWRFParamDescriptor(options, totalDimensionData, paramconvs);
  NFmiTimeDescriptor timeDesc = CreateWRFTimeDescriptor(ncFile);
  NFmiHPlaceDescriptor hplaceDesc = CreateWRFHplaceDescriptor(totalDimensionData, areaData);
  NFmiVPlaceDescriptor vplaceDesc =
      CreateWRFVplaceDescriptor(options, totalDimensionData, ncFile, areaData);
  if (paramDesc.Size() && timeDesc.Size() && hplaceDesc.Size())
  {
    return new NFmiQueryInfo(paramDesc, timeDesc, hplaceDesc, vplaceDesc);
  }

  return 0;
}

static std::string GetParamNamesListString(boost::shared_ptr<NFmiQueryData> &data)
{
  std::string namesStr;
  const NFmiParamDescriptor &parDesc = data->Info()->ParamDescriptor();
  for (unsigned long i = 0; i < parDesc.Size(); i++)
  {
    if (i > 0) namesStr += ", ";
    namesStr += parDesc.Param(i).GetParamName();
  }
  return namesStr;
}

static bool IsBetweenLimits(float value, float limit1, float limit2)
{
  // laitetaan limitit aina nousevaan j‰rjestykseen
  if (limit1 > limit2) std::swap(limit1, limit2);
  if (limit1 <= value && value <= limit2)
    return true;
  else
    return false;
}

static bool FindClosestLevelIndexies(float searchedLevelValue,
                                     NFmiFastQueryInfo &staggeredInfo,
                                     unsigned long &lowerLevelIndex,
                                     unsigned long &upperLevelIndex)
{
  float levelValue = kFloatMissing;
  float previousLevelValue = kFloatMissing;
  for (staggeredInfo.ResetLevel(); staggeredInfo.NextLevel();)
  {
    levelValue = staggeredInfo.Level()->LevelValue();
    if (previousLevelValue != kFloatMissing)
    {
      if (IsBetweenLimits(searchedLevelValue, levelValue, previousLevelValue))
      {
        if (levelValue < previousLevelValue)
        {
          lowerLevelIndex = staggeredInfo.LevelIndex();
          upperLevelIndex = lowerLevelIndex - 1;
          return true;
        }
        else
        {
          upperLevelIndex = staggeredInfo.LevelIndex();
          lowerLevelIndex = lowerLevelIndex - 1;
          return true;
        }
      }
    }
    previousLevelValue = levelValue;
  }
  return false;
}

static boost::shared_ptr<NFmiQueryData> MakeLevelStaggeredDataFix(
    const boost::shared_ptr<NFmiQueryData> &data,
    const WRFData::TotalDimensionDataSet::value_type &totalDimensionData,
    const BaseGridAreaData &areaData)
{
  if (totalDimensionData.first.levDimStaggered)
  {
    // 1. luo uusi data, jossa base-hybrid level rakenne
    NFmiQueryInfo newInnerInfo(data->Info()->ParamDescriptor(),
                               data->Info()->TimeDescriptor(),
                               data->Info()->HPlaceDescriptor(),
                               areaData.baseHybridLevels,
                               data->InfoVersion());
    boost::shared_ptr<NFmiQueryData> newData(NFmiQueryDataUtil::CreateEmptyData(newInnerInfo));
    if (newData)
    {
      // 2. Tee fastInfot molemmille datoille (kaksi vanhalle interpolointeja varten)
      NFmiFastQueryInfo newInfo(newData.get());
      NFmiFastQueryInfo lowerLevel(data.get());  // t‰h‰n haetaan leveli mik‰ on l‰himp‰n‰ newInfo
                                                 // leveli‰, mutta pienempi arvoltaan
      NFmiFastQueryInfo upperLevel(data.get());  // t‰h‰n haetaan leveli mik‰ on l‰himp‰n‰ newInfo
                                                 // leveli‰, mutta suurempi arvoltaan
      // 3. T‰yt‰ uusi data interpoloimalla level arvojen v‰liin
      // 3.1. Etsi t‰ytett‰v‰n datan leveli‰ ylemp‰n‰ ja alempana olevat levelit vanhasta datasta
      for (newInfo.ResetLevel(); newInfo.NextLevel();)
      {
        float newLevelValue = newInfo.Level()->LevelValue();
        unsigned long lowerLevelIndex = gMissingIndex;
        unsigned long upperLevelIndex = gMissingIndex;
        if (FindClosestLevelIndexies(newLevelValue, lowerLevel, lowerLevelIndex, upperLevelIndex))
        {
          lowerLevel.LevelIndex(lowerLevelIndex);
          float lowerLevelValue = lowerLevel.Level()->LevelValue();
          upperLevel.LevelIndex(upperLevelIndex);
          float upperLevelValue = upperLevel.Level()->LevelValue();
          float factor = (newLevelValue - lowerLevelValue) / (upperLevelValue - lowerLevelValue);
          for (newInfo.ResetParam(), lowerLevel.ResetParam(), upperLevel.ResetParam();
               newInfo.NextParam() && lowerLevel.NextParam() && upperLevel.NextParam();)
          {
            for (newInfo.ResetTime(), lowerLevel.ResetTime(), upperLevel.ResetTime();
                 newInfo.NextTime() && lowerLevel.NextTime() && upperLevel.NextTime();)
            {
              for (newInfo.ResetLocation(), lowerLevel.ResetLocation(), upperLevel.ResetLocation();
                   newInfo.NextLocation() && lowerLevel.NextLocation() &&
                   upperLevel.NextLocation();)
              {
                // 3.2. Tee normaali lineaarinen interpolaatio level arvojen avulla (olisiko
                // logaritmisesta interpolaatiosta hyˆty‰?)
                float lowerValue = lowerLevel.FloatValue();
                float upperValue = upperLevel.FloatValue();
                float interpolatedValue =
                    static_cast<float>(NFmiInterpolation::Linear(factor, lowerValue, upperValue));
                newInfo.FloatValue(interpolatedValue);
              }
            }
          }
        }
      }
      // 4. Palauta uusi data
      return newData;
    }
    else
      std::cerr << "Error in " << __FUNCTION__ << ": created fixed data was zero pointer (error in "
                                                  "program or data), continuing with staggered data"
                << std::endl;
  }

  return data;
}

static boost::shared_ptr<NFmiQueryData> MakeStaggeredDataFix(
    nctools::Options &options,
    const boost::shared_ptr<NFmiQueryData> &data,
    const WRFData::TotalDimensionDataSet::value_type &totalDimensionData,
    const BaseGridAreaData &areaData)
{
  if (options.fixstaggered)
  {
    boost::shared_ptr<NFmiQueryData> newData = data;
    if (totalDimensionData.first.xDimStaggered || totalDimensionData.first.yDimStaggered)
    {
      newData = boost::shared_ptr<NFmiQueryData>(
          NFmiQueryDataUtil::Interpolate2OtherGrid(data.get(), &areaData.baseGrid));
      if (newData)
      {
        if (options.verbose)
          std::cerr << "Made staggered data fix for params: " << GetParamNamesListString(newData)
                    << std::endl;
      }
      else
        std::cerr << "Error in " << __FUNCTION__
                  << ": created fixed data was zero pointer (error in program or data), continuing "
                     "with staggered data"
                  << std::endl;
    }
    newData = MakeLevelStaggeredDataFix(newData, totalDimensionData, areaData);
    return newData;
  }
  return data;
}

static bool AreDataCombinable(const boost::shared_ptr<NFmiQueryData> &data1,
                              const boost::shared_ptr<NFmiQueryData> &data2)
{
  // riitt‰‰ kun hila ja level tiedot on samoja, t‰llˆin fiksatut datat voidaan yhdist‰‰ base-dataan
  if (data1->Info()->HPlaceDescriptor() == data2->Info()->HPlaceDescriptor() &&
      data1->Info()->VPlaceDescriptor() == data2->Info()->VPlaceDescriptor())
    return true;
  else
    return false;
}

static bool DataIndexUsed(size_t index, const std::set<size_t> &usedDataIndexies)
{
  std::set<size_t>::iterator it = usedDataIndexies.find(index);
  if (it != usedDataIndexies.end())
    return true;
  else
    return false;
}

static std::vector<boost::shared_ptr<NFmiQueryData> > CombineFixedStaggeredData(
    nctools::Options &options, const std::vector<boost::shared_ptr<NFmiQueryData> > &dataVector)
{
  if (options.fixstaggered && dataVector.size() > 1)
  {
    std::vector<boost::shared_ptr<NFmiQueryData> > newDataVector;
    std::set<size_t> usedDataIndexies;  // t‰h‰n talletetaan ne j-data indeksit, jotka on jo
                                        // k‰ytetty, ett‰ ei tule turhia p‰‰llekk‰isyyksi‰
    for (size_t i = 0; i < dataVector.size() - 1; i++)
    {
      if (!DataIndexUsed(i, usedDataIndexies))
      {
        boost::shared_ptr<NFmiQueryData> data1 = dataVector[i];
        for (size_t j = i + 1; j < dataVector.size(); j++)
        {
          if (!DataIndexUsed(j, usedDataIndexies))
          {
            boost::shared_ptr<NFmiQueryData> data2 = dataVector[j];
            if (AreDataCombinable(data1, data2))
            {
              boost::shared_ptr<NFmiQueryData> newData = boost::shared_ptr<NFmiQueryData>(
                  NFmiQueryDataUtil::CombineParams(data1.get(), data2.get()));
              if (newData)
              {
                data1 = newData;
                usedDataIndexies.insert(j);
                if (options.verbose)
                  std::cerr << "Made fixed data combination (" << i << "+" << j << ") in function "
                            << __FUNCTION__ << std::endl;
              }
              else
                std::cerr << "Error in " << __FUNCTION__
                          << ": combining data failed (error in program or data), continuing with "
                             "original data"
                          << std::endl;
            }
          }
        }
        newDataVector.push_back(data1);
      }
    }

    // lopuksi pit‰‰ lis‰t‰ viimeinen data originaali listasta uuden listan loppuun, jos sit‰ ei ole
    // jo k‰ytetty yhdistelyiss‰
    if (!DataIndexUsed(dataVector.size() - 1, usedDataIndexies))
      newDataVector.push_back(dataVector[dataVector.size() - 1]);

    return newDataVector;
  }
  return dataVector;
}

static std::vector<boost::shared_ptr<NFmiQueryData> > DoFinalProjisionToData(
    nctools::Options &options,
    const BaseGridAreaData &areaData,
    std::vector<boost::shared_ptr<NFmiQueryData> > &dataVector)
{
  if (areaData.doFinalDataProjection)
  {
    std::vector<boost::shared_ptr<NFmiQueryData> > newDataVector;
    for (size_t i = 0; i < dataVector.size(); i++)
    {
      boost::shared_ptr<NFmiQueryData> oldData = dataVector[i];
      if (*(oldData->Info()->Grid()) == areaData.finalDataGrid)
      {  // konversiota ei tarvita, data oli jo halutussa hilassa
        newDataVector.push_back(oldData);
      }
      else
      {
        if (options.verbose) std::cerr << "Doing projision to data " << i << std::endl;
        boost::shared_ptr<NFmiQueryData> newData(
            NFmiQueryDataUtil::Interpolate2OtherGrid(oldData.get(), &areaData.finalDataGrid));
        if (newData)
          newDataVector.push_back(newData);
        else
        {
          std::cerr << "Error in " << __FUNCTION__
                    << ": creating projised data failed (error in program or data), continuing "
                       "with original data"
                    << std::endl;
          newDataVector.push_back(oldData);
        }
      }
    }
    return newDataVector;
  }

  return dataVector;
}

static std::string MakeFinalProducerName(
    const std::string &producerName,
    const WRFData::TotalDimensionDataSet::value_type &totalDimensionData,
    const NFmiFastQueryInfo &info)
{
  bool surfaceData = info.SizeLevels() == 1;
  std::string finalProducerName = producerName;
  if (surfaceData)
    finalProducerName += "_sfc";
  else
  {
    FmiLevelType usedLeveltype = info.Level()->LevelType();
    if (usedLeveltype == kFmiPressureLevel)
      finalProducerName += "_pressure";
    else if (usedLeveltype == kFmiHybridLevel)
    {
      if (ci_find_substr(totalDimensionData.first.levDimName, std::string("soil")) != -1)
        finalProducerName += "_soil";
      else
        finalProducerName += "_hybrid";
    }
    else
      finalProducerName += "_level";
  }

  return finalProducerName;
}

static std::string GetProducerNamePostFix(boost::shared_ptr<NFmiQueryData> &data)
{
  std::string producerName = data->Info()->Producer()->GetName().CharPtr();
  std::string::size_type pos = producerName.find_last_of('_');
  if (pos != std::string::npos) return std::string(producerName.begin() + pos, producerName.end());

  return "";
}

// Yrit‰n kehitt‰‰ uutta NetCdf purkuohjelmaa, joka
// ei v‰lit‰ CV k‰yt‰nnˆist‰, vaan yritt‰‰ tulkita suoraan dataa,
// sen dimensioita ja variable:ita.
// T‰m‰ oon alustavasti tehty WRF-datojen pohjalta.
static int DoWrfData(nctools::Options &options,
                     const NcFile &ncFile,
                     const nctools::ParamConversions &paramconvs)
{
  WRFData::DimensionMap dimensionMap = GetNCDimensionMap(options, ncFile);
  WRFData::TotalDimensionDataSet dimDataSet = GetNCTotalDimensionDataSet(options, ncFile);
  ::PrintWRFGlobalAttributes(options, ncFile);
  BaseGridAreaData areaData = GetBaseAreaData(options, ncFile);
  std::vector<boost::shared_ptr<NFmiQueryData> > dataVector;
  for (WRFData::TotalDimensionDataSet::iterator it = dimDataSet.begin(); it != dimDataSet.end();
       ++it)
  {
    boost::shared_ptr<NFmiQueryInfo> qInfo(
        ::CreateNewInnerInfo(options, *it, areaData, ncFile, paramconvs));
    if (qInfo)
    {
      boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(*qInfo));
      if (data)
      {
        NFmiFastQueryInfo info(data.get());
        info.SetProducer(NFmiProducer(options.producernumber,
                                      MakeFinalProducerName(options.producername, *it, info)));

        nctools::copy_values(options, ncFile, info, paramconvs, true);
        // TODO: Handle unit conversions too!

        data = MakeStaggeredDataFix(options, data, *it, areaData);

        dataVector.push_back(data);
      }
    }
  }

  if (dataVector.size())
  {
    dataVector = DoFinalProjisionToData(options, areaData, dataVector);
    dataVector = CombineFixedStaggeredData(options, dataVector);

    if (options.outfile == "-")
      dataVector[0]->Write();  // stdout tapauksessa tulostetaan vain 1. data
    for (size_t i = 0; i < dataVector.size(); i++)
    {
      std::string producerNamePostFix = GetProducerNamePostFix(dataVector[i]);
      dataVector[i]->Write(options.outfile + "_" + NFmiStringTools::Convert(i) +
                           producerNamePostFix);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char *argv[])
{
  if (!nctools::parse_options(argc, argv, options)) return 0;

  // Default is to exit in some non fatal situations
  NcError errormode(NcError::silent_nonfatal);
  NcFile ncfile(options.infile.c_str(), NcFile::ReadOnly);

  if (!ncfile.is_valid())
    throw std::runtime_error("File '" + options.infile + "' does not contain valid NetCDF");

  // Parameter conversions

  nctools::ParamConversions paramconvs = nctools::read_netcdf_config(options);

#if DEBUG_PRINT
  nctools::debug_output(ncfile);
#endif

  return ::DoWrfData(options, ncfile, paramconvs);
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
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  catch (...)
  {
    std::cerr << "Error: Caught an unknown exception" << std::endl;
    return 1;
  }
}
