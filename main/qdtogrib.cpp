// Example encoding program.

#include "GribTools.h"
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiQueryData.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <grib_api.h>
#include <map>
#include <string>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // winkkari puolella fopen -funktio koetaan turvattomaksi ja siit‰
                                 // tulee varoituksia (MSVC++ k‰‰nt‰j‰n 4996)
#endif

typedef std::vector<ParamChangeItem> ParamChangeTable;

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options() = default;

  std::string infile = "-";        // -i --infile
  std::string outfile = "-";       // -o --outfile
  std::string packing = "";        // -p --packing, empty implies use ECCODES default
  bool grib1 = false;              // -1, --grib1
  bool grib2 = false;              // -2, --grib2
  bool split = false;              // -s --split
  bool crop = false;               // -d --delete
  bool ignore_origintime = false;  // -I --ignore-origintime
  bool verbose = false;            // -v --verbose
  bool dump = false;               // -D --dump ; generate a grib_api dump
  std::string centre = "";         // -C --centre
  int subcentre = 0;               // -S --subcentre
  bool list_centres = false;       // -L --list-centres
  NFmiLevel level;                 // -l --level
  ParamChangeTable ptable;         // -c --config
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Known centres
 */
// ----------------------------------------------------------------------

std::map<std::string, int> centres = {{"WMO", 0},
                                      {"ammc", 1},
                                      {"Melbourne", 2},
                                      {"rums", 4},
                                      {"Moscow", 5},
                                      {"kwbc", 7},
                                      {"NCEP", 7},
                                      {"NWSTG", 8},
                                      {"Cairo", 10},
                                      {"Dakar", 12},
                                      {"Nairobi", 14},
                                      {"Atananarivo", 16},
                                      {"Tunis", 18},
                                      {"Casablanca", 18},
                                      {"Las Palmas", 20},
                                      {"Algiers", 21},
                                      {"Lagos", 22},
                                      {"fapr", 24},
                                      {"Pretoria", 24},
                                      {"Khabarovsk", 26},
                                      {"vabb", 28},
                                      {"dems", 29},
                                      {"Novosibirsk", 30},
                                      {"Tashkent", 32},
                                      {"Jeddah", 33},
                                      {"rjtd", 34},
                                      {"Tokyo", 34},
                                      {"Bankok", 36},
                                      {"Ulan Bator", 37},
                                      {"babj", 38},
                                      {"Beijing", 38},
                                      {"rksl", 40},
                                      {"Seoul", 40},
                                      {"Buenos Aires", 41},
                                      {"Brasilia", 43},
                                      {"Santiago", 45},
                                      {"sbsj", 46},
                                      {"Miami", 51},
                                      {"cwao", 54},
                                      {"Montreal", 54},
                                      {"San Francisco", 55},
                                      {"fnmo", 58},
                                      {"NOAA", 59},
                                      {"NCAR", 60},
                                      {"Honolulu", 64},
                                      {"Darwin", 65},
                                      {"Melbourne", 67},
                                      {"Wellington", 69},
                                      {"egrr", 74},
                                      {"Exeter", 74},
                                      {"edzw", 78},
                                      {"Offenbach", 78},
                                      {"cnmc", 80},
                                      {"Rome", 80},
                                      {"eswi", 82},
                                      {"Norrkoping", 82},
                                      {"lfpw", 84},
                                      {"efkl", 86},
                                      {"Helsinki", 86},
                                      {"Belgrade", 87},
                                      {"enmi", 88},
                                      {"Oslo", 88},
                                      {"Prague", 89},
                                      {"Episkopi", 90},
                                      {"Ankara", 91},
                                      {"Frankfurt", 92},
                                      {"London", 93},
                                      {"WAFC", 93},
                                      {"ekmi", 94},
                                      {"Copenhagen", 94},
                                      {"Rota", 95},
                                      {"Athens", 96},
                                      {"ESA", 97},
                                      {"ecmf", 98},
                                      {"DeBilt", 99},
                                      {"Hong-Kong", 110},
                                      {"wiix", 195},
                                      {"Frascati", 210},
                                      {"Lannion", 211},
                                      {"Lisboa", 212},
                                      {"Reykjavik", 213},
                                      {"lemm", 214},
                                      {"lssw", 215},
                                      {"Zurich", 215},
                                      {"Bratislava", 217},
                                      {"habp", 218},
                                      {"Budapest", 218},
                                      {"Ljubljana", 219},
                                      {"Warsaw", 220},
                                      {"Zagreb", 221},
                                      {"Albania", 222},
                                      {"Armenia", 223},
                                      {"lowm", 224},
                                      {"Austria", 224},
                                      {"ebum", 227},
                                      {"Belgium", 227},
                                      {"Bosnia and Herzegovina", 228},
                                      {"Bulgaria", 229},
                                      {"Cyprus", 230},
                                      {"Estonia", 231},
                                      {"Georgia", 232},
                                      {"eidb", 233},
                                      {"Dublin", 233},
                                      {"Israel", 234},
                                      {"ingv", 235},
                                      {"crfc", 239},
                                      {"Malta", 240},
                                      {"Monaco", 241},
                                      {"Romania", 242},
                                      {"vuwien", 244},
                                      {"knmi", 245},
                                      {"ifmk", 246},
                                      {"Kiel  ", 246},
                                      {"hadc", 247},
                                      {"Hadley", 247},
                                      {"cosmo", 250},
                                      {"MetCoOp", 251},
                                      {"mpim", 252},
                                      {"eums", 254},
                                      {"consensus", 255},
                                      {"Angola", 256},
                                      {"Benin", 257},
                                      {"Botswana", 258},
                                      {"Burkina Faso", 259},
                                      {"Burundi", 260},
                                      {"Cameroon", 261},
                                      {"Cabo Verde", 262},
                                      {"Central African Republic", 263},
                                      {"Chad", 264},
                                      {"Comoros", 265},
                                      {"Congo", 266},
                                      {"Djibouti", 267},
                                      {"Eritrea", 268},
                                      {"Ethiopia", 269},
                                      {"Gabon", 270},
                                      {"Gambia", 271},
                                      {"Ghana", 272},
                                      {"Guinea", 273},
                                      {"Guinea-Bissau", 274},
                                      {"Lesotho", 275},
                                      {"Liberia", 276},
                                      {"Malawi", 277},
                                      {"Mali", 278},
                                      {"Mauritania", 279},
                                      {"Namibia", 280},
                                      {"Nigeria", 281},
                                      {"Rwanda", 282},
                                      {"Sao Tome and Principe", 283},
                                      {"Sierra Leone", 284},
                                      {"Somalia", 285},
                                      {"Sudan", 286},
                                      {"Swaziland", 287},
                                      {"Togo", 288},
                                      {"Zambia", 289},
                                      {"Missing", 65535}};

// ----------------------------------------------------------------------

NFmiLevel parse_level(const std::string &theLevelInfoStr)
{
  NFmiLevel usedLevel(
      static_cast<FmiLevelType>(0),
      0);  // jos level type on 0, se tarkoittaa puuttuvaa ja level info otetaan datasta
  try
  {
    std::vector<float> levelValues =
        NFmiStringTools::Split<std::vector<float> >(theLevelInfoStr, ",");
    if (levelValues.size() != 2)
      throw std::runtime_error(
          "GetWantedLevel: level-info contain two values separated with comma: type,value (e.g. "
          "105,2).");
    usedLevel =
        NFmiLevel(static_cast<FmiLevelType>(static_cast<int>(levelValues[0])), levelValues[1]);
  }
  catch (std::exception &e)
  {
    std::string errStr("Error: Level info option -l was illegal:\n");
    errStr += e.what();
    throw std::runtime_error(errStr);
  }
  return usedLevel;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char *argv[])
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;

  std::string params;
  std::string level;
#ifdef UNIX
  std::string config = "/usr/share/smartmet/formats/grib.conf";
#else
  std::string config = "";
#endif

  std::string msg1 = "configuration file with conversion information (default='" + config + "')";

#ifdef UNIX
  struct winsize wsz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz);
  const int desc_width = (wsz.ws_col < 80 ? 80 : wsz.ws_col);
#else
  const int desc_width = 100;
#endif

  po::options_description desc("Allowed options", desc_width);
  // clang-format off
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "dump,D", po::bool_switch(&options.dump), "dump GRIB contents using grib_api dumper")(
      "infile,i", po::value(&options.infile), "input querydata")(
      "outfile,o", po::value(&options.outfile), "output grib file")(
      "grib1,1", po::bool_switch(&options.grib1), "output GRIB1")(
      "grib2,2", po::bool_switch(&options.grib2), "output GRIB2 (the default)")(
      "packing,p", po::value(&options.packing), "packing method (grid_simple, grid_ieee, grid_second_order, grid_jpeg etc)")(
      "centre,C", po::value(&options.centre), "originating centre (default = none)")(
      "subcentre,S", po::value(&options.subcentre), "subcentre (default = 0)")(
      "list-centres,L", po::bool_switch(&options.list_centres), "list known centres")(
      "delete,d",
      po::bool_switch(&options.crop),
      "ignore parameters which are not listed in the config")(
      "ignore-origintime,I",
      po::bool_switch(&options.ignore_origintime),
      "use first valid time instead of origin time as the forecast time")(
      "split,s", po::bool_switch(&options.split), "split individual timesteps")(
      "level,l", po::value(&level), "level to extract")(
      "config,c", po::value(&config), msg1.c_str());
  // clang-format on

  po::positional_options_description p;
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdtogrib v1.4 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: qdtogrib [options] infile outfile" << std::endl
              << std::endl
              << "Converts querydata to GRIB format." << std::endl
              << std::endl
              << desc << std::endl;
    return false;
  }

  if (options.list_centres)
  {
    std::cout << "Known centres:\n";
    for (const auto &name_value : centres)
      std::cout << name_value.second << "\t= " << name_value.first << "\n";
    return false;
  }

  if (opt.count("infile") == 0) throw std::runtime_error("Expecting input file as parameter 1");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input file '" + options.infile + "' does not exist");

  // Handle the level argument

  if (!level.empty()) options.level = parse_level(level);

  // Validate GRIB selection

  if (options.grib1 && options.grib2) throw std::runtime_error("Must select either GRIB1 or GRIB2");

  // Make default selection if none was chosen

  if (!options.grib1) options.grib2 = true;

  // Read the configuration file

  if (!config.empty()) options.ptable = ReadGribConf(config);

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the parameter should be ignored
 */
// ----------------------------------------------------------------------

bool ignore_param(long id)
{
  for (size_t i = 0; i < options.ptable.size(); ++i)
  {
    // Note: conversion in reverse direction!
    if (id == options.ptable[i].itsWantedParam.GetIdent()) return false;
  }
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Fix longitudes to 0...360 as required by GRIB2
 */
// ----------------------------------------------------------------------

double fix_longitude(double lon)
{
  if (options.grib1) return lon;
  if (lon < 0) return 360 + lon;
  return lon;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find smallest timestep in the data (or return 0 if there is no step)
 */
// ----------------------------------------------------------------------

long get_smallest_timestep(NFmiFastQueryInfo &theInfo)
{
  boost::optional<NFmiMetTime> last_time;
  long timestep = 0;

  for (theInfo.ResetTime(); theInfo.NextTime();)
  {
    if (last_time)
    {
      long diff = theInfo.ValidTime().DifferenceInMinutes(*last_time);
      if (timestep == 0 || diff < timestep) timestep = diff;
    }
    last_time = theInfo.ValidTime();
  }

  return timestep;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set latlon projection metadata
 */
// ----------------------------------------------------------------------

void set_latlon_geometry(NFmiFastQueryInfo &theInfo,
                         grib_handle *gribHandle,
                         std::vector<double> &theValueArray)
{
  gset(gribHandle, "gridType", "regular_ll");

  NFmiPoint bl(theInfo.Area()->BottomLeftLatLon());
  NFmiPoint tr(theInfo.Area()->TopRightLatLon());

  gset(gribHandle, "longitudeOfFirstGridPointInDegrees", fix_longitude(bl.X()));
  gset(gribHandle, "latitudeOfFirstGridPointInDegrees", bl.Y());

  gset(gribHandle, "longitudeOfLastGridPointInDegrees", fix_longitude(tr.X()));
  gset(gribHandle, "latitudeOfLastGridPointInDegrees", tr.Y());

  long nx = theInfo.Grid()->XNumber();
  long ny = theInfo.Grid()->YNumber();

  gset(gribHandle, "Ni", nx);
  gset(gribHandle, "Nj", ny);

  double gridCellHeightInDegrees = (tr.Y() - bl.Y()) / (ny - 1);
  double gridCellWidthInDegrees = (tr.X() - bl.X()) / (nx - 1);

  gset(gribHandle, "iDirectionIncrementInDegrees", gridCellWidthInDegrees);
  gset(gribHandle, "jDirectionIncrementInDegrees", gridCellHeightInDegrees);

  gset(gribHandle, "jScansPositively", 1);
  gset(gribHandle, "iScansNegatively", 0);

  // DUMP(gribHandle, "geography");

  theValueArray.resize(nx * ny);  // tehd‰‰ datan siirto taulusta oikean kokoinen
}

// ----------------------------------------------------------------------
/*!
 * \brief Set rotated latlon projection metadata
 *
 * This is untested since qgis cannot visualize the projection
 */
// ----------------------------------------------------------------------

void set_rotated_latlon_geometry(NFmiFastQueryInfo &theInfo,
                                 grib_handle *gribHandle,
                                 std::vector<double> &theValueArray)
{
  gset(gribHandle, "gridType", "rotated_ll");

  const auto &a = *theInfo.Area();
  NFmiPoint bl(a.LatLonToWorldXY(a.BottomLeftLatLon()));
  NFmiPoint tr(a.LatLonToWorldXY(a.TopRightLatLon()));

  gset(gribHandle, "longitudeOfFirstGridPointInDegrees", fix_longitude(bl.X()));
  gset(gribHandle, "latitudeOfFirstGridPointInDegrees", bl.Y());

  gset(gribHandle, "longitudeOfLastGridPointInDegrees", fix_longitude(tr.X()));
  gset(gribHandle, "latitudeOfLastGridPointInDegrees", tr.Y());

  long nx = theInfo.Grid()->XNumber();
  long ny = theInfo.Grid()->YNumber();

  gset(gribHandle, "Ni", nx);
  gset(gribHandle, "Nj", ny);

  double gridCellHeightInDegrees = (tr.Y() - bl.Y()) / (ny - 1);
  double gridCellWidthInDegrees = (tr.X() - bl.X()) / (nx - 1);

  gset(gribHandle, "iDirectionIncrementInDegrees", gridCellWidthInDegrees);
  gset(gribHandle, "jDirectionIncrementInDegrees", gridCellHeightInDegrees);

  // Get north pole location

  auto plat = a.Proj().GetDouble("o_lat_p");
  auto plon = a.Proj().GetDouble("o_lon_p");

  if (!plat || !plon) throw std::runtime_error("Rotated latlon north pole location not set");

  // Calculate respective south pole location

  gset(gribHandle, "longitudeOfSouthernPoleInDegrees", *plon);
  gset(gribHandle, "latitudeOfSouthernPoleInDegrees", -(*plat));

  gset(gribHandle, "jScansPositively", 1);
  gset(gribHandle, "iScansNegatively", 0);

  // DUMP(gribHandle, "geography");

  theValueArray.resize(nx * ny);  // tehd‰‰ datan siirto taulusta oikean kokoinen
}

// ----------------------------------------------------------------------
/*!
 * \brief Set stereographic projection metadata
 *
 * Defaults obtained by a DUMP call once the projection is set:
 *
 * Nx = 16
 * Ny = 31
 * latitudeOfFirstGridPointInDegrees = 60
 * longitudeOfFirstGridPointInDegrees = 0
 * LaDInDegrees = 0
 * orientationOfTheGridInDegrees = 0
 * DxInMetres = 2000
 * DyInMetres = 2000
 * iScansNegatively = 0
 * jScansPositively = 0
 * jPointsAreConsecutive = 0
 * gridType = "polar_stereographic"
 * bitmapPresent = 0
 *
 * HOWEVER: GRIB1 has a fixed true latitude of 60 degrees, atleast if you
 * look at /usr/share/grib_api/definitions/grib1/grid_definition_5.def
 */
// ----------------------------------------------------------------------

void set_stereographic_geometry(NFmiFastQueryInfo &theInfo,
                                grib_handle *gribHandle,
                                std::vector<double> &theValueArray)
{
  gset(gribHandle, "gridType", "polar_stereographic");

  NFmiPoint bl(theInfo.Area()->BottomLeftLatLon());
  NFmiPoint tr(theInfo.Area()->TopRightLatLon());

  gset(gribHandle, "longitudeOfFirstGridPointInDegrees", fix_longitude(bl.X()));
  gset(gribHandle, "latitudeOfFirstGridPointInDegrees", bl.Y());

  long nx = theInfo.Grid()->XNumber();
  long ny = theInfo.Grid()->YNumber();

  gset(gribHandle, "Ni", nx);
  gset(gribHandle, "Nj", ny);

  double dx = theInfo.Area()->WorldXYWidth() / (nx - 1);
  double dy = theInfo.Area()->WorldXYHeight() / (ny - 1);

  gset(gribHandle, "DxInMetres", dx);
  gset(gribHandle, "DyInMetres", dy);

  auto *a = theInfo.Area();

  auto clon = a->Proj().GetDouble("lon_0");
  auto clat = a->Proj().GetDouble("lat_0");
  auto tlat = a->Proj().GetDouble("lat_ts");

  if (!clon) clon = 0;
  if (!clat) clat = 90;
  if (!tlat) tlat = 90;

  if (*clat == 90)
    gset(gribHandle, "projecionCenterFlag", 0);
  else if (*clat == -90)
    gset(gribHandle, "projectionCenterFlag", 1);
  else
    throw std::runtime_error("GRIB format supports only polar stereographic projections");

  gset(gribHandle, "orientationOfTheGridInDegrees", *clon);

  if (!options.grib2)
  {
  }
  else
  {
    gset(gribHandle, "LaDInDegrees", *clat);
    // "shapeOfTheEarth"
    // "scaleFactorOfRadiusOfSphericalEarth"
    // "scaledValueOfRadiusOfSphericalEarth"
    // "scaleFactorOfMajorAxisOfOblateSpheroidEarth"
    // "scaledValueOfMajorAxisOfOblateSpheroidEarth"
    // "scaleFactorOfMinorAxisOfOblateSpheroidEarth"
    // "scaledValueOfMinorAxisOfOblateSpheroidEarth"
  }

  gset(gribHandle, "jScansPositively", 1);
  gset(gribHandle, "iScansNegatively", 0);

  // DUMP(gribHandle,"geography");

  theValueArray.resize(nx * ny);  // tehd‰‰ datan siirto taulusta oikean kokoinen
}

// ----------------------------------------------------------------------
/*!
 * \brief Set mercator projection metadata
 *
 */
// ----------------------------------------------------------------------

void set_mercator_geometry(NFmiFastQueryInfo &theInfo,
                           grib_handle *gribHandle,
                           std::vector<double> &theValueArray)
{
  gset(gribHandle, "gridType", "mercator");

  NFmiPoint bl(theInfo.Area()->BottomLeftLatLon());
  NFmiPoint tr(theInfo.Area()->TopRightLatLon());

  gset(gribHandle, "longitudeOfFirstGridPointInDegrees", fix_longitude(bl.X()));
  gset(gribHandle, "latitudeOfFirstGridPointInDegrees", bl.Y());

  gset(gribHandle, "longitudeOfLastGridPointInDegrees", fix_longitude(tr.X()));
  gset(gribHandle, "latitudeOfLastGridPointInDegrees", tr.Y());

  long nx = theInfo.Grid()->XNumber();
  long ny = theInfo.Grid()->YNumber();

  gset(gribHandle, "Ni", nx);
  gset(gribHandle, "Nj", ny);

  double lon_0 = 0;
  double lat_ts = 0;

  gset(gribHandle, "orientationOfTheGridInDegrees", lon_0);
  gset(gribHandle, "LaDInDegrees", lat_ts);

  gset(gribHandle, "jScansPositively", 1);
  gset(gribHandle, "iScansNegatively", 0);

  theValueArray.resize(nx * ny);  // tehd‰‰ datan siirto taulusta oikean kokoinen
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the producer
 */
// ----------------------------------------------------------------------

static void set_producer(grib_handle *gribHandle)
{
  if (options.centre.empty()) return;

  auto it = centres.find(options.centre);
  int centre = 0;

  if (it != centres.end())
    centre = it->second;
  else
  {
    try
    {
      centre = Fmi::stoi(options.centre);
    }
    catch (...)
    {
      throw std::runtime_error("Unknown centre: '" + options.centre + "'");
    }
  }

  gset(gribHandle, "centre", centre);
  gset(gribHandle, "subCentre", options.subcentre);
}

// ----------------------------------------------------------------------
/*!
 * \brief Set packing method (grid_simple, grid_jpeg etc)
 */
// ----------------------------------------------------------------------

static void set_packing(grib_handle *gribHandle)
{
  if (options.packing.empty()) return;

  gset(gribHandle, "packingType", options.packing);
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the geometry. Called only once since in querydata all geometries are equal
 */
// ----------------------------------------------------------------------

static void set_geometry(NFmiFastQueryInfo &theInfo,
                         grib_handle *gribHandle,
                         std::vector<double> &theValueArray)
{
  auto id = theInfo.Area()->Proj().DetectClassId();

  if (id == kNFmiLatLonArea)
    set_latlon_geometry(theInfo, gribHandle, theValueArray);
  else if (id == kNFmiRotatedLatLonArea)
    set_rotated_latlon_geometry(theInfo, gribHandle, theValueArray);
  else if (id == kNFmiStereographicArea)
    set_stereographic_geometry(theInfo, gribHandle, theValueArray);
  else if (id == kNFmiMercatorArea)
    set_mercator_geometry(theInfo, gribHandle, theValueArray);
  else
    throw std::runtime_error("Projection '" + theInfo.Area()->ProjStr() + "' is not supported");
}

// ----------------------------------------------------------------------

static NFmiMetTime get_origintime(NFmiFastQueryInfo &theInfo)
{
  if (!options.ignore_origintime) return theInfo.OriginTime();

  // Get first time without altering the time index
  auto idx = theInfo.TimeIndex();
  theInfo.FirstTime();
  auto t = theInfo.ValidTime();
  theInfo.TimeIndex(idx);
  return t;
}

// ----------------------------------------------------------------------

static void set_times(NFmiFastQueryInfo &theInfo, grib_handle *gribHandle, bool use_minutes)
{
  NFmiMetTime orig_time = get_origintime(theInfo);

  long dateLong = orig_time.GetYear() * 10000 + orig_time.GetMonth() * 100 + orig_time.GetDay();

  long timeLong = orig_time.GetHour() * 100;
  if (use_minutes) timeLong += orig_time.GetMin();

  gset(gribHandle, "dataDate", dateLong);
  gset(gribHandle, "dataTime", timeLong);

  // P1 max 255 minutes is not enough, we need to enable P2
  if (use_minutes && options.grib1) gset(gribHandle, "timeRangeIndicator", 10);

  // step units in stepUnits.table: m h D M Y 10Y 30Y C 3h 6h 12h s 15m 30m

  if (use_minutes)
    gset(gribHandle, "indicatorOfUnitOfTimeRange", 0);
  else
    gset(gribHandle, "indicatorOfUnitOfTimeRange", 1);
}

// ----------------------------------------------------------------------

void set_parameters(grib_handle *gribHandle, const NFmiParam &theParam)
{
  // Parametrin nime‰ on kai turha asetella, ei mene mihink‰‰n oikeasti?!?!?
  //	size_t paramNameLen = param->GetName().GetLen();
  //	grib_set_string(gribHandle, "param" , param->GetName(), &paramNameLen);

  long usedParId = theParam.GetIdent();

  for (size_t i = 0; i < options.ptable.size(); ++i)
  {
    // Note: conversion in reverse direction!
    if (usedParId == options.ptable[i].itsWantedParam.GetIdent())
    {
      usedParId = options.ptable[i].itsOriginalParamId;
      break;
    }
  }

  gset(gribHandle, "paramId", usedParId);
}

// ----------------------------------------------------------------------

void get_conversion(long id, float *scale, float *offset)
{
  for (size_t i = 0; i < options.ptable.size(); ++i)
  {
    // Note: conversion in reverse direction!
    if (id == options.ptable[i].itsWantedParam.GetIdent())
    {
      *scale = options.ptable[i].itsConversionScale;
      *offset = options.ptable[i].itsConversionBase;
      break;
    }
  }
}

const char *level_name(FmiLevelType theType)
{
  switch (theType)
  {
    case kFmiSoundingLevel:
      throw std::runtime_error("Sounding levels not supported by GRIB2");
    case kFmiAmdarLevel:
      throw std::runtime_error("AMDAR levels not supported by GRIB2");
    case kFmiPressureLevel:
      return "isobaricinhPa";
    case kFmiMeanSeaLevel:
      return "meanSea";
    case kFmiAltitude:
      return "heightAboveGround";
    case kFmiHeight:
      return "heightAboveSea";
    case kFmiHybridLevel:
      return "hybridLayer";
    case kFmiDepth:
      return "depthBelowSea";
    case kFmiFlightLevel:
      throw std::runtime_error("Flight levels not supported by GRIB2");
    case kFmiRoadClass1:
    case kFmiRoadClass2:
    case kFmiRoadClass3:
      throw std::runtime_error("Road maintenance classes not supported by GRIB2");
    case kFmi:
    case kFmiAnyLevelType:
    case kFmiNoLevelType:
    case kFmiGroundSurface:
    default:
      return "surface";
  }
}

// ----------------------------------------------------------------------

// asettaa gribiin level-tiedot. Jos on annettu komento rivilt‰ over-ride level tieto, otetaan arvot
// theWantedLevel-oliosta, muuten dataan liittyv‰st‰ level-oliosta theLevelFromData.
// Jos theWantedLevel-olion levelType on 0 (=missing arvo), silloin k‰ytet‰‰n datasta saatua
// leveli‰.

void set_level(grib_handle *gribHandle, const NFmiLevel &theLevelFromData)
{
  const NFmiLevel &usedLevel = options.level.LevelType() == 0 ? theLevelFromData : options.level;

  gset(gribHandle, "typeOfLevel", level_name(usedLevel.LevelType()));
  gset(gribHandle, "level", static_cast<long>(usedLevel.LevelValue()));
}

// ----------------------------------------------------------------------

// kopioidaan kurrentti aika/param/level hila annettuun grib-handeliin.
bool copy_values(NFmiFastQueryInfo &theInfo,
                 grib_handle *gribHandle,
                 std::vector<double> &theValueArray,
                 bool use_minutes)
{
  // NOTE: This froecastTime part is not edition independent
  const NFmiMetTime oTime = get_origintime(theInfo);
  const NFmiMetTime &vTime = theInfo.ValidTime();

  long mdiff = vTime.DifferenceInMinutes(oTime);

  // Note that we round up and origin time is rounded down in set_times
  long diff = (use_minutes ? mdiff : std::ceil(mdiff / 60.0));

  // Forecast time cannot be negative. This may happen for example
  // when using the SmartMet Editor. We simply ignore such lines.

  if (diff < 0)
  {
    if (options.verbose)
      std::cout << "Ignoring timestep " << vTime << " for having a negative lead time" << std::endl;
    return false;
  }

  if (options.grib1)
  {
    if (!use_minutes)
      gset(gribHandle, "P1", diff);
    else
    {
      // timeRangeIndicator=10 was set by set_times
      gset(gribHandle, "P1", diff >> 8);
      gset(gribHandle, "P2", diff % 256);
    }
  }
  else
    gset(gribHandle, "forecastTime", diff);

  NFmiParam param(*(theInfo.Param().GetParam()));
  set_parameters(gribHandle, param);

  set_level(gribHandle, (*theInfo.Level()));

  float scale = 1.0;
  float offset = 0.0;
  get_conversion(param.GetIdent(), &scale, &offset);

  int i = 0;
  bool missingValuesExist = false;

  for (theInfo.ResetLocation(); theInfo.NextLocation();)
  {
    float value = theInfo.FloatValue();
    if (value != kFloatMissing)
    {
      theValueArray[i] = (value - offset) / scale;
    }
    else
    {
      theValueArray[i] = 9999;  // GRIB1 missing value by default
      missingValuesExist = true;
    }
    i++;
  }

  if (missingValuesExist)
  {
    grib_set_long(gribHandle, "bitmapPresent", 1);
  }
  grib_set_double_array(gribHandle, "values", &theValueArray[0], theValueArray.size());

  return true;
}

// ----------------------------------------------------------------------

void write_grib(FILE *out, grib_handle *gribHandle)
{
  const void *mesg;
  size_t mesg_len;
  grib_get_message(gribHandle, &mesg, &mesg_len);
  fwrite(mesg, 1, mesg_len, out);
}

// ----------------------------------------------------------------------

std::string make_file_suffix(NFmiFastQueryInfo &theInfo)
{
  std::string str;
  str += "_";
  str += NFmiStringTools::Convert<unsigned long>(theInfo.Param().GetParamIdent());
  str += "_";
  str += theInfo.Time().ToStr("YYYYMMDDHHmm");
  return str;
}

// ----------------------------------------------------------------------

void write_grib(NFmiFastQueryInfo &theInfo, grib_handle *gribHandle, const std::string &theFileName)
{
  std::string fullSplitFileName(theFileName);
  fullSplitFileName += ::make_file_suffix(theInfo);
  FILE *out = fopen(fullSplitFileName.c_str(), "wb");
  if (!out) throw std::runtime_error("ERROR: cannot open file for writing: " + fullSplitFileName);

  const void *mesg;
  size_t mesg_len;
  grib_get_message(gribHandle, &mesg, &mesg_len);
  fwrite(mesg, 1, mesg_len, out);
  fclose(out);
}

// ----------------------------------------------------------------------

int run(const int argc, char *argv[])
{
  if (!parse_options(argc, argv)) return 0;

  if (options.verbose) std::cout << "Opening file '" << options.infile << "'" << std::endl;

  NFmiQueryData qd(options.infile);
  NFmiFastQueryInfo qi(&qd);

  // TODO: This function is deprecated, use grib_handle_new_from_samples instead

  grib_context *context = grib_context_get_default();

  grib_handle *gribHandle;
  if (options.grib1)
    gribHandle = grib_handle_new_from_samples(context, "GRIB1");
  else if (options.grib2)
    gribHandle = grib_handle_new_from_samples(context, "GRIB2");
  else
    throw std::runtime_error("Invalid GRIB format selected");  // never happens

#ifdef SOME_OTHER_VERSION
  int option_flags = GRIB_DUMP_FLAG_VALUES | GRIB_DUMP_FLAG_OPTIONAL | GRIB_DUMP_FLAG_READ_ONLY;
#else
  // xodin grib_api does not know GRIB_DUMP_FLAG_OPTIONAL
  int option_flags = GRIB_DUMP_FLAG_VALUES | GRIB_DUMP_FLAG_READ_ONLY;
#endif

  if (gribHandle == 0) throw std::runtime_error("ERROR: Unable to create grib handle\n");

  if (qi.IsGrid() == false)
    throw std::runtime_error("ERROR: data wasn't grid data, but station data.\n");

  FILE *out = 0;
  if (!options.split)
  {
    out = fopen(options.outfile.c_str(), "wb");
    if (!out) throw std::runtime_error("ERROR: cannot open file for writing: " + options.outfile);
  }

  try
  {
    qi.First();
    std::vector<double> valueArray;  // t‰t‰ vektoria k‰ytet‰‰n siirt‰m‰‰n dataa querydatasta
                                     // gribiin (aina saman kokoinen)
    set_producer(gribHandle);
    set_packing(gribHandle);
    set_geometry(qi, gribHandle, valueArray);
    const long timestep = get_smallest_timestep(qi);
    const bool use_minutes = (timestep < 60);
    set_times(qi, gribHandle, use_minutes);

    if (options.verbose) std::cout << "Smallest timestep = " << timestep << std::endl;

    for (qi.ResetLevel(); qi.NextLevel();)
    {
      for (qi.ResetParam(); qi.NextParam(false);)
      {
        if (ignore_param(qi.Param().GetParamIdent()))
        {
          // if(options.verbose)
          std::cout << "Ignoring parameter " << qi.Param().GetParamName().CharPtr() << " ("
                    << qi.Param().GetParamIdent() << ")" << std::endl;
        }
        else
        {
          for (qi.ResetTime(); qi.NextTime();)
          {
            if (copy_values(qi, gribHandle, valueArray, use_minutes))
            {
              if (options.dump)
                grib_dump_content(gribHandle, stdout, "serialize", option_flags, nullptr);
              if (!options.split)
                write_grib(out, gribHandle);
              else
                write_grib(qi, gribHandle, options.outfile);
            }
          }
        }
      }
    }
  }
  catch (...)
  {
    if (out) fclose(out);  // suljetaan outputfile myˆs virhe tilanteessa
    throw;                 // paiskataan kiinniotettu poikkeus edelleen matkaan
  }
  if (out) fclose(out);  // lopuksi suljetaan outputfile

  return 0;
}

// ----------------------------------------------------------------------

int main(const int argc, char *argv[]) try
{
  return run(argc, argv);
}
catch (std::exception &e)
{
  std::cerr << e.what() << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Caught an unknown exception" << std::endl;
  return 1;
}
