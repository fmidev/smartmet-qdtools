// ======================================================================
/*!
 * \file qdinfo.cpp
 * \brief Implementation of the qdinfo program
 */
// ======================================================================
/*!
 * \page qdinfo
 *
 * The qdinfo program prints information relating to newbase, or
 * the given querydata.
 *
 * The known options are
 *
 * <dl>
 * <dt>-q [queryfile]</dt>
 * <dd>
 * Specifies the name of the queryfile.
 * </dd>
 * <dt>-A</dt>
 * <dd>
 * All the options below combined.
 * </dd>
 * <dt>-l</dt>
 * <dd>
 * Produces a listing of recognized parameternames. This option
 * does not require a queryfile to be specified.
 * <dt>-p</dt>
 * <dd>
 * Produces a listing of parameters in the given queryfile.
 * </dd>
 * <dt>-T</dt>
 * <dd>
 * Shows querydata origin time and data times in UTC.
 * </dd>
 * <dt>-a</dt>
 * <dd>
 * All the options below combined.
 * </dd>
 * <dt>-v</dt>
 * <dd>
 * Display the queryinfo version number.
 * </dd>
 * <dt>-P</dt>
 * <dd>
 * Produces a listing of parameters in the given queryfile.
 * The difference to -p is that subparameters are also listed.
 * </dd>
 * <dt>-t</dt>
 * <dd>
 * Shows querydata origin time and data times in local time.
 * </dd>
 * <dt>-x</dt>
 * <dd>
 * Shows information of querydata projection.
 * <dt>-X</dt>
 * <dd>
 * Shows information of querydata locations.
 * </dd>
 * <dt>-z</dt>
 * <dd>
 * Shows querydata level information.
 * </dd>
 * <dt>-r</dt>
 * <dd>
 * Shows querydata producer information.
 * </dd>
 * <dt>-m key</dt>
 * <dd>
 * Shows the metadata value for the given key
 * </dd>
 * <dt>-M</dt>
 * <dd>
 * Shows the metadata values for all keys in the data.
 * </dd>
 * </dl>
 */
// ======================================================================

#include <macgyver/StringConversion.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiEquidistArea.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGnomonicArea.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiKKJArea.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiMercatorArea.h>
#include <newbase/NFmiPKJArea.h>
#include <newbase/NFmiRotatedLatLonArea.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiStereographicArea.h>
#include <newbase/NFmiStringList.h>
#include <newbase/NFmiYKJArea.h>

#ifdef UNIX
#include <ogr_spatialref.h>
#endif

#include <algorithm>
#include <ctime>
#include <list>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * Convert number to printable form handling missing values
 */
// ----------------------------------------------------------------------

std::string ToString(float theValue)
{
  if (theValue == kFloatMissing) return "-";
  return Fmi::to_string(theValue);
}

// ----------------------------------------------------------------------
/*!
 * Report version information
 *
 * \param q The queryinfo to report on
 */
// ----------------------------------------------------------------------

void ReportVersion(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  double version = q->InfoVersion();

  cout << endl << "Info version = " << '\t' << version << endl << endl;
}

// ----------------------------------------------------------------------
/*!
 * Report known parameternames by printing all enums NFmiEnumConverter
 * knows.
 */
// ----------------------------------------------------------------------

void ReportParameterNames(void)
{
  NFmiEnumConverter converter;
  const list<string> names = converter.Names();

  cout << "The list of parameters known by newbase is:" << endl << endl;

  for (list<string>::const_iterator it = names.begin(); it != names.end(); ++it)
  {
    cout << converter.ToEnum(*it) << '\t' << *it << endl;
  }
  cout << endl << "There are " << names.size() << " known parameters in total" << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return name of interpolation method
 */
// ----------------------------------------------------------------------

std::string interpolation_name(FmiInterpolationMethod method)
{
  switch (method)
  {
    case kNoneInterpolation:
      return "None";
    case kLinearly:
      return "Linear";
    case kNearestPoint:
      return "NearestPoint";
    case kByCombinedParam:
      return "ByCombinedParam";
    case kLinearlyFast:
      return "LinearlyFast";
    case kLagrange:
      return "Lagrange";
    default:
      return "Unknown";
  }
}

// ----------------------------------------------------------------------
/*!
 * Report parameters in the given queryfile.
 *
 * \param q The queryinfo to report on
 * \param ignoresubs Whether to ignore subparameters or not
 */
// ----------------------------------------------------------------------

void ReportParameters(NFmiFastQueryInfo *q, bool ignoresubs)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  NFmiEnumConverter converter;

  unsigned int count = 0;

  cout << "\n"
       << "The parameters stored in the querydata are:\n\n"
       << "Number  Name                                    Description                             "
          "Interpolation      Precision  Lolimit  Hilimit\n"
       << "======  ====                                    ===========                             "
          "=============      =========  =======  =======\n";

  q->ResetParam();
  while (q->NextParam(ignoresubs))
  {
    ++count;

    const NFmiString description = q->Param().GetParamName();
    const int id = q->Param().GetParamIdent();
    const string name = converter.ToString(id);

    string paramtype;

    if (q->Param().HasDataParams())
      paramtype = "+";
    else if (q->IsSubParamUsed())
      paramtype = "-";
    else
      paramtype = "";

    cout << setw(8) << left << id << setw(40) << paramtype + name << setw(40)
         << description.CharPtr() << setw(16)
         << interpolation_name(q->Param().GetParam()->InterpolationMethod()) << setw(12) << right
         << q->Param().GetParam()->Precision().CharPtr() << setw(9)
         << ToString(q->Param().GetParam()->MinValue()) << setw(9)
         << ToString(q->Param().GetParam()->MaxValue()) << endl;
  }
  cout << endl << "There are " << count << " stored parameters in total" << endl;
  return;
}

// ----------------------------------------------------------------------
/*!
 * \brief User defined output format for date string
 *
 */
// ----------------------------------------------------------------------

const string format_time(const NFmiMetTime &mtime, const string &dateFormat, bool theUtcFlag)
{
  // The UTC time
  struct ::tm utc;
  utc.tm_year = mtime.GetYear() - 1900;
  utc.tm_mon = mtime.GetMonth() - 1;
  utc.tm_mday = mtime.GetDay();
  utc.tm_hour = mtime.GetHour();
  utc.tm_min = mtime.GetMin();
  utc.tm_sec = mtime.GetSec();

  ::time_t epochtime = NFmiStaticTime::my_timegm(&utc);  // timegm is a GNU extension

  // As local time. Note that localtime owns the struct
  // that it will create statically

  struct ::tm *local = ::localtime(&epochtime);

  const ::size_t MAXLEN = 100;
  char buffer[MAXLEN];
  ::size_t n = strftime(buffer, MAXLEN, dateFormat.c_str(), local);
  string ret(buffer, 0, n);
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the given timezone
 *
 * \param theZone The time zone descriptor
 */
// ----------------------------------------------------------------------

void set_timezone(const string &theZone)
{
  // must be static, see putenv specs!
  static string tzvalue = "TZ=" + theZone;
  putenv(const_cast<char *>(tzvalue.c_str()));
  tzset();
}

// ----------------------------------------------------------------------
/*!
 * Return the given time as a string
 *
 * \param theTime The time to be converted into a string
 * \param theUtcFlag True, if the time is to be given in UTC
 * \return The time as a string
 */
// ----------------------------------------------------------------------

const string TimeString(const NFmiMetTime &theTime, bool theUtcFlag, string dateFormat)
{
  if (theUtcFlag) set_timezone("UTC");

  string out = format_time(theTime, dateFormat, theUtcFlag);

  if (theUtcFlag)
    out += " UTC";
  else
    out += " local time";
  return out;
}

// ----------------------------------------------------------------------
/*!
 * Report on the origin time and data times in the query data
 *
 * \param q The queryinfo to report on
 * \param theUtcFlag True if times are to be printed in UTC
 */
// ----------------------------------------------------------------------

void ReportTimes(NFmiFastQueryInfo *q, bool theUtcFlag, string dateFormat)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  cout << endl << "Time information on the querydata:" << endl << endl;

  cout << "Origin time\t= " << TimeString(q->OriginTime(), theUtcFlag, dateFormat) << endl;

  cout << "First time\t= ";
  if (q->FirstTime())
    cout << TimeString(q->ValidTime(), theUtcFlag, dateFormat);
  else
    cout << "?";
  cout << endl;

  cout << "Last time\t= ";
  if (q->LastTime())
    cout << TimeString(q->ValidTime(), theUtcFlag, dateFormat);
  else
    cout << "?";
  cout << endl;

  cout << "Time step\t= ";
  if (q->FirstTime() && q->NextTime())
  {
    q->FirstTime();
    NFmiTime t1 = q->ValidTime();
    q->NextTime();
    NFmiTime t2 = q->ValidTime();
    cout << t2.DifferenceInMinutes(t1) << " minutes";
  }
  else
    cout << "?";
  cout << endl;

  cout << "Timesteps\t= " << q->SizeTimes() << endl;

  return;
}

// ----------------------------------------------------------------------
/*!
 * Returns the description of the level as in FmiLevelType enumeration.
 *
 * \param theLevel Enumerated level type value
 * \return The name of the level as a string.
 */
// ----------------------------------------------------------------------

string LevelName(FmiLevelType theLevel)
{
  // Note: default case is intentionally omitted so that
  // g++ will warn us if some case is missing

  string str;
  switch (theLevel)
  {
    case kFmiGroundSurface:
    {
      str = "GroundSurface";
      break;
    }
    case kFmiPressureLevel:
    {
      str = "PressureLevel";
      break;
    }
    case kFmiMeanSeaLevel:
    {
      str = "MeanSeaLevel";
      break;
    }
    case kFmiAltitude:
    {
      str = "Altitude";
      break;
    }
    case kFmiHeight:
    {
      str = "Height";
      break;
    }
    case kFmiHybridLevel:
    {
      str = "HybridLevel";
      break;
    }
    case kFmi:
    {
      str = "?";
      break;
    }
    case kFmiAnyLevelType:
    {
      str = "AnyLevelType";
      break;
    }
    case kFmiRoadClass1:
    {
      str = "RoadClass1";
      break;
    }
    case kFmiRoadClass2:
    {
      str = "RoadClass2";
      break;
    }
    case kFmiRoadClass3:
    {
      str = "RoadClass3";
      break;
    }
    case kFmiSoundingLevel:
    {
      str = "SoundingLevel";
      break;
    }
    case kFmiAmdarLevel:
    {
      str = "AmdarLevel";
      break;
    }
    case kFmiFlightLevel:
    {
      str = "FlightLevel";
      break;
    }
    case kFmiDepth:
    {
      str = "Depth";
      break;
    }
    case kFmiNoLevelType:
    {
      str = "NoLevel";
      break;
    }
  }

  return str;
}

// ----------------------------------------------------------------------
/*!
 * Report on the levels in the query data
 *
 * \param q The queryinfo to report on
 */
// ----------------------------------------------------------------------

void ReportLevels(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  cout << endl
       << "Level information on the querydata:" << endl
       << endl
       << "Value\tTypeId\tType\t\tName" << endl
       << "=====\t======\t====\t\t====" << endl
       << endl;

  q->ResetLevel();
  while (q->NextLevel())
  {
    const NFmiLevel &level = *q->Level();

    cout << level.LevelValue() << '\t' << level.LevelTypeId() << '\t'
         << LevelName(level.LevelType()) << '\t' << level.GetName().CharPtr() << endl;
  }

  cout << endl << "There are " << q->SizeLevels() << " levels in total" << endl;

  return;
}

// ----------------------------------------------------------------------
/*!
 * Report on the producer of the given query data
 *
 * \param q The queryinfo to report on
 */
// ----------------------------------------------------------------------

void ReportProducer(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  // Must select some parameter, producer is parameter dependent
  q->FirstParam();

  NFmiProducer &prod = *q->Producer();

  cout << endl << "Information on the querydata producer:" << endl << endl;

  cout << "id\t= " << prod.GetIdent() << endl;
  cout << "name\t= " << prod.GetName().CharPtr() << endl;

  cout << endl;

  return;
}

// ----------------------------------------------------------------------
/*!
 * Report on the projection of the query data
 *
 * \param q The queryinfo to report on
 */
// ----------------------------------------------------------------------

void ReportProjection(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  cout << endl << "Information on the querydata area:" << endl << endl;

  const NFmiArea *area = q->Area();
  const NFmiGrid *grid = q->Grid();

  // IsArea may return false even though Area may return one (for grids!)
  if (area == 0)
  {
    cout << "The querydata has no area" << endl;
    return;
  }

  unsigned long classid = area->ClassId();
  const auto rect = area->WorldRect();

#ifdef UNIX
  const auto wkt = area->WKT();
  OGRSpatialReference crs;
  if (crs.SetFromUserInput(wkt.c_str()) != OGRERR_NONE)
    throw std::runtime_error("GDAL does not understand the WKT in the data");
#endif

  cout << "projection\t\t= " << area->ClassName() << endl;

  cout << "top left lonlat\t\t= " << area->TopLeftLatLon().X() << ',' << area->TopLeftLatLon().Y()
       << endl;
  cout << "top right lonlat\t= " << area->TopRightLatLon().X() << ',' << area->TopRightLatLon().Y()
       << endl;
  cout << "bottom left lonlat\t= " << area->BottomLeftLatLon().X() << ','
       << area->BottomLeftLatLon().Y() << endl;
  cout << "bottom right lonlat\t= " << area->BottomRightLatLon().X() << ','
       << area->BottomRightLatLon().Y() << endl;
  cout << "center lonlat\t\t= " << area->CenterLatLon().X() << ',' << area->CenterLatLon().Y()
       << endl
       << std::setprecision(9) << "bbox\t\t\t= [" << rect.Left() << " " << rect.Right() << " "
       << std::min(rect.Bottom(), rect.Top()) << " " << std::max(rect.Bottom(), rect.Top()) << "]"
       << std::setprecision(6) << endl
       << endl;

  cout << "fmiarea\t= " << area->AreaStr() << endl;
#ifdef UNIX
  char *proj4 = nullptr;
  crs.exportToProj4(&proj4);
  cout << "wktarea\t= " << area->WKT() << endl << "proj4\t= " << proj4 << endl;
  OGRFree(proj4);
#endif

  cout << endl
       << "top\t= " << area->Top() << endl
       << "left\t= " << area->Left() << endl
       << "right\t= " << area->Right() << endl
       << "bottom\t= " << area->Bottom() << endl
       << endl;

  if (grid)
  {
    cout << "xnumber\t\t= " << grid->XNumber() << endl
         << "ynumber\t\t= " << grid->YNumber() << endl
         << "dx\t\t= " << area->WorldXYWidth() / grid->XNumber() / 1000.0 << " km" << endl
         << "dy\t\t= " << area->WorldXYHeight() / grid->YNumber() / 1000.0 << " km" << endl
         << endl;
  }

  cout << "xywidth\t\t= " << area->WorldXYWidth() / 1000.0 << " km" << endl
       << "xyheight\t= " << area->WorldXYHeight() / 1000.0 << " km" << endl
       << "aspectratio\t= " << area->WorldXYAspectRatio() << endl
       << endl;

  switch (classid)
  {
    case kNFmiEquiDistArea:
    case kNFmiGnomonicArea:
    case kNFmiStereographicArea:
#if 0
	case kNFmiPerspectiveArea:
#endif
    {
      const NFmiAzimuthalArea *a = dynamic_cast<const NFmiAzimuthalArea *>(area);
      cout << "central longitude\t= " << a->CentralLongitude() << endl
           << "central latitude\t= " << a->CentralLatitude() << endl
           << "true latitude\t\t= " << a->TrueLatitude() << endl;

#if 0
		if(classid == kFmiPerspectiveArea)
		  {
			NFmiPerspectiveArea * b = dynamic_cast<const NFmiAzimuthalArea *>(area);
			cout << "distancetosurface\t= " << b->DistanceToSurface() << endl
				 << "viewangle\t\t= " << b->ViewAngle() << endl
				 << "zoomfactor\t\t= " << b->ZoomFactor() << endl
				 << "globeradius\t\t= " << b->GlobeRadius() << endl;
		  }
#endif
      break;
    }
    case kNFmiKKJArea:
    case kNFmiPKJArea:
    case kNFmiYKJArea:
    case kNFmiLambertConformalConicArea:
    case kNFmiWebMercatorArea:
    {
      break;
    }
    case kNFmiLatLonArea:
    case kNFmiRotatedLatLonArea:
    {
      const NFmiLatLonArea *a = dynamic_cast<const NFmiLatLonArea *>(area);
      cout << "xscale\t\t= " << a->XScale() << endl << "yscale\t\t= " << a->YScale() << endl;
      break;
    }
    case kNFmiMercatorArea:
    {
      const NFmiMercatorArea *a = dynamic_cast<const NFmiMercatorArea *>(area);
      cout << "xscale\t\t= " << a->XScale() << endl << "yscale\t\t= " << a->YScale() << endl;
      break;
    }
    default:
      cout << "The projection is unknown to qdinfo" << endl;
      break;
  }

  return;
}

// ----------------------------------------------------------------------
/*!
 * Report on the locations of the query data
 *
 * \param q The queryinfo to report on
 */
// ----------------------------------------------------------------------

void ReportLocations(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  cout << endl << "Information on the locations:" << endl << endl;

  if (q->IsGrid())
  {
    cout << "The querydata is in grid form and has no named locations" << endl;
    return;
  }

  q->ResetLocation();
  while (q->NextLocation())
  {
    const NFmiLocation *loc = q->Location();

    cout << setw(8) << left << loc->GetIdent() << setw(32) << loc->GetName().CharPtr()
         << loc->GetLongitude() << ',' << loc->GetLatitude() << endl;
  }

  cout << endl << "There are " << q->SizeLocations() << " locations in total" << endl;

  return;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print metadata information
 */
// ----------------------------------------------------------------------

void ReportMetadata(NFmiFastQueryInfo *q)
{
  if (q == 0) throw runtime_error("Option -q is required for the given options");

  cout << endl << "Metadata information:" << endl << endl;

  NFmiStringList allkeys = q->GetAllKeys(false);

  if (allkeys.Reset())
  {
    do
    {
      NFmiString key = *allkeys.Current();
      if (q->FindFirstKey(key))
      {
        do
        {
          cout << key.CharPtr() << "\t" << q->GetCurrentKeyValue().CharPtr() << endl;
        } while (q->FindNextKey(key));
      }
    } while (allkeys.Next());
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdinfo [options]" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-q <qd>\t\tThe querydata file or directory to inspect" << endl
       << "\t-A\t\tAll the options below combined" << endl
       << "\t-l\t\tList all recognized parameters. Does not require option -q" << endl
       << "\t-p\t\tList all parameters in the querydata" << endl
       << "\t-T [%Y%m%d%H]\tShow querydata times in UTC time." << endl
       << endl
       << "\t-a\t\tAll the options below combined" << endl
       << "\t-v\t\tShow querydata version number" << endl
       << "\t-P\t\tShow all parameters in the querydata, including subparameters" << endl
       << "\t-t [%Y%m%d%H]\tShow querydata times in local time" << endl
       << "\t-x\t\tShow querydata projection information" << endl
       << "\t-X\t\tShow querydata location information" << endl
       << "\t-z\t\tShow querydata level information" << endl
       << "\t-r\t\tShow querydata producer information" << endl
       << "\t-M\t\tShow all metadata values" << endl
       << "\t-m key\t\tShow metadata value for the given key" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print metadata information
 */
// ----------------------------------------------------------------------

void ReportMetadata(NFmiFastQueryInfo *q, const string &theKey)
{
  NFmiString key(theKey);

  if (q->FindFirstKey(key))
  {
    do
    {
      cout << q->GetCurrentKeyValue().CharPtr() << endl;
    } while (q->FindNextKey(key));
  }
  else
    throw runtime_error("Key " + theKey + " has no value");
}

// ----------------------------------------------------------------------
/*!
 * The main program
 *
 * \param argc The number of arguments
 * \param argv Array of argument strings
 * \return 0 for success, nonzero on failure
 */
// ----------------------------------------------------------------------

int domain(int argc, const char *argv[])
{
  string opt_queryfile = "";

  NFmiCmdLine cmdline(argc, argv, "hq!avAlpPt:rT:xXzm!M");

  if (cmdline.Status().IsError())
  {
    throw runtime_error("Error: Invalid command line.");
  }

  if (cmdline.NumberofParameters() != 0 && !(cmdline.isOption('t') || cmdline.isOption('T')))
  {
    throw runtime_error("Error: No parameters are expected, only options.");
    return 1;
  }

  // help information

  if (cmdline.isOption('h'))
  {
    usage();
    return 0;
  }

  string datapath;
  try
  {
    // datapath = NFmiSettings::Require<string>("qdpoint::querydata_path");
    datapath = "/tmp";
  }
  catch (std::exception &e)
  {
    cerr << "Problems with settings:\n";
    cerr << e.what() << std::endl;
    return 1;
  }

  // option -q

  if (cmdline.isOption('q'))
  {
    if (cmdline.OptionValue('q') != nullptr)
    {
      opt_queryfile = cmdline.OptionValue('q');
      opt_queryfile = NFmiFileSystem::FileComplete(opt_queryfile, datapath);
    }
    else
    {
      throw runtime_error("Error: Missing argument for option -q.");
    }
  }

  // Actual processing begins

  auto_ptr<NFmiQueryInfo> qi;

  if (cmdline.isOption('q'))
  {
    string filename = opt_queryfile;
    if (NFmiFileSystem::DirectoryExists(opt_queryfile))
    {
      string newestfile = NFmiFileSystem::NewestFile(opt_queryfile);
      if (newestfile.empty())
      {
        throw runtime_error("Error: Directory " + opt_queryfile + " is empty.");
      }
      filename += '/';
      filename += newestfile;
    }

    qi.reset(new NFmiQueryInfo(filename));
  }

  NFmiFastQueryInfo *q = (qi.get() == 0 ? 0 : new NFmiFastQueryInfo(*qi));

  bool opt_all_extended = cmdline.isOption('A');
  bool opt_all = cmdline.isOption('a') || opt_all_extended;

  // Option -v shows the queryinfo version number

  if (cmdline.isOption('v') || opt_all) ReportVersion(q);

  // Option -l lists the known parameternames

  if (cmdline.isOption('l') || opt_all_extended) ReportParameterNames();

  // Option -p lists the parameters in the queryfile

  if (cmdline.isOption('p') || opt_all_extended) ReportParameters(q, true);

  // Option -P lists the parameters in the queryfile, including subparameters

  if (cmdline.isOption('P') || opt_all) ReportParameters(q, false);

  // Option -r shows producer information

  if (cmdline.isOption('r') || opt_all) ReportProducer(q);

  // Option -t shows time information

  string dateFormat = "";

  if (cmdline.isOption('t') || opt_all)
  {
    if (cmdline.OptionValue('t') == nullptr)
      dateFormat = "%Y%m%d%H";
    else
      dateFormat = cmdline.OptionValue('t');
    ReportTimes(q, false, dateFormat);
  }

  // Option -T shows time information in UTC

  if (cmdline.isOption('T') || opt_all_extended)
  {
    if (cmdline.OptionValue('T') == nullptr)
      dateFormat = "%Y%m%d%H";
    else
      dateFormat = cmdline.OptionValue('T');
    ReportTimes(q, true, dateFormat);
  }

  // Option -x shows projection information

  if (cmdline.isOption('x') || opt_all) ReportProjection(q);

  // Option -X shows location information

  if (cmdline.isOption('X') || opt_all) ReportLocations(q);

  // Option -z shows level information

  if (cmdline.isOption('z') || opt_all) ReportLevels(q);

  // Option -M shows all metadata information

  if (cmdline.isOption('M') || opt_all) ReportMetadata(q);

  // Option -m shows only the given metadata key

  if (cmdline.isOption('m')) ReportMetadata(q, cmdline.OptionValue('m'));

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 */
// ----------------------------------------------------------------------

int main(int argc, const char *argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (exception &e)
  {
    cerr << "Error: " << e.what() << endl;
  }
  catch (...)
  {
    cerr << "Error: An unknown exception occurred" << endl;
  }
  return 1;
}
