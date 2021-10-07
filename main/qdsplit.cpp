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
 *   - -t [n] split several times simultaneously
 *   - -T [n] split several times simultaneously using memory mapping
 *   - -m limit for assigning a limit on the amount of missing data (%)
 *   - -O set origin time equal to output valid time
 *
 */
// ======================================================================

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose{false};
  bool shortnames{false};
  bool setorigintime{false};
  bool memorymapping{false};
  int simultaneoustimes{1};
  float missinglimit{100};
  string inputfile;
  string outputdir;

  Options() {}
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
       << "\t-t [n]\thow many times to process simultaneously (default is 1)" << endl
       << "\t-T [n]\thow many times to process simultaneously with memory mapping" << endl
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
  NFmiCmdLine cmdline(argc, argv, "hvst!T!m!O");

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

  if (cmdline.isOption('t') && cmdline.isOption('T'))
    throw std::runtime_error("Cannot use options t and T simultaneously");

  if (cmdline.isOption('t'))
    options.simultaneoustimes = boost::lexical_cast<int>(cmdline.OptionValue('t'));

  if (cmdline.isOption('T'))
  {
    options.simultaneoustimes = boost::lexical_cast<int>(cmdline.OptionValue('T'));
    options.memorymapping = true;
  }

  if (options.simultaneoustimes < 1)
    throw runtime_error("Option t/T argument must be at least 1");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a single time timedescriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor make_timedescriptor(const NFmiFastQueryInfo& theQ)
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
 * \brief Calculate percentage of missing values
 */
//----------------------------------------------------------------------

float calc_missing(NFmiFastQueryInfo& info)
{
  if (options.missinglimit >= 100)
    return -1;

  int total = 0;
  int missing = 0;

  for (info.ResetParam(); info.NextParam(false);)
  {
    for (info.ResetLocation(); info.NextLocation();)
    {
      for (info.ResetLevel(); info.NextLevel();)
      {
        for (info.ResetTime(); info.NextTime();)
        {
          float value = info.FloatValue();
          if (value == kFloatMissing)
            ++missing;
          ++total;
        }
      }
    }
  }

  return (total > 0 ? 100.0 * missing / total : 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish final and temporary names for the querydata
 */
// ----------------------------------------------------------------------

pair<string, string> make_outnames(NFmiFastQueryInfo& info)
{
  string outname = (options.outputdir + '/' + info.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr());

#ifdef UNIX
  string tmpname = (options.outputdir + "/." + info.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr());
#else
  string tmpname =
      (options.outputdir + "/" + info.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr()) + ".tmp";
#endif

  if (options.shortnames)
  {
    outname += ".sqd";
    tmpname += ".sqd";
  }
  else
  {
    std::string suffix =
        '_' + NFmiFileSystem::BaseName(NFmiFileSystem::FindQueryData(options.inputfile));
    outname += suffix;
    tmpname += suffix;
  }

  return make_pair(outname, tmpname);
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

  unique_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  NFmiFastQueryInfo dstinfo(data.get());

  if (data.get() == nullptr)
    throw runtime_error("Could not allocate memory for result data");

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

  float misses = calc_missing(dstinfo);

  pair<string, string> names = make_outnames(dstinfo);
  const string& outname = names.first;
  const string& tmpname = names.second;

  if (options.missinglimit < 100 && misses > options.missinglimit)
  {
    if (options.verbose)
      cout << "Skipping " << outname << " since missing percentage is " << misses << endl;
  }
  else
  {
    // write the data out

    if (options.verbose)
      cout << "Writing '" << outname << " (missing " << misses << "%)" << endl;

    // Use dotfile to prevent for example roadmodel crashes

    data->Write(tmpname);
    if (boost::filesystem::exists(outname))
      boost::filesystem::remove(outname);
    boost::filesystem::rename(tmpname, outname);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the given time indices from the querydata
 */
// ----------------------------------------------------------------------

void extract_times(NFmiFastQueryInfo& theQ, unsigned long index1, unsigned long index2)
{
  // Make sure we're within limits (one beyond last possible index)
  index2 = std::min(index2, theQ.SizeTimes());

  // create new data containers

  std::vector<NFmiQueryData*> datas;
  std::vector<NFmiFastQueryInfo*> infos;

  for (unsigned long idx = index1; idx < index2; ++idx)
  {
    theQ.TimeIndex(idx);
    NFmiTimeDescriptor tdesc = make_timedescriptor(theQ);

    NFmiFastQueryInfo tmpinfo(
        theQ.ParamDescriptor(), tdesc, theQ.HPlaceDescriptor(), theQ.VPlaceDescriptor());

    NFmiQueryData* qd = nullptr;

    if (!options.memorymapping)
      qd = NFmiQueryDataUtil::CreateEmptyData(tmpinfo);
    else
    {
      pair<string, string> names = make_outnames(theQ);
      qd = NFmiQueryDataUtil::CreateEmptyData(tmpinfo, names.second, false);
    }

    auto* info = new NFmiFastQueryInfo(qd);

    datas.push_back(qd);
    infos.push_back(info);
  }

  // copy the data for time selected times

  for (theQ.ResetParam(); theQ.NextParam();)
  {
    for (auto& info : infos)
      info->ParamIndex(theQ.ParamIndex());

    for (theQ.ResetLocation(); theQ.NextLocation();)
    {
      for (auto& info : infos)
        info->LocationIndex(theQ.LocationIndex());

      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        for (auto& info : infos)
          info->LevelIndex(theQ.LevelIndex());
        {
          for (unsigned long i = index1; i < index2; ++i)
          {
            theQ.TimeIndex(i);
            float value = theQ.FloatValue();
            infos[i - index1]->FloatValue(value);
          }
        }
      }
    }
  }

  // Write the datas

  for (std::size_t i = 0; i < datas.size(); i++)
  {
    // Count the amount of missing values if needed

    float misses = calc_missing(*infos[i]);

    infos[i]->FirstTime();
    pair<string, string> names = make_outnames(*infos[i]);
    const string& outname = names.first;
    const string& tmpname = names.second;

    if (options.missinglimit < 100 && misses > options.missinglimit)
    {
      if (options.verbose)
      {
        cout << "Skipping " << outname << " since missing percentage is " << misses << endl;
        if (options.memorymapping)
          boost::filesystem::remove(tmpname);
      }
    }
    else
    {
      // write the data out

      if (options.verbose)
      {
        if (misses >= 0)
          cout << "Writing '" << outname << " (missing " << misses << "%)" << endl;
        else
          cout << "Writing '" << outname << endl;
      }

      // Use dotfile to prevent for example roadmodel crashes

      if (!options.memorymapping)
        datas[i]->Write(tmpname);

      if (boost::filesystem::exists(outname))
        boost::filesystem::remove(outname);
      boost::filesystem::rename(tmpname, outname);
    }

    delete datas[i];
    delete infos[i];
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  if (!parse_command_line(argc, argv))
    return 0;

  // Read the data

  NFmiQueryData qd(options.inputfile);
  NFmiFastQueryInfo qi(&qd);

  // Process all the timesteps

  for (unsigned long index1 = 0; index1 < qi.SizeTimes(); index1 += options.simultaneoustimes)
  {
    unsigned long index2 = index1 + options.simultaneoustimes;
    extract_times(qi, index1, index2);
  }

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
