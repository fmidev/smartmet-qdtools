// ======================================================================
/*!
 * \brief Implementation of command qdsounding
 */
// ======================================================================
/*!
 * \page qdsounding qdsounding
 *
 * The qdsounding program prints a sounding of the given querydata.
 *
 * Usage:
 * \code
 * qdsounding [options] [querydata]
 * \endcode
 *
 * If the input argument is a directory, the newest file in it is
 * used.
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -P [param1,param2..] for specifying the parameters
 *   - -w [wmo1,wmo2..] for specifying the stations
 *   - -p [location1,location2,...] for specifying the locations
 *   - -t [zone] for selecting the timezone (default is Europe/Helsinki)
 *   - -c [coordfile] for selecting a coordinate file
 *   - -z for printing level value after level index
 */
// ======================================================================

#include "TimeTools.h"

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiLevel.h>
#include <newbase/NFmiLocationFinder.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiStringTools.h>

#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // winkkari puolella putenv ja tzset -funktiot on deprekoitu ja
// suositellaan k‰ytt‰m‰‰n _-nimisi‰ versioita, poistin sitten n‰m‰
// varoitukset pragmalla
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  string inputfile;
  vector<FmiParameterName> parameters;
  vector<int> stations;
  vector<string> locations;
  string timezone;
  string coordfile;
  bool printlevelvalue;

  Options()
      : inputfile(),
        parameters(),
        stations(),
        locations(),
        timezone(NFmiSettings::Optional<string>("qdpoint::timezone", "Europe/Helsinki")),
        coordfile(NFmiSettings::Optional<string>("qdpoint::coordinates",
                                                 "/smartmet/share/coordinates/default.txt")),
        printlevelvalue(false)
  {
  }
};

// ----------------------------------------------------------------------
/*!
 * \brief Global instance of the parsed command line options
 */
// ----------------------------------------------------------------------

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Global instance of enum converter for init speed
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter(kParamNames);

// ----------------------------------------------------------------------
/*!
 * \brief Print usage
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdsounding [options] querydata" << endl
       << endl
       << "qdsounding extracts soundings from the given querydata" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\t\t\tprint this help information" << endl
       << "\t-P [param1,param2...]\tthe desired parameters" << endl
       << "\t-w [wmo1,wmo2...]\tthe station numbers" << endl
       << "\t-p [loc1,loc2...]\tthe location names (also lon1,lat1,lon2... is allowed)" << endl
       << "\t-x [lon]\t\tthe longitude" << endl
       << "\t-y [lat]\t\tthe latitude" << endl
       << "\t-t [zone]\t\tthe time zone, default is Europe/Helsinki" << endl
       << "\t-c [file]\t\tthe coordinate file" << endl
       << "\t-z\t\t\tprint level value after level index" << endl
       << endl
       << "The default is to print the soundings for all stations if the data is point data."
       << endl
       << "For grid data locations must be specified explicitly." << endl
       << endl
       << "Option -P is required so that no one can rely on the parameter order" << endl
       << "in the querydata itself in scripts." << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 *
 * \return False, if execution is to be stopped
 */
// ----------------------------------------------------------------------

bool parse_command_line(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "hw!p!P!t!c!zx!y!");

  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help-option must be checked first

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  // then the required parameters

  if (cmdline.NumberofParameters() != 1)
    throw runtime_error("Incorrect number of command line parameters");

  options.inputfile = cmdline.Parameter(1);

  // options

  if (cmdline.isOption('P'))
  {
    const vector<string> args = NFmiStringTools::Split(cmdline.OptionValue('P'));
    for (vector<string>::const_iterator it = args.begin(); it != args.end(); ++it)
    {
      FmiParameterName param = FmiParameterName(converter.ToEnum(*it));
      if (param == kFmiBadParameter)
        throw runtime_error(string("Parameter '" + *it + "' is not recognized"));
      options.parameters.push_back(param);
    }
  }

  if (cmdline.isOption('t'))
  {
    options.timezone = cmdline.OptionValue('t');
  }

  if (cmdline.isOption('p')) options.locations = NFmiStringTools::Split(cmdline.OptionValue('p'));

  if (cmdline.isOption('x') && cmdline.isOption('y'))
  {
    options.locations.push_back(cmdline.OptionValue('x'));
    options.locations.push_back(cmdline.OptionValue('y'));
  }
  else if (cmdline.isOption('x') || cmdline.isOption('y'))
    throw runtime_error("Always use both -x and -y options simultaneously");

  if (cmdline.isOption('w'))
  {
    options.stations = NFmiStringTools::Split<vector<int> >(cmdline.OptionValue('w'));
  }

  if (cmdline.isOption('c')) options.coordfile = cmdline.OptionValue('c');

  if (cmdline.isOption('z')) options.printlevelvalue = true;

  if (!options.locations.empty() && !options.stations.empty())
    throw runtime_error("Options -p and -w are not allowed simultaneously");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a date for output
 */
// ----------------------------------------------------------------------

const string format_date(const NFmiTime &theTime)
{
  NFmiTime localtime = TimeTools::toLocalTime(theTime);
  return localtime.ToStr(kYYYYMMDDHHMM).CharPtr();
}

// ----------------------------------------------------------------------
/*!
 * \brief Find coordinates for places
 */
// ----------------------------------------------------------------------

const vector<NFmiPoint> find_places(const vector<string> & /* theNames */)
{
  string coordpath = NFmiSettings::Optional<string>("qdpoint::coordinates_path", ".");
  string coordfile = NFmiFileSystem::FileComplete(options.coordfile, coordpath);

  NFmiLocationFinder finder;
  if (NFmiFileSystem::FileExists(coordfile) && !finder.AddFile(coordfile, false))
    throw std::runtime_error("Reading file " + coordfile + " failed");

  std::vector<NFmiPoint> coords;

  for (unsigned int i = 0; i < options.locations.size(); i++)
  {
    if (i + 1 == options.locations.size())
    {
      NFmiPoint lonlat = finder.Find(options.locations[i].c_str());
      if (!finder.LastSearchFailed())
        coords.push_back(lonlat);
      else if (!NFmiFileSystem::FileExists(coordfile))
        throw std::runtime_error("Coordinate file '" + coordfile + "' does not exist");
      else
        throw std::runtime_error("Location " + options.locations[i] + " is not in the database");
    }
    else
    {
      try
      {
        float lon = NFmiStringTools::Convert<float>(options.locations[i]);
        float lat = NFmiStringTools::Convert<float>(options.locations[i + 1]);
        options.locations[i] = options.locations[i] + "," + options.locations[i + 1];
        coords.push_back(NFmiPoint(lon, lat));
        options.locations.erase(options.locations.begin() + i + 1);
      }
      catch (...)
      {
        NFmiPoint lonlat = finder.Find(options.locations[i].c_str());
        if (!finder.LastSearchFailed())
          coords.push_back(lonlat);
        else if (!NFmiFileSystem::FileExists(coordfile))
          throw std::runtime_error("Coordinate file '" + coordfile + "' does not exist");
        else
          throw std::runtime_error("Location " + options.locations[i] + " is not in the database");
      }
    }
  }

  return coords;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print named locations from gridded data
 */
// ----------------------------------------------------------------------

void print_locations(NFmiFastQueryInfo &theQ)
{
  if (!theQ.IsGrid()) throw runtime_error("Cannot use option -p for point data");

  const vector<NFmiPoint> coords = find_places(options.locations);

  for (vector<NFmiPoint>::size_type i = 0; i < coords.size(); i++)
  {
    for (theQ.ResetTime(); theQ.NextTime();)
      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        ostringstream out;
        out << options.locations[i] << ' ' << format_date(theQ.ValidTime()) << ' '
            << theQ.LevelIndex();

        if (options.printlevelvalue) out << ' ' << theQ.Level()->LevelValue();

        bool foundvalid = false;

        for (vector<FmiParameterName>::const_iterator it = options.parameters.begin();
             it != options.parameters.end();
             ++it)
        {
          if (!theQ.Param(*it))
            throw runtime_error("Parameter '" + converter.ToString(*it) +
                                "' is not available in the query data");

          float value = theQ.InterpolatedValue(coords[i]);
          if (value == kFloatMissing)
            out << " -";
          else
          {
            out << ' ' << value;
            foundvalid = true;
          }
        }
        if (foundvalid) cout << out.str() << endl;
      }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print listed stations from point data
 */
// ----------------------------------------------------------------------

void print_stations(NFmiFastQueryInfo &theQ)
{
  if (theQ.IsGrid()) throw runtime_error("Cannot use option -w for grid data");

  for (vector<int>::const_iterator wt = options.stations.begin(); wt != options.stations.end();
       ++wt)
  {
    if (!theQ.Location(*wt))
      throw runtime_error("Station '" + NFmiStringTools::Convert(*wt) +
                          "' is not available in the data");

    for (theQ.ResetTime(); theQ.NextTime();)
      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        ostringstream out;
        out << theQ.Location()->GetIdent() << ' ' << format_date(theQ.ValidTime()) << ' '
            << theQ.LevelIndex();

        if (options.printlevelvalue) out << ' ' << theQ.Level()->LevelValue();

        bool foundvalid = false;

        for (vector<FmiParameterName>::const_iterator it = options.parameters.begin();
             it != options.parameters.end();
             ++it)
        {
          if (!theQ.Param(*it))
            throw runtime_error("Parameter '" + converter.ToString(*it) +
                                "' is not available in the query data");

          float value = theQ.FloatValue();
          if (value == kFloatMissing)
            out << " -";
          else
          {
            out << ' ' << value;
            foundvalid = true;
          }
        }
        if (foundvalid) cout << out.str() << endl;
      }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print all stations from point data
 */
// ----------------------------------------------------------------------

void print_all_stations(NFmiFastQueryInfo &theQ)
{
  if (theQ.IsGrid()) throw runtime_error("Must use option -p for grid data");

  for (theQ.ResetLocation(); theQ.NextLocation();)
    for (theQ.ResetTime(); theQ.NextTime();)
      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        ostringstream out;
        out << theQ.Location()->GetIdent() << ' ' << format_date(theQ.ValidTime()) << ' '
            << theQ.LevelIndex();

        if (options.printlevelvalue) out << ' ' << theQ.Level()->LevelValue();

        bool foundvalid = false;

        for (vector<FmiParameterName>::const_iterator it = options.parameters.begin();
             it != options.parameters.end();
             ++it)
        {
          if (!theQ.Param(*it))
            throw runtime_error("Parameter '" + converter.ToString(*it) +
                                "' is not available in the query data");

          float value = theQ.FloatValue();
          if (value == kFloatMissing)
            out << " -";
          else
          {
            out << ' ' << value;
            foundvalid = true;
          }
        }
        if (foundvalid) cout << out.str() << endl;
      }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the timezone
 *
 * \param theZone The zone name, for example Europe/Helsinki or UTC
 */
// ----------------------------------------------------------------------

void set_timezone(const string &theZone)
{
  static string tzvalue = "TZ=" + theZone;
  putenv(const_cast<char *>(tzvalue.c_str()));
  tzset();
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int domain(int argc, const char *argv[])
{
  // Parse the command line
  if (!parse_command_line(argc, argv)) return 0;

  // Read the querydata

  NFmiQueryData qd(options.inputfile);
  unique_ptr<NFmiFastQueryInfo> q(new NFmiFastQueryInfo(&qd));

  // Establish time zone

  set_timezone(options.timezone);

  // Establish what to do

  if (!options.locations.empty())
    print_locations(*q);

  else if (!options.stations.empty())
    print_stations(*q);

  else
    print_all_stations(*q);

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
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
    cout << "Error: " << e.what() << endl;
    return 1;
  }
  catch (...)
  {
    cout << "Error: Caught an unknown exception" << endl;
    return 1;
  }
}
