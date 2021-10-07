// ======================================================================
/*!
 * \brief Implementation of command qddiff
 */
// ======================================================================
/*!
 * \page qddiff qddiff
 *
 * The qddiff program finds the timesteps that have changed
 * in the given 2 querydata files and saves querydata with
 * only the changed timesteps.
 *
 * Usage:
 * \code
 * qddiff [options] <inputfile> <inputfile2> <outfile>
 * qddiff [options] <inputdir> <outfile>
 * \endcode
 *
 * If there are two command line parameters, the first parameter
 * is assumed to be the input directory from which the two newest
 * files are to be chosen for the differencing.
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -t for extracting only the new timesteps
 *   - -p for extracting only the common times with changed parameters
 *   - -v for verbose mode
 *   - -V for highly verbose mode, an analysis of differences is printed
 *   - -d for debug mode (nothing is written, -v is implied)
 *
 */
// ======================================================================

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>

#include <fstream>
#include <map>
#include <memory>
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
  bool verbose{false};
  bool veryverbose{false};
  bool debug{false};
  bool extractall{true};
  bool extracttimes{false};
  bool extractparams{false};

  string inputdir;
  string inputfile1;
  string inputfile2;
  string outputfile;

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
  cout << "Usage: qddiff [options] inputfile1 inputfile2 outputfile" << endl
       << "       qddiff [options] inputdir outputfile" << endl
       << endl
       << "The second form is equivalent with the first form with the" << endl
       << "second newest file in inputdir substituted for inputfile1" << endl
       << "and the newest file substituted for inputfile." << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\tprint this help information" << endl
       << "\t-t\textract only thew new timesteps" << endl
       << "\t-p\textract only common times with changed parameters" << endl
       << "\t-v\tverbose mode" << endl
       << "\t-V\tvery verbose mode, prints analysis of differences" << endl
       << "\t-d\tdebug mode, verbose mode is implied" << endl
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
  NFmiCmdLine cmdline(argc, argv, "htpvVd");

  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help-option must be checked first

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  // then the required parameters

  switch (cmdline.NumberofParameters())
  {
    case 2:
      options.inputdir = cmdline.Parameter(1);
      options.outputfile = cmdline.Parameter(2);
      break;
    case 3:
      options.inputfile1 = cmdline.Parameter(1);
      options.inputfile2 = cmdline.Parameter(2);
      options.outputfile = cmdline.Parameter(3);
      break;
    default:
      throw runtime_error("Incorrect number of command line parameters");
  }

  // options

  options.debug = cmdline.isOption('d');
  options.verbose = cmdline.isOption('v') || options.debug;
  options.veryverbose = cmdline.isOption('V');

  if (cmdline.isOption('t'))
  {
    if (!options.extractall)
      throw runtime_error("Cannot apply other filters simultaneously with -t");
    options.extractall = false;
    options.extracttimes = true;
  }

  if (cmdline.isOption('p'))
  {
    if (!options.extractall)
      throw runtime_error("Cannot apply other filters simultaneously with -p");
    options.extractall = false;
    options.extractparams = true;
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print an analysis of data changes
 */
// ----------------------------------------------------------------------

void print_change_analysis(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  NFmiEnumConverter converter;

  // Mark all parameters as unchanged

  using ParamStatus = map<FmiParameterName, int>;
  ParamStatus param_status;

  for (theQ2.ResetParam(); theQ2.NextParam(false);)
    param_status.insert(
        ParamStatus::value_type(FmiParameterName(theQ2.Param().GetParam()->GetIdent()), 0));

  // Analyze the changes

  int missing_times = 0;
  int missing_params = 0;
  int missing_levels = 0;
  int unchanged_times = 0;
  int changed_times = 0;

  for (theQ2.ResetTime(); theQ2.NextTime();)
  {
    map<FmiParameterName, bool> param_checked;

    bool different = false;
    if (!theQ1.Time(theQ2.ValidTime()))
    {
      different = true;
      missing_times++;
    }
    else
    {
      for (theQ2.ResetLevel(); !different && theQ2.NextLevel();)
      {
        if (!theQ1.Level(*theQ2.Level()))
        {
          different = true;
          missing_levels++;
        }
        else
        {
          for (theQ2.ResetParam(); theQ2.NextParam(false);)
          {
            bool paramdifferent = false;

            // The output data may contain the same parameter as a standalone
            // parameter and as a subparameter. We ignore the second test,
            // it is the first parameter only that matters
            auto p = FmiParameterName(theQ2.Param().GetParam()->GetIdent());
            if (param_checked[p])
              continue;

            param_checked[p] = true;

            // Do not compare producers, only parameter numbers
            auto param = FmiParameterName(theQ2.Param().GetParamIdent());

            if (!theQ1.Param(param))
            {
              different = true;
              missing_params++;
            }
            else
            {
              for (theQ2.ResetLocation(), theQ1.ResetLocation();
                   !paramdifferent && theQ2.NextLocation() && theQ1.NextLocation();)
              {
                paramdifferent = (theQ1.FloatValue() != theQ2.FloatValue());
              }
              if (paramdifferent)
              {
                ++param_status[p];
                different = true;
              }
            }
          }
        }
      }
    }
    if (different)
      changed_times++;
    else
      unchanged_times++;
  }

  // Print summary of changes

  cout << missing_times << " times were missing from data 1" << endl
       << missing_params << " parameters were missing from data 1" << endl
       << missing_levels << " levels were missing from data 1" << endl;

  cout << unchanged_times << " timesteps were unchanged" << endl
       << changed_times << " timesteps were changed" << endl;

  cout << endl;

  for (ParamStatus::const_iterator it = param_status.begin(); it != param_status.end(); ++it)
  {
    cout << (it->second ? "CHANGED " : "unchanged ") << converter.ToString(it->first);
    if (it->second)
      cout << " in " << it->second << " timesteps";
    cout << endl;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract any changes
 */
// ----------------------------------------------------------------------

NFmiTimeList extract_all(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  NFmiTimeList times;

  for (theQ2.ResetTime(); theQ2.NextTime();)
  {
    map<FmiParameterName, bool> param_checked;

    bool different = false;
    // If data 1 does not have the time, we must output it
    if (!theQ1.Time(theQ2.ValidTime()))
      different = true;
    else
    {
      for (theQ2.ResetLevel(); !different && theQ2.NextLevel();)
      {
        if (!theQ1.Level(*theQ2.Level()))
          different = true;
        else
        {
          for (theQ2.ResetParam(); !different && theQ2.NextParam();)
          {
            auto p = FmiParameterName(theQ2.Param().GetParam()->GetIdent());
            if (param_checked[p])
              continue;
            param_checked[p] = true;

            if (!theQ1.Param(theQ2.Param()))
              different = true;
            else
            {
              for (theQ2.ResetLocation(), theQ1.ResetLocation();
                   !different && theQ2.NextLocation() && theQ1.NextLocation();)
                different = (theQ1.FloatValue() != theQ2.FloatValue());
            }
          }
        }
      }
    }
    if (different)
    {
      if (options.verbose)
        cout << "Different time: " << theQ2.ValidTime() << endl;
      times.Add(new NFmiMetTime(theQ2.ValidTime()));
    }
  }

  return times;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract new timesteps
 */
// ----------------------------------------------------------------------

NFmiTimeList extract_times(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  NFmiTimeList times;

  for (theQ2.ResetTime(); theQ2.NextTime();)
  {
    // If data 1 does not have the time, we must output it
    if (!theQ1.Time(theQ2.ValidTime()))
    {
      if (options.verbose)
        cout << "Different time: " << theQ2.ValidTime() << endl;
      times.Add(new NFmiMetTime(theQ2.ValidTime()));
    }
  }

  return times;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract changed parameters
 */
// ----------------------------------------------------------------------

NFmiTimeList extract_params(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  NFmiTimeList times;

  for (theQ2.ResetTime(); theQ2.NextTime();)
  {
    map<FmiParameterName, bool> param_checked;

    bool different = false;
    if (theQ1.Time(theQ2.ValidTime()))
    {
      for (theQ2.ResetLevel(); !different && theQ2.NextLevel();)
      {
        if (theQ1.Level(*theQ2.Level()))
        {
          for (theQ2.ResetParam(); !different && theQ2.NextParam();)
          {
            auto p = FmiParameterName(theQ2.Param().GetParam()->GetIdent());
            if (param_checked[p])
              continue;
            param_checked[p] = true;

            if (!theQ1.Param(theQ2.Param()))
              different = true;
            else
            {
              for (theQ2.ResetLocation(), theQ1.ResetLocation();
                   !different && theQ2.NextLocation() && theQ1.NextLocation();)
                different = (theQ1.FloatValue() != theQ2.FloatValue());
            }
          }
        }
      }
    }
    if (different)
    {
      if (options.verbose)
        cout << "Different time: " << theQ2.ValidTime() << endl;
      times.Add(new NFmiMetTime(theQ2.ValidTime()));
    }
  }

  return times;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the difference querydata and save it
 */
// ----------------------------------------------------------------------

void process_difference()
{
  // Read the data

  if (options.verbose)
    cout << "Reading '" << options.inputfile1 << "'" << endl;

  NFmiQueryData qd1(options.inputfile1);

  if (options.verbose)
    cout << "Reading '" << options.inputfile2 << "'" << endl;

  NFmiQueryData qd2(options.inputfile2);

  // Create equivalent data with different timebag

  NFmiFastQueryInfo q1(&qd1);
  NFmiFastQueryInfo q2(&qd2);

  // In highly verbose mode, print an analysis of changes

  if (options.veryverbose)
    print_change_analysis(q1, q2);

  // Find the times from data 2 which have changed from data 1

  NFmiTimeList times;

  if (options.extracttimes)
    times = extract_times(q1, q2);
  else if (options.extractparams)
    times = extract_params(q1, q2);
  else if (options.extractall)
    times = extract_all(q1, q2);

  // If the timelist is empty, there is nothing to do

  if (times.NumberOfItems() == 0)
  {
    if (options.verbose)
      cout << "The data is completely identical, nothing to do" << endl;
    return;
  }

  // Create the output data. We can abort now if we're in debug mode

  if (options.verbose)
    cout << "There were " << times.NumberOfItems() << " different timesteps" << endl;

  if (options.debug)
  {
    cout << "Not writing output since in debug mode" << endl;
    return;
  }

  NFmiTimeDescriptor tdesc(q2.OriginTime(), times);

  NFmiFastQueryInfo info(q2.ParamDescriptor(), tdesc, q2.HPlaceDescriptor(), q2.VPlaceDescriptor());

  unique_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  NFmiFastQueryInfo q(data.get());

  if (data.get() == nullptr)
    throw runtime_error("Could not allocate memory for result data");

  // Copy the values

  for (q.ResetLevel(), q2.ResetLevel(); q.NextLevel() && q2.NextLevel();)
    for (q.ResetParam(), q2.ResetParam(); q.NextParam() && q2.NextParam();)
      for (q.ResetTime(); q.NextTime();)
      {
        if (!q2.Time(q.ValidTime()))
          throw runtime_error("Failed to copy a required time");
        for (q.ResetLocation(), q2.ResetLocation(); q.NextLocation() && q2.NextLocation();)
          q.FloatValue(q2.FloatValue());
      }

  // And write the data

  if (options.verbose)
    cout << "Writing " << options.outputfile << endl;

  if (options.outputfile == "-")
    cout << *data;
  else
  {
    ofstream out(options.outputfile.c_str(), ios::binary | ios::out);
    if (!out)
      throw runtime_error("Failed to open '" + options.outputfile + "' for writing");
    out << *data;
    out.close();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the two newest input files
 */
// ----------------------------------------------------------------------

void find_inputfiles()
{
  // Gather all files in the directory

  if (!NFmiFileSystem::DirectoryExists(options.inputdir))
    throw runtime_error("Directory '" + options.inputdir + "' does not exist");

  list<string> files = NFmiFileSystem::DirectoryFiles(options.inputdir);

  // Handle easy special cases

  if (files.size() == 0)
    throw runtime_error("Directory '" + options.inputdir + "' is empty");

  if (files.size() == 1)
  {
    options.inputfile1 = "";
    options.inputfile2 = options.inputdir + '/' + files.front();
    return;
  }

  // Sort the files by modification time. We allow for the chance
  // that there are two files created on the same second

  using StampedFiles = multimap<time_t, string>;

  StampedFiles stampedfiles;
  for (list<string>::const_iterator it = files.begin(); it != files.end(); ++it)
  {
    const string filename = options.inputdir + '/' + *it;
    const time_t modtime = NFmiFileSystem::FileModificationTime(filename);
    stampedfiles.insert(StampedFiles::value_type(modtime, filename));
  }

  StampedFiles::const_iterator jt = stampedfiles.end();
  options.inputfile2 = (--jt)->second;
  options.inputfile1 = (--jt)->second;
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

  // Establish the two latest files

  if (!options.inputdir.empty())
  {
    find_inputfiles();
    if (options.verbose)
      cout << "Input file 1 = '" << options.inputfile1 << "'" << endl
           << "Input file 2 = '" << options.inputfile2 << "'" << endl;
  }

  // Do nothing if the filenames are equal

  if (options.inputfile1 == options.inputfile2)
  {
    if (options.verbose)
      cout << "Nothing to do, the input files are equivalent" << endl;
    return 0;
  }

  // Copy the second file if the input directory contained only one file

  if (options.inputfile1.empty())
  {
    if (options.verbose)
      cout << "Copying the newest file since 2nd newest is missing" << endl;
    if (!options.debug)
      NFmiFileSystem::CopyFile(options.inputfile2, options.outputfile);
    return 0;
  }

  // Read the two input files and write the difference

  process_difference();

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
