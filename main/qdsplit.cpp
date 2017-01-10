// ======================================================================
/*!
 * \brief Implementation of command qdsplit
 */
// ======================================================================
/*!
 * \page qdsplit qdsplit
 *
 * The qdsplit program reads the input querydata and splits
 * every time step in it into separate files. The output filenames
 * will be the same as the input filename except that the filename
 * is prefixed with the timestamp in UTC time.
 *
 * Usage:
 * \code
 * qdsplit [options] <inputfile> <outputdir>
 * \endcode
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -v for verbose mode
 *   - -s use timestamp only in the name, not the original name
 *   - -m limit for assigning a limit on the amount of missing data (%)
 *   - -O set origin time equal to output valid time
 *
 */
// ======================================================================

#include <NFmiCmdLine.h>
#include <NFmiFastQueryInfo.h>
#include <NFmiFileSystem.h>
#include <NFmiQueryData.h>
#include <NFmiQueryDataUtil.h>
#include <NFmiTimeDescriptor.h>
#include <NFmiTimeList.h>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose;
  bool shortnames;
  bool setorigintime;
  float missinglimit;
  string inputfile;
  string outputdir;

  Options()
      : verbose(false),
        shortnames(false),
        setorigintime(false),
        missinglimit(100),
        inputfile(),
        outputdir()
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
 * \brief Print usage
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdsplit [options] inputfile outputdir" << endl
       << endl
       << "qdsplit splits every time step in the input querydata into" << endl
       << "separate queryfiles in the output directory" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\tprint this help information" << endl
       << "\t-v\tverbose mode" << endl
       << "\t-s\tcreate short filenames" << endl
       << "\t-m limit\thow much data is allowed to be missing (%, default is 100)" << endl
       << "\t-O\tset origin time = valid time" << endl
       << endl;
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
  NFmiCmdLine cmdline(argc, argv, "hvsm!O");

  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

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
  options.outputdir = cmdline.Parameter(2);

  // options

  options.verbose = cmdline.isOption('v');
  options.shortnames = cmdline.isOption('s');
  options.setorigintime = cmdline.isOption('O');

  if (cmdline.isOption('m'))
  {
    options.missinglimit = boost::lexical_cast<float>(cmdline.OptionValue('m'));
    if (options.missinglimit < 0 || options.missinglimit > 100)
      throw runtime_error("Missing limit value should be 0-100");
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a single time timedescriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor make_timedescriptor(const NFmiFastQueryInfo theQ)
{
  NFmiTimeList times;
  times.Add(new NFmiMetTime(theQ.ValidTime()));

  if (options.setorigintime)
    return NFmiTimeDescriptor(theQ.ValidTime(), times);
  else
    return NFmiTimeDescriptor(theQ.OriginTime(), times);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the active time from the querydata
 */
// ----------------------------------------------------------------------

void extract_time(NFmiFastQueryInfo& theQ)
{
  // create new data container

  NFmiTimeDescriptor tdesc = make_timedescriptor(theQ);

  NFmiFastQueryInfo info(
      theQ.ParamDescriptor(), tdesc, theQ.HPlaceDescriptor(), theQ.VPlaceDescriptor());

  auto_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  NFmiFastQueryInfo dstinfo(data.get());

  if (data.get() == 0) throw runtime_error("Could not allocate memory for result data");

  // copy the data for time selected time

  for (dstinfo.ResetParam(), theQ.ResetParam(); dstinfo.NextParam() && theQ.NextParam();)
  {
    for (dstinfo.ResetLocation(), theQ.ResetLocation();
         dstinfo.NextLocation() && theQ.NextLocation();)
    {
      for (dstinfo.ResetLevel(), theQ.ResetLevel(); dstinfo.NextLevel() && theQ.NextLevel();)
      {
        float value = theQ.FloatValue();
        dstinfo.FloatValue(value);
      }
    }
  }

  // Count the amount of missing values if needed

  int total = 0;
  int missing = 0;

  if (options.missinglimit < 100)
  {
    for (dstinfo.ResetParam(), theQ.ResetParam();
         dstinfo.NextParam(false) && theQ.NextParam(false);)
    {
      for (dstinfo.ResetLocation(), theQ.ResetLocation();
           dstinfo.NextLocation() && theQ.NextLocation();)
      {
        for (dstinfo.ResetLevel(), theQ.ResetLevel(); dstinfo.NextLevel() && theQ.NextLevel();)
        {
          float value = theQ.FloatValue();
          if (value == kFloatMissing) ++missing;
          ++total;
        }
      }
    }
  }

  string outname = (options.outputdir + '/' + theQ.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr());

#ifdef UNIX
  string tmpname = (options.outputdir + "/." + theQ.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr());
#endif

  if (options.shortnames)
    outname += ".sqd";
  else
  {
    std::string filename = NFmiFileSystem::FindQueryData(options.inputfile);
    outname += '_' + NFmiFileSystem::BaseName(filename);
  }

  float miss = (total > 0 ? 100.0 * missing / total : 0);

  if (options.missinglimit < 100 && miss > options.missinglimit)
  {
    if (options.verbose)
      cout << "Skipping " << outname << " since missing percentage is " << miss << endl;
  }
  else
  {
    // write the data out

    if (options.verbose) cout << "Writing '" << outname << " (missing " << miss << "%)" << endl;

#ifdef UNIX
    // Use dotfile to prevent for example roadmodel crashes

    data->Write(tmpname);
    if (boost::filesystem::exists(outname)) boost::filesystem::remove(outname);
    boost::filesystem::rename(tmpname, outname);
#else
    data->Write(outname);
#endif
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  if (!parse_command_line(argc, argv)) return 0;

  // Read the data

  NFmiQueryData qd(options.inputfile);
  NFmiFastQueryInfo qi(&qd);

  // Process all the timesteps

  for (qi.ResetTime(); qi.NextTime();)
    extract_time(qi);

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
