// ======================================================================
/*!
 * \file qdpoint.cpp
 * \brief Implementation of the qdpoint program
 */
// ======================================================================
/*!
 * \page qdpoint
 *
 * The program prints the values from the given querydata for the given
 * coordinate, wmo numbers or named points.
 *
 * Examples:
 *
 *	qdpoint -q hirlam.sqd -p Helsinki
 *	qdpoint -x 25 -y 60
 *	qdpoint -p Helsinki -q havainnot.fqd
 *	qdpoint -p Helsinki -P Temperature,Precipitation1h
 *  qdpoint -w -q havainnot.sqd -n 1
 *  qdpoint -p Helsinki -N 10 -d 100 -q havainnot.sqd -n 1
 *  qdpoint -u "mst.weatherproof.fi/gram.php" -p Helsinki -q ennuste.sqd
 *
 * \endcode
 *
 */
// ======================================================================

#include "MoonPhase.h"
#include "QueryDataManager.h"
#include "TimeTools.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <ogr_geometry.h>
#include <macgyver/StringConversion.h>
#include <macgyver/WorldTimeZones.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiDataIntegrator.h>
#include <newbase/NFmiDataModifierProb.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiLocation.h>
#include <newbase/NFmiLocationFinder.h>
#include <newbase/NFmiMetMath.h>
#include <newbase/NFmiPreProcessor.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiValueString.h>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

using namespace std;

//! Must be one single global instance for speed, constructing is expensive
static NFmiEnumConverter converter;

// WGS84 to FMI sphere conversion
std::unique_ptr<OGRCoordinateTransformation> wgs84_to_latlon;

// ----------------------------------------------------------------------
// -l optiolla annetut sijainnit
// ----------------------------------------------------------------------

struct Location
{
  string name;
  NFmiPoint latlon;

  Location(const string& theName, const NFmiPoint& theLatLon) : name(theName), latlon(theLatLon) {}
};

typedef list<Location> LocationList;

// ----------------------------------------------------------------------
// Lue LocationList annetusta tiedostosta
// ----------------------------------------------------------------------

LocationList read_locationlist(const string& theFile)
{
  LocationList ret;

  const bool strip_pound = true;
  NFmiPreProcessor processor(strip_pound);
  if (!processor.ReadAndStripFile(theFile)) throw runtime_error("Unable to preprocess " + theFile);

  string text = processor.GetString();
  istringstream input(text);
  string line;
  while (getline(input, line))
  {
    if (line.empty()) continue;
    vector<string> parts = NFmiStringTools::Split(line, ",");
    if (parts.size() != 3)
      cerr << "Warning: Invalid line '" + line + "' in file '" + theFile + "'" << endl;
    else
    {
      try
      {
        string name = parts[0];
        double lon = NFmiStringTools::Convert<double>(parts[1]);
        double lat = NFmiStringTools::Convert<double>(parts[2]);
        ret.push_back(Location(name, NFmiPoint(lon, lat)));
      }
      catch (...)
      {
        cerr << "Error while reading '" + theFile + "'";
        throw;
      }
    }
  }
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose = false;
  string timezonefile = NFmiSettings::Optional<string>(
      "qdpoint::tzfile", "/usr/share/smartmet/timezones/timezone.shz");
  string timezone = NFmiSettings::Optional<string>("qdpoint::timezone", "local");
  string queryfile = NFmiSettings::Optional<string>("qdpoint::querydata_file", "");
  string coordinatefile = NFmiSettings::Optional<string>("qdpoint::coordinates",
                                                         "/smartmet/share/coordinates/default.txt");
  double longitude = kFloatMissing;
  double latitude = kFloatMissing;
  int rows = -1;
  int max_missing_gap = -1;
  vector<string> params;
  vector<string> places;
  vector<int> stations;
  bool all_stations = false;
  bool list_stations = false;
  bool force = false;
  bool validate = false;
  bool future = false;
  bool multimode = false;
  bool wgs84 = false;
  double max_distance = 100;  // km
  int nearest_stations = 1;
  string locationfile = "";
  LocationList locations;
  string missingvalue = "-";
  string uid;
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of station numbers
 */
// ----------------------------------------------------------------------

vector<int> parse_stations(const std::string& str)
{
  vector<int> ret;

  if (str.empty()) return ret;

  list<string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of(","));

  BOOST_FOREACH (const auto& str, parts)
  {
    ret.push_back(Fmi::stoi(str));
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line options
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char* argv[])
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;

  string opt_stations;
  string opt_places;
  string opt_params;

#ifdef UNIX
  struct winsize wsz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz);
  const int desc_width = (wsz.ws_col < 80 ? 80 : wsz.ws_col);
#else
  const int desc_width = 100;
#endif

  po::options_description desc("Available options", desc_width);
  desc.add_options()("help,h", "print out help message")(
      "verbose,v", po::bool_switch(&options.verbose), "verbose mode")("version,V",
                                                                      "display version number")(
      "querydata,q", po::value(&options.queryfile), "input querydata (qdpoint::querydata_file)")(
      "multidata,Q", po::bool_switch(&options.multimode), "use all files from input directories")(
      "coordinatefile,c",
      po::value(&options.coordinatefile),
      "location configuration file (qdpoint::coordinates or "
      "/smartmet/share/coordinates/default.txt)")("timezonefile,z",
                                                  po::value(&options.timezonefile),
                                                  "timezone configuration file (qdpoint::tzfile)")(
      "timezone,t", po::value(&options.timezone), "timezone (qdpoint::timezone)")(
      "params,P", po::value(&opt_params), "parameter name/number list")(
      "places,p", po::value(&opt_places), "place name list")(
      "longitude,x", po::value(&options.longitude), "longitude")(
      "latitude,y", po::value(&options.latitude), "latitude")(
      "wgs84", po::bool_switch(&options.wgs84), "coordinates are in WGS84")(
      "rows,n",
      po::value(&options.rows)->default_value(-1)->implicit_value(1),
      "number of data rows for each location")(
      "stations,w",
      po::value(&opt_stations)->default_value("")->implicit_value("all"),
      "WMO numbers (plain -w or -w all implies all stations)")(
      "list,s", po::bool_switch(&options.list_stations), "print station metadata")(
      "locations,l", po::value(&options.locationfile), "file containing list of locations")(
      "force,f", po::bool_switch(&options.force), "force print of empty rows too")(
      "maxdist,d",
      po::value(&options.max_distance),
      "maximum search distance stations (default: 100 km)")(
      "count,N",
      po::value(&options.nearest_stations),
      "maximum number of nearby stations (default: 1)")(
      "validate,C", po::bool_switch(&options.validate), "validate rows")(
      "missingvalue,m",
      po::value(&options.missingvalue),
      "string to print for missing values (default: '-')")(
      "maxgap,i",
      po::value(&options.max_missing_gap),
      "maximum time gap in minutes to fill with interpolation")(
      "future,F", po::bool_switch(&options.future), "print only times in the future")(
      "uid,u", po::value(&options.uid), "unused legacy option");

  po::positional_options_description p;
  p.add("querydata", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdpoint v 2.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: qdpoint [options] querydata" << std::endl
              << std::endl
              << "Extract a timeseries from the input data" << std::endl
              << std::endl
              << desc << std::endl
              << std::endl
              << "Available meta parameters:" << std::endl
              << std::endl
              << " * MetaDST - 1/0 depending on whether daylight savings is on or not" << std::endl
              << " * MetaElevationAngle - sun elevation angle" << std::endl
              << " * MetaFeelsLike - feels like temperature" << std::endl
              << " * MetaIsDark - 1/0 depending on the sun elevation angle" << std::endl
              << " * MetaMoonIlluminatedFraction - fraction of moon visible to earth" << std::endl
              << " * MetaN - total cloudiness in 8ths" << std::endl
              << " * MetaNN - bottom/middle level cloudiness in 8ths" << std::endl
              << " * MetaNorth - grid north direction" << std::endl
              << " * MetaRainProbability - crude estimate of probability of precipitation"
              << std::endl
              << " * MetaSnowProb - crude estimate of probability of snow" << std::endl
              << " * MetaSummerSimmer - the summer simmer index" << std::endl
              << " * MetaSurfaceRadiation - estimated solar radiation" << std::endl
              << " * MetaThetaE - theta-E from temperature, pressure and humidity" << std::endl
              << " * MetaWindChill - wind chill factor" << std::endl
              << std::endl;
  }

  if (!options.locationfile.empty()) options.locations = read_locationlist(options.locationfile);

  if (opt_stations == "all")
    options.all_stations = true;
  else
    options.stations = parse_stations(opt_stations);

  options.params = NFmiStringTools::Split(opt_params);
  options.places = NFmiStringTools::Split(opt_places);

  // Create WGS84 to latlon conversion only if necessary
  if (options.wgs84)
  {
    std::unique_ptr<OGRSpatialReference> wgs84_crs;
    std::unique_ptr<OGRSpatialReference> fmi_crs;

    wgs84_crs.reset(new OGRSpatialReference);
    OGRErr err = wgs84_crs->SetFromUserInput("+proj=longlat +ellps=WGS84 +towgs84=0,0,0");
    if (err != OGRERR_NONE) throw std::runtime_error("Failed to setup WGS84 spatial reference");

    // TODO: WGS84 center may be 100 meters off, according to Wikipedia. Perhaps should not have
    // zeros here?
    fmi_crs.reset(new OGRSpatialReference);
    err = fmi_crs->SetFromUserInput("+proj=longlat +a=6371220 +b=6371220 +towgs84=0,0,0 +no_defs");
    if (err != OGRERR_NONE)
      throw std::runtime_error("Failed to setup FMI sphere spatial reference");

    // copies crs's
    wgs84_to_latlon.reset(OGRCreateCoordinateTransformation(wgs84_crs.get(), fmi_crs.get()));
    if (!wgs84_to_latlon)
      throw std::runtime_error("Failed to create WGS84 to FMI Sphere coordinate conversion");
  }

  return true;
}

// ----------------------------------------------------------------------
// Testaa onko koordinaatti laiton
// ----------------------------------------------------------------------

bool IsBad(const NFmiPoint& thePoint)
{
  return (thePoint.X() == kFloatMissing || thePoint.Y() == kFloatMissing);
}

// ----------------------------------------------------------------------
// Convert from WGS84 to FMi Sphere if necessary
// ----------------------------------------------------------------------

NFmiPoint FixCoordinate(const NFmiPoint& thePoint)
{
  if (!options.wgs84) return thePoint;

  double x = thePoint.X();
  double y = thePoint.Y();
  if (!wgs84_to_latlon->Transform(1, &x, &y))
    throw std::runtime_error("Failed to transform input coordinate from WGS84 to FMI Sphere");

  if (options.verbose)
    std::cout << "# Converted from " << thePoint.X() << "," << thePoint.Y() << " to " << x << ","
              << y << " distance = "
              << NFmiGeoTools::GeoDistance(thePoint.X(), thePoint.Y(), x, y) / 1000 << " km\n";

  return NFmiPoint(x, y);
}

// ----------------------------------------------------------------------
// Etsii annetun nimisten pisteiden koordinaatit tietokannasta
// ----------------------------------------------------------------------

std::map<std::string, NFmiPoint> FindPlaces(const std::vector<std::string>& thePlaces,
                                            const std::string& theCoordFile,
                                            bool theForceFlag)
{
  typedef std::map<std::string, NFmiPoint> ReturnType;
  ReturnType ret;

  if (!NFmiFileSystem::FileExists(theCoordFile))
    throw std::runtime_error("File '" + theCoordFile + "' does not exist");

  NFmiLocationFinder locfinder;
  if (!locfinder.AddFile(theCoordFile, false))
    throw std::runtime_error("Reading file " + theCoordFile + " failed");

  for (vector<string>::const_iterator it = thePlaces.begin(); it != thePlaces.end(); ++it)
  {
    NFmiPoint lonlat = locfinder.Find(it->c_str());

    // Tarkistetaan nimen loytyminen.

    if (locfinder.LastSearchFailed() && !theForceFlag)
      throw std::runtime_error("Location " + *it + " is not in the database");
    else
      ret.insert(ReturnType::value_type(*it, lonlat));
  }

  return ret;
}

// ----------------------------------------------------------------------
// Is it DST time?
// ----------------------------------------------------------------------

bool IsDst(NFmiFastQueryInfo& qd)
{
  NFmiMetTime t = qd.ValidTime();

  struct ::tm utc;
  utc.tm_sec = t.GetSec();
  utc.tm_min = t.GetMin();
  utc.tm_hour = t.GetHour();
  utc.tm_mday = t.GetDay();
  utc.tm_mon = t.GetMonth() - 1;     // tm months start from 0
  utc.tm_year = t.GetYear() - 1900;  // tm years start from 1900
  utc.tm_wday = -1;
  utc.tm_yday = -1;
  utc.tm_isdst = -1;

  ::time_t epochtime = NFmiStaticTime::my_timegm(&utc);
  struct ::tm tlocal;

#ifdef _MSC_VER
  // OBS! There are no thread safe localtime(_r) or gmtime(_r) functions in MSVC++ 2008 (or before).
  // Closest things available are some what safer (but not thread safe) and with almost same
  // function
  // definitions are the localtime_s and gmtime_s -functions. Parameters are ordered otherway round
  // and their return value is success status, not struct tm pointer.
  ::localtime_s(&tlocal, &epochtime);
#else
  ::localtime_r(&epochtime, &tlocal);
#endif

  return (tlocal.tm_isdst == 1);
}

// ----------------------------------------------------------------------
// Laskee, onko annetussa pisteessä annettuun aikaan pimeää
// ----------------------------------------------------------------------

bool IsDark(const NFmiPoint& theLatLon, const NFmiTime& theTime)
{
  // Seuraava pöllitty NFmiLocation.cpp tiedostosta

  const double kRefractCorr = -0.0145386;
  NFmiLocation loc(theLatLon.X(), theLatLon.Y());
  NFmiMetTime t(theTime, 1);
  double angle = loc.ElevationAngle(t);
  return (angle < kRefractCorr);
}

// ----------------------------------------------------------------------
// Laskee auringon nousukulman
// ----------------------------------------------------------------------

double ElevationAngle(const NFmiPoint& theLatLon, const NFmiTime& theTime)
{
  // Seuraava pöllitty NFmiLocation.cpp tiedostosta

  NFmiLocation loc(theLatLon.X(), theLatLon.Y());
  NFmiMetTime t(theTime, 1);
  double angle = loc.ElevationAngle(t);
  return angle;
}

// ----------------------------------------------------------------------
// Laskee pakkasen purevuuden
// ----------------------------------------------------------------------

float WindChill(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float temperature, windspeed;

  if (!qd.Param(kFmiTemperature)) return kFloatMissing;

  if (qd.IsGrid())
    temperature = qd.InterpolatedValue(theLatLon);
  else
    temperature = qd.FloatValue();

  if (!qd.Param(kFmiWindSpeedMS)) return kFloatMissing;

  if (qd.IsGrid())
    windspeed = qd.InterpolatedValue(theLatLon);
  else
    windspeed = qd.FloatValue();

  if (temperature == kFloatMissing || windspeed == kFloatMissing) return kFloatMissing;

  return FmiWindChill(windspeed, temperature);
}

// ----------------------------------------------------------------------
// Laskee FeelsLike lukeman
// ----------------------------------------------------------------------

float FeelsLike(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float temperature, windspeed, humidity, radiation;

  if (!qd.Param(kFmiTemperature)) return kFloatMissing;

  if (qd.IsGrid())
    temperature = qd.InterpolatedValue(theLatLon);
  else
    temperature = qd.FloatValue();

  if (!qd.Param(kFmiWindSpeedMS)) return kFloatMissing;

  if (qd.IsGrid())
    windspeed = qd.InterpolatedValue(theLatLon);
  else
    windspeed = qd.FloatValue();

  if (temperature == kFloatMissing || windspeed == kFloatMissing) return kFloatMissing;

  if (!qd.Param(kFmiHumidity)) return kFloatMissing;

  if (qd.IsGrid())
    humidity = qd.InterpolatedValue(theLatLon);
  else
    humidity = qd.FloatValue();

  radiation = kFloatMissing;
  if (qd.Param(kFmiRadiationGlobal))
  {
    if (qd.IsGrid())
      radiation = qd.InterpolatedValue(theLatLon);
    else
      radiation = qd.FloatValue();
  }

  return FmiFeelsLikeTemperature(windspeed, humidity, temperature, radiation);
}

// ----------------------------------------------------------------------
// Laskee SummerSimmer lukeman
// ----------------------------------------------------------------------

float SummerSimmer(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float temperature, humidity;

  if (!qd.Param(kFmiTemperature)) return kFloatMissing;

  if (qd.IsGrid())
    temperature = qd.InterpolatedValue(theLatLon);
  else
    temperature = qd.FloatValue();

  if (!qd.Param(kFmiHumidity)) return kFloatMissing;

  if (qd.IsGrid())
    humidity = qd.InterpolatedValue(theLatLon);
  else
    humidity = qd.FloatValue();

  return FmiSummerSimmerIndex(humidity, temperature);
}

// ----------------------------------------------------------------------
// Laskee ThetaE parametrin
// ----------------------------------------------------------------------

float ThetaE(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float temperature, pressure, humidity;

  if (!qd.Param(kFmiTemperature)) return kFloatMissing;
  if (qd.IsGrid())
    temperature = qd.InterpolatedValue(theLatLon);
  else
    temperature = qd.FloatValue();

  if (!qd.Param(kFmiHumidity)) return kFloatMissing;
  if (qd.IsGrid())
    humidity = qd.InterpolatedValue(theLatLon);
  else
    humidity = qd.FloatValue();

  if (qd.Param(kFmiPressure))
  {
    if (qd.IsGrid())
      pressure = qd.InterpolatedValue(theLatLon);
    else
      pressure = qd.FloatValue();
  }
  else if (qd.Level()->LevelType() == kFmiPressureLevel)
  {
    pressure = qd.Level()->LevelValue();
  }
  else
    return kFloatMissing;

  if (temperature == kFloatMissing || pressure == kFloatMissing || humidity == kFloatMissing)
    return kFloatMissing;

  float thetae =
      (273.15 + temperature) * pow(1000.0 / pressure, 0.286) +
      (3 *
       (humidity * (3.884266 * pow(10.0, ((7.5 * temperature) / (237.7 + temperature)))) / 100));

  return thetae - 273.15;
}

// ----------------------------------------------------------------------
// Laskee sateen todennäköisyyden annetussa pisteessä asetettuun aikaan.
//
// Huom: Aluemaskin säde riippuu ennustuspituudesta T seuraavasti:
//
//       T  < 4h : 30km
//       T = 24h : 75km
//       T = 48h : 100km
//       T = 96h : 150km
//
// Kullakin välillä maskin sädettä interpoloidaan lineaarisesti,
// 96h tunnin jälkeen extrapoloidaan lineaarisesti, eli säde
// kasvaa 50km 48 tunnissa.
//
// ----------------------------------------------------------------------

float RainProbability(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  if (qd.Grid() == 0) return kFloatMissing;

  if (!qd.Param(kFmiPrecipitation1h)) return kFloatMissing;

  const NFmiTime validTime = qd.ValidTime();
  const NFmiTime originTime = qd.OriginTime();

  const long diff = validTime.DifferenceInHours(originTime);

  const int T1 = 4;
  const int T2 = 24;
  const int T3 = 48;
  const int T4 = 96;

  const float R1 = 30;
  const float R2 = 75;
  const float R3 = 100;
  const float R4 = 150;

  float radius;
  if (diff <= T1)
    radius = R1;
  else if (diff <= T2)
    radius = R1 + (R2 - R1) * (diff - T1) / static_cast<float>(T2 - T1);
  else if (diff <= T3)
    radius = R2 + (R3 - R2) * (diff - T2) / static_cast<float>(T3 - T2);
  else
    radius = R3 + (R4 - R3) * (diff - T3) / static_cast<float>(T4 - T3);

  const NFmiIndexMask mask = NFmiIndexMaskTools::MaskDistance(*qd.Grid(), theLatLon, radius);

  NFmiDataModifierProb modif(kValueGreaterOrEqualThanLimit, 0.1);
  const float result = NFmiDataIntegrator::Integrate(qd, mask, modif);

  return result;
}

// ----------------------------------------------------------------------
// Pilvisyys N 8-osina
// ----------------------------------------------------------------------

float CloudinessN(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float n;

  if (!qd.Param(kFmiTotalCloudCover)) return kFloatMissing;

  if (qd.IsGrid())
    n = qd.InterpolatedValue(theLatLon);
  else
    n = qd.FloatValue();

  if (n == kFloatMissing) return kFloatMissing;

  return round(n / 100 * 8);
}

// ----------------------------------------------------------------------
// Pilvisyys NN 8-osina
// ----------------------------------------------------------------------

float CloudinessNN(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float n;

  if (!qd.Param(kFmiMiddleAndLowCloudCover)) return kFloatMissing;

  if (qd.IsGrid())
    n = qd.InterpolatedValue(theLatLon);
  else
    n = qd.FloatValue();

  if (n == kFloatMissing) return kFloatMissing;

  return round(n / 100 * 8);
}

// ----------------------------------------------------------------------
// Lumisateen todennäköisyys
// ----------------------------------------------------------------------

float SnowProb(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  float t2m = kFloatMissing;
  float rh = kFloatMissing;

  if (!qd.Param(kFmiTemperature)) return kFloatMissing;
  if (qd.IsGrid())
    t2m = qd.InterpolatedValue(theLatLon);
  else
    t2m = qd.FloatValue();

  if (!qd.Param(kFmiHumidity)) return kFloatMissing;
  if (qd.IsGrid())
    rh = qd.InterpolatedValue(theLatLon);
  else
    rh = qd.FloatValue();

  if (t2m == kFloatMissing || rh == kFloatMissing) return kFloatMissing;

  return static_cast<float>(100 * (1 - 1 / (1 + exp(22 - 2.7 * t2m - 0.2 * rh))));
}

// ----------------------------------------------------------------------
// Kuun vaihe UTC-ajalle
// ----------------------------------------------------------------------

float MoonIlluminatedFraction(const NFmiTime& theTime)
{
  return static_cast<float>(MoonFraction(theTime));
}

// ----------------------------------------------------------------------
/*!
 * \brief North adjustment
 */
// ----------------------------------------------------------------------

float North(NFmiFastQueryInfo& qd, const NFmiPoint& theLatLon)
{
  auto area = qd.Area();
  if (!area) return kFloatMissing;

  return qd.Area()->TrueNorthAzimuth(theLatLon).ToDeg();
}

// ----------------------------------------------------------------------
// Keskimääräinen säteily pinnalla
// ----------------------------------------------------------------------

float SurfaceRadiation(const NFmiTime& theTime, const NFmiPoint& theLatLon)
{
  NFmiLocation loc(theLatLon);
  double elev = loc.ElevationAngle(theTime);

  const int month = theTime.GetMonth();
  const int day = theTime.GetDay();

  const double zday = ((month - 1) * 30.4375 + day - 1.9) * 2 * kPii / 365;
  const double sa =
      1360 * (1 + 0.034221 * cos(zday) + 0.00128 * sin(zday) + 0.000719 * cos(2 * zday));

  return static_cast<float>(max(0.0, sa * cos((90 - elev) * kPii / 180)));
}

// ----------------------------------------------------------------------
// Laskee metafunktion arvon
// ----------------------------------------------------------------------

float MetaFunction(NFmiFastQueryInfo& qd, const string& name, NFmiPoint lonlat)
{
  if (name == "MetaIsDark")
    return IsDark(lonlat, qd.ValidTime());
  else if (name == "MetaElevationAngle")
    return static_cast<float>(ElevationAngle(lonlat, qd.ValidTime()));
  else if (name == "MetaRainProbability")
    return RainProbability(qd, lonlat);
  else if (name == "MetaWindChill")
    return WindChill(qd, lonlat);
  else if (name == "MetaN")
    return CloudinessN(qd, lonlat);
  else if (name == "MetaNN")
    return CloudinessNN(qd, lonlat);
  else if (name == "MetaMoonIlluminatedFraction")
    return MoonIlluminatedFraction(qd.ValidTime());
  else if (name == "MetaSnowProb")
    return SnowProb(qd, lonlat);
  else if (name == "MetaSurfaceRadiation")
    return SurfaceRadiation(qd.ValidTime(), lonlat);
  else if (name == "MetaThetaE")
    return ThetaE(qd, lonlat);
  else if (name == "MetaDST")
    return IsDst(qd);
  else if (name == "MetaFeelsLike")
    return FeelsLike(qd, lonlat);
  else if (name == "MetaSummerSimmer")
    return SummerSimmer(qd, lonlat);
  else if (name == "MetaNorth")
    return North(qd, lonlat);
  else
    return kFloatMissing;
}

// ----------------------------------------------------------------------
// Palauttaa metafunktion tarkkuuden
// ----------------------------------------------------------------------

const char* MetaPrecision(const string& name)
{
  if (name == "MetaIsDark")
    return "%g";
  else if (name == "MetaElevationAngle")
    return "%.3f";
  else if (name == "MetaWindChill")
    return "%.1f";
  else if (name == "MetaN")
    return "%.0f";
  else if (name == "MetaNN")
    return "%.0f";
  else if (name == "MetaMoonIlluminatedFraction")
    return "%.1f";
  else if (name == "MetaSnowProb")
    return "%.1f";
  else if (name == "MetaSurfaceRadiation")
    return "%1.f";
  else if (name == "MetaThetaE")
    return "%.1f";
  else if (name == "MetaFeelsLike")
    return "%.1f";
  else if (name == "SummerSimmer")
    return "%.1f";
  else if (name == "MetaDST")
    return "%g";
  else if (name == "MetaNorth")
    return "%.1f";
  else
    return "%f";
}

// ----------------------------------------------------------------------
// Convert parameter description (name/number) to enum
// ----------------------------------------------------------------------

FmiParameterName ParamEnum(const string& theParam)
{
  FmiParameterName num = FmiParameterName(converter.ToEnum(theParam));
  if (num != kFmiBadParameter) return num;

  try
  {
    return FmiParameterName(boost::lexical_cast<unsigned int>(theParam));
  }
  catch (...)
  {
    return kFmiBadParameter;
  }
}

// ----------------------------------------------------------------------
// Irrottaa parametrinimen speksistä param:level
// ----------------------------------------------------------------------

const string ParamName(const string& theParam)
{
  if (theParam.empty()) return theParam;

  vector<string> parts = NFmiStringTools::Split(theParam, ":");
  return parts[0];
}

// ----------------------------------------------------------------------
// Irrottaa parametrilevelin speksistä param:level
// ----------------------------------------------------------------------

long ParamLevel(const string& theParam)
{
  if (theParam.empty()) return -1;

  vector<string> parts = NFmiStringTools::Split(theParam, ":");
  if (parts.size() != 2) return -1;
  return NFmiStringTools::Convert<long>(parts[1]);
}

// ----------------------------------------------------------------------
// Testaa ovatko asetetun sijainnin ja ajan halutut parametrit valideja.
// Yksi ainoa validi arvo riittää.
// ----------------------------------------------------------------------

bool ValidRow(NFmiFastQueryInfo& qd,
              bool ignoresubs,
              NFmiPoint lonlat = NFmiPoint(kFloatMissing, kFloatMissing))
{
  bool valid = false;

  qd.ResetParam();

  vector<string>::const_iterator iter = options.params.begin();

  while (!valid)
  {
    float value;

    if (options.params.empty())
    {
      if (!qd.NextParam(ignoresubs)) break;
      if (qd.IsGrid())
        value = qd.InterpolatedValue(lonlat);
      else
        value = qd.FloatValue();
    }
    else
    {
      if (iter == options.params.end()) break;

      string name = ParamName(*iter);
      long level = ParamLevel(*iter);

      if (level < 0)
        qd.FirstLevel();
      else
      {
        for (qd.ResetLevel(); qd.NextLevel();)
          if (qd.Level()->LevelValue() == static_cast<unsigned int>(level)) break;
      }

      if (!qd.IsLevel())
      {
        value = kFloatMissing;
      }
      else
      {
        if (name.substr(0, 4) != "Meta")
        {
          if (!qd.Param(ParamEnum(name)))
          {
            // throw runtime_error("Unknown parameter: "+name);
          }
          ++iter;

          if (qd.IsGrid())
            value = qd.InterpolatedValue(lonlat);
          else
            value = qd.FloatValue();
        }
        else
          value = MetaFunction(qd, name, lonlat);
      }
    }

    valid = (value != kFloatMissing);
  }

  qd.FirstLevel();

  return valid;
}

// ----------------------------------------------------------------------
// Laskee puuttuvan datajakson pituuden
// ----------------------------------------------------------------------

int MissingGap(NFmiFastQueryInfo& qd,
               const NFmiPoint& lonlat = NFmiPoint(kFloatMissing, kFloatMissing))
{
  bool interpolate = (lonlat.X() != kFloatMissing);

  int oldTimeIndex = qd.TimeIndex();
  int timeIndex1 = oldTimeIndex;
  int timeIndex2 = oldTimeIndex;
  for (; qd.PreviousTime();)
  {
    float value = (interpolate ? qd.InterpolatedValue(lonlat) : qd.FloatValue());
    if (value == kFloatMissing)
      timeIndex1 = qd.TimeIndex();
    else
      break;
  }

  qd.TimeIndex(oldTimeIndex);
  for (; qd.NextTime();)
  {
    float value = (interpolate ? qd.InterpolatedValue(lonlat) : qd.FloatValue());
    if (value == kFloatMissing)
      timeIndex2 = qd.TimeIndex();
    else
      break;
  }

  qd.TimeIndex(timeIndex1);
  int timeRes1 = qd.TimeResolution();
  NFmiMetTime time1(qd.Time());
  qd.TimeIndex(timeIndex2);
  NFmiMetTime time2(qd.Time());
  qd.TimeIndex(oldTimeIndex);
  int diff = time2.DifferenceInMinutes(time1) + 2 * timeRes1;

  return diff;
}

// ----------------------------------------------------------------------
// Laskee interpoloidun arvon annettuun koordinaaattiin
// ----------------------------------------------------------------------

float InterpolatedValue(NFmiFastQueryInfo& qd, int maxmissminutes, const NFmiPoint& lonlat)
{
  float value = qd.InterpolatedValue(lonlat);
  if (maxmissminutes <= 0 || value != kFloatMissing) return value;

  // Try to interpolate the value

  int gap = MissingGap(qd, lonlat);
  if (gap > maxmissminutes) return kFloatMissing;

  return qd.InterpolatedValue(lonlat, qd.ValidTime(), maxmissminutes);
}

// ----------------------------------------------------------------------
// Laskee interpoloidun arvon asetettuun pisteeseen
// ----------------------------------------------------------------------

float InterpolatedValue(NFmiFastQueryInfo& qd, int maxmissminutes)
{
  float value = qd.FloatValue();
  if (maxmissminutes <= 0 || value != kFloatMissing) return value;

  // Try to interpolate the value

  int gap = MissingGap(qd);
  if (gap > maxmissminutes) return kFloatMissing;

  return qd.InterpolatedValue(qd.ValidTime(), maxmissminutes);
}

// ----------------------------------------------------------------------
// Tulosta asetetun sijainnin ja ajan halutut parametrit. Annettu boolean
// määrää, tulostetaanko wmo numero.
// ----------------------------------------------------------------------

void PrintRow(NFmiFastQueryInfo& qd,
              bool ignoresubs,
              const Fmi::WorldTimeZones& zones,
              NFmiPoint lonlat = NFmiPoint(kFloatMissing, kFloatMissing))
{
  // Kello nyt UTC-ajassa minuutin tarkkuudella

  NFmiMetTime now(1);

  // Kellonaika

  NFmiMetTime utctime = qd.ValidTime();

  // Ei näytetä, jos käyttäjä ei halua menneisyyttä

  if (options.future && utctime.IsLessThan(now)) return;

  // Rivi näytetään

  NFmiTime t;

  if (options.timezone == "local")
  {
#if 0
	  // Do not use LocalTime approximations if the point seems to be in Finland
	  if(lonlat.X() > 19 &&
		 lonlat.X() < 32 &&
		 lonlat.Y() > 59 &&
		 lonlat.Y() < 71)
		t = utctime.CorrectLocalTime();
	  else
		t = utctime.LocalTime(static_cast<float>(lonlat.X()));
#else
    string tz = zones.zone_name(lonlat.X(), lonlat.Y());
    t = TimeTools::timezone_time(utctime, tz);
#endif
  }
  else
    t = TimeTools::timezone_time(utctime, options.timezone);

  cout << t.ToStr(kYYYYMMDDHHMM).CharPtr();

  qd.ResetParam();

  vector<string>::const_iterator iter = options.params.begin();

  while (true)
  {
    float value;
    NFmiString precision;

    if (options.params.empty())
    {
      if (!qd.NextParam(ignoresubs)) break;
      precision = qd.Param().GetParam()->Precision();

      if (qd.IsGrid())
        value = InterpolatedValue(qd, options.max_missing_gap, lonlat);
      else
        value = InterpolatedValue(qd, options.max_missing_gap);
    }
    else
    {
      if (iter == options.params.end()) break;

      string name = ParamName(*iter);
      long level = ParamLevel(*iter);

      if (level < 0)
        qd.FirstLevel();
      else
      {
        for (qd.ResetLevel(); qd.NextLevel();)
          if (qd.Level()->LevelValue() == static_cast<unsigned int>(level)) break;
      }

      if (!qd.IsLevel())
      {
        value = kFloatMissing;
      }
      else if (name.substr(0, 4) != "Meta")
      {
        FmiParameterName p = ParamEnum(name);
        if (!qd.Param(p))
        {
          // throw runtime_error("Unknown parameter: "+name);
        }

        precision = qd.Param().GetParam()->Precision();

        if (qd.IsGrid())
          value = InterpolatedValue(qd, options.max_missing_gap, lonlat);
        else
          value = InterpolatedValue(qd, options.max_missing_gap);
      }
      else
      {
        value = MetaFunction(qd, name, lonlat);
        precision = MetaPrecision(name);
      }
      ++iter;
    }

    // Pyöristetään negatiivinen nolla nollaksi
    if (value == -0) value = 0;

    NFmiString tmp;
    if (value == kFloatMissing)
      tmp = options.missingvalue;
    else
      tmp = NFmiValueString(value, precision);

    cout << " " << tmp.CharPtr();
  }
  cout << endl;
}

// ----------------------------------------------------------------------
/*
 * Tulostaa tiedot asetetusta asemasta. Jos mukana annetaan koordinaatti,
 * tulostetaan myös etäisyys koordinaattiin.
 */
// ----------------------------------------------------------------------

void PrintLocationInfo(QueryDataManager& theMgr,
                       bool printdist = false,
                       const NFmiPoint lonlat = NFmiPoint(kFloatMissing, kFloatMissing))
{
  NFmiFastQueryInfo& qd = theMgr.info();

  const char separator = '\t';
  string wmostr = NFmiValueString(static_cast<long>(qd.Location()->GetIdent()), "%05d").CharPtr();
  cout << "# StationName" << separator << wmostr << separator << qd.Location()->GetName().CharPtr()
       << endl;
  cout << "# StationLoc" << separator << wmostr << separator << qd.Location()->GetLongitude()
       << separator << qd.Location()->GetLatitude() << endl;
  if (printdist)
    cout << "# StationDist" << separator << wmostr << separator
         << qd.Location()->Distance(lonlat) / 1000 << endl;
}

// ----------------------------------------------------------------------
/*!
 * Tulostaa tiedot datan timebagistä.
 *
 * \todo Muuta NFmiFastQueryInfo luokan TimeResolution metodi constiksi
 */
// ----------------------------------------------------------------------

void ReportTimes(QueryDataManager& theMgr)
{
  if (!theMgr.isset()) return;

  const char separator = '\t';

  NFmiFastQueryInfo& qd = theMgr.info();

  // Oma kopio, jotta voidaan siirtyä ensimmäiseen/viimeiseen aikaan
  qd.FirstTime();
  NFmiTime t1 = qd.Time().CorrectLocalTime();

  while (qd.NextTime())
    ;
  qd.PreviousTime();
  // qd.LastTime();

  NFmiTime t2 = qd.Time().CorrectLocalTime();

  cout << "# TimeStart" << separator << t1.ToStr(kYYYYMMDDHHMM).CharPtr() << endl
       << "# TimeEnd" << separator << t2.ToStr(kYYYYMMDDHHMM).CharPtr() << endl
       << "# TimeStep" << separator << qd.TimeResolution() << endl
       << "# TimeSteps" << separator << qd.SizeTimes() << endl;
}

// ----------------------------------------------------------------------
// Print location info for the user
// ----------------------------------------------------------------------

void ReportStations(QueryDataManager& theMgr, const vector<int> theWmos, const NFmiPoint& theLonLat)
{
  if (theWmos.empty())
    PrintLocationInfo(theMgr, !IsBad(theLonLat), theLonLat);
  else
  {
    if (options.verbose) cout << "# Stations " << theWmos.size() << endl;
    vector<int>::const_iterator begin = theWmos.begin();
    vector<int>::const_iterator end = theWmos.end();
    for (vector<int>::const_iterator iter = begin; iter != end; ++iter)
    {
      theMgr.setstation(*iter);
      PrintLocationInfo(theMgr, !IsBad(theLonLat), theLonLat);
    }
  }
}

// ----------------------------------------------------------------------
// Printtaa tietoa parametreista
// ----------------------------------------------------------------------

void ReportParams(QueryDataManager& theMgr,
                  const vector<int>& theWmos,
                  const vector<string>& theParams)
{
  if (!theMgr.isset()) return;

  int sarake = 1;
  if (!theWmos.empty()) cout << "# Column " << sarake++ << ": WMO-number" << endl;
  cout << "# Column " << sarake++ << ": Local time" << endl;

  NFmiFastQueryInfo& qd = theMgr.info();

  qd.FirstLevel();
  qd.FirstTime();

  qd.ResetParam();
  int i = 2;

  bool ignoresubs = false;

  if (theParams.empty())
  {
    while (qd.NextParam(ignoresubs))
    {
      NFmiString name = qd.Param().GetParamName();
      long ident = qd.Param().GetParamIdent();
      string name2 = converter.ToString(ident);
      cout << "# Column " << sarake++ << ": " << name.CharPtr() << " ( kFmi" << name2.c_str()
           << " = " << ident << " )" << endl;
      i++;
    }
  }

  else
  {
    vector<string>::const_iterator iter;
    for (iter = theParams.begin(); iter != theParams.end(); ++iter)
    {
      string name = ParamName(*iter);

      long ident;
      string name2;

      if (name.substr(0, 4) == "Meta")
      {
        ident = -1;
        name2 = "-";
      }
      else
      {
        if (!qd.Param(ParamEnum(*iter)))
        {
          // throw runtime_error("Unknown parameter: "+*iter);
        }
        name = qd.Param().GetParamName();
        ident = qd.Param().GetParamIdent();
        name2 = "kFmi" + converter.ToString(ident);
      }
      cout << "# Column " << sarake++ << ": " << name << " ( " << name2 << " = " << ident << " )"
           << endl;
      i++;
    }
  }
}

// ----------------------------------------------------------------------
// Paaohjelma
//
// 1. Luetaan optiot
// 2. Konvertoidaan mahdollinen paikannimi koordinaateiksi
// 3. Tarkistetaan koordinaattien jarkevyys
// 4. Luetaan data
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv)) return 0;

  typedef map<string, NFmiPoint> PlacesType;
  PlacesType places;

  // Initialize the querydata manager

  QueryDataManager qmgr;
  qmgr.searchpath(NFmiSettings::Optional<string>("qdpoint::querydata_path", "."));
  qmgr.addfiles(NFmiStringTools::Split(options.queryfile));

  if (options.multimode) qmgr.multimode();

  // Initialize timezone finder

  Fmi::WorldTimeZones zones(options.timezonefile);

  // Muodostetaan paikka -x ja -y koordinaateista

  if (options.longitude != kFloatMissing && options.latitude != kFloatMissing)
    places.insert(PlacesType::value_type("-", NFmiPoint(options.longitude, options.latitude)));
  else if (options.longitude != kFloatMissing || options.latitude != kFloatMissing)
    throw std::runtime_error("Both longitude and latitude must be given");

  // Muunnetaan mahdolliset placename optiot koordinaateiksi

  if (!options.places.empty())
  {
    // Muuttujalle coordfile annetaan oletusarvo, eli tassa on
    // ylivarmistusta.

    if (options.coordinatefile.empty())
      throw runtime_error("Places defined but no coordinates file is set");

    places = FindPlaces(options.places, options.coordinatefile, options.force);

    if (options.verbose)
    {
      for (map<string, NFmiPoint>::const_iterator it = places.begin(); it != places.end(); ++it)
        cout << "# Location: " << it->first << endl
             << "# Coordinate: " << it->second.X() << ' ' << it->second.Y() << endl;
    }
  }

  // Referenssipiste lähimpiä pisteitä haettaessa

  NFmiPoint referencelonlat(kFloatMissing, kFloatMissing);
  if (options.nearest_stations >= 1 && places.size() == 1) referencelonlat = places.begin()->second;

  // Listataan kaikki asemat tarvittaessa

  if (options.all_stations)
  {
    std::set<int> stats = qmgr.stations();
    std::copy(stats.begin(), stats.end(), back_inserter(options.stations));
  }
  // Listataan kaikki lähimmat asemat tarvittaessa

  else if (options.nearest_stations > 1 || options.validate)
  {
    if (places.size() != 1)
      throw runtime_error("Must specify exactly 1 reference point for -N or -C");

    std::map<double, int> stats = qmgr.nearest(
        referencelonlat, options.nearest_stations, 1000 * options.max_distance, options.validate);

    for (std::map<double, int>::const_iterator it = stats.begin(); it != stats.end(); ++it)
      options.stations.push_back(it->second);

    // Ja putsataan places - sitä tarvittiin vain lähimpien pisteiden hakuun
    places.clear();
  }

  // Tarkistetaan, etta saatiin jarkevat koordinaatit jotain kautta
  // (Halutaan joko wmo-numerot tai koordinaatit)

  if (options.stations.empty() && !options.all_stations && places.empty() &&
      options.locations.empty())
    throw runtime_error("No valid coordinates given");

  // Näytetään asemat, jos -v tai -s on annettu

  if (options.list_stations || options.verbose)
  {
    if (!places.empty()) qmgr.setpoint(places.begin()->second, 1000 * options.max_distance);
    ReportStations(qmgr, options.stations, referencelonlat);
  }

  // Näytetään aika-askeleet, jos -v on annettu

  if (options.verbose) ReportTimes(qmgr);

  // Näytetään parametrinimet, jos -v on annettu

  if (options.verbose) ReportParams(qmgr, options.stations, options.params);

  // Jos options.rows > 0, halutaan N viimeisintä aikaa, muutoin kaikki ajat

  bool ignoresubs = false;

  NFmiFastQueryInfo* qi;

  if (!options.stations.empty())
  {
    vector<int>::const_iterator begin = options.stations.begin();
    vector<int>::const_iterator end = options.stations.end();
    for (vector<int>::const_iterator iter = begin; iter != end; ++iter)
    {
      qmgr.setstation(*iter);
      qi = &qmgr.info();

      qi->FirstLevel();

      if (!qi->Location(*iter)) continue;

      NFmiPoint lonlat = FixCoordinate(qi->Location()->GetLocation());

      if (options.rows < 0)
      {
        qi->ResetTime();
        while (qi->NextTime())
        {
          // Taaksepäin yhteensopivuus vaatii, että
          // tulostetaan WMO-numero vain kun niitä on useita
          if (options.stations.size() > 1) cout << NFmiValueString(*iter, "%05d").CharPtr() << ' ';
          PrintRow(*qi, ignoresubs, zones, lonlat);
        }
      }
      else
      {
        while (qi->NextTime())
          ;
        qi->PreviousTime();
        // qi->LastTime();

        int rows = options.rows;
        do
        {
          if (options.max_missing_gap > 0 || ValidRow(*qi, ignoresubs))
          {
            cout << NFmiValueString(*iter, "%05d").CharPtr() << ' ';
            PrintRow(*qi, ignoresubs, zones, lonlat);
            --rows;
          }
        } while (rows > 0 && qi->PreviousTime());
        if (rows > 0 && options.verbose)
          cout << "# Warning: " << rows << " missing rows for WMO-number " << *iter
               << " due to insufficient data in the queryfile" << endl;
      }
    }
  }
  else if (!options.locations.empty())
  {
    for (LocationList::const_iterator it = options.locations.begin(); it != options.locations.end();
         ++it)
    {
      NFmiPoint lonlat = FixCoordinate(it->latlon);
      try
      {
        qmgr.setpoint(lonlat, 1000 * options.max_distance);
        qi = &qmgr.info();
      }
      catch (...)
      {
        if (options.force) continue;
        throw;
      }

      qi->FirstLevel();
      if (options.rows < 0)
      {
        qi->ResetTime();
        while (qi->NextTime())
        {
          cout << it->name << ' ';
          PrintRow(*qi, ignoresubs, zones, lonlat);
        }
      }
      else
      {
        while (qi->NextTime())
          ;
        qi->PreviousTime();
        // qi->LastTime()
        int rows = options.rows;
        do
        {
          if (options.max_missing_gap > 0 || ValidRow(*qi, ignoresubs, lonlat))
          {
            cout << it->name << ' ';
            PrintRow(*qi, ignoresubs, zones, lonlat);
            --rows;
          }
        } while (rows > 0 && qi->PreviousTime());
        if (rows > 0 && options.verbose)
          cout << "# Warning: " << rows << " missing rows due to insufficient data in the queryfile"
               << endl;
      }
    }
  }
  else
  {
    for (PlacesType::const_iterator it = places.begin(); it != places.end(); ++it)
    {
      if (places.size() > 1) cout << "Location: " << it->first << endl;

      NFmiPoint lonlat = FixCoordinate(it->second);
      try
      {
        qmgr.setpoint(lonlat, 1000 * options.max_distance);
        qi = &qmgr.info();
      }
      catch (...)
      {
        if (options.force) continue;
        throw;
      }

      qi->FirstLevel();
      if (options.rows < 0)
      {
        qi->ResetTime();
        while (qi->NextTime())
          PrintRow(*qi, ignoresubs, zones, lonlat);
      }
      else
      {
        while (qi->NextTime())
          ;
        qi->PreviousTime();
        // qi->LastTime()
        int rows = options.rows;
        do
        {
          if (options.max_missing_gap > 0 || ValidRow(*qi, ignoresubs, lonlat))
          {
            PrintRow(*qi, ignoresubs, zones, lonlat);
            --rows;
          }
        } while (rows > 0 && qi->PreviousTime());
        if (rows > 0 && options.verbose)
          cout << "# Warning: " << rows << " missing rows due to insufficient data in the queryfile"
               << endl;
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------
// Paaohjelma
//
// 1. Luetaan optiot
// 2. Konvertoidaan mahdollinen paikannimi koordinaateiksi
// 3. Tarkistetaan koordinaattien jarkevyys
// 4. Luetaan data
// ----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (const std::runtime_error& e)
  {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }
  catch (...)
  {
    cerr << "Error: An unknown exception occurred" << endl;
    return 1;
  }
}
