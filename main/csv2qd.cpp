// ======================================================================
/*!
 * \brief Implementation of command csv2qd
 */
// ======================================================================
/*!
 * \page csv2qd csv2qd
 *
 * csv2qd reads point data in CSV format and produces a querydata
 * file from the data.
 *
 * Usage:
 * \code
 * csv2qd [options] <inputfile> <outputfile>
 * \endcode
 */
// ======================================================================

// macgyver
#include <CsvReader.h>
#include <TimeParser.h>
#include <TimeZoneFactory.h>

// newbase
#include <NFmiEnumConverter.h>
#include <NFmiFastQueryInfo.h>
#include <NFmiHPlaceDescriptor.h>
#include <NFmiParamDescriptor.h>
#include <NFmiProducerName.h>
#include <NFmiQueryData.h>
#include <NFmiQueryDataUtil.h>
#include <NFmiTimeDescriptor.h>
#include <NFmiTimeList.h>
#include <NFmiVPlaceDescriptor.h>

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>

#include <iostream>
#include <list>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

const string default_paramsfile = "/usr/share/smartmet/parameters.csv";
const string default_stationsfile = "/smartmet/share/csv/stations.csv";
const string default_missingvalue = "";
const int default_producer_number = kFmiSYNOP;
const string default_producer_name = "SYNOP";

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool verbose;

  string order;
  int timecolumn;
  int stationcolumn;
  int levelcolumn;
  int datacolumn;

  vector<string> files;
  string infile;
  string outfile;

  string missingvalue;
  int producernumber;
  string producername;
  vector<int> params;

  string paramsfile;
  string stationsfile;
  string origintime;
  string timezone;

  int leveltype;
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options()
    : verbose(false),
      order("idtime"),
      timecolumn(1),
      stationcolumn(0),
      levelcolumn(-1),
      datacolumn(2),
      files(),
      infile(),
      outfile(),
      missingvalue(default_missingvalue),
      producernumber(default_producer_number),
      producername(default_producer_name),
      params(),
      paramsfile(default_paramsfile),
      stationsfile(default_stationsfile),
      origintime(),
      timezone("UTC"),
      leveltype(5000)
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
  namespace fs = boost::filesystem;

  string params;

  string prodnumdesc =
      ("producer number (default=" + boost::lexical_cast<string>(default_producer_number) + ")");

  string prodnamedesc = "producer name (default=" + string(default_producer_name + ")");

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "version,V", "display version number")("missing,m",
                                             po::value(&options.missingvalue),
                                             "missing value string (default is empty string)")(
      "prodnum", po::value(&options.producernumber), prodnumdesc.c_str())(
      "prodname", po::value(&options.producername), prodnamedesc.c_str())(
      "infile,i", po::value(&options.infile), "input csv file")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "files", po::value<vector<string> >(&options.files), "all input files and the output file")(
      "params,p", po::value(&params), "parameter names of csv columns")(
      "paramsfile,P",
      po::value(&options.paramsfile),
      ("parameter configuration file (" + default_paramsfile + ")").c_str())(
      "order,O",
      po::value(&options.order),
      "column ordering "
      "(idtime|timeid|idtimelevel|idleveltime|timeidlevel|timelevelid|levelidtime|leveltimeid")(
      "stationsfile,S",
      po::value(&options.stationsfile),
      ("station configuration file (" + default_stationsfile + ")").c_str())(
      "origintime", po::value(&options.origintime), "origin time")("timezone,t",
                                                                   po::value(&options.timezone))(
      "leveltype", po::value(&options.leveltype), "leveltype as number");

  po::positional_options_description p;
  p.add("files", -1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    cout << "csv2qd v1.4 (" << __DATE__ << ' ' << __TIME__ << ')' << endl;
  }

  if (opt.count("help"))
  {
    cout << "Usage: csv2qd [options] infile outfile" << endl
         << endl
         << "Converts ASCII data in csv format to querydata." << endl
         << "First column must contain station id, second the UTC time." << endl
         << endl
         << desc << endl;
    return false;
  }

  if (opt.count("files") == 0)
  {
    if (opt.count("infile") == 0) throw runtime_error("Expecting input file as parameter 1");

    if (opt.count("outfile") == 0) throw runtime_error("Expecting output file as parameter 2");

    options.files.push_back(opt["infile"].as<string>());
  }
  else
  {
    options.files = opt["files"].as<vector<string> >();

    if (opt.count("infile") > 0)
      throw runtime_error(
          "Invalid command line combination (do not use --infile with positional arguments");
    if (opt.count("outfile") == 0)
    {
      options.outfile = options.files.back();
      options.files.pop_back();
    }

    if (options.files.empty()) throw runtime_error("Output file not specified");
  }

  if (!fs::exists(options.paramsfile))
    throw runtime_error("Parameters file '" + options.paramsfile + "' does not exist");

  if (!fs::exists(options.stationsfile))
    throw runtime_error("Stations file '" + options.stationsfile + "' does not exist");

  // Parse parameter settings

  if (params.empty()) throw runtime_error("Parameter list must be given");

  NFmiEnumConverter converter;
  vector<string> parts;
  boost::algorithm::split(parts, params, boost::algorithm::is_any_of(","));
  BOOST_FOREACH (const string& str, parts)
  {
    int id = converter.ToEnum(str);
    if (id != kFmiBadParameter)
      options.params.push_back(id);
    else
    {
      try
      {
        id = boost::lexical_cast<int>(str);
        options.params.push_back(id);
      }
      catch (...)
      {
        throw runtime_error("Unknown parameter '" + str + "'");
      }
    }
  }

  // Parse order selection

  if (options.order == "timeid")
  {
    options.timecolumn = 0;
    options.stationcolumn = 1;
    options.datacolumn = 2;
    options.levelcolumn = -1;
  }
  else if (options.order == "idtime")
  {
    options.stationcolumn = 0;
    options.timecolumn = 1;
    options.datacolumn = 2;
    options.levelcolumn = -1;
  }
  else if (options.order == "idtimelevel")
  {
    options.stationcolumn = 0;
    options.timecolumn = 1;
    options.levelcolumn = 2;
    options.datacolumn = 3;
  }
  else if (options.order == "idleveltime")
  {
    options.stationcolumn = 0;
    options.levelcolumn = 1;
    options.timecolumn = 2;
    options.datacolumn = 3;
  }
  else if (options.order == "timeidlevel")
  {
    options.timecolumn = 0;
    options.stationcolumn = 1;
    options.levelcolumn = 2;
    options.datacolumn = 3;
  }
  else if (options.order == "timelevelid")
  {
    options.timecolumn = 0;
    options.levelcolumn = 1;
    options.stationcolumn = 2;
    options.datacolumn = 3;
  }
  else if (options.order == "leveltimeid")
  {
    options.levelcolumn = 0;
    options.timecolumn = 1;
    options.stationcolumn = 2;
    options.datacolumn = 3;
  }
  else if (options.order == "levelidtime")
  {
    options.levelcolumn = 0;
    options.stationcolumn = 1;
    options.timecolumn = 2;
    options.datacolumn = 3;
  }
  else
  {
    throw runtime_error("Unknown column order: '" + options.order + "'");
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Container for CSV data
 */
// ----------------------------------------------------------------------

typedef list<Fmi::CsvReader::row_type> CsvTable;

// ----------------------------------------------------------------------
/*!
 * \brief Data reader
 */
// ----------------------------------------------------------------------

struct Csv
{
  void addrow(const Fmi::CsvReader::row_type& row) { table.push_back(row); }
  CsvTable table;
};

// ----------------------------------------------------------------------
/*!
 * \brief Parameter information
 */
// ----------------------------------------------------------------------

struct ParamInfo
{
  int id;
  string name;
  double minvalue;
  double maxvalue;
  string interpolation;
  string precision;
};
typedef map<int, ParamInfo> Params;

// ----------------------------------------------------------------------
/*!
 * \brief Parse parameter info from CSV table
 */
// ----------------------------------------------------------------------

Params parse_params(const CsvTable& csv)
{
  Params params;

  int rownum = 0;
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    ++rownum;
    try
    {
      if (row.size() != 6)
        throw runtime_error("Invalid row of size " + boost::lexical_cast<string>(row.size()));

      ParamInfo info;
      info.id = boost::lexical_cast<int>(row[0]);
      info.name = row[1];
      info.minvalue = boost::lexical_cast<double>(row[2]);
      info.maxvalue = boost::lexical_cast<double>(row[3]);
      info.interpolation = row[4];
      info.precision = row[5];
      params[info.id] = info;
    }
    catch (exception& e)
    {
      throw runtime_error(string(e.what()) + " in row " + boost::lexical_cast<string>(rownum) +
                          " of file '" + options.paramsfile + "'");
    }
  }
  if (options.verbose)
    cout << "Read " << rownum << " rows from '" << options.paramsfile << "'" << endl;

  return params;
}

// ----------------------------------------------------------------------
/*!
 * \brief Station information
 */
// ----------------------------------------------------------------------

struct StationInfo
{
  string id;
  int number;
  double lon;
  double lat;
  string name;
};

typedef map<string, StationInfo> Stations;

// ----------------------------------------------------------------------
/*!
 * \brief Parse station info from CSV table
 */
// ----------------------------------------------------------------------

Stations parse_stations(const CsvTable& csv)
{
  Stations stations;

  int rownum = 0;
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    ++rownum;
    try
    {
      if (row.size() != 5)
        throw runtime_error("Invalid row of size " + boost::lexical_cast<string>(row.size()));

      StationInfo info;
      info.id = row[0];
      info.number = boost::lexical_cast<int>(row[1]);
      info.lon = boost::lexical_cast<double>(row[2]);
      info.lat = boost::lexical_cast<double>(row[3]);
      info.name = row[4];
      stations[info.id] = info;
    }
    catch (exception& e)
    {
      throw runtime_error(string(e.what()) + " in row " + boost::lexical_cast<string>(rownum) +
                          " of file '" + options.stationsfile + "'");
    }
  }
  if (options.verbose)
    cout << "Read " << rownum << " rows from '" << options.stationsfile << "'" << endl;

  return stations;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create HPlaceDescriptor
 *
 * We need to create a list of all locations in the CSV and then
 * build a location bag based on that list.
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc(const CsvTable& csv, const Stations& stations)
{
  // List all stations

  set<string> used;
  string last_id = "";
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    const string& id = row[options.stationcolumn];
    if (id != last_id)
    {
      used.insert(id);
      last_id = id;
    }
  }

  if (options.verbose) cout << "Found " << used.size() << " stations from input" << endl;

  // Build LocationBag

  NFmiLocationBag lbag;
  BOOST_FOREACH (const string& id, used)
  {
    Stations::const_iterator it = stations.find(id);
    if (it == stations.end())
      throw runtime_error("No information found for station id '" + id + "'");

    NFmiStation station(it->second.number, it->second.name, it->second.lon, it->second.lat);
    lbag.AddLocation(station);
  }

  return NFmiHPlaceDescriptor(lbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create VPlaceDescriptor
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc(const CsvTable& csv)
{
  // default is sufficient for point data

  if (options.levelcolumn < 0) return NFmiVPlaceDescriptor();

  // List all unique levels

  set<int> used;
  int last_level = 0;
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    const string& tmp = row[options.levelcolumn];
    int level = boost::lexical_cast<int>(tmp);

    if (used.empty() || level != last_level)
    {
      used.insert(level);
      last_level = level;
    }
  }

  if (options.verbose) cout << "Found " << used.size() << " levels from input" << endl;

  // Build LevelBag

  FmiLevelType ltype = static_cast<FmiLevelType>(options.leveltype);
  NFmiLevelBag lbag;
  BOOST_FOREACH (int value, used)
  {
    NFmiLevel tmp(ltype, value);
    lbag.AddLevel(tmp);
  }

  return NFmiVPlaceDescriptor(lbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create ParamDescriptor
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor create_pdesc(const Params& params)
{
  NFmiParamBag pbag;

  BOOST_FOREACH (int id, options.params)
  {
    Params::const_iterator it = params.find(id);
    if (it == params.end())
      throw runtime_error("Unknown parameter number " + boost::lexical_cast<string>(id) +
                          ", add definition to '" + options.paramsfile + "'");

    FmiInterpolationMethod interp;

    if (it->second.interpolation == "linear")
      interp = kLinearly;
    else if (it->second.interpolation == "nearest")
      interp = kNearestPoint;
    else if (it->second.interpolation == "lagrange")
      interp = kLagrange;
    else
      throw runtime_error("Unknown interpolation method '" + it->second.interpolation + "'");

    NFmiParam param(id,
                    it->second.name,
                    it->second.minvalue,
                    it->second.maxvalue,
                    kFloatMissing,
                    kFloatMissing,
                    it->second.precision,
                    interp);
    NFmiDataIdent ident(param);
    pbag.Add(ident);
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
 * \brief Create TimeDescriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(const CsvTable& csv, const boost::local_time::time_zone_ptr& tz)
{
  using boost::posix_time::ptime;

  // List all times

  set<ptime> used;
  string last_t = "";
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    const string& t = row[options.timecolumn];
    if (t != last_t)
    {
      used.insert(Fmi::TimeParser::parse(t, tz).utc_time());
      last_t = t;
    }
  }

  if (options.verbose) cout << "Found " << used.size() << " unique times from input" << endl;

  // Build TimeList

  NFmiTimeList tlist;
  BOOST_FOREACH (const ptime& t, used)
  {
    tlist.Add(new NFmiMetTime(tomettime(t)));
  }

  NFmiMetTime origintime;

  if (!options.origintime.empty())
  {
    boost::posix_time::ptime t = Fmi::TimeParser::parse(options.origintime, tz).utc_time();
    origintime = tomettime(t);
  }

  return NFmiTimeDescriptor(origintime, tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Make location index
 */
// ----------------------------------------------------------------------

typedef map<string, unsigned int> LocationIndex;

LocationIndex make_location_index(NFmiFastQueryInfo& info,
                                  const CsvTable& csv,
                                  const Stations& stations)
{
  LocationIndex index;

  Stations::const_iterator station = stations.end();

  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    const string& id = row[options.stationcolumn];
    if (station == stations.end() || station->second.id != id)
    {
      if (index.find(id) == index.end())
      {
        station = stations.find(id);
        NFmiPoint latlon(station->second.lon, station->second.lat);
        info.Location(station->second.number);
        index[id] = info.LocationIndex();
      }
    }
  }
  return index;
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy CSV to querydata
 */
// ----------------------------------------------------------------------

void copy_values(NFmiFastQueryInfo& info,
                 const CsvTable& csv,
                 const Stations& stations,
                 const boost::local_time::time_zone_ptr& tz)
{
  using boost::posix_time::ptime;

  // first level activate by default
  info.First();

  LocationIndex locindex = make_location_index(info, csv, stations);

  string last_t;

  int rownum = 0;
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    ++rownum;

    if (row[options.timecolumn] != last_t)
    {
      ptime t = Fmi::TimeParser::parse(row[options.timecolumn], tz).utc_time();
      info.Time(tomettime(t));
      last_t = row[options.timecolumn];
    }

    unsigned long idx = locindex[row[options.stationcolumn]];
    info.LocationIndex(idx);

    if (options.levelcolumn >= 0)
    {
      int value = boost::lexical_cast<int>(row[options.levelcolumn]);
      NFmiLevel level(static_cast<FmiLevelType>(options.leveltype), value);
      if (!info.Level(level))
        throw runtime_error("Failed to set level " + boost::lexical_cast<string>(level));
    }

    info.ResetParam();
    for (unsigned int i = options.datacolumn; i < row.size(); i++)
    {
      info.NextParam();
      try
      {
        if (row[i] != options.missingvalue) info.FloatValue(boost::lexical_cast<double>(row[i]));
      }
      catch (...)
      {
        throw runtime_error("Invalid number at row " + boost::lexical_cast<string>(rownum) + ": " +
                            row[i]);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Basic error checking
 */
// ----------------------------------------------------------------------

void validate_csv(const CsvTable& csv)
{
  // Each row must contain time,id and params

  unsigned int columns = options.params.size();
  if (options.levelcolumn >= 0) ++columns;
  if (options.timecolumn >= 0) ++columns;
  if (options.stationcolumn >= 0) ++columns;

  int rownum = 0;
  BOOST_FOREACH (const CsvTable::value_type& row, csv)
  {
    ++rownum;
    if (row.size() != columns)
      throw runtime_error("Row " + boost::lexical_cast<string>(rownum) + " contains " +
                          boost::lexical_cast<string>(row.size()) +
                          " elements but should contain " + boost::lexical_cast<string>(columns));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create and write querydata from CSV
 */
// ----------------------------------------------------------------------

void write_querydata(const CsvTable& csv, const Params& params, const Stations& stations)
{
  validate_csv(csv);

  boost::local_time::time_zone_ptr tz =
      Fmi::TimeZoneFactory::instance().time_zone_from_string(options.timezone);

  NFmiHPlaceDescriptor hdesc = create_hdesc(csv, stations);
  NFmiVPlaceDescriptor vdesc = create_vdesc(csv);
  NFmiParamDescriptor pdesc = create_pdesc(params);
  NFmiTimeDescriptor tdesc = create_tdesc(csv, tz);

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  auto_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  NFmiFastQueryInfo info(data.get());

  if (data.get() == 0) throw runtime_error("Could not allocate memory for result data");

  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  copy_values(info, csv, stations, tz);

  ofstream out(options.outfile.c_str());
  out << *data;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv, options)) return 0;

  Csv csv, csvparams, csvstations;
  Fmi::CsvReader::read(options.paramsfile, boost::bind(&Csv::addrow, &csvparams, _1));
  Fmi::CsvReader::read(options.stationsfile, boost::bind(&Csv::addrow, &csvstations, _1));
  BOOST_FOREACH (const string& infile, options.files)
  {
    Fmi::CsvReader::read(infile, boost::bind(&Csv::addrow, &csv, _1));
  }

  Params params = parse_params(csvparams.table);
  Stations stations = parse_stations(csvstations.table);

  write_querydata(csv.table, params, stations);

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
  catch (exception& e)
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
