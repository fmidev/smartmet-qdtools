// ======================================================================
/*!
 * \brief Ash advisory to querydata conversion for MetOffice CSV files
 *
 * The program takes as input a directory containing CSV files.
 * If a model run time is given as an option, the CSV files
 * for that run time are processed. Otherwise the directory
 * is scanned for all CSV advisory files, and the newest model
 * run time will be processed. The model run results are converted
 * into a single querydata with 3 different flight levels. The
 * data is valid for a range of flight levels. The top flight
 * level will be used to identify the data.
 *
 * The current custom is to output data for 3 different concentration
 * levels only. This would indicate we should use 4 different enumerations
 * for the concentration: none, low, medium and high. However, the
 * file name format enables producing output for an arbitrary concentration
 * (with an accuracy of 2 decimals). In case some unusual concentrations
 * are produced, we should thus make the ash concentration a regular
 * real number variable, even though it will typically only have
 * 4 different values.
 *
 */
// ======================================================================

#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <macgyver/TimeFormatter.h>

#include <newbase/NFmiArea.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiIndexMaskTools.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiLevelType.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiSvgPath.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = boost::filesystem;

// File name structure

const int origintime_position_in_filename = 30;
const int validtime_position_in_filename = 44;
const int level_position_in_filename = 19;
const int concentration_position_in_filename = 16;

const char* concentration_regex =
    "V..06_ASH_CONC_C.\\d{2}FL\\d{3}-\\d{3}DT\\d{12}VT\\d{12}IT\\d{6}_T\\d{3}.CSV";
const char* boundary_regex =
    "V..01_ASH_CONC_C.\\d{2}FL\\d{3}-\\d{3}DT\\d{12}VT\\d{12}IT\\d{6}_T\\d{3}.CSV";

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool verbose;                         // -v --verbose
  bool boundaries;                      // -b --boundary
  boost::posix_time::ptime origintime;  // -t --time
  std::string projection;               // -P --projection
  std::string indir;                    // -i --indir
  std::string outfile;                  // -o --outfile
  std::string producername;             // --producername
  long producernumber;                  // --producernumber
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options()
    : verbose(false),
      boundaries(false),
      origintime(),
      projection("stereographic,10,90,60:-19.22,25,79.7,57:265,205")  // FMI Met Editor projection
      ,
      indir("."),
      outfile("-"),
      producername("EGRR_VAAC"),
      producernumber(120)
{
}
// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 *
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char* argv[], Options& options)
{
  namespace po = boost::program_options;

  std::string producerinfo;
  std::string timeinfo;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "version,V", "display version number")("time,t", po::value(&timeinfo), "model run time")(
      "projection,P", po::value(&options.projection), "projection")(
      "indir,i", po::value(&options.indir), "input CSV directory")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "boundary,b",
      po::bool_switch(&options.boundaries),
      "extract boundary instead of concentrations")(
      "producer,p", po::value(&producerinfo), "producer number,name")(
      "producernumber", po::value(&options.producernumber), "producer number")(
      "producername", po::value(&options.producername), "producer name");

  po::positional_options_description p;
  p.add("indir", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "ashtoqd v2.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: ashtoqd [options] indir outfile" << std::endl
              << std::endl
              << "Converts MetOffice CSV ash advisories to querydata." << std::endl
              << "Note that you can extract only concentrations or" << std::endl
              << "boundaries since they seem to always have different" << std::endl
              << "model run times." << std::endl
              << std::endl
              << desc << std::endl;
    return false;
  }

  if (opt.count("indir") == 0) throw std::runtime_error("Expecting input directory as parameter 1");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.indir))
    throw std::runtime_error("Input directory '" + options.indir + "' does not exist");

  if (!fs::is_directory(options.indir))
    throw std::runtime_error("Input source is not a directory: '" + options.indir + "'");

  // Handle the alternative ways to define the producer

  if (!producerinfo.empty())
  {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, producerinfo, boost::algorithm::is_any_of(","));
    if (parts.size() != 2)
      throw std::runtime_error("Option --producer expects a comma separated number,name argument");

    options.producernumber = Fmi::stol(parts[0]);
    options.producername = parts[1];
  }

  // Handle the optional model run time

  if (!timeinfo.empty())
  {
    options.origintime = Fmi::TimeParser::parse(timeinfo);
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find ash concentration advisories
 */
// ----------------------------------------------------------------------

std::list<fs::path> find_ash_files(const char* re)
{
  // First collect all files

  fs::path p(options.indir);

  if (!fs::is_directory(p)) throw std::runtime_error("Not a directory: '" + options.indir + "'");

  std::list<fs::path> files;
  copy(fs::directory_iterator(p), fs::directory_iterator(), back_inserter(files));

  // Extract the ones with the correct pattern

  boost::regex expression(re, boost::regex::perl | boost::regex::icase);
  std::list<fs::path> ashfiles;
  BOOST_FOREACH (const fs::path& file, files)
  {
    if (boost::regex_match(file.filename().string(), expression)) ashfiles.push_back(file);
  }

  return ashfiles;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find available model run times. The returned list is sorted.
 */
// ----------------------------------------------------------------------

std::list<boost::posix_time::ptime> find_model_run_times(const std::list<fs::path>& files,
                                                         int origintime_position)
{
  std::set<std::string> stamps;
  BOOST_FOREACH (const fs::path& file, files)
  {
    std::string stamp = file.filename().string().substr(origintime_position, 12);
    stamps.insert(stamp);
  }

  std::list<boost::posix_time::ptime> times;
  BOOST_FOREACH (const std::string& stamp, stamps)
  {
    boost::posix_time::ptime t = Fmi::TimeParser::parse(stamp);
    times.push_back(t);
  }

  return times;
}

// ----------------------------------------------------------------------
/*!
 * \brief Select model run time
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime select_model_run_time(const std::list<boost::posix_time::ptime>& times)
{
  if (options.origintime.is_not_a_date_time())
  {
    // select newest run, which is assumed to be the last one in the list
    return *(--times.end());
  }
  else
  {
    std::list<boost::posix_time::ptime>::const_iterator pos =
        find(times.begin(), times.end(), options.origintime);
    if (pos == times.end())
      throw std::runtime_error("The selected origintime " + to_simple_string(options.origintime) +
                               " is not available in directory '" + options.indir + "'");

    return options.origintime;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the ash files with the given run time
 */
// ----------------------------------------------------------------------

std::list<fs::path> select_ash_files(const std::list<fs::path>& files,
                                     const boost::posix_time::ptime& tmodel,
                                     int origintime_position)
{
  std::list<fs::path> ret;

  boost::shared_ptr<Fmi::TimeFormatter> formatter(Fmi::TimeFormatter::create("timestamp"));

  std::string selected_stamp = formatter->format(tmodel);

  BOOST_FOREACH (const fs::path& file, files)
  {
    if (selected_stamp == file.filename().string().substr(origintime_position, 12))
      ret.push_back(file);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the parameter descriptor
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor create_pdesc()
{
  NFmiParamBag pbag;

  // Pictures come up better when linear interpolation is not on.
  // This is due to the data only having4 discrete values.
  // p.InterpolationMethod(kLinearly);

  if (!options.boundaries)
  {
    NFmiParam p(kFmiAshConcentration, "AshConcentration");
    p.Precision("%.8f");
    pbag.Add(NFmiDataIdent(p));
  }
  else
  {
    NFmiParam p(kFmiAshOnOff, "AshOnOff");
    p.Precision("%.0f");
    pbag.Add(NFmiDataIdent(p));
  }

  return NFmiParamDescriptor(pbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct NFmiMetTime from posix time
 */
// ----------------------------------------------------------------------

NFmiMetTime tomettime(const boost::posix_time::ptime& t)
{
  return NFmiMetTime(t.date().year(),
                     t.date().month(),
                     t.date().day(),
                     t.time_of_day().hours(),
                     t.time_of_day().minutes(),
                     t.time_of_day().seconds(),
                     1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the time descriptor based on the file names
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(const std::list<fs::path>& files,
                                const boost::posix_time::ptime& origintime,
                                int validtime_position)
{
  // Collect all unique times in sorted order
  std::set<std::string> stamps;
  BOOST_FOREACH (const fs::path& file, files)
  {
    std::string stamp = file.filename().string().substr(validtime_position, 12);
    stamps.insert(stamp);
  }

  // Build a time list
  NFmiTimeList tlist;
  BOOST_FOREACH (const std::string& stamp, stamps)
  {
    boost::posix_time::ptime t = Fmi::TimeParser::parse(stamp);
    tlist.Add(new NFmiMetTime(tomettime(t)));
  }

  // Done
  return NFmiTimeDescriptor(tomettime(origintime), tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the horizontal place descriptor
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc()
{
  boost::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(options.projection);
  int width = static_cast<int>(round(area->XYArea(area.get()).Width()));
  int height = static_cast<int>(round(area->XYArea(area.get()).Height()));

  NFmiGrid grid(area->Clone(), width, height);
  return NFmiHPlaceDescriptor(grid);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the vertical place descriptor
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc(const std::list<fs::path>& files)
{
  if (!options.boundaries)
  {
    // Collect unique flight level descriptions. Format: FLaaa-bbb

    std::set<std::string> levels;
    BOOST_FOREACH (const fs::path& file, files)
    {
      std::string level = file.filename().string().substr(level_position_in_filename, 9);
      levels.insert(level);
    }

    // Create the descriptor. We use the top flight level as the actual value

    FmiLevelType leveltype = kFmiFlightLevel;
    NFmiLevelBag lbag;
    BOOST_FOREACH (const std::string& levelname, levels)
    {
      double levelvalue = boost::lexical_cast<double>(levelname.substr(6, 3));
      NFmiLevel l(leveltype, levelname, levelvalue);
      lbag.AddLevel(l);
    }
    return NFmiVPlaceDescriptor(lbag);
  }

  else  // !!options.boundaries
  {
    FmiLevelType leveltype = kFmiFlightLevel;
    NFmiLevelBag lbag;
    lbag.AddLevel(NFmiLevel(leveltype, "SFC/FL200", 200));
    lbag.AddLevel(NFmiLevel(leveltype, "FL200/FL350", 350));
    lbag.AddLevel(NFmiLevel(leveltype, "FL350/FL550", 550));
    return NFmiVPlaceDescriptor(lbag);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a coordinate string to a number
 */
// ----------------------------------------------------------------------

double convert_coordinate(const std::string& value)
{
  if (value.size() == 6)
  {
    double degrees = Fmi::stod(value.substr(0, 2));
    double minutes = Fmi::stod(value.substr(2, 2));
    double seconds = Fmi::stod(value.substr(4));
    return degrees + (minutes + seconds / 60) / 60;
  }
  else if (value.size() == 7)
  {
    double degrees = Fmi::stod(value.substr(0, 3));
    double minutes = Fmi::stod(value.substr(3, 2));
    double seconds = Fmi::stod(value.substr(5));
    return degrees + (minutes + seconds / 60) / 60;
  }
  else
    throw std::runtime_error("Invalid coordinate value '" + value + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the coordinate from an advisory line
 *
 * The syntax has been verified
 */
// ----------------------------------------------------------------------

std::pair<double, double> extract_coordinate(const std::string& line)
{
  int comma_position = line.find(',');

  std::string ystring = line.substr(1, comma_position - 1);
  std::string xstring = line.substr(comma_position + 2);

  double lon = convert_coordinate(xstring);
  double lat = convert_coordinate(ystring);

  if (line[0] == 'S') lat = -lat;

  if (line[comma_position + 1] == 'W') lon = -lon;

  return std::make_pair(lon, lat);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the ash cloud polygon from the given file
 *
 * Polygons start with POLY. If there are none, there is a NO POLYS line
 */
// ----------------------------------------------------------------------

NFmiSvgPath read_ash_concentration_polygon(const fs::path& file)
{
  NFmiSvgPath path;

  std::ifstream input(file.c_str());

  if (!input)
    throw std::runtime_error("Failed to open '" + file.filename().string() + "' for reading");

  bool polygons_started = false;
  bool moveto = true;
  std::string firstpoint;

  // I encountered an example with 7 digits, implying 3 digits for the seconds part
  boost::regex expression("[NS]\\d{6,},[WE]\\d{6,}");

  for (std::string line; getline(input, line);)
  {
    boost::algorithm::trim(line);

    if (line.empty())
      continue;
    else if (polygons_started)
    {
      if (line.substr(0, 4) == "POLY")
      {
        moveto = true;
      }
      else if (line.substr(0, 4) != "POLY" && !boost::regex_match(line, expression))
      {
        throw std::runtime_error("File '" + file.filename().string() +
                                 "' contains an invalid coordinate line '" + line + "'");
      }
      else
      {
        if (moveto) firstpoint = line;

        std::pair<double, double> p = extract_coordinate(line);

        if (moveto)
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementMoveto, p.first, p.second));
        else if (line == firstpoint)
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementClosePath));
        else
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementLineto, p.first, p.second));

        moveto = false;
      }
    }
    else if (line.substr(0, 8) == "NO POLYS")
    {
      return path;
    }
    else if (line.substr(0, 4) == "POLY")
    {
      polygons_started = true;
      firstpoint.clear();
    }
    else
    {
      // header metadata is skipped
    }
  }

  return path;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the ash cloud polygon from the given file
 *
 * Polygons start with POLY. If there are none, there is a NO POLYS line
 */
// ----------------------------------------------------------------------

std::map<std::string, NFmiSvgPath> read_ash_boundary_polygons(const fs::path& file)
{
  std::map<std::string, NFmiSvgPath> paths;

  std::ifstream input(file.c_str());

  if (!input)
    throw std::runtime_error("Failed to open '" + file.filename().string() + "' for reading");

  std::string flightlevel;
  NFmiSvgPath path;

  bool polygons_started = false;
  bool moveto = true;
  std::string firstpoint;

  // I encountered an example with 7 digits, implying 3 digits for the seconds part
  boost::regex expression("[NS]\\d{6,},[WE]\\d{6,}");

  for (std::string line; getline(input, line);)
  {
    boost::algorithm::trim(line);

    if (line.empty())
      continue;
    else if (polygons_started)
    {
      if (line.substr(0, 4) == "POLY")
      {
        paths.insert(std::make_pair(flightlevel, path));
        flightlevel = line.substr(line.size() - 11, 11);
        path.clear();
        moveto = true;
      }
      else if (line.substr(0, 4) != "POLY" && !boost::regex_match(line, expression))
      {
        throw std::runtime_error("File '" + file.filename().string() +
                                 "' contains an invalid coordinate line '" + line + "'");
      }
      else
      {
        if (moveto) firstpoint = line;

        std::pair<double, double> p = extract_coordinate(line);

        if (moveto)
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementMoveto, p.first, p.second));
        else if (line == firstpoint)
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementClosePath));
        else
          path.push_back(NFmiSvgPath::Element(NFmiSvgPath::kElementLineto, p.first, p.second));

        moveto = false;
      }
    }
    else if (line.substr(0, 8) == "NO POLYS")
    {
      return paths;
    }
    else if (line.substr(0, 4) == "POLY")
    {
      polygons_started = true;
      firstpoint.clear();
      flightlevel = line.substr(line.size() - 11, 11);
      path.clear();
    }
    else
    {
      // header metadata is skipped
    }
  }

  // Flush out the last polygon too
  if (!path.empty()) paths.insert(std::make_pair(flightlevel, path));

  return paths;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract concentration value from ash advisory file name
 */
// ----------------------------------------------------------------------

double extract_concentration(const fs::path& file)
{
  std::string encoded_value =
      file.filename().string().substr(concentration_position_in_filename, 3);

  // A == 0.00000001 = 1e-8 etc

  double multiplier = Fmi::stod(encoded_value.substr(1));
  int exponent = encoded_value[0] - 'A' - 8;

  return multiplier * pow(10, exponent);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the information from a single ash advisory file
 */
// ----------------------------------------------------------------------

void copy_ash_concentration_file(NFmiFastQueryInfo& info, const fs::path& file)
{
  info.First();
  info.Param(kFmiAshConcentration);

  // The time

  std::string stamp = file.filename().string().substr(validtime_position_in_filename, 12);
  boost::posix_time::ptime validtime = Fmi::TimeParser::parse(stamp);

  if (!info.Time(tomettime(validtime)))
    throw std::runtime_error("Internal error in setting validtime " + to_simple_string(validtime));

  // The level from FLaaa-bbb

  std::string levelname = file.filename().string().substr(level_position_in_filename, 9);
  double levelvalue = boost::lexical_cast<double>(levelname.substr(6, 3));

  if (!info.Level(NFmiLevel(kFmiFlightLevel, levelname, levelvalue)))
    throw std::runtime_error("Internal error in setting level " + levelname);

  // Read the polygon

  NFmiSvgPath path = read_ash_concentration_polygon(file);

  // Poke the concentration values into the querydata.
  // High concentrations are inside low concentration areas,
  // so we do not overwrite old values if they are higher.

  double concentration = extract_concentration(file);

#if 0
  // Slow fool proof way
  for(info.ResetLocation(); info.NextLocation(); )
	{
	  if(info.FloatValue() == kFloatMissing)
		info.FloatValue(0);
	  if(concentration > info.FloatValue())
		{
		  const NFmiPoint & latlon = info.LatLon();
		  if(path.IsInside(latlon))
			info.FloatValue(concentration);
		}
	}
#else
  // Fast way
  for (info.ResetLocation(); info.NextLocation();)
    if (info.FloatValue() == kFloatMissing) info.FloatValue(0);

  NFmiIndexMask mask = NFmiIndexMaskTools::MaskInside(*info.Grid(), path);

  for (NFmiIndexMask::const_iterator it = mask.begin(); it != mask.end(); ++it)
  {
    info.LocationIndex(*it);
    if (concentration > info.FloatValue()) info.FloatValue(concentration);
  }

#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the information from a single ash advisory file
 */
// ----------------------------------------------------------------------

void copy_ash_boundary_file(NFmiFastQueryInfo& info, const fs::path& file)
{
  info.First();
  info.Param(kFmiAshOnOff);

  // The time

  std::string stamp = file.filename().string().substr(validtime_position_in_filename, 12);
  boost::posix_time::ptime validtime = Fmi::TimeParser::parse(stamp);

  if (!info.Time(tomettime(validtime)))
    throw std::runtime_error("Internal error in setting validtime " + to_simple_string(validtime));

  // Read the polygon

  std::map<std::string, NFmiSvgPath> paths = read_ash_boundary_polygons(file);

  // Poke the on/off values into the querydata.

  // Needed since BOOST_FOREACH does not like templates in it, atleast not with g++
  typedef std::map<std::string, NFmiSvgPath>::value_type value_type;

  BOOST_FOREACH (const value_type& vt, paths)
  {
    const std::string& flightlevel = vt.first;
    const NFmiSvgPath& path = vt.second;
    double levelvalue = Fmi::stod(flightlevel.substr(flightlevel.size() - 3, 3));

    // Set the correct level

    if (!info.Level(NFmiLevel(kFmiFlightLevel, flightlevel, levelvalue)))
      throw std::runtime_error("Internal error in setting level " + flightlevel);

    // Quick way to set values
    for (info.ResetLocation(); info.NextLocation();)
      if (info.FloatValue() == kFloatMissing) info.FloatValue(0);

    NFmiIndexMask mask = NFmiIndexMaskTools::MaskInside(*info.Grid(), path);

    for (NFmiIndexMask::const_iterator it = mask.begin(); it != mask.end(); ++it)
    {
      info.LocationIndex(*it);
      info.FloatValue(1);
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
  if (!parse_options(argc, argv, options)) return 0;

  if (options.verbose)
    std::cout << "Scanning directory '" << options.indir << "' for ash advisories" << std::endl;

  // Determine the files to be processed based on the options

  const char* re = (!options.boundaries ? concentration_regex : boundary_regex);

  // Find files

  std::list<fs::path> files = find_ash_files(re);
  if (files.empty())
    throw std::runtime_error("There are no ash advisories in directory '" + options.indir + "'");

  // Extract model run times

  std::list<boost::posix_time::ptime> times =
      find_model_run_times(files, origintime_position_in_filename);
  if (times.empty())
    throw std::runtime_error("Did not find any model run times from the file names");

  // Pick one or the one given on the command line

  boost::posix_time::ptime tmodel = select_model_run_time(times);
  if (options.verbose) std::cout << "Selected model run time: " << tmodel << std::endl;

  // And filter out other files

  files = select_ash_files(files, tmodel, origintime_position_in_filename);

  // For debugging

  if (options.verbose)
  {
    std::cout << "Ash files to be processed:" << std::endl;
    BOOST_FOREACH (const fs::path& file, files)
      std::cout << "  " << file.filename() << std::endl;
  }

  // Build the querydata descriptors from the file names etc

  NFmiParamDescriptor pdesc = create_pdesc();
  NFmiTimeDescriptor tdesc = create_tdesc(files, tmodel, validtime_position_in_filename);
  NFmiHPlaceDescriptor hdesc = create_hdesc();
  NFmiVPlaceDescriptor vdesc = create_vdesc(files);

  // Initialize the data to missing values

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (data.get() == 0) throw std::runtime_error("Could not allocate memory for result data");

  NFmiFastQueryInfo info(data.get());

  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  // Add each file to the data

  BOOST_FOREACH (const fs::path& file, files)
  {
    if (!options.boundaries)
      copy_ash_concentration_file(info, file);
    else
      copy_ash_boundary_file(info, file);
  }

  // Output

  if (options.outfile == "-")
    std::cout << *data;
  else
  {
    std::ofstream out(options.outfile.c_str());
    out << *data;
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (std::exception& e)
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
