// ======================================================================
/*!
 * \brief Implementation of command kriging2qd
 */
// ======================================================================
/*!
 * \page kriging2qd kriging2qd
 *
 * The kriging2qd program reads the input ASCII file containing
 * the output of one of FMI:s Kriging analysis programs and converts
 * it to querydata.
 *
 * Usage:
 * \code
 * kriging2qd [options] <inputfile> <outputfile>
 * \endcode
 *
 * If the input argument is a directory, the newest file in it is
 * converted.
 *
 * The input file is expected to contain data in YKJ-coordinates
 * in X Y VALUE order like this:
 * \code
 * 3405000 6705000     3.0     3.5    ...
 * 3415000 6705000     7.6     7.6
 * 3425000 6705000     6.4     6.4
 * 3435000 6705000     0.8     0.9
 * 3445000 6705000     7.0     7.9
 * 3455000 6705000    26.3    26.2
 * 3465000 6705000    15.5     5.5
 * 3475000 6705000     5.2     5.2
 * 3485000 6705000     0.0     0.1
 * 3495000 6705000     0.0     0.2
 * 3505000 6705000     0.0     0.3
 * 3515000 6705000     0.0     0.4    ...
 * \endcode
 * The ordering of the coordinates does not matter, the program
 * will automatically establish the limits and size of the grid.
 * The program will abort if the grid cannot be determined
 * automatically.
 *
 * Any row starting with character '#' will be discarded.
 * The number of columns after the 2 coordinate columns
 * must match the number of parameters given using option -p.
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -v for verbose mode
 *   - -p [paramname1,paramname2,...] for specifying the parameter, default is Temperature
 *   - -t [stamp] for specifying the data time in UTC, default is now
 *   - -T [stamp] for specifying origin time (overrides -t)
 */
// ======================================================================

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <newbase/NFmiAreaTools.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief The container for Kriging-data
 */
// ----------------------------------------------------------------------

typedef std::vector<double> Values;
typedef map<NFmiPoint, Values> KrigingData;

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose;
  string inputfile;
  string outputfile;
  std::vector<FmiParameterName> parameters;
  NFmiTime validtime;
  NFmiTime origintime;
  double missingvalue;

  Options()
      : verbose(false),
        inputfile(),
        outputfile(),
        parameters({kFmiTemperature}),
        validtime(),
        origintime(validtime),
        missingvalue(32700)
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
  cout << "Usage: kriging2qd [options] inputfile outputfile" << endl
       << endl
       << "kriging2qd converts the ASCII data produced by a Kriging-analysis" << endl
       << "into querydata and stores it into the given file" << endl
       << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\t\tprint this help information" << endl
       << "\t-v\t\tverbose mode" << endl
       << "\t-p [param1,param2,param3]\tthe parameter, default is Temperature" << endl
       << "\t-t [time]\tthe valid time stamp, default is now" << endl
       << "\t-T [time]\tthe origin time, default is the valid time above" << endl
       << "\t-m missing data value, default is 32700" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a timestamp of the form YYYYMMDDHHMI
 */
// ----------------------------------------------------------------------

const NFmiTime parse_stamp(const string& theStamp)
{
  if (theStamp.size() != 12)
    throw runtime_error("The length of the stamp is not 12");

  int year, month, day, hour, minute;
  try
  {
    year = NFmiStringTools::Convert<int>(theStamp.substr(0, 4));
    month = NFmiStringTools::Convert<int>(theStamp.substr(4, 2));
    day = NFmiStringTools::Convert<int>(theStamp.substr(6, 2));
    hour = NFmiStringTools::Convert<int>(theStamp.substr(8, 2));
    minute = NFmiStringTools::Convert<int>(theStamp.substr(10, 2));
  }
  catch (...)
  {
    throw runtime_error("The stamp is not numeric");
  }

  if (year < 1900 || year > 2200)
    throw runtime_error("The year is out of range 1900-2200");
  if (month < 1 || month > 12)
    throw runtime_error("The month is out of range 1-12");
  if (day < 1 || day > 31)
    throw runtime_error("The day is out of range 1-31");
  if (hour < 0 || hour > 23)
    throw runtime_error("The hour is out of range 0-23");
  if (minute < 0 || minute > 59)
    throw runtime_error("The minute is out of range 0-59");

  return NFmiTime(year, month, day, hour, minute);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 *
 * \return False, if execution is to be stopped
 */
// ----------------------------------------------------------------------

bool parse_command_line(int argc, const char* argv[])
{
  NFmiCmdLine cmdline(argc, argv, "hvp!t!T!m!");

  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help-option must be checked first

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  // then the required parameters

  if (cmdline.NumberofParameters() != 2)
    throw runtime_error("Incorrect number of command line parameters");

  options.inputfile = cmdline.Parameter(1);
  options.outputfile = cmdline.Parameter(2);

  // options

  options.verbose = cmdline.isOption('v');

  if (cmdline.isOption('p'))
  {
    std::vector<std::string> params;
    options.parameters.clear();

    const std::string paramlist = cmdline.OptionValue('p');
    boost::algorithm::split(params, paramlist, boost::algorithm::is_any_of(","));
    for (const auto& p : params)
    {
      auto paramnum = FmiParameterName(converter.ToEnum(p));
      if (paramnum == kFmiBadParameter)
        throw runtime_error(string("Parameter '") + p + "' is not recognized");

      options.parameters.push_back(paramnum);
    }
  }

  if (cmdline.isOption('t'))
  {
    const string stamp = cmdline.OptionValue('t');
    try
    {
      options.validtime = parse_stamp(stamp);
    }
    catch (exception& e)
    {
      throw runtime_error(string("Option -t argument is not of the form YYYYMMDDHHMI: ") +
                          e.what());
    }
  }

  if (!cmdline.isOption('T'))
    options.origintime = options.validtime;
  else
  {
    const string stamp = cmdline.OptionValue('T');
    try
    {
      options.origintime = parse_stamp(stamp);
    }
    catch (exception& e)
    {
      throw runtime_error(string("Option -T argument is not of the form YYYYMMDDHHMI: ") +
                          e.what());
    }
  }

  if (cmdline.isOption('m'))
  {
    const string mval = cmdline.OptionValue('m');
    double missing;

    stringstream ss(mval);

    if (!(ss >> missing))
    {
      throw std::runtime_error("Bad missing value: " + mval);
    }

    options.missingvalue = missing;
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief If the input file is a directory, find the newest file in it
 *
 * Throws if the directory is empty.
 */
// ----------------------------------------------------------------------

void complete_inputname()
{
  if (NFmiFileSystem::DirectoryExists(options.inputfile))
  {
    string file = NFmiFileSystem::NewestFile(options.inputfile);
    if (file.empty())
      throw runtime_error("Input directory is empty");
    options.inputfile += '/';
    options.inputfile += file;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the Kriging-data
 */
// ----------------------------------------------------------------------

const KrigingData read_kriging_data()
{
  if (options.verbose)
    cout << "Reading '" << options.inputfile << "'" << endl;

  KrigingData kdata;

  ifstream input(options.inputfile.c_str());
  if (!input)
    throw runtime_error("Failed to open '" + options.inputfile + "' for reading");

  // Process all the lines

  string line;
  while (getline(input, line))
  {
    // Ignore empty lines and comment lines

    if (line.empty() || line[0] == '#')
      continue;

    // Split to doubles

    double x;
    double y;
    vector<double> values;

    istringstream tokens(line);
    tokens >> x >> y;
    double value;
    while (tokens >> value)
      values.push_back(value);

    if (!tokens.eof())
    {
      cerr << "Warning: Line '" << line << "' does not contain numeric values" << endl;
    }

    // Ignore lines of incorrect length

    if (values.size() != options.parameters.size())
    {
      cerr << "Warning: Line '" << line << "' does not contain the correct number of values"
           << endl;
      continue;
    }

    kdata.insert(KrigingData::value_type(NFmiPoint(x, y), values));
  }
  input.close();
  return kdata;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the smallest adjacent difference from a set
 *
 * Returns -1 if the set contains less than 2 values, otherwise
 * the value is always nonnegative
 */
// ----------------------------------------------------------------------

template <typename T>
typename T::value_type smallest_step(const T& theValues)
{
  typename T::value_type mindiff = -1;

  for (typename T::const_iterator it = theValues.begin(), end = theValues.end(); it != end;)
  {
    typename T::value_type value1 = *it;
    ++it;
    if (it != end)
    {
      typename T::value_type value2 = *it;
      typename T::value_type diff = value2 - value1;
      if (mindiff < 0 || diff < mindiff)
        mindiff = diff;
    }
  }

  return mindiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the horizontal place descriptor based on the Kriging-data
 */
// ----------------------------------------------------------------------

const NFmiHPlaceDescriptor make_hdesc(const KrigingData& theData)
{
  // Find the unique set of x/y coordinates

  if (theData.empty())
    throw runtime_error("The Kriging data is empty!");

  set<double> xset;
  set<double> yset;

  for (KrigingData::const_iterator it = theData.begin(), end = theData.end(); it != end; ++it)
  {
    xset.insert(it->first.X());
    yset.insert(it->first.Y());
  }

  // We require atleast a 2x2 grid

  if (xset.size() < 2)
    throw runtime_error("Not enough x-coordinates in to deduce grid size");
  if (yset.size() < 2)
    throw runtime_error("Not enough y-coordinates to deduce grid size");

  // Minimum x/y is now easy, since the sets are sorted

  const double xmin = *xset.begin();
  const double ymin = *yset.begin();
  const double xmax = *xset.rbegin();
  const double ymax = *yset.rbegin();

  // Establish the smallest grid step

  const double dx = smallest_step(xset);
  const double dy = smallest_step(yset);

  // The grid size is then

  const int width = static_cast<int>((xmax - xmin) / dx + 0.5) + 1;
  const int height = static_cast<int>((ymax - ymin) / dy + 0.5) + 1;

  // Now we can create the projection

  NFmiArea* area =
      NFmiArea::CreateFromBBox("epsg:2393", NFmiPoint(xmin, ymin), NFmiPoint(xmax, ymax));

  if (area == 0)
    throw runtime_error("Failed to construct the YKJ projection");

  // Then the grid

  NFmiGrid tmpgrid(area, width, height);

  // And finally the descriptor

  NFmiHPlaceDescriptor hdesc(tmpgrid);

  if (options.verbose)
  {
    cout << "Calculated grid information:" << endl
         << setprecision(16) << "  xrange = " << xmin << "..." << xmax << endl
         << "  yrange = " << ymin << "..." << ymax << endl
         << "  dxdy   = " << dx << 'x' << dy << endl
         << "  grid   = " << width << 'x' << height << endl;
  }

  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the vertical place descriptor
 */
// ----------------------------------------------------------------------

const NFmiVPlaceDescriptor make_vdesc()
{
  NFmiLevelBag bag(kFmiAnyLevelType, 0, 0, 0);
  NFmiVPlaceDescriptor vdesc(bag);
  return vdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the parameter descriptor
 */
// ----------------------------------------------------------------------

const NFmiParamDescriptor make_pdesc()
{
  NFmiParamBag bag;
  for (const auto& p : options.parameters)
  {
    string paramname = converter.ToString(p);
    bag.Add(NFmiDataIdent(NFmiParam(p, paramname)));
  }
  NFmiParamDescriptor pdesc(bag);
  return pdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the time descriptor
 */
// ----------------------------------------------------------------------

const NFmiTimeDescriptor make_tdesc()
{
  NFmiTimeList times;
  times.Add(new NFmiMetTime(options.validtime, 1));
  NFmiTimeDescriptor tdesc(options.validtime, times);
  return tdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create querydata from the Kriging-data
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiQueryData> create_querydata(const KrigingData& theData)
{
  if (options.verbose)
    cout << "Filling the querydata" << endl;

  NFmiHPlaceDescriptor hdesc(make_hdesc(theData));
  NFmiVPlaceDescriptor vdesc(make_vdesc());
  NFmiParamDescriptor pdesc(make_pdesc());
  NFmiTimeDescriptor tdesc(make_tdesc());

  // create the new querydata

  NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));

  if (data.get() == 0)
    throw runtime_error("Failed to allocate querydata");

  // And begin filling the data

  NFmiFastQueryInfo q(data.get());
  q.First();

  for (KrigingData::const_iterator it = theData.begin(), end = theData.end(); it != end; ++it)
  {
    NFmiPoint latlon = q.Area()->WorldXYToLatLon(it->first);

    if (!q.NearestPoint(latlon))
    {
      ostringstream out;
      out << setprecision(16) << it->first.X() << ',' << it->first.Y();

      throw runtime_error("Failed to set coordinate " + out.str() +
                          " in the querydata, perhaps grid is wrong?");
    }

    q.FirstParam();
    for (const auto& value : it->second)
    {
      if (value == options.missingvalue)
        q.FloatValue(kFloatMissing);
      else
        q.FloatValue(value);
      q.NextParam();
    }
  }

  return data;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int domain(int argc, const char* argv[])
{
  if (!parse_command_line(argc, argv))
    return 0;

  // Make sure we know the full filename if directory was given

  complete_inputname();

  // Read the ASCII data

  KrigingData kdata = read_kriging_data();

  // Create the querydata

  boost::shared_ptr<NFmiQueryData> qd = create_querydata(kdata);

  if (qd.get() == 0)
    throw runtime_error("Failed to create the querydata");

  // Write the querydata

  ofstream out(options.outputfile.c_str(), ios::out | ios::binary);
  if (!out)
    throw runtime_error("Failed to open '" + options.outputfile + "' for writing");

  if (options.verbose)
    cout << "Writing '" << options.outputfile << "'" << endl;

  out << *qd;

  if (out.fail())
  {
    NFmiFileSystem::RemoveFile(options.outputfile);
    throw runtime_error("Failed to write '" + options.outputfile + "'");
  }

  out.close();

  if (options.verbose)
    cout << "Done" << endl;

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return domain(argc, argv);
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
