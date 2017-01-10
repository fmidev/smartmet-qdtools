// ======================================================================
/*!
 * \brief Implementation of command qdmissing
 */
// ======================================================================
/*!
 * \page qdmissing qdmissing
 *
 * The qdmissing program prints the percentage of missing values
 * in the querydata as in integer value.
 *
 * Usage:
 * \code
 * qdmissing [options] [querydata]
 * \endcode
 *
 * If the input argument is a directory, the newest file in it is
 * used.
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -n check for number of NaN values instead of % of kFloatMissing
 *   - -N for printing the count instead of the percentage
 *   - -t for analyzing each timestep separately
 *   - -w for analyzing each station separately
 *   - -T [zone] for specifying the timezone
 *   - -P [param1,param2..] for specifying the parameters
 *   - -Z do not print a result if the result is zero
 *
 * Note! The code relies on the g++ macro definition for NAN.
 * For other compilers we must define the constant by ourselves.
 * Note that the bit pattern is different on Big Endian and Little
 * Endian machines, better leave the problem to GCC.
 */
// ======================================================================

#include "TimeTools.h"

#include <NFmiCmdLine.h>
#include <NFmiEnumConverter.h>
#include <NFmiFastQueryInfo.h>
#include <NFmiFileSystem.h>
#include <NFmiQueryData.h>
#include <NFmiSettings.h>
#include <NFmiStringTools.h>

#include <ctime>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

// Windows _isnan definition is here
#ifndef UNIX
#include <float.h>
#endif

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  string inputfile;
  string timezone;
  vector<FmiParameterName> parameters;
  bool checknan;
  bool printcount;
  bool alltimesteps;
  bool allstations;
  bool checkErrorLimit;
  bool printzero;
  double errorLimit;

  Options()
      : inputfile(),
        timezone(NFmiSettings::Optional<string>("qdmissing::timezone", "Europe/Helsinki")),
        parameters(),
        checknan(false),
        printcount(false),
        alltimesteps(false),
        allstations(false),
        checkErrorLimit(false),
        printzero(true),
        errorLimit()
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
  cout << "Usage: qdmissing [options] querydata" << endl
       << endl
       << "qdmissing returns the number of missing values in the querydata" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\t\t\tprint this help information" << endl
       << "\t-n\t\t\tcount NaN values instead of missing percentage" << endl
       << "\t-N\t\t\tprint count instead of percentage" << endl
       << "\t-t\t\t\tanalyze all timesteps separately" << endl
       << "\t-w\t\t\tanalyze all stations separately" << endl
       << "\t-e [limit]\t\tstops running the program if there's more than [limit] missing values "
       << endl
       << "\t-T [zone]\t\tthe timezone" << endl
       << "\t-P [param1,param2...]\tthe desired parameters" << endl
       << "\t-Z\tdisable printing of results whose value is zero" << endl
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
  NFmiCmdLine cmdline(argc, argv, "htwnZNP!T!e!");

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

  if (cmdline.isOption('t')) options.alltimesteps = true;

  if (cmdline.isOption('w')) options.allstations = true;

  if (cmdline.isOption('n')) options.checknan = true;

  if (cmdline.isOption('N')) options.printcount = true;

  if (cmdline.isOption('Z')) options.printzero = false;

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

  if (cmdline.isOption('T')) options.timezone = cmdline.OptionValue('T');

  if (cmdline.isOption('e'))
  {
    options.checkErrorLimit = true;
    options.errorLimit = NFmiStringTools::Convert<double>(cmdline.OptionValue('e'));
  }

  if (options.alltimesteps && options.allstations)
    throw runtime_error("Options -t and -w are mutually exclusive");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values
 */
// ----------------------------------------------------------------------

int analyze_all_parameters(NFmiFastQueryInfo& theQ)
{
  bool ignoresubs = false;
  size_t total_count = 0;
  size_t missing_count = 0;

  for (theQ.ResetParam(); theQ.NextParam(ignoresubs);)
    for (theQ.ResetLocation(); theQ.NextLocation();)
      for (theQ.ResetLevel(); theQ.NextLevel();)
        for (theQ.ResetTime(); theQ.NextTime();)
        {
          ++total_count;
          if (options.checknan)
          {
#ifdef UNIX
            if (isnan(theQ.FloatValue()))
#else
            if (_isnan(theQ.FloatValue()))
#endif
              ++missing_count;
          }
          else if (theQ.FloatValue() == kFloatMissing)
            ++missing_count;
        }

  if (options.checknan || options.printcount) return missing_count;

  if (total_count == 0) return 100;

  return static_cast<int>((100.0 * missing_count) / total_count);
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values from specified parameters
 */
// ----------------------------------------------------------------------

int analyze_given_parameters(NFmiFastQueryInfo& theQ)
{
  vector<float> percentages;

  for (unsigned int i = 0; i < options.parameters.size(); i++)
  {
    if (!theQ.Param(options.parameters[i]))
    {
      if (options.printcount)
        percentages.push_back(kFloatMissing);
      else
        percentages.push_back(100.0);
    }
    else
    {
      size_t total_count = 0;
      size_t missing_count = 0;

      for (theQ.ResetLocation(); theQ.NextLocation();)
        for (theQ.ResetLevel(); theQ.NextLevel();)
          for (theQ.ResetTime(); theQ.NextTime();)
          {
            ++total_count;
            if (options.checknan)
            {
#ifdef UNIX
              if (isnan(theQ.FloatValue()))
#else
              if (_isnan(theQ.FloatValue()))
#endif
                ++missing_count;
            }
            else if (theQ.FloatValue() == kFloatMissing)
              ++missing_count;
          }

      if (options.checknan || options.printcount)
        percentages.push_back(missing_count);
      else if (total_count == 0)
        percentages.push_back(100.0);
      else
        percentages.push_back(100.0 * missing_count / total_count);
    }
  }

  if (percentages.size() == 0)
  {
    if (options.checknan || options.printcount)
      return 0;
    else
      return 100;
  }

  float sum = accumulate(percentages.begin(), percentages.end(), 0.0);
  if (options.checknan || options.printcount)
    return static_cast<int>(sum);
  else
    return static_cast<int>(sum / percentages.size());
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values for set station
 */
// ----------------------------------------------------------------------

int analyze_all_station_parameters(NFmiFastQueryInfo& theQ)
{
  bool ignoresubs = false;
  size_t total_count = 0;
  size_t missing_count = 0;

  for (theQ.ResetParam(); theQ.NextParam(ignoresubs);)
    for (theQ.ResetLevel(); theQ.NextLevel();)
      for (theQ.ResetTime(); theQ.NextTime();)
      {
        ++total_count;
        if (options.checknan)
        {
#ifdef UNIX
          if (isnan(theQ.FloatValue()))
#else
          if (_isnan(theQ.FloatValue()))
#endif
            ++missing_count;
        }
        else if (theQ.FloatValue() == kFloatMissing)
          ++missing_count;
      }

  if (options.checknan || options.printcount) return missing_count;

  if (total_count == 0) return 100;

  return static_cast<int>((100.0 * missing_count) / total_count);
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values for set station and parameters
 */
// ----------------------------------------------------------------------

int analyze_given_station_parameters(NFmiFastQueryInfo& theQ)
{
  vector<float> percentages;

  for (unsigned int i = 0; i < options.parameters.size(); i++)
  {
    if (!theQ.Param(options.parameters[i]))
    {
      if (options.checknan)
        percentages.push_back(0);
      else
        percentages.push_back(100.0);
    }
    else
    {
      size_t total_count = 0;
      size_t missing_count = 0;

      for (theQ.ResetLevel(); theQ.NextLevel();)
        for (theQ.ResetTime(); theQ.NextTime();)
        {
          ++total_count;
          if (options.checknan)
          {
#ifdef UNIX
            if (isnan(theQ.FloatValue()))
#else
            if (_isnan(theQ.FloatValue()))
#endif
              ++missing_count;
          }
          else if (theQ.FloatValue() == kFloatMissing)
            ++missing_count;
        }

      if (options.checknan || options.printcount)
        percentages.push_back(missing_count);
      else if (total_count == 0)
        percentages.push_back(100.0);
      else
        percentages.push_back(100.0 * missing_count / total_count);
    }
  }

  if (percentages.size() == 0)
  {
    return (options.checknan || options.printcount ? 0 : 100);
  }

  float sum = accumulate(percentages.begin(), percentages.end(), 0.0);
  if (options.checknan || options.printcount)
    return static_cast<int>(sum);
  else
    return static_cast<int>(sum / percentages.size());
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values now
 */
// ----------------------------------------------------------------------

int analyze_all_parameters_now(NFmiFastQueryInfo& theQ)
{
  bool ignoresubs = false;
  size_t total_count = 0;
  size_t missing_count = 0;

  for (theQ.ResetParam(); theQ.NextParam(ignoresubs);)
    for (theQ.ResetLocation(); theQ.NextLocation();)
      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        ++total_count;
        if (options.checknan)
        {
#ifdef UNIX
          if (isnan(theQ.FloatValue()))
#else
          if (_isnan(theQ.FloatValue()))
#endif
            ++missing_count;
        }
        else if (theQ.FloatValue() == kFloatMissing)
          ++missing_count;
      }

  if (options.checknan || options.printcount) return missing_count;

  if (total_count == 0) return 100;

  return static_cast<int>((100.0 * missing_count) / total_count);
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate percentage of missing values from specified parameters now
 */
// ----------------------------------------------------------------------

int analyze_given_parameters_now(NFmiFastQueryInfo& theQ)
{
  vector<float> percentages;

  for (unsigned int i = 0; i < options.parameters.size(); i++)
  {
    if (!theQ.Param(options.parameters[i]))
    {
      if (options.checknan)
        percentages.push_back(0);
      else
        percentages.push_back(100.0);
    }
    else
    {
      size_t total_count = 0;
      size_t missing_count = 0;

      for (theQ.ResetLocation(); theQ.NextLocation();)
        for (theQ.ResetLevel(); theQ.NextLevel();)
        {
          ++total_count;
          if (options.checknan)
          {
#ifdef UNIX
            if (isnan(theQ.FloatValue()))
#else
            if (_isnan(theQ.FloatValue()))
#endif
              ++missing_count;
          }
          else if (theQ.FloatValue() == kFloatMissing)
            ++missing_count;
        }

      if (options.checknan || options.printcount)
        percentages.push_back(missing_count);
      else if (total_count == 0)
        percentages.push_back(100.0);
      else
        percentages.push_back(100.0 * missing_count / total_count);
    }
  }

  if (percentages.size() == 0)
  {
    return (options.checknan || options.printcount ? 0 : 100);
  }

  float sum = accumulate(percentages.begin(), percentages.end(), 0.0);
  if (options.checknan || options.printcount)
    return static_cast<int>(sum);
  else
    return static_cast<int>(sum / percentages.size());
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  // Parse the command line
  if (!parse_command_line(argc, argv)) return 0;

  int percentage = (options.checknan || options.printcount ? 0 : 100);

  // Read the querydata

  NFmiQueryData qd(options.inputfile);
  NFmiFastQueryInfo q(&qd);

  // Establish what to do

  if (options.alltimesteps)
  {
    for (q.ResetTime(); q.NextTime();)
    {
      if (options.parameters.empty())
        percentage = analyze_all_parameters_now(q);
      else
        percentage = analyze_given_parameters_now(q);

      NFmiTime t = TimeTools::timezone_time(q.ValidTime(), options.timezone);
      if (options.printzero || percentage != 0)
        cout << t.ToStr(kYYYYMMDDHHMM).CharPtr() << ' ' << percentage << endl;
      if (options.checkErrorLimit && percentage >= options.errorLimit)
        throw runtime_error("Given error limit has been exceeded, exiting.");
    }
  }
  else if (options.allstations)
  {
    if (q.IsGrid()) throw runtime_error("Option -w can be used only for point data");
    for (q.ResetLocation(); q.NextLocation();)
    {
      if (options.parameters.empty())
        percentage = analyze_all_station_parameters(q);
      else
        percentage = analyze_given_station_parameters(q);
      const NFmiLocation* loc = q.Location();
      if (options.printzero || percentage != 0)
        cout << loc->GetIdent() << '\t' << loc->GetName().CharPtr() << '\t' << loc->GetLongitude()
             << '\t' << loc->GetLatitude() << '\t' << percentage << endl;
      if (options.checkErrorLimit && percentage >= options.errorLimit)
        throw runtime_error("Given error limit has been exceeded, exiting.");
    }
  }
  else
  {
    if (options.parameters.empty())
      percentage = analyze_all_parameters(q);
    else
      percentage = analyze_given_parameters(q);
    if (options.printzero || percentage != 0) cout << percentage << endl;
    if (options.checkErrorLimit && percentage >= options.errorLimit)
      throw runtime_error("Given error limit has been exceeded, exiting.");
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
