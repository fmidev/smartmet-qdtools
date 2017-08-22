// ======================================================================
/*!
 * \file
 * \brief Implementation of qdarea - a program to integrate querydata
 */
// ======================================================================

#include <calculator/Acceptor.h>
#include <calculator/AnalysisSources.h>
#include <calculator/GridForecaster.h>
#include <calculator/HourPeriodGenerator.h>
#include <calculator/IntervalPeriodGenerator.h>
#include <calculator/LatestWeatherSource.h>
#include <calculator/ListedPeriodGenerator.h>
#include <calculator/MaximumCalculator.h>
#include <calculator/MinimumCalculator.h>
#include <calculator/NullPeriodGenerator.h>
#include <calculator/RangeAcceptor.h>
#include <calculator/RegularMaskSource.h>
#include <calculator/Settings.h>
#include <calculator/WeatherArea.h>
#include <calculator/WeatherFunction.h>
#include <calculator/WeatherParameter.h>
#include <calculator/WeatherPeriod.h>
#include <calculator/WeatherResult.h>

#include <newbase/NFmiArea.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiSettings.h>

#include <boost/shared_ptr.hpp>

#include <cstdlib>  // putenv
#include <ctime>    // tzset
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <vector>

using namespace std;
using namespace boost;
using namespace TextGen;

// ----------------------------------------------------------------------
/*!
 * \brief Holder for a single parameter calculation
 */
// ----------------------------------------------------------------------

struct ParameterRequest
{
  ParameterRequest(const string& theRequest);
  WeatherParameter parameter;
  WeatherFunction areafunction;
  WeatherFunction timefunction;
  boost::shared_ptr<Acceptor> tester;

 private:
  ParameterRequest();
  WeatherParameter parse_parameter(const string& theParameter) const;
  WeatherFunction parse_function(const string& theString) const;
  boost::shared_ptr<Acceptor> parse_acceptor(const string& theString) const;

  string extract_function(const string& theString) const;
  string extract_acceptor(const string& theString) const;
};

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function name from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString
 * \return The function name alone
 */
// ----------------------------------------------------------------------

string ParameterRequest::extract_function(const string& theString) const
{
  string::size_type pos = theString.find('[');
  if (pos == string::npos)
    return theString;
  else
    return theString.substr(0, pos);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function modifier from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString
 * \return The function modified alone (possibly empty string)
 */
// ----------------------------------------------------------------------

string ParameterRequest::extract_acceptor(const string& theString) const
{
  string::size_type pos1 = theString.find('[');
  if (pos1 == string::npos) return "";

  string::size_type pos2 = theString.find(']', pos1);
  if (pos2 == string::npos && pos2 != theString.size() - 1)
    throw runtime_error("Invalid function modifier in '" + theString + "'");

  return theString.substr(pos1 + 1, pos2 - pos1 - 1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse an acceptor
 *
 * The only currently recognized format is "lo:hi", from which either
 * the lo or hi may be missing, but not both. This format results in
 * a RangeAcceptor.
 *
 * \param theString The string to parse
 * \return The generated acceptor as a shared pointer
 */
// ----------------------------------------------------------------------

boost::shared_ptr<Acceptor> ParameterRequest::parse_acceptor(const string& theString) const
{
  const string::size_type pos = theString.find(':');

  if (pos == string::npos) throw runtime_error("Unrecognized modifier format '" + theString + "'");

  const string lo = theString.substr(0, pos);
  const string hi = theString.substr(pos + 1);

  if (lo.empty() && hi.empty())
    throw runtime_error("Both lower and upper limit are missing from a modifier");

  boost::shared_ptr<RangeAcceptor> acceptor(new RangeAcceptor);

  if (!lo.empty()) acceptor->lowerLimit(NFmiStringTools::Convert<float>(lo));
  if (!hi.empty()) acceptor->upperLimit(NFmiStringTools::Convert<float>(hi));

  return acceptor;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter name
 *
 * Throws if the name is not recognized.
 *
 * \param theParameter
 * \return The respective enumerated value
 */
// ----------------------------------------------------------------------

WeatherParameter ParameterRequest::parse_parameter(const string& theParameter) const
{
  static const char* names[] = {"Temperature",
                                "Precipitation",
                                "PrecipitationType",
                                "PrecipitationForm",
                                "PrecipitationProbability",
                                "Cloudiness",
                                "Frost",
                                "SevereFrost",
                                "RelativeHumidity",
                                "WindSpeed",
                                "WindDirection",
                                "Thunder",
                                "RoadTemperature",
                                "RoadCondition",
                                "WaveHeight",
                                "WaveDirection",
                                "t2m",
                                "t",
                                "rr1h",
                                "rtype",
                                "rform",
                                "pop",
                                "n",
                                "rh",
                                "wspd",
                                "ff",
                                "wdir",
                                "dd",
                                "pot",
                                "troad",
                                "wroad",
                                "ForestFireWarning",
                                "Evaporation",
                                "evap",
                                "mpi",
                                "DewPoint",
                                "tdew",
                                "HourlyMaximumGust",
                                "gust",
                                "FogIntensity",
                                "fog",
                                "MaximumWind",
                                "HourlyMaximumWindSpeed",
                                "wmax",
                                "PrecipitationRate",
                                "rr",
                                "EffectiveTemperatureSum",
                                "growthtsum",
                                "WaterEquivalentOfSnow",
                                "snowdepth",
                                ""};

  static WeatherParameter parameters[] = {
      Temperature,               // "Temperature"
      Precipitation,             // "Precipitation"
      PrecipitationType,         // "PrecipitationType"
      PrecipitationForm,         // "PrecipitationForm"
      PrecipitationProbability,  // "PrecipitationProbability"
      Cloudiness,                // "Cloudiness"
      Frost,                     // "Frost"
      SevereFrost,               // "SevereFrost"
      RelativeHumidity,          // "RelativeHumidity"
      WindSpeed,                 // "WindSpeed"
      WindDirection,             // "WindDirection"
      Thunder,                   // "Thunder"
      RoadTemperature,           // "RoadTemperature"
      RoadCondition,             // "RoadCondition"
      WaveHeight,                // "WaveHeight"
      WaveDirection,             // "WaveDirection"
      Temperature,               // "t2m"
      Temperature,               // "t"
      Precipitation,             // "rr1h"
      PrecipitationType,         // "rtype"
      PrecipitationForm,         // "rform"
      PrecipitationProbability,  // "pop"
      Cloudiness,                // "n"
      RelativeHumidity,          // "rh"
      WindSpeed,                 // "wspd"
      WindSpeed,                 // "ff"
      WindDirection,             // "wdir"
      WindDirection,             // "dd"
      Thunder,                   // "pot"
      RoadTemperature,           // "troad"
      RoadCondition,             // "wroad"
      ForestFireIndex,           // "ForestFireWarning"
      Evaporation,               // "Evaporation"
      Evaporation,               // "evap"
      ForestFireIndex,           // "mpi"
      DewPoint,                  // "DewPoint"
      DewPoint,                  // "tdew"
      GustSpeed,                 // "HourlyMaximumGust",
      GustSpeed,                 // "gust"
      Fog,                       // "FogIntensity"
      Fog,                       // "fog"
      MaximumWind,               // "MaximumWind"
      MaximumWind,               // "HourlyMaximumWindSpeed"
      MaximumWind,               // "wmax"
      PrecipitationRate,         // "PrecipitationRate"
      PrecipitationRate,         // "rr"
      EffectiveTemperatureSum,   // "EffectiveTemperatureSum"
      EffectiveTemperatureSum,   // "growthtsum"
      WaterEquivalentOfSnow,     // "WaterEquivalentOfSnow"
      WaterEquivalentOfSnow      // "snowdepth"
  };

  for (unsigned int i = 0; strlen(names[i]) > 0; i++)
  {
    if (names[i] == theParameter) return parameters[i];
  }

  throw runtime_error("Unrecognized parameter name '" + theParameter + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a function name
 *
 * Throws if the name is not recognized.
 *
 * \param theFunction
 * \return The respective enumerated value
 */
// ----------------------------------------------------------------------

WeatherFunction ParameterRequest::parse_function(const string& theFunction) const
{
  static const char* names[] = {
      "mean", "max", "min", "sum", "sdev", "trend", "change", "count", "percentage", ""};

  static WeatherFunction functions[] = {
      Mean, Maximum, Minimum, Sum, StandardDeviation, Trend, Change, Count, Percentage};

  for (unsigned int i = 0; strlen(names[i]) > 0; i++)
  {
    if (names[i] == theFunction) return functions[i];
  }

  throw runtime_error("Unrecognized function name '" + theFunction + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ParameterRequest::ParameterRequest(const string& theRequest)
{
  // split into parts separated by parenthesis
  list<string> parts;
  string::size_type pos1 = 0;
  while (pos1 < theRequest.size())
  {
    string::size_type pos2 = pos1;
    for (; pos2 < theRequest.size(); ++pos2)
      if (theRequest[pos2] == '(' || theRequest[pos2] == ')') break;
    if (pos2 - pos1 > 0) parts.push_back(theRequest.substr(pos1, pos2 - pos1));
    pos1 = pos2 + 1;
  }

  if (parts.size() < 2 || parts.size() > 3)
    throw runtime_error("Errorneous parameter request: '" + theRequest + "'");

  const string paramname = parts.back();
  const string functionname1 = parts.front();
  parts.pop_back();
  parts.pop_front();
  const string functionname2 = (parts.empty() ? functionname1 : parts.front());

  parameter = parse_parameter(paramname);
  areafunction = parse_function(extract_function(functionname1));
  timefunction = parse_function(extract_function(functionname2));

  const string acceptor1 = extract_acceptor(functionname1);
  const string acceptor2 = extract_acceptor(functionname2);

  tester = boost::shared_ptr<Acceptor>(new NullAcceptor);

  if (areafunction == Percentage || areafunction == Count)
  {
    tester = parse_acceptor(acceptor1);
  }
  else if (!acceptor1.empty())
  {
    throw runtime_error("Extraneous modifier in '" + functionname1 + "'");
  }

  if (timefunction == Percentage || timefunction == Count)
  {
    if (areafunction == Percentage || areafunction == Count)
      throw runtime_error(
          "Cannot have Percentage/Count functions in both the area and time integrals");
    tester = parse_acceptor(acceptor2);
  }
  else if (!acceptor2.empty())
  {
    throw runtime_error("Extraneous modifier in '" + functionname2 + "'");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdarea [options]" << endl
       << endl
       << "Options:" << endl
       << endl
       << "   -P [paramlist]\tFor example 'mean(min(t2m)),mean(max(t2m)'" << endl
       << "   -p [location]\tFor example Helsinki:25 or merialueet/B1" << endl
       << "   -T [interval]\tFor example 06-18 for days or 6:12 for nights too" << endl
       << "   -t [timezone]\tFor example Europe/Helsinki or UTC, default is specified in fmi.conf"
       << endl
       << "   -q [querydata]\tDefault is specified in fmi.conf" << endl
       << "   -c [coordinatefile]\tDefault is specified in fmi.conf" << endl
       << "   -s\t\t\tPrint results as a PHP hash table" << endl
       << "   -S [namelist]\tPrint results as a PHP hash table with named data fields" << endl
       << "   -E\t\t\tPrint times in Epoch seconds" << endl
       << "   -v\t\t\tVerbose mode on" << endl
       << endl
       << "For example:" << endl
       << endl
       << "qdarea -T 24 -p merialueet/B1 -P 'mean(max(wspd)),mean(wdir)'" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Command line options container
 */
// ----------------------------------------------------------------------

struct options_list
{
  map<string, WeatherArea> areas;
  boost::shared_ptr<WeatherPeriodGenerator> generator;
  list<ParameterRequest> parameters;
  string timezone;
  vector<string> querydata;
  string coordinatefile;
  bool verbose;
  bool php;
  bool epoch_time;
  vector<string> php_names;

  vector<AnalysisSources> sources;
  boost::shared_ptr<WeatherPeriod> period;
};

static options_list options;

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter request
 *
 * The request consists of comma separated individual
 * parameter requests, which can be in one of the following forms:
 *
 *  - function(function(param))
 *  - function(param)
 *  - function[lo:hi](param)
 *  - function(function[lo:hi](param))
 *  - function[lo:hi](function(param))
 *
 * The parsed request is stored in options.parameters
 *
 * \param theRequest The request in string form
 */
// ----------------------------------------------------------------------

void parse_parameter_option(const string& theRequest)
{
  // Split into individual requests
  const vector<string> requests = NFmiStringTools::Split(theRequest);

  // Parse the individual requests
  for (vector<string>::const_iterator it = requests.begin(); it != requests.end(); ++it)
  {
    options.parameters.push_back(ParameterRequest(*it));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a php name list option
 *
 * The requests of comma separated names for each parameter.
 * The names are stored into options.php_names.
 *
 * \param theRequest The request in string form
 */
// ----------------------------------------------------------------------

void parse_php_names_option(const string& theRequest)
{
  const vector<string> words = NFmiStringTools::Split(theRequest);
  copy(words.begin(), words.end(), back_inserter(options.php_names));
  if (options.php_names.size() != options.parameters.size())
    throw runtime_error("The number of names does not match the number of parameters");
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the area option
 *
 * The recognized formats should be as follows
 *
 *  - pointname - for example Helsinki
 *  - lon,lat - for example 25,60
 *  - point:radius - for example Helsinki:50
 *  - lon,lat:radius - for example 25,60:50
 *  - areaname - for example maakunta/uusimaa
 *
 * The parsed request is stored in options.area.
 *
 * \param theRequest The request in string form
 *
 * \todo Only the \c areaname form is currently supported
 */
// ----------------------------------------------------------------------

void parse_area_option(const string& theRequest)
{
  vector<string> areas = NFmiStringTools::Split(theRequest, "::");
  for (vector<string>::const_iterator it = areas.begin(); it != areas.end(); ++it)
  {
    WeatherArea area(*it);
    options.areas.insert(make_pair(*it, area));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the time period covered by the data
 */
// ----------------------------------------------------------------------

void establish_time_period()
{
  boost::shared_ptr<NFmiQueryData> qdata =
      options.sources.front().getWeatherSource()->data(options.querydata.front());
  NFmiFastQueryInfo qinfo = NFmiFastQueryInfo(qdata.get());

  qinfo.FirstTime();
  const TextGenPosixTime firsttime = qinfo.ValidTime();
  qinfo.LastTime();
  const TextGenPosixTime lasttime = qinfo.ValidTime();

  options.period = boost::shared_ptr<WeatherPeriod>(new WeatherPeriod(
      TextGenPosixTime::LocalTime(firsttime), TextGenPosixTime::LocalTime(lasttime)));

  if (options.verbose)
  {
    cout << "# UTC time range: " << options.period->utcStartTime().ToStr(kYYYYMMDDHHMM) << ' '
         << options.period->utcEndTime().ToStr(kYYYYMMDDHHMM) << endl;
    cout << "# Timezone " << options.timezone
         << " time range: " << options.period->localStartTime().ToStr(kYYYYMMDDHHMM) << ' '
         << options.period->localEndTime().ToStr(kYYYYMMDDHHMM) << endl;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the time period covered by the data
 */
// ----------------------------------------------------------------------

void make_data_timestep_generator()
{
  boost::shared_ptr<ListedPeriodGenerator> lgen(new ListedPeriodGenerator(*options.period));

  boost::shared_ptr<NFmiQueryData> qdata =
      options.sources.front().getWeatherSource()->data(options.querydata.front());
  NFmiFastQueryInfo qinfo = NFmiFastQueryInfo(qdata.get());

  for (qinfo.ResetTime(); qinfo.NextTime();)
  {
    const TextGenPosixTime t = TextGenPosixTime::LocalTime(qinfo.ValidTime());
    WeatherPeriod p(t, t);
    lgen->add(p);
  }

  options.generator = lgen;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the interval option
 *
 * The parsed request is stored in options.generator
 *
 * The supported formats are
 *
 *  - starthour-endhour
 *  - starthour-endhour:maxstarthour-minendhour
 *  - interval
 *  - starthour:interval
 *  - starthour:interval:mininterval
 *
 * \param theRequest The interval definition in string form
 */
// ----------------------------------------------------------------------

void parse_interval_option(const string& theRequest)
{
  if (options.period.get() == 0)
    throw runtime_error("Trying to parse interval before data period is established");

  vector<string> words = NFmiStringTools::Split(theRequest, "-");

  switch (words.size())
  {
    case 1:  // interval or starthour:interval or starthour:interval:mininterval
    {
      if (words.front() == "all")
      {
        options.generator =
            boost::shared_ptr<WeatherPeriodGenerator>(new NullPeriodGenerator(*options.period));
        break;
      }

      if (words.front() == "data")
      {
        make_data_timestep_generator();
        break;
      }

      list<string> words2 = NFmiStringTools::Split<list<string> >(words.front(), ":");
      if (words2.size() > 3) throw runtime_error("Invalid period definition '" + theRequest + "'");
      int starthour = 0;
      int interval = NFmiStringTools::Convert<int>(words2.back());
      int mininterval = interval;
      words2.pop_back();
      if (words2.size() >= 1)
      {
        starthour = NFmiStringTools::Convert<int>(words2.front());
        words2.pop_front();
      }
      if (words2.size() >= 1) interval = NFmiStringTools::Convert<int>(words2.front());

      options.generator = boost::shared_ptr<WeatherPeriodGenerator>(
          new IntervalPeriodGenerator(*options.period, starthour, interval, mininterval));
      break;
    }

    case 2:  // starthour-endhour
    {
      const int starthour = NFmiStringTools::Convert<int>(words.front());
      const int endhour = NFmiStringTools::Convert<int>(words.back());
      options.generator = boost::shared_ptr<WeatherPeriodGenerator>(
          new HourPeriodGenerator(*options.period, starthour, endhour, starthour, endhour));
      break;
    }

    case 3:  // starthour-endhour:maxstarthour-minendhour
    {
      vector<string> words2 = NFmiStringTools::Split(*(++words.begin()), ":");
      if (words2.size() != 2) throw runtime_error("Invalid period definition '" + theRequest + "'");
      const int starthour = NFmiStringTools::Convert<int>(words.front());
      const int endhour = NFmiStringTools::Convert<int>(words2.front());
      const int maxstarthour = NFmiStringTools::Convert<int>(words2.back());
      const int minendhour = NFmiStringTools::Convert<int>(words.back());
      options.generator = boost::shared_ptr<WeatherPeriodGenerator>(
          new HourPeriodGenerator(*options.period, starthour, endhour, maxstarthour, minendhour));
      break;
    }

    default:
      throw runtime_error("Invalid period definition '" + theRequest + "'");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Command line parser
 */
// ----------------------------------------------------------------------

void parse_command_line(int argc, const char* argv[])
{
  if (argc == 1)
  {
    usage();
    throw runtime_error("Error: Insufficient command line options");
  }

  // Establish the defaults

  options.verbose = false;
  options.php = false;
  options.epoch_time = false;
  options.timezone = Settings::optional_string("qdarea::timezone", "local");
  options.querydata = NFmiStringTools::Split(Settings::optional_string("qdarea::querydata", ""));
  options.coordinatefile =
      Settings::optional_string("qdarea::coordinates", "/smartmet/share/coordinates/default.txt");

  // Parse the command line

  NFmiCmdLine cmdline(argc, argv, "P!p!T!t!q!c!S!Esvh");

  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  if (cmdline.NumberofParameters() != 0)
    throw runtime_error("No command line parameters are expected");

  if (cmdline.isOption('h'))
  {
    usage();
    exit(0);
  }

  // verbose mode check first, so we can be verbose from the start

  if (cmdline.isOption('v')) options.verbose = true;

  if (cmdline.isOption('E')) options.epoch_time = true;

  // -q option must be parsed before -T option
  if (cmdline.isOption('q'))
    options.querydata = NFmiStringTools::Split<vector<string> >(cmdline.OptionValue('q'));

  if (options.querydata.empty())
    throw runtime_error("No querydata specified via -q or via qdarea::querydata");

  // must initialize data sources right after -q

  options.sources.resize(options.querydata.size());

  for (vector<AnalysisSources>::iterator it = options.sources.begin(); it != options.sources.end();
       ++it)
  {
    boost::shared_ptr<WeatherSource> weathersource(new LatestWeatherSource());
    boost::shared_ptr<MaskSource> masksource(new RegularMaskSource());

    it->setWeatherSource(weathersource);
    it->setMaskSource(masksource);
  }

  // Must set timezone before parsing -T option
  if (cmdline.isOption('t')) options.timezone = cmdline.OptionValue('t');
  TextGenPosixTime::SetThreadTimeZone(options.timezone);

  // This must be done after the timezone has been set and data has been read
  establish_time_period();

  if (cmdline.isOption('T'))
    parse_interval_option(cmdline.OptionValue('T'));
  else
    parse_interval_option("24");

  if (cmdline.isOption('P'))
    parse_parameter_option(cmdline.OptionValue('P'));
  else
    throw runtime_error("Option -P must be given");

  // NOTE: -S must be parsed after the -P option

  if (cmdline.isOption('s')) options.php = true;

  if (cmdline.isOption('S'))
  {
    options.php = true;
    parse_php_names_option(cmdline.OptionValue('S'));
  }

  if (cmdline.isOption('c')) options.coordinatefile = cmdline.OptionValue('c');

  if (options.timezone.empty()) throw runtime_error("The specified timezone string is empty");

  if (options.querydata.empty()) throw runtime_error("The specified querydata string is empty");

  if (options.coordinatefile.empty())
    throw runtime_error("The specified coordinatefile string is empty");

  for (vector<string>::const_iterator qt = options.querydata.begin(); qt != options.querydata.end();
       ++qt)
  {
    if (!NFmiFileSystem::FileExists(*qt) && !NFmiFileSystem::DirectoryExists(*qt))
      throw runtime_error("The querydata '" + *qt + "' does not exist");
  }

  if (!NFmiFileSystem::FileExists(options.coordinatefile))
    throw runtime_error("The coordinatefile '" + options.coordinatefile + "' does not exist");

  Settings::set("textgen::coordinates", options.coordinatefile);

  // NOTE: Must be done after coordinate source has been defined

  if (cmdline.isOption('p'))
    parse_area_option(cmdline.OptionValue('p'));
  else
    throw runtime_error("Option -p must be given");
}

// ----------------------------------------------------------------------
/*!
 * \brief A container for calculated results
 */
// ----------------------------------------------------------------------

typedef vector<WeatherResult> Results;
typedef map<WeatherPeriod, Results> TimedResults;
typedef map<string, TimedResults> AreaResults;

static AreaResults results;

// ----------------------------------------------------------------------
/*!
 * \brief Establish the data source for the given area
 *
 * Note that expansion radiuses are not taken into account
 * in the insidedness test, they are considered to be small
 * enough to be discarded in any reasonable problem.
 */
// ----------------------------------------------------------------------

AnalysisSources find_source(const WeatherArea& theArea)
{
  unsigned int idx = 0;

  for (; idx < options.querydata.size(); idx++)
  {
    const string& filename = options.querydata[idx];
    AnalysisSources& sources = options.sources[idx];
    boost::shared_ptr<NFmiQueryData> data = sources.getWeatherSource()->data(filename);

    NFmiFastQueryInfo q = NFmiFastQueryInfo(data.get());

    if (theArea.isPoint())
    {
      const NFmiPoint& p = theArea.point();
      if (q.Area()->IsInside(p)) break;
    }
    else
    {
      const NFmiSvgPath& p = theArea.path();
      bool inside = true;
      for (NFmiSvgPath::const_iterator it = p.begin(); inside && it != p.end(); ++it)
      {
        switch (it->itsType)
        {
          case NFmiSvgPath::kElementMoveto:
          case NFmiSvgPath::kElementLineto:
            inside = q.Area()->IsInside(NFmiPoint(it->itsX, it->itsY));
          case NFmiSvgPath::kElementNotValid:
          case NFmiSvgPath::kElementClosePath:
            break;
        }
      }
      if (inside) break;
    }
  }

  if (idx >= options.querydata.size())
  {
    if (theArea.isNamed())
      throw runtime_error(theArea.name() + " is not contained in any querydata");
    else
      throw runtime_error("The area is not contained in any querydata");
  }

  Settings::set("textgen::default_forecast", options.querydata[idx]);
  return options.sources[idx];
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the requested integrals
 *
 * Algorithm:
 *
 *   -# For each generated period
 *     -# For each requested function
 *       -# Calculate the result and store it
 */
// ----------------------------------------------------------------------

void calculate_results()
{
  GridForecaster forecaster;

  const WeatherPeriodGenerator::size_type n = options.generator->size();

  for (map<string, WeatherArea>::const_iterator at = options.areas.begin();
       at != options.areas.end();
       ++at)
  {
    TimedResults timedresults;

    const WeatherArea& area = at->second;

    AnalysisSources sources = find_source(area);

    for (WeatherPeriodGenerator::size_type i = 1; i <= n; ++i)
    {
      const WeatherPeriod period = options.generator->period(i);
      Results newresults;

      for (list<ParameterRequest>::const_iterator it = options.parameters.begin();
           it != options.parameters.end();
           ++it)
      {
        WeatherResult result = forecaster.analyze(sources,
                                                  it->parameter,
                                                  it->areafunction,
                                                  it->timefunction,
                                                  area,
                                                  period,
                                                  DefaultAcceptor(),
                                                  DefaultAcceptor(),
                                                  *(it->tester));
        newresults.push_back(result);
      }

      timedresults.insert(TimedResults::value_type(period, newresults));
    }
    results.insert(AreaResults::value_type(at->first, timedresults));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a time interval for printing
 *
 * \param thePeriod The time interval
 * \param theSeparator The separator character between the start and end times
 * \return The formatted string
 */
// ----------------------------------------------------------------------

string format_time_period(const WeatherPeriod& thePeriod, char theSeparator)
{
  ostringstream out;
  if (options.epoch_time)
  {
    out << thePeriod.utcStartTime().EpochTime() << theSeparator
        << thePeriod.utcEndTime().EpochTime();
  }
  else
  {
    out << thePeriod.localStartTime().ToStr(kYYYYMMDDHHMM) << theSeparator
        << thePeriod.localEndTime().ToStr(kYYYYMMDDHHMM);
  }
  return out.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a value for printing
 *
 * \param theValue The value to be printed
 * \param theMissing The string to print if the value is a special missing value
 * \return The formatted string
 */
// ----------------------------------------------------------------------

string format_value(double theValue, const string& theMissing)
{
  ostringstream out;

  if (theValue == kFloatMissing)
    out << theMissing;
  else
    out << fixed << setprecision(1) << theValue;
  return out.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the calculated results
 */
// ----------------------------------------------------------------------

void print_results()
{
  for (AreaResults::const_iterator at = results.begin(); at != results.end(); ++at)
  {
    for (TimedResults::const_iterator it = at->second.begin(); it != at->second.end(); ++it)
    {
      cout << at->first << ' ' << format_time_period(it->first, ' ');
      for (Results::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
      {
        cout << ' ' << format_value(jt->value(), "-");
      }
      cout << endl;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the calculated results as a PHP hash table
 */
// ----------------------------------------------------------------------

void print_php_results()
{
  const bool print_names = !(options.php_names.empty());

  cout << "array(" << endl;
  for (AreaResults::const_iterator at = results.begin(); at != results.end();)
  {
    cout << " \"" << at->first << "\" => array(" << endl;

    // Print the values themselves
    cout << "  \"values\" => array(" << endl;
    for (TimedResults::const_iterator it = at->second.begin(); it != at->second.end();)
    {
      cout << "   \"" << format_time_period(it->first, '-') << "\" => array(";

      unsigned int j = 0;
      for (Results::const_iterator jt = it->second.begin(); jt != it->second.end();)
      {
        if (print_names) cout << '"' << options.php_names[j++] << "\"=>";
        cout << format_value(jt->value(), "x");
        ++jt;
        if (jt != it->second.end()) cout << ',';
      }
      ++it;
      if (it == at->second.end())
        cout << ")" << endl;
      else
        cout << ")," << endl;
    }
    cout << "   )," << endl;

    // Print the maximum values
    unsigned int j = 0;
    cout << "  \"maxima\" => array(";
    for (unsigned int i = 0; i < options.parameters.size(); i++)
    {
      MaximumCalculator calculator;
      for (TimedResults::const_iterator it = at->second.begin(); it != at->second.end(); ++it)
        calculator(it->second[i].value());

      if (i > 0) cout << ',';
      if (print_names) cout << '"' << options.php_names[j++] << "\"=>";
      cout << format_value(calculator(), "x");
    }
    cout << ")," << endl;

    // Print the minimum values
    j = 0;
    cout << "  \"minima\" => array(";
    for (unsigned int i = 0; i < options.parameters.size(); i++)
    {
      MinimumCalculator calculator;
      for (TimedResults::const_iterator it = at->second.begin(); it != at->second.end(); ++it)
        calculator(it->second[i].value());

      if (i > 0) cout << ',';
      if (print_names) cout << '"' << options.php_names[j++] << "\"=>";
      cout << format_value(calculator(), "x");
    }
    cout << ")" << endl;

    ++at;
    if (at == results.end())
      cout << " )" << endl;
    else
      cout << " )," << endl;
  }
  cout << ");" << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int domain(int argc, const char* argv[])
{
  // Initialize global settings
  NFmiSettings::Init();
  // set parameters
  Settings::set(NFmiSettings::ToString());

  // Read the command line arguments

  parse_command_line(argc, argv);

  // Calculate the desired integrals

  calculate_results();

  // And print them

  if (options.php)
    print_php_results();
  else
    print_results();

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 *
 * The main program is only an error trapping driver for domain
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (const std::exception& e)
  {
    cerr << "Error: Caught an exception:" << endl << "--> " << e.what() << endl << endl;
    return 1;
  }
  catch (...)
  {
    cerr << "Error: Caught an unknown exception" << endl;
  }
}
