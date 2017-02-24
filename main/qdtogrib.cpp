// Example encoding program.

#include "GribTools.h"

#include "NFmiArea.h"
#include "NFmiGrid.h"
#include "NFmiCmdLine.h"
#include "NFmiFastQueryInfo.h"
#include "NFmiFileString.h"
#include "NFmiRotatedLatLonArea.h"
#include "NFmiStereographicArea.h"
#include "NFmiQueryData.h"

#include "grib_api.h"

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <string>

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
  Options();

  std::string infile;       // -i --infile
  std::string outfile;      // -o --outfile
  bool grib1;               // -1, --grib1
  bool grib2;               // -2, --grib2
  bool split;               // -s --split
  bool crop;                // -d --delete
  bool verbose;             // -v --verbose
  NFmiLevel level;          // -l --level
  ParamChangeTable ptable;  // -c --config
};

Options options;

Options::Options()
    : infile("-"),
      outfile("-"),
      grib1(false),
      grib2(false),
      split(false),
      crop(false),
      verbose(false),
      level(),
      ptable()
{
}

// ----------------------------------------------------------------------

NFmiLevel parse_level(const std::string &theLevelInfoStr)
{
  NFmiLevel usedLevel(
      static_cast<FmiLevelType>(0),
      0);  // jos level type on 0, se tarkoittaa puuttuvaa ja level info otetaan datasta
  try
  {
    checkedVector<float> levelValues =
        NFmiStringTools::Split<checkedVector<float> >(theLevelInfoStr, ",");
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

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "infile,i", po::value(&options.infile), "input querydata")(
      "outfile,o", po::value(&options.outfile), "output grib file")(
      "grib1,1", po::bool_switch(&options.grib1), "output GRIB1")(
      "grib2,2", po::bool_switch(&options.grib2), "output GRIB2 (the default)")(
      "delete,d",
      po::bool_switch(&options.crop),
      "ignore parameters which are not listed in the config")(
      "split,s", po::bool_switch(&options.split), "split individual timesteps")(
      "level,l", po::value(&level), "level to extract")(
      "config,c", po::value(&config), msg1.c_str());

  po::positional_options_description p;
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdtogrib v1.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
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
 * \brief Set latlon projection metadata
 */
// ----------------------------------------------------------------------

void set_latlon_geometry(NFmiFastQueryInfo &theInfo,
                         grib_handle *gribHandle,
                         std::vector<double> &theValueArray)
{
  gset(gribHandle, "typeOfGrid", "regular_ll");

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
  gset(gribHandle, "typeOfGrid", "rotated_ll");

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

  const NFmiRotatedLatLonArea *a = dynamic_cast<const NFmiRotatedLatLonArea *>(theInfo.Area());

  if (a->SouthernPole().X() != 0)
    throw std::runtime_error(
        "GRIB does not support rotated latlon areas where longitude is also rotated");

  gset(gribHandle, "angleOfRotationInDegrees", a->SouthernPole().Y());

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
  gset(gribHandle, "typeOfGrid", "polar_stereographic");

  NFmiPoint bl(theInfo.Area()->BottomLeftLatLon());
  NFmiPoint tr(theInfo.Area()->TopRightLatLon());

  gset(gribHandle, "longitudeOfFirstGridPointInDegrees", fix_longitude(bl.X()));
  gset(gribHandle, "latitudeOfFirstGridPointInDegrees", bl.Y());

  long nx = theInfo.Grid()->XNumber();
  long ny = theInfo.Grid()->YNumber();

  gset(gribHandle, "Ni", nx);
  gset(gribHandle, "Nj", ny);

  double dx = theInfo.Area()->WorldXYWidth() / nx;
  double dy = theInfo.Area()->WorldXYHeight() / ny;

  gset(gribHandle, "DxInMetres", dx);
  gset(gribHandle, "DyInMetres", dy);

  const NFmiStereographicArea *a = dynamic_cast<const NFmiStereographicArea *>(theInfo.Area());

  double lon_0 = a->CentralLongitude();
  double lat_0 = a->CentralLatitude();
  double lat_ts = a->TrueLatitude();

  gset(gribHandle, "orientationOfTheGridInDegrees", lon_0);

  if (options.grib2)
    gset(gribHandle, "LaDInDegrees", lat_ts);
  else if (lat_ts != 60)
    throw std::runtime_error(
        "GRIB1 true latitude can only be 60 for polar stereographic projections with grib_api "
        "library");

  if (lat_0 != 90 && lat_0 != -90)
    throw std::runtime_error("GRIB format supports only polar stereographic projections");

  if (lat_0 != 90)
    throw std::runtime_error("Only N-pole polar stereographic projections are supported");

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
  gset(gribHandle, "typeOfGrid", "mercator");

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

// T‰t‰ kutsutaan vain kerran, koska querydatassa kaikki hilat ja areat ovat samoja
static void set_geometry(NFmiFastQueryInfo &theInfo,
                         grib_handle *gribHandle,
                         std::vector<double> &theValueArray)
{
  switch (theInfo.Area()->ClassId())
  {
    case kNFmiLatLonArea:
      set_latlon_geometry(theInfo, gribHandle, theValueArray);
      break;
    case kNFmiRotatedLatLonArea:
      set_rotated_latlon_geometry(theInfo, gribHandle, theValueArray);
      break;
    case kNFmiStereographicArea:
      set_stereographic_geometry(theInfo, gribHandle, theValueArray);
      break;
    case kNFmiMercatorArea:
      set_mercator_geometry(theInfo, gribHandle, theValueArray);
      break;
    case kNFmiEquiDistArea:
      throw std::runtime_error("Equidistant projection is not supported by GRIB");
    case kNFmiGnomonicArea:
      throw std::runtime_error("Gnomonic projection is not supported by GRIB");
    case kNFmiPKJArea:
      throw std::runtime_error("PKJ projection is not supported by GRIB");
    case kNFmiYKJArea:
      throw std::runtime_error("YKJ projection is not supported by GRIB");
    case kNFmiKKJArea:
      throw std::runtime_error("KKJ projection is not supported by GRIB");
    default:
      throw std::runtime_error("Unsupported projection in input data");
  }
}

// ----------------------------------------------------------------------

static void set_times(NFmiFastQueryInfo &theInfo, grib_handle *gribHandle)
{
  const NFmiMetTime &vTime = theInfo.OriginTime();
  long dateLong = vTime.GetYear() * 10000 + vTime.GetMonth() * 100 + vTime.GetDay();
  long timeLong = vTime.GetHour() * 100 + vTime.GetMin();
  gset(gribHandle, "dataDate", dateLong);
  gset(gribHandle, "dataTime", timeLong);
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
void copy_values(NFmiFastQueryInfo &theInfo,
                 grib_handle *gribHandle,
                 std::vector<double> &theValueArray)
{
  // NOTE: This froecastTime part is not edition independent
  const NFmiMetTime &oTime = theInfo.OriginTime();
  const NFmiMetTime &vTime = theInfo.ValidTime();
  long diff = vTime.DifferenceInHours(oTime);

  if (options.grib1)
    gset(gribHandle, "P1", diff);
  else
    gset(gribHandle, "forecastTime", diff);

  NFmiParam param(*(theInfo.Param().GetParam()));
  set_parameters(gribHandle, param);

  set_level(gribHandle, (*theInfo.Level()));

  float scale = 1.0;
  float offset = 0.0;
  get_conversion(param.GetIdent(), &scale, &offset);

  int i = 0;
  for (theInfo.ResetLocation(); theInfo.NextLocation();)
  {
    float value = theInfo.FloatValue();
    if (value != kFloatMissing)
      theValueArray[i] = (value - offset) / scale;
    else
      theValueArray[i] = 999;  // GRIB1 missing value by default
    i++;
  }

  grib_set_double_array(gribHandle, "values", &theValueArray[0], theValueArray.size());
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
  fclose(out);  // suljetaan outputfile
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
    set_geometry(qi, gribHandle, valueArray);
    set_times(qi, gribHandle);

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
            copy_values(qi, gribHandle, valueArray);
            if (options.verbose)
#if (GRIB_API_MAJOR_VERSION < 1)  // jos versio esim. 0.8.2, grib_dump_content-rajapinta erilainen
                                  // kuin uusilla grib_api versioilla
              // grib_dump_content(gribHandle, stdout, "serialize", option_flags);
              grib_dump_content(gribHandle, stdout, "serialize", option_flags, NULL);
#else
              grib_dump_content(gribHandle, stdout, "serialize", option_flags, NULL);
#endif
            if (!options.split)
              write_grib(out, gribHandle);
            else
              write_grib(qi, gribHandle, options.outfile);
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
