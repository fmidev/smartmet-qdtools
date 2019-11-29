#include "TimeTools.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiParameterName.h>
#include <newbase/NFmiQueryData.h>
#include <cmath>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

// This is global so that we can not just parse but also print errors
NFmiEnumConverter converter;

const int column_width = 10;

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options() = default;

  std::string infile = "-";

  bool all_times = false;
  bool all_stations = false;
  bool all_levels = false;
  bool percentages = false;
  bool distribution = false;
  std::size_t bins = 20;
  std::size_t barsize = 60;
  double ignored_value = std::numeric_limits<double>::quiet_NaN();  // never compares ==

  std::set<boost::posix_time::ptime> these_times;
  std::set<FmiParameterName> these_params;
  std::set<int> these_stations;
  std::set<float> these_levels;
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of timestamps
 */
// ----------------------------------------------------------------------

std::set<boost::posix_time::ptime> parse_times(const std::string& str)
{
  std::set<boost::posix_time::ptime> ret;

  if (str.empty()) return ret;

  std::list<std::string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of(","));

  for (const auto& stamp : parts)
  {
    ret.insert(Fmi::TimeParser::parse(stamp));
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of parameters
 */
// ----------------------------------------------------------------------

std::set<FmiParameterName> parse_params(const std::string& str)
{
  std::set<FmiParameterName> ret;

  if (str.empty()) return ret;

  std::list<std::string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of(","));

  for (const auto& param : parts)
  {
    FmiParameterName p = FmiParameterName(converter.ToEnum(param));
    if (p == kFmiBadParameter) throw std::runtime_error("Bad parameter name: '" + param + "'");
    ret.insert(p);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of station numbers
 */
// ----------------------------------------------------------------------

std::set<int> parse_stations(const std::string& str)
{
  std::set<int> ret;

  if (str.empty()) return ret;

  std::list<std::string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of(","));

  for (const auto& str : parts)
  {
    ret.insert(Fmi::stoi(str));
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of levels
 */
// ----------------------------------------------------------------------

std::set<float> parse_levels(const std::string& str)
{
  std::set<float> ret;

  if (str.empty()) return ret;

  std::list<std::string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of(","));

  for (const auto& str : parts)
  {
    ret.insert(Fmi::stof(str));
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char* argv[])
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;

  std::string opt_stamps;
  std::string opt_params;
  std::string opt_stations;
  std::string opt_levels;

#ifdef UNIX
  struct winsize wsz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz);
  const int desc_width = (wsz.ws_col < 80 ? 80 : wsz.ws_col);
#else
  const int desc_width = 100;
#endif

  po::options_description desc("Available options", desc_width);
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "infile,i", po::value(&options.infile), "input querydata")(
      "alltimes,T", po::bool_switch(&options.all_times), "for all times")(
      "allstations,W", po::bool_switch(&options.all_stations), "for all stations")(
      "allevels,Z", po::bool_switch(&options.all_levels), "for all levels")(
      "percentages,r",
      po::bool_switch(&options.percentages),
      "print percentages instead of counts")(
      "distribution,d", po::bool_switch(&options.distribution), "print distribution of values")(
      "ignore,I", po::value(&options.ignored_value), "ignore this value in statistics")(
      "bins,b", po::value(&options.bins), "max number of bins in the distribution")(
      "barsize,B", po::value(&options.barsize), "width of the bar distribution")(
      "times,t", po::value(&opt_stamps), "times to process")(
      "params,p", po::value(&opt_params), "parameters to process")(
      "stations,w", po::value(&opt_stations), "stations to process")(
      "levels,z", po::value(&opt_levels), "levels to process");

  po::positional_options_description p;
  p.add("infile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdstat v1.1 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: qdstat [options] querydata\n"
                 "       qdstat -i querydata [options]\n"
                 "       qdstat [options] < querydata\n"
                 "       cat querydata | qdstat [options]\n"
                 "\n"
                 "Calculate statistics on querydata values.\n"
                 "\n"
              << desc << std::endl;
    return false;
  }

  if (opt.count("infile") == 0)
    throw std::runtime_error("Excpecting input querydata as parameter 1");

  if (options.infile != "-" && !fs::exists(options.infile))
    throw std::runtime_error("Input file '" + options.infile + "' does not exist");

  // Parse the optional lists

  options.these_times = parse_times(opt_stamps);
  options.these_params = parse_params(opt_params);
  options.these_stations = parse_stations(opt_stations);
  options.these_levels = parse_levels(opt_levels);

  // Check invalid values

  if (options.bins < 2) throw std::runtime_error("Must have at least 2 bins");

  if (options.barsize < 10) throw std::runtime_error("Bar graph width must be at leats 10");

  // Check incompatible options

  if (options.all_times && !options.these_times.empty())
    throw std::runtime_error("Cannot use options -T and -t simultaneously");

  if (options.all_stations && !options.these_stations.empty())
    throw std::runtime_error("Cannot use options -W and -w simultaneously");

  if (options.all_levels && !options.these_levels.empty())
    throw std::runtime_error("Cannot use options -Z and -z simultaneously");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Statistics collector
 */
// ----------------------------------------------------------------------

class Stats
{
 public:
  Stats();
  void operator()(double value);
  static std::string header();
  std::string report() const;
  void param(FmiParameterName theParam) { itsParam = theParam; }
  const char* desc(double value) const;

 private:
  FmiParameterName itsParam;    // parameter name
  std::size_t itsCount;         // total count
  std::size_t itsValidCount;    // finite and not kFloatMissing
  std::size_t itsMissingCount;  // kFloatMissing
  std::size_t itsInfCount;      // -Inf or +Inf
  std::size_t itsNaNCount;      // NaN
  double itsSum;
  double itsMin;
  double itsMax;
  std::map<double, std::size_t> itsCounts;
};

Stats::Stats()
    : itsParam(kFmiBadParameter),
      itsCount(0),
      itsValidCount(0),
      itsMissingCount(0),
      itsInfCount(0),
      itsNaNCount(0),
      itsSum(std::numeric_limits<double>::quiet_NaN()),
      itsMin(std::numeric_limits<double>::quiet_NaN()),
      itsMax(std::numeric_limits<double>::quiet_NaN())
{
}

void Stats::operator()(double value)
{
  if (value == options.ignored_value) return;

  ++itsCount;
  if (value == kFloatMissing) ++itsMissingCount;
  if (std::isnan(value)) ++itsNaNCount;
  if (std::isinf(value)) ++itsInfCount;
  if (std::isfinite(value) && value != kFloatMissing)
  {
    if (itsValidCount == 0)
    {
      itsValidCount = 1;
      itsMin = value;
      itsMax = value;
      itsSum = value;
    }
    else
    {
      ++itsValidCount;
      itsMin = std::min(itsMin, value);
      itsMax = std::max(itsMax, value);
      itsSum += value;
    }
  }

  // Count only normal values
  if (options.distribution && std::isfinite(value) && value != kFloatMissing) ++itsCounts[value];
}

std::string Stats::header()
{
  std::ostringstream out;
  out << std::setw(column_width + 1) << std::right << "Min" << std::setw(column_width + 1)
      << std::right << "Mean" << std::setw(column_width + 1) << std::right << "Max"
      << std::setw(column_width + 1) << std::right << "Count" << std::setw(column_width + 1)
      << std::right << "Valid" << std::setw(column_width + 1) << std::right << "Miss"
      << std::setw(column_width) << std::right << "NaN" << std::setw(column_width) << std::right
      << "Inf";
  return out.str();
}

const char* Stats::desc(double value) const
{
  switch (itsParam)
  {
    case kFmiPrecipitationForm:
    case kFmiPotentialPrecipitationForm:
    {
      switch (static_cast<int>(value))
      {
        case 0:
          return " drizzle";
        case 1:
          return " water";
        case 2:
          return " sleet";
        case 3:
          return " snow";
        case 4:
          return " freezing drizzle";
        case 5:
          return " freezing rain";
        case 6:
          return " hail";
        case 7:
          return " snow grains";
        case 8:
          return " ice pellets";
        default:
          return "";
      }
    }
    case kFmiPrecipitationType:
    {
      switch (static_cast<int>(value))
      {
        case 1:
          return " large scale";
        case 2:
          return " convetive";
        default:
          return "";
      }
    }
    case kFmiPotentialPrecipitationType:
    {
      switch (static_cast<int>(value))
      {
        case 0:
          return " large scale or convective";
        case 1:
          return " large scale";
        case 2:
          return " convetive";
        default:
          return "";
      }
    }
    case kFmiFogIntensity:
    {
      switch (static_cast<int>(value))
      {
        case 0:
          return " none";
        case 1:
          return " moderate fog";
        case 2:
          return " dense fog";
        case 3:
          return " sandstorm";
        default:
          return "";
      }
    }
    case kFmiWeatherSymbol3:
    {
      switch (static_cast<int>(value))
      {
        case 1:
          return " sunny";
        case 2:
          return " partly cloudy";
        case 3:
          return " cloudy";
        case 21:
          return " light showers";
        case 22:
          return " showers";
        case 23:
          return " heavy showers";
        case 31:
          return " light rain";
        case 32:
          return " rain";
        case 33:
          return " heavy rain";
        case 41:
          return " light snow showers";
        case 42:
          return " snow showers";
        case 43:
          return " heavy snow showers";
        case 51:
          return " light snowfall";
        case 52:
          return " snowfall";
        case 53:
          return " heavy snowfall";
        case 61:
          return " thundershowers";
        case 62:
          return " heavy thundershowers";
        case 63:
          return " thunder";
        case 64:
          return " heavy thunder";
        case 71:
          return " light sleet showers";
        case 72:
          return " sleet showers";
        case 73:
          return " heavy sleet showers";
        case 81:
          return " light sleet rain";
        case 82:
          return " sleet rain";
        case 83:
          return " heavy sleet rain";
        case 91:
          return " fog";
        case 92:
          return " fog";
        default:
          return " ";
      }
    }
    case kFmiPresentWeather:
    {
      // http://badc.nerc.ac.uk/data/surface/code.html
      switch (static_cast<int>(value))
      {
        case 0:
          return " clear";
        case 1:
          return " dissolving cloudiness";
        case 2:
          return " unchanged cloudiness";
        case 3:
          return " increasing cloudiness";
        case 4:
          return " smoke";
        case 5:
          return " haze";
        case 6:
          return " dust not raised by wind";
        case 7:
          return " dust raised by wind";
        case 8:
          return " dust or sand whirls";
        case 9:
          return " duststorm or sandstorm";
        case 10:
          return " mist";
        case 11:
          return " patches of shallow fog";
        case 12:
          return " shallow fog";
        case 13:
          return " thunder nearby";
        case 14:
          return " precipitation sighting in air";
        case 15:
          return " distant precipitation sighting";
        case 16:
          return " nearby precipitation sighting";
        case 17:
          return " thunderstorm without precipitation";
        case 18:
          return " squalls";
        case 19:
          return " funnel clouds";
        case 20:
          return " drizzle";
        case 21:
          return " rain";
        case 22:
          return " snow";
        case 23:
          return " sleet";
        case 24:
          return " freezing drizzle or rain";
        case 25:
          return " rain showers";
        case 26:
          return " snow showers";
        case 27:
          return " hail";
        case 28:
          return " fog";
        case 29:
          return " thunderstorm";
        case 30:
          return " decreasing moderate sandstorm";
        case 31:
          return " moderate sandstorm";
        case 32:
          return " increasing moderate sandstrom";
        case 33:
          return " decreasing severe sandstrom";
        case 34:
          return " severe sandstorm";
        case 35:
          return " increasing severe sandstorm";
        case 36:
          return " drifting snow";
        case 37:
          return " heavy drifting snow";
        case 38:
          return " blowing snow";
        case 39:
          return " heavy blowing snow";
        case 40:
          return " distant fog";
        case 41:
          return " fog patches";
        case 42:
          return " thinning fog, sky visible";
        case 43:
          return " thinning fog, sky invisible";
        case 44:
          return " fog, sky visible";
        case 45:
          return " fog, sky invisible";
        case 46:
          return " thickening fog, sky visible";
        case 47:
          return " thickening fog, sky invisible";
        case 48:
          return " fog depositing rime, sky visible";
        case 49:
          return " fog repositing rime, sky invisible";
        case 50:
          return " slight intermittent drizzle";
        case 51:
          return " slight continuous drizzle";
        case 52:
          return " moderate intermittent drizzle";
        case 53:
          return " moderate continuous drizzle";
        case 54:
          return " heavy intermittent drizzle";
        case 55:
          return " heavy continuous drizzle";
        case 56:
          return " slight freezing drizzle";
        case 57:
          return " dense freezing drizzle";
        case 58:
          return " slight rain and drizzle";
        case 59:
          return " moderate or heavy rain and drizzle";
        case 60:
          return " slight intermittent rain";
        case 61:
          return " slight continuous rain";
        case 62:
          return " moderate intermittent rain";
        case 63:
          return " moderate continous rain";
        case 64:
          return " heavy intermittent rain";
        case 65:
          return " heavy continuous rain";
        case 66:
          return " slight freezing rain";
        case 67:
          return " moderate or heavy freezing rain";
        case 68:
          return " slight rain or drizzle and snow";
        case 69:
          return " moderate or heavy rain or drizzle and snow";
        case 70:
          return " slight intermittent fall of snowflakes";
        case 71:
          return " slight continuous fall of snowflakes";
        case 72:
          return " moderate intermittent fall of snowflakes";
        case 73:
          return " moderate continuous fall of snowflakes";
        case 74:
          return " heavy intermittent fall of snowflakes";
        case 75:
          return " heavy continuous fall of snowflakes";
        case 76:
          return " diamond dust";
        case 77:
          return " snow grains";
        case 78:
          return " isolated star-like snow crystals";
        case 79:
          return " ice pellets";
        case 80:
          return " slight rain showers";
        case 81:
          return " moderate or heavy rain showers";
        case 82:
          return " violent rain showers";
        case 83:
          return " slight showers of rain and snow";
        case 84:
          return " moderate or heavy showers of rain and snow";
        case 85:
          return " slight snow showers";
        case 86:
          return " moderate or heavy snow showers";
        case 87:
          return " slight hail showers";
        case 88:
          return " moderate or heavy hail showers";
        case 89:
          return " slight hail showers, no thunder";
        case 90:
          return " moderate or heavy hail showers, no thunder";
        case 91:
          return " slight rain, thunderstorm passed";
        case 92:
          return " moderate or heavy rain, thunderstorm passed";
        case 93:
          return " slight sleet, thunderstorm passed";
        case 94:
          return " moderate or heavy sleet, thunderstorm passed";
        case 95:
          return " slight thunderstorm";
        case 96:
          return " thunderstorm with hail";
        case 97:
          return " heavy thunderstorm";
        case 98:
          return " thunderstorm with dust/sandstorm";
        case 99:
          return " heavy thunderstorm with hail";
        default:
          return "";
      }
    }
    case kFmiRoadConditionSeverity:  // croad
    {
      switch (static_cast<int>(value))
      {
        case 1:
          return "normal";
        case 2:
          return "bad";
        case 3:
          return "very bad";
        default:
          return "";
      }
    }
    case kFmiRoadNotification:  // roadnotification
    {
      switch (static_cast<int>(value))
      {
        case 0:
          return "normal";
        case 1:
          return "frost";
        case 2:
          return "partly icy";
        case 3:
          return "icy";
        default:
          return "";
      }
    }
    case kFmiRoadWarning:  // roadwarning
    {
      switch (static_cast<int>(value))
      {
        case 1:
          return "frost";
        case 2:
          return "icy";
        case 3:
          return "snowy road surface";
        case 4:
          return "snow on cold";
        case 5:
          return "wind warning";
        case 6:
          return "whirl";
        case 7:
          return "heavy snowfall";
        case 8:
          return "quick change";
        case 9:
          return "sleetfall on icy";
        case 10:
          return "water on icy";
        case 11:
          return "supercooled rain";
        case 21:
          return "hydroplaning";
        default:
          return "";
      }
    }
    case kFmiRoadCondition:             // wroad
    case kFmiRoadConditionAlternative:  // wroad
    {
      switch (static_cast<int>(value))
      {
        case 1:
          return "dry";
        case 2:
          return "moist";
        case 3:
          return "wet";
        case 4:
          return "slush";
        case 5:
          return "frost";
        case 6:
          return "partly icy";
        case 7:
          return "icy";
        case 8:
          return "snow";
        default:
          return "";
      }
    }
    case kFmiRoadMaintenanceMeasure:
    {
      switch (static_cast<int>(value))
      {
        case 0:
          return "no action";
        case 1:
          return "plowing";
        case 2:
          return "salting";
        case 3:
          return "plowing and salting";
        default:
          return "";
      }
    }
    case kFmiSmartSymbol:
    {
      switch (static_cast<int>(value))
      {
        case 10000000:
          return "clear";
        case 10000020:
          return "mostly clear";
        case 10000030:
          return "partly cloudy";
        case 10000060:
          return "mostly cloudy";
        case 10000080:
          return "overcast";
        case 10000100:
          return "fog";
        case 11000000:
          return "isolated thundershowers";
        case 11000060:
          return "scattered thundershowers";
        case 11000080:
          return "thundershowers";
        case 10121000:
          return "isolated showers";
        case 10121060:
          return "scattered showers";
        case 10121080:
          return "showers";
        case 10401000:
          return "freezing drizzle";
        case 10501000:
          return "freezing rain";
        case 10001000:
          return "drizzle";
        case 10111000:
          return "periods of light rain";
        case 10111060:
          return "periods of light rain";
        case 10111080:
          return "light rain";
        case 10113000:
          return "periods of moderate rain";
        case 10113060:
          return "periods of moderate rain";
        case 10113080:
          return "moderate rain";
        case 10116000:
          return "periods of heavy rain";
        case 10116060:
          return "periods of heavy rain";
        case 10116080:
          return "heavy rain";
        case 10201000:
          return "isolated light sleet showers";
        case 10201060:
          return "scattered light sleet showers";
        case 10201080:
          return "light sleet";
        case 10203000:
          return "isolated moderate sleet showers";
        case 10203060:
          return "scattered moderate sleet showers";
        case 10203080:
          return "moderate sleet";
        case 10204000:
          return "isolated heavy sleet showers";
        case 10204060:
          return "scattered heavy sleet showers";
        case 10204080:
          return "heavy sleet";
        case 10301000:
          return "isolated light snow showers";
        case 10301060:
          return "scattered light snow showers";
        case 10301080:
          return "light snowfall";
        case 10303000:
          return "isolated moderate snow showers";
        case 10303060:
          return "scattered moderate snow showers";
        case 10303080:
          return "moderate snowfall";
        case 10304000:
          return "isolated heavy snow showers";
        case 10304060:
          return "scattered heavy snow showers";
        case 10304080:
          return "heavy snowfall";
        case 10601000:
          return "isolated hail showers";
        case 10601060:
          return "scattered hail showers";
        case 10601080:
          return "hail showers";
        default:
          return "";
      }
    }
    default:
      return "";
  }
}

// Select nice step size for binning

void autotick(double theRange, std::size_t maxbins, double& tick, int& precision)
{
  if (theRange == 0)
  {
    tick = 0;
    precision = 0;
  }
  else
  {
    double xx = theRange / maxbins;
    double xlog = log10(xx);
    int ilog = static_cast<int>(xlog);
    if (xlog < 0) --ilog;
    precision = -ilog;

    double pwr = pow(10, ilog);
    double frac = xx / pwr;
    if (frac <= 2)
      tick = 2 * pwr;
    else if (frac <= 5)
      tick = 5 * pwr;
    else
    {
      tick = 10 * pwr;
      --precision;
    }
    // Handle tick >= 10
    precision = std::max(0, precision);
  }
}

// Autoscale binning to get nice min/max values
void autoscale(const double theMin,
               const double theMax,
               std::size_t maxbins,
               double& newmin,
               double& newmax,
               double& tick,
               int& precision)
{
  newmin = theMin;
  newmax = theMax;

  if (theMin == theMax)
  {
    newmin = std::floor(theMin);
    if (newmin == theMin) newmin -= 1;
    newmax = std::ceil(theMax);
    if (newmax == theMax) newmax += 1;
    tick = 1;
    precision = 0;
  }
  else
  {
    autotick(newmax - newmin, maxbins, tick, precision);
    newmin = tick * std::floor(newmin / tick);
    newmax = tick * std::ceil(newmax / tick);
  }
}

// Estimate precision for a parameter with a small number of different values
int estimate_precision(const std::map<double, std::size_t>& theCounts)
{
  int precision = 0;
  for (const auto& value_count : theCounts)
  {
    double value = value_count.first;
    if (std::floor(value) != value)
    {
      // We assume some parameters may have values with steps of 0.1 or 0.01, but nothing
      // smaller than that. This assumption works also for Precipitation1h when packed
      // into a WeatherAndCloudiness parameter - all the printed values are unique.
      // Checking whether the accuracy is 10**N for some negative N does not seem to
      // be worth the trouble.
      precision = 2;
    }
  }

  return precision;
}

std::string Stats::report() const
{
  std::ostringstream out;

  double mean = std::numeric_limits<double>::quiet_NaN();
  if (itsValidCount > 0) mean = itsSum / itsValidCount;

  if (options.percentages)
  {
    out << std::fixed << std::setprecision(2) << ' ' << std::setw(column_width) << std::right
        << itsMin << std::setprecision(2) << ' ' << std::setw(column_width) << std::right << mean
        << std::setprecision(2) << ' ' << std::setw(column_width) << std::right << itsMax
        << std::setw(column_width + 1) << std::right << itsCount << std::setw(column_width + 1)
        << std::right << 100.0 * itsValidCount / itsCount << std::setw(column_width + 1)
        << std::right << 100.0 * itsMissingCount / itsCount << std::setw(column_width) << std::right
        << 100.0 * itsNaNCount / itsCount << std::setw(column_width) << std::right
        << 100.0 * itsInfCount / itsCount;
  }
  else
  {
    out << std::fixed << std::setprecision(2) << ' ' << std::setw(column_width) << std::right
        << itsMin << std::setprecision(2) << ' ' << std::setw(column_width) << std::right << mean
        << std::setprecision(2) << ' ' << std::setw(column_width) << std::right << itsMax
        << std::setw(column_width + 1) << std::right << itsCount << std::setw(column_width + 1)
        << std::right << itsValidCount << std::setw(column_width + 1) << std::right
        << itsMissingCount << std::setw(column_width) << std::right << itsNaNCount
        << std::setw(column_width) << std::right << itsInfCount;
  }

  if (!itsCounts.empty())
  {
    out << std::endl << std::endl;

    // We need special handling if the number of unique values is smallish. Usually this
    // happens only for enumerated values. 50 is enough for example to cover wind direction
    // in steps of 10 degrees.

    const int max_parameter_values = 50;

    if (itsCounts.size() < options.bins || itsCounts.size() < max_parameter_values)
    {
      const int precision = estimate_precision(itsCounts);

      for (const auto& value_count : itsCounts)
      {
        double value = value_count.first;
        std::size_t count = value_count.second;

        if (options.percentages)
        {
          out << std::fixed << std::setprecision(precision) << std::setw(10) << std::right << value
              << std::setw(column_width + 1) << std::right << 100.0 * count / itsValidCount;
        }
        else
        {
          out << std::fixed << std::setprecision(precision) << std::setw(10) << std::right << value
              << std::setw(column_width + 1) << std::right << count;
        }

        int w = static_cast<int>(std::round(options.barsize * count / itsValidCount));
        out << std::setw(5) << std::right << '|' << std::string(w, '=')
            << std::setw(options.barsize - w) << std::right << '|' << desc(value) << std::endl;
      }
    }
    else
    {
      double binmin, binmax, tick;
      int precision;
      autoscale(itsMin, itsMax, options.bins, binmin, binmax, tick, precision);

      for (std::size_t i = 0;; i++)
      {
        double minvalue = binmin + i * tick;
        if (minvalue >= itsMax) break;
        double maxvalue = minvalue + tick;
        std::size_t count = 0;
        for (const auto& value_count : itsCounts)
        {
          double value = value_count.first;
          // The max value must be counted into the last bin
          if ((value >= minvalue && value < maxvalue))
            count += value_count.second;
          else if (value == itsMax && value == maxvalue)
            count += value_count.second;
        }

        std::ostringstream range;
        range << std::fixed << std::setprecision(precision) << minvalue << "..." << maxvalue;

        out << std::setw(20) << std::right << range.str();
        if (options.percentages)
          out << std::setw(column_width + 1) << std::right << 100.0 * count / itsValidCount;
        else
          out << std::setw(column_width + 1) << std::right << count;

        int w = static_cast<int>(std::round(options.barsize * count / itsValidCount));
        out << std::setw(5) << std::right << '|' << std::string(w, '=')
            << std::setw(options.barsize - w) << std::right << '|' << std::endl;
      }
    }
  }

  return out.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Print station information
 */
// ----------------------------------------------------------------------

std::string station_header(NFmiFastQueryInfo& qi)
{
  const NFmiLocation* loc = qi.Location();
  std::ostringstream out;
  out << loc->GetIdent() << ':' << loc->GetName().CharPtr();
  return out.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish maximum parameter name width
 */
// ----------------------------------------------------------------------

std::size_t max_param_width(NFmiFastQueryInfo& qi)
{
  std::size_t widest = 0;
  for (auto p : options.these_params)
  {
    qi.Param(p);
    std::string name = converter.ToString(qi.Param().GetParam()->GetIdent());
    if (name.empty()) name = Fmi::to_string(qi.Param().GetParam()->GetIdent());
    widest = std::max(widest, name.size());
  }
  return widest;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish maximum station name width
 */
// ----------------------------------------------------------------------

std::size_t max_station_width(NFmiFastQueryInfo& qi)
{
  std::size_t widest = 0;
  for (int wmo : options.these_stations)
  {
    qi.Location(wmo);
    widest = std::max(widest, station_header(qi).size());
  }
  return widest;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the desired level
 */
// ----------------------------------------------------------------------

void set_level(NFmiFastQueryInfo& qi, float levelvalue)
{
  for (qi.ResetLevel(); qi.NextLevel();)
  {
    if (qi.Level()->LevelValue() == levelvalue) return;
  }
  throw std::runtime_error("Level value " + Fmi::to_string(levelvalue) +
                           " not available in the data");
}

// ----------------------------------------------------------------------
/*!
 * \brief Analysis over all locations and times
 */
// ----------------------------------------------------------------------

void stat_locations_times(NFmiFastQueryInfo& qi)
{
  int param_width = max_param_width(qi);

  if (options.these_levels.empty())
    std::cout << std::setw(param_width) << std::right << "Parameter" << Stats::header()
              << std::endl;
  else
    std::cout << std::setw(param_width) << std::right << "Parameter" << std::setw(column_width)
              << "Level" << Stats::header() << std::endl;

  for (auto p : options.these_params)
  {
    qi.Param(p);
    std::string name = converter.ToString(qi.Param().GetParam()->GetIdent());
    if (name.empty()) name = Fmi::to_string(qi.Param().GetParam()->GetIdent());

    if (options.these_levels.empty())
    {
      Stats stats;
      stats.param(p);

      for (qi.ResetLocation(); qi.NextLocation();)
        for (qi.ResetLevel(); qi.NextLevel();)
          for (qi.ResetTime(); qi.NextTime();)
            stats(qi.FloatValue());
      std::cout << std::setw(param_width) << name << stats.report() << std::endl;
    }
    else
    {
      for (auto levelvalue : options.these_levels)
      {
        set_level(qi, levelvalue);

        Stats stats;
        stats.param(p);

        for (qi.ResetLocation(); qi.NextLocation();)
          for (qi.ResetTime(); qi.NextTime();)
            stats(qi.FloatValue());
        std::cout << std::setw(param_width) << name << std::setw(column_width) << levelvalue
                  << stats.report() << std::endl;
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Analysis over all locations for selected times
 */
// ----------------------------------------------------------------------

void stat_locations_these_times(NFmiFastQueryInfo& qi)
{
  int param_width = max_param_width(qi);

  for (auto p : options.these_params)
  {
    qi.Param(p);
    std::string name = converter.ToString(qi.Param().GetParam()->GetIdent());
    if (name.empty()) name = Fmi::to_string(qi.Param().GetParam()->GetIdent());

    if (options.these_levels.empty())
      std::cout << std::setw(param_width) << std::right << "Parameter" << std::setw(18)
                << std::right << "Time" << Stats::header() << std::endl;
    else
      std::cout << std::setw(param_width) << std::right << "Parameter" << std::setw(column_width)
                << "Level" << std::setw(18) << std::right << "Time" << Stats::header() << std::endl;

    for (const auto& pt : options.these_times)
    {
      NFmiMetTime t = pt;
      qi.Time(t);

      if (options.these_levels.empty())
      {
        Stats stats;
        stats.param(p);

        for (qi.ResetLocation(); qi.NextLocation();)
          for (qi.ResetLevel(); qi.NextLevel();)
            stats(qi.FloatValue());

        std::cout << std::setw(param_width) << std::right << name << std::setw(18) << std::right
                  << to_iso_string(t.PosixTime()) << stats.report() << std::endl;
      }
      else
      {
        for (auto levelvalue : options.these_levels)
        {
          set_level(qi, levelvalue);

          Stats stats;
          stats.param(p);

          for (qi.ResetLocation(); qi.NextLocation();)
            stats(qi.FloatValue());

          std::cout << std::setw(param_width) << std::right << name << std::setw(column_width)
                    << levelvalue << std::setw(18) << std::right << to_iso_string(t.PosixTime())
                    << stats.report() << std::endl;
        }
      }
    }
    std::cout << std::endl;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Analysis of specific stations for selected times
 */
// ----------------------------------------------------------------------

void stat_these_stations_these_times(NFmiFastQueryInfo& qi)
{
  for (auto p : options.these_params)
  {
    std::cout << std::setw(20) << "" << Stats::header() << std::endl;

    qi.Param(p);
    std::string name = converter.ToString(qi.Param().GetParam()->GetIdent());
    if (name.empty()) name = Fmi::to_string(qi.Param().GetParam()->GetIdent());
    std::cout << name << std::endl;

    for (int wmo : options.these_stations)
    {
      qi.Location(wmo);

      std::cout << "  " << station_header(qi) << std::endl;

      for (const auto& pt : options.these_times)
      {
        NFmiMetTime t = pt;
        qi.Time(t);

        Stats stats;
        stats.param(p);
        for (qi.ResetLevel(); qi.NextLevel();)
          stats(qi.FloatValue());
        std::cout << "    " << to_iso_string(t.PosixTime()) << ' ' << stats.report() << std::endl;
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Analysis of specific stations over all times
 */
// ----------------------------------------------------------------------

void stat_these_stations_times(NFmiFastQueryInfo& qi)
{
  int station_width = max_station_width(qi);
  int param_width = max_param_width(qi);

  std::cout << std::setw(station_width) << std::right << "Station" << std::setw(param_width + 1)
            << std::right << "Parameter" << Stats::header() << std::endl;

  for (auto p : options.these_params)
  {
    qi.Param(p);
    std::string name = converter.ToString(qi.Param().GetParam()->GetIdent());
    if (name.empty()) name = Fmi::to_string(qi.Param().GetParam()->GetIdent());

    for (int wmo : options.these_stations)
    {
      qi.Location(wmo);

      Stats stats;
      stats.param(p);

      for (qi.ResetLevel(); qi.NextLevel();)
        for (qi.ResetTime(); qi.NextTime();)
          stats(qi.FloatValue());
      std::cout << std::setw(station_width) << std::right << station_header(qi)
                << std::setw(param_width + 1) << std::right << name << stats.report() << std::endl;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv)) return 0;

  NFmiQueryData qd(options.infile);
  NFmiFastQueryInfo qi(&qd);

  // Some initial checks

  if (qi.IsGrid())
    if (options.all_stations || !options.these_stations.empty())
      throw std::runtime_error("Cannot request analysis of stations for grid data");

  // If no parameters were requested, analyze all

  if (options.these_params.empty())
  {
    for (qi.ResetParam(); qi.NextParam(false);)
      options.these_params.insert(FmiParameterName(qi.Param().GetParam()->GetIdent()));
  }
  else
  {
    // Otherwise validate the parameters first
    for (auto p : options.these_params)
    {
      if (!qi.Param(p))
        throw std::runtime_error("Requested parameter not available in the data: '" +
                                 converter.ToString(p) + "'");
    }
  }

  // Validate levels

  for (auto levelvalue : options.these_levels)
    set_level(qi, levelvalue);

  // If all times were requested, update the specific list

  if (options.all_times)
    for (qi.ResetTime(); qi.NextTime();)
      options.these_times.insert(qi.ValidTime().PosixTime());

  // If all stations were requested, update the specific list

  if (options.all_stations)
    for (qi.ResetLocation(); qi.NextLocation();)
      options.these_stations.insert(qi.Location()->GetIdent());

  // If all levels were requested, update the specific list

  if (options.all_levels)
    for (qi.ResetLevel(); qi.NextLevel();)
      options.these_levels.insert(qi.Level()->LevelValue());

  // We can never summarize parameters collectively, there is always a loop

  if (!options.these_stations.empty())
  {
    if (!options.these_levels.empty())
      throw std::runtime_error("Levels options not supported for station data");

    if (!options.these_times.empty())
      stat_these_stations_these_times(qi);
    else
      stat_these_stations_times(qi);
  }
  else
  {
    if (!options.these_times.empty())
      stat_locations_these_times(qi);
    else
      stat_locations_times(qi);
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char* argv[]) try
{
  return run(argc, argv);
}
catch (const std::exception& e)
{
  std::cerr << "Error: " << e.what() << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Error: Caught an unknown exception" << std::endl;
  return 1;
}
