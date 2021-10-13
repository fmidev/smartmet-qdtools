// ======================================================================
/*!
 * \file
 * \brief Progran to smoothen gridded querydata
 */
// ======================================================================
/*!
 * \page qdsmoother qdsmoother
 *
 * Usage:
 * \code
 * qdsmoother [options] [cnf] [inputdata] [outputdata]
 * \endcode
 *
 * A sample configuration file:
 * \code
 * parameters = Temperature
 * parameters += Precipitation1h
 * parameters += WindSpeedMS
 * parameters += WindDirection
 * parameters += Pressure
 * parameters += FogIntensity
 * parameters += ProbabilityThunderstorm
 * parameters += MiddleAndLowCloudCover
 * parameters += TotalCloudCover
 * parameters += DewPoint
 * parameters += Humidity
 * parameters += PoP
 *
 * # parameters += PrecipitationForm
 * # parameters += PrecipitationType
 * parameters += WeatherAndCloudiness
 *
 * times::require = 0:48:1
 *
 * # Local variable to enable easier configuration
 *
 * defaultradius = 100
 *
 * # The settings for individual parameters. If the settings
 * # for some parameter is omitted, a "None" type smoother
 * # is assumed.
 *
 * smoother::Humidity
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 16
 * }
 *
 * smoother::Temperature
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 12
 * }
 *
 * smoother::ProbabilityThunderstorm
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 12
 * }
 *
 * smoother::MiddleAndLowCloudCover
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 12
 * }
 *
 * smoother::TotalCloudCover
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 12
 * }
 *
 * smoother::DewPoint
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 12
 * }
 *
 * smoother::Pressure
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 8
 * }
 *
 * smoother::PoP
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 8
 * }
 *
 * smoother::WindSpeedMS
 * {
 * 	type = PseudoGaussian
 * 	radius = $(defaultradius)
 * 	factor = 6
 * }
 * \endcode
 */
// ======================================================================

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiSettings.h>
#include <newbase/NFmiSmoother.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>

#include <boost/shared_ptr.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdsmoother [options] <conf> <input> <output>" << endl
       << endl
       << "Where <conf> is the path to the configuration file," << endl
       << "<input> is path to querydata or a querydata directory, " << endl
       << "and <output> is path to output querydata or a directory." << endl
       << endl
       << "If <input> is a directory, the newest file in the directory" << endl
       << "will be used. If <output> is a directory, the input filename" << endl
       << "will be used." << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\tPrint this help information" << endl
       << "\t-v\tVerbose mode" << endl
       << "\t\tBy default all data is smoothened" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose;

  string config;
  string inputdata;
  string outputdata;

  Options() : verbose(false) {}
};

// ----------------------------------------------------------------------
/*!
 * \brief Instance of command line options
 */
// ----------------------------------------------------------------------

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 *
 * \return False, if execution is to be stopped
 */
// ----------------------------------------------------------------------

bool parse_command_line(int argc, const char* argv[])
{
  NFmiCmdLine cmdline(argc, argv, "hvf");

  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help-option must be checked first

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  // then required parameters

  if (cmdline.NumberofParameters() != 3)
    throw runtime_error("Three command line arguments are expected");

  options.config = cmdline.Parameter(1);
  options.inputdata = cmdline.Parameter(2);
  options.outputdata = cmdline.Parameter(3);

  // Options

  if (cmdline.isOption('v'))
    options.verbose = !options.verbose;

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build output querydata filename
 *
 * \param theInfile The input filename
 * \param theOutpath The output path or filename
 * \return The output filename
 */
// ----------------------------------------------------------------------

const string make_output_name(const string& theInfile, const string& theOutpath)
{
  if (!NFmiFileSystem::DirectoryExists(theOutpath))
    return theOutpath;

  string outfile = theOutpath + '/' + NFmiFileSystem::BaseName(theInfile);

  return outfile;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a parameter descriptor from the configuration settings
 */
// ----------------------------------------------------------------------

const NFmiParamDescriptor make_param_descriptor(NFmiFastQueryInfo& theQ)
{
  NFmiParamBag bag;

  const string paramnames = NFmiSettings::Require<string>("parameters");
  const vector<string> params = NFmiStringTools::Split(paramnames);

  if (params.size() == 0)
    throw runtime_error("The parameter list in variable 'parameters' is empty");

  NFmiEnumConverter converter;

  for (vector<string>::const_iterator it = params.begin(); it != params.end(); ++it)
  {
    FmiParameterName paramnum = FmiParameterName(converter.ToEnum(*it));
    if (paramnum == kFmiBadParameter)
      throw runtime_error("Parameter '" + *it + "' is not known to newbase");
    if (!theQ.Param(paramnum))
      throw runtime_error("Source data does not contain parameter '" + *it + "'");
    bag.Add(theQ.Param());
  }

  NFmiParamDescriptor desc(bag);
  return desc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Add or remove times from a set
 *
 * \param theFirstTime The first available time
 * \param theLastTime The last available time
 * \param theTimes The set to operate on
 * \param theSpecs Vector of time interval specifications of the form t1:t2:dt
 * \param theFlag True, if times are to be added, false if removed
 */
// ----------------------------------------------------------------------

void handle_times(const NFmiMetTime& theFirstTime,
                  const NFmiMetTime& theLastTime,
                  set<NFmiMetTime>& theTimes,
                  const vector<string>& theSpecs,
                  bool theFlag)
{
  for (vector<string>::const_iterator it = theSpecs.begin(); it != theSpecs.end(); ++it)
  {
    vector<int> words = NFmiStringTools::Split<vector<int> >(*it, ":");
    if (words.size() != 3)
      throw runtime_error("Invalid time specification '" + *it + "'");
    const int t1 = words[0];
    const int t2 = words[1];
    const int dt = words[2];
    if (t1 < 0 || t2 < 0 || dt < 0)
      throw runtime_error("Invalid time specification '" + *it + "'");

    for (int t = t1; t <= t2; ++t)
    {
      NFmiMetTime tx = theFirstTime;
      tx.ChangeByHours(t);

      if (theLastTime.IsLessThan(tx))
        break;

      if (tx.GetHour() % dt == 0)
      {
        if (theFlag)
          theTimes.insert(tx);
        else
          theTimes.erase(tx);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a time descriptor from the configuration settings
 */
// ----------------------------------------------------------------------

const NFmiTimeDescriptor make_time_descriptor(NFmiFastQueryInfo& theQ)
{
  // Build a set of available times

  set<NFmiMetTime> times;
  for (theQ.ResetTime(); theQ.NextTime();)
    times.insert(theQ.ValidTime());

  theQ.FirstTime();
  NFmiMetTime firsttime = theQ.ValidTime();
  theQ.LastTime();
  NFmiMetTime lasttime = theQ.ValidTime();

  // Omissions

  const string omit = NFmiSettings::Optional<string>("times::omit", "");
  const vector<string> omissions = NFmiStringTools::Split(omit);
  handle_times(firsttime, lasttime, times, omissions, false);

  // Add requirements

  const string req = NFmiSettings::Optional<string>("times::require", "");
  const vector<string> requirements = NFmiStringTools::Split(req);
  handle_times(firsttime, lasttime, times, requirements, true);

  // Build the final timelist

  NFmiTimeList tlist;
  for (set<NFmiMetTime>::const_iterator it = times.begin(); it != times.end(); ++it)
    tlist.Add(new NFmiMetTime(*it));

  NFmiTimeDescriptor desc(theQ.OriginTime(), tlist);
  return desc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Smoothen data from source to destination
 */
// ----------------------------------------------------------------------

void smoothen_data(NFmiQueryData& theQD, NFmiFastQueryInfo& theQ)
{
  NFmiEnumConverter converter;

  NFmiFastQueryInfo q(&theQD);

  auto coordinates = theQ.LocationsWorldXY(*theQ.Area());

  for (q.ResetParam(); q.NextParam();)
  {
    if (!theQ.Param(q.Param()))
      throw runtime_error("Parameter not available in source data");

    // Establish the smoothening options

    const int paramnum = q.Param().GetParamIdent();
    const string paramname = converter.ToString(paramnum);

    const string var = "smoother::" + paramname;
    const string typevar = var + "::type";
    const string radiusvar = var + "::radius";
    const string factorvar = var + "::factor";

    const string method = NFmiSettings::Optional<string>(typevar.c_str(), "None");
    const double radius = NFmiSettings::Optional<double>(radiusvar.c_str(), 0);
    const int factor = NFmiSettings::Optional<int>(factorvar.c_str(), 0);

    if (options.verbose)
      cout << "   " << paramname << " (" << paramnum << ") with method " << method << '(' << radius
           << ',' << factor << ')' << endl;

    for (q.ResetLevel(); q.NextLevel();)
    {
      if (!theQ.Level(*q.Level()))
        throw runtime_error("Level not available in source data");

      for (q.ResetTime(); q.NextTime();)
      {
        auto values = theQ.Values(q.ValidTime());

        NFmiSmoother smoother(method, factor, 1000 * radius);
        values = smoother.Smoothen(coordinates, values);

        q.SetValues(values);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create smoothed data
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiQueryData> create_data(NFmiFastQueryInfo& theQ)
{
  // Create the new descriptors

  NFmiHPlaceDescriptor hdesc(theQ.HPlaceDescriptor());
  NFmiTimeDescriptor tdesc = make_time_descriptor(theQ);
  NFmiVPlaceDescriptor vdesc(theQ.VPlaceDescriptor());
  NFmiParamDescriptor pdesc = make_param_descriptor(theQ);

  // And new data

  NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> qd(NFmiQueryDataUtil::CreateEmptyData(info));

  if (qd.get() == 0)
    throw runtime_error("Insufficient memory for result data");

  // Fill the querydata with smoothened values

  smoothen_data(*qd, theQ);

  return qd;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main algorithm
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  if (!parse_command_line(argc, argv))
    return 0;

  NFmiSettings::Read(options.config);

  // Establish input filename
  std::string infile = options.inputdata;
  if (infile != "-")
    infile = NFmiFileSystem::FindQueryData(infile);

  // Establish output filename

  std::string outfile = options.outputdata;
  if (outfile != "-" && NFmiFileSystem::DirectoryExists(outfile))
    outfile += "/" + NFmiFileSystem::BaseName(infile);

  // Read the data and process it

  if (options.verbose)
    cout << "Reading " << infile << endl;
  NFmiQueryData qd(infile);

  NFmiFastQueryInfo q(&qd);

  if (!q.IsGrid())
    throw runtime_error("Cannot smoothen non-gridded querydata");

  if (options.verbose)
    cout << "Smoothening data" << endl;
  boost::shared_ptr<NFmiQueryData> outqd = create_data(q);

  if (outqd.get() == 0)
    throw runtime_error("Failed to create a smoothened querydata object");

  if (options.verbose)
    cout << "Writing " << outfile << endl;

  outqd->Write(outfile);

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
