// ======================================================================
/*!
 * \brief Implementation of command qddifference
 */
// ======================================================================
/*!
 * \page qddifference qddifference
 *
 * The qddifference program reads in two querydata files and prints
 * out how much they differ.
 *
 * Usage:
 * \code
 * qddifference [options] [querydata1] [querydata2]
 * \endcode
 *
 * If the input argument is a directory, the newest file in it is
 * used.
 *
 * The available options are:
 *
 *   - -h for help information
 *   - -t for analyzing each timestep separately
 *   - -p for calculating percentage of different grid points instead
 *   - -P [param1,param2..] for specifying the parameters to check
 *   - -e epsilon maximum allowed difference
 *
 * The program assumes the two files have the same levels,
 * locations, times and parameters. If not, the program will
 * not crash, but the analysis will be pretty meaningless.
 */
// ======================================================================

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Options holder
 */
// ----------------------------------------------------------------------

struct Options
{
  string inputfile1;
  string inputfile2;
  vector<FmiParameterName> parameters;
  bool allparams{false};
  bool alltimesteps{false};
  bool percentage{false};
  double epsilon{0};

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
  cout << "Usage: qddifference [options] querydata1 querydata2" << endl
       << endl
       << "qddifference displays the difference between two query files" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "\t-h\t\t\tprint this help information" << endl
       << "\t-t\t\t\tanalyze all timesteps separately" << endl
       << "\t-P [param1,param2...]\tthe desired parameters" << endl
       << "\t-p\t\tanalyze percentage of different points" << endl
       << "\t-e eps\t\tmaximum allowed difference for exit value 0" << endl
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
  NFmiCmdLine cmdline(argc, argv, "htpP!e!");

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

  options.inputfile1 = cmdline.Parameter(1);
  options.inputfile2 = cmdline.Parameter(2);

  // options

  if (cmdline.isOption('t'))
    options.alltimesteps = true;

  if (cmdline.isOption('p'))
    options.percentage = true;

  if (cmdline.isOption('e'))
    options.epsilon = boost::lexical_cast<double>(cmdline.OptionValue('e'));

  if (cmdline.isOption('P'))
  {
    const vector<string> args = NFmiStringTools::Split(cmdline.OptionValue('P'));
    if (args.size() == 1 && args[0] == "all")
      options.allparams = true;
    else
    {
      for (const auto& arg : args)
      {
        auto param = FmiParameterName(converter.ToEnum(arg));
        if (param == kFmiBadParameter)
          throw runtime_error(string("Parameter '" + arg + "' is not recognized"));
        options.parameters.push_back(param);
      }
    }
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate maximum difference
 */
// ----------------------------------------------------------------------

double analyze_all_parameters(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  bool ignoresubs = false;
  double maxdiff = 0.0;
  int points = 0;
  int differentpoints = 0;

  for (theQ1.ResetLocation(), theQ2.ResetLocation(); theQ1.NextLocation() && theQ2.NextLocation();)
    for (theQ1.ResetTime(), theQ2.ResetTime(); theQ1.NextTime() && theQ2.NextTime();)
      for (theQ1.ResetLevel(), theQ2.ResetLevel(); theQ1.NextLevel() && theQ2.NextLevel();)
        for (theQ1.ResetParam(), theQ2.ResetParam();
             theQ1.NextParam(ignoresubs) && theQ2.NextParam(ignoresubs);)
        {
          bool iswinddir = (theQ1.Param().GetParam()->GetIdent() == kFmiWindDirection);

          const double value1 = theQ1.FloatValue();
          const double value2 = theQ2.FloatValue();

          if (value1 != kFloatMissing && value2 != kFloatMissing)
          {
            double diff = abs(value2 - value1);
            if (iswinddir)
              diff = min(diff, abs(abs(value2 - value1) - 360));

            maxdiff = max(diff, maxdiff);
            ++points;
            if (diff != 0.0)
              ++differentpoints;
          }
        }

  if (options.percentage)
    return 100.0 * differentpoints / points;
  else
    return maxdiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate difference of specified parameters
 */
// ----------------------------------------------------------------------

double analyze_given_parameters(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  double maxdiff = 0.0;
  int points = 0;
  int differentpoints = 0;

  for (auto& parameter : options.parameters)
  {
    if (!theQ1.Param(parameter) || !theQ2.Param(parameter))
      throw runtime_error("The files must contain the parameters given with -P");

    bool iswinddir = (theQ1.Param().GetParam()->GetIdent() == kFmiWindDirection);

    for (theQ1.ResetLocation(), theQ2.ResetLocation();
         theQ1.NextLocation() && theQ2.NextLocation();)
      for (theQ1.ResetTime(), theQ2.ResetTime(); theQ1.NextTime() && theQ2.NextTime();)
        for (theQ1.ResetLevel(), theQ2.ResetLevel(); theQ1.NextLevel() && theQ2.NextLevel();)
        {
          const double value1 = theQ1.FloatValue();
          const double value2 = theQ2.FloatValue();

          if (value1 != kFloatMissing && value2 != kFloatMissing)
          {
            double diff = abs(value2 - value1);

            if (iswinddir)
              diff = min(diff, abs(abs(value2 - value1) - 360));

            maxdiff = max(diff, maxdiff);
            ++points;
            if (diff != 0.0)
              ++differentpoints;
          }
        }
  }

  if (options.percentage)
    return 100.0 * differentpoints / points;
  else
    return maxdiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate difference of set parameter
 */
// ----------------------------------------------------------------------

double analyze_this_parameter(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  bool iswinddir = (theQ1.Param().GetParam()->GetIdent() == kFmiWindDirection);

  double maxdiff = 0.0;
  int points = 0;
  int differentpoints = 0;

  for (theQ1.ResetLocation(), theQ2.ResetLocation(); theQ1.NextLocation() && theQ2.NextLocation();)
    for (theQ1.ResetTime(), theQ2.ResetTime(); theQ1.NextTime() && theQ2.NextTime();)
      for (theQ1.ResetLevel(), theQ2.ResetLevel(); theQ1.NextLevel() && theQ2.NextLevel();)
      {
        const double value1 = theQ1.FloatValue();
        const double value2 = theQ2.FloatValue();

        if (value1 != kFloatMissing && value2 != kFloatMissing)
        {
          double diff = abs(value2 - value1);

          if (iswinddir)
            diff = min(diff, abs(abs(value2 - value1) - 360));

          maxdiff = max(diff, maxdiff);
          ++points;
          if (diff != 0.0)
            ++differentpoints;
        }
      }

  if (options.percentage)
    return 100.0 * differentpoints / points;
  else
    return maxdiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate difference now
 */
// ----------------------------------------------------------------------

double analyze_all_parameters_now(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  bool ignoresubs = false;
  double maxdiff = 0.0;
  int points = 0;
  int differentpoints = 0;

  for (theQ1.ResetLocation(), theQ2.ResetLocation(); theQ1.NextLocation() && theQ2.NextLocation();)
    for (theQ1.ResetLevel(), theQ2.ResetLevel(); theQ1.NextLevel() && theQ2.NextLevel();)
      for (theQ1.ResetParam(), theQ2.ResetParam();
           theQ1.NextParam(ignoresubs) && theQ2.NextParam(ignoresubs);)
      {
        bool iswinddir = (theQ1.Param().GetParam()->GetIdent() == kFmiWindDirection);
        const double value1 = theQ1.FloatValue();
        const double value2 = theQ2.FloatValue();
        if (value1 != kFloatMissing && value2 != kFloatMissing)
        {
          double diff = abs(value2 - value1);
          if (iswinddir)
            diff = min(diff, abs(abs(value2 - value1) - 360));

          maxdiff = max(diff, maxdiff);
          ++points;
          if (diff != 0.0)
            ++differentpoints;
        }
      }

  if (options.percentage)
    return 100.0 * differentpoints / points;
  else
    return maxdiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate difference of specified parameters now
 */
// ----------------------------------------------------------------------

double analyze_given_parameters_now(NFmiFastQueryInfo& theQ1, NFmiFastQueryInfo& theQ2)
{
  double maxdiff = 0.0;
  int points = 0;
  int differentpoints = 0;

  for (auto& parameter : options.parameters)
  {
    if (!theQ1.Param(parameter) || !theQ2.Param(parameter))
      throw runtime_error("The files must contain the parameters given with -P");

    bool iswinddir = (theQ1.Param().GetParam()->GetIdent() == kFmiWindDirection);

    for (theQ1.ResetLocation(), theQ2.ResetLocation();
         theQ1.NextLocation() && theQ2.NextLocation();)
      for (theQ1.ResetLevel(), theQ2.ResetLevel(); theQ1.NextLevel() && theQ2.NextLevel();)
      {
        const double value1 = theQ1.FloatValue();
        const double value2 = theQ2.FloatValue();

        if (value1 != kFloatMissing && value2 != kFloatMissing)
        {
          double diff = abs(value2 - value1);

          if (iswinddir)
            diff = min(diff, abs(abs(value2 - value1) - 360));

          maxdiff = max(diff, maxdiff);
          ++points;
          if (diff != 0.0)
            ++differentpoints;
        }
      }
  }

  if (options.percentage)
    return 100.0 * differentpoints / points;
  else
    return maxdiff;
}

// ----------------------------------------------------------------------
/*!
 * \brief Make sure the data is comparable
 */
// ----------------------------------------------------------------------

void validate_comparison(NFmiFastQueryInfo& q1, NFmiFastQueryInfo& q2)
{
  for (q1.ResetTime(), q2.ResetTime(); q1.NextTime() && q2.NextTime();)
  {
    if (q1.ValidTime() != q2.ValidTime())
      throw runtime_error("Data not comparable: valid times differ");
  }

  for (q1.ResetLevel(), q2.ResetLevel(); q1.NextLevel() && q2.NextLevel();)
  {
    if (q1.LevelType() != q2.LevelType())
      throw runtime_error("Data not comparable: level types differ");
    if (q1.Level()->LevelValue() != q2.Level()->LevelValue())
      throw runtime_error("Data not comparable: level values differ");
  }

  if (q1.IsGrid() ^ q2.IsGrid())
    throw runtime_error("Data not comparable: one has a grid, one does not");

  if (q1.IsArea() && q2.IsArea())
  {
    if (*q1.Area() != *q2.Area())
      throw runtime_error("Data not comparable, areas differ");
  }

  if (q1.GridHashValue() != q2.GridHashValue())
    throw runtime_error("Data not comparable, grids differ");
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program without error trapping
 */
// ----------------------------------------------------------------------

int domain(int argc, const char* argv[])
{
  // Parse the command line
  if (!parse_command_line(argc, argv))
    return 0;

  // Read the querydata

  NFmiStreamQueryData sqd1;
  if (!sqd1.SafeReadLatestData(options.inputfile1))
    throw runtime_error("Unable to read '" + options.inputfile1 + "'");

  NFmiStreamQueryData sqd2;
  if (!sqd2.SafeReadLatestData(options.inputfile2))
    throw runtime_error("Unable to read '" + options.inputfile2 + "'");

  NFmiFastQueryInfo* q1 = sqd1.QueryInfoIter();
  NFmiFastQueryInfo* q2 = sqd2.QueryInfoIter();

  validate_comparison(*q1, *q2);

  NFmiEnumConverter converter;

  // Establish what to do

  double difference = 0.0;
  if (options.alltimesteps)
  {
    for (q1->ResetTime(), q2->ResetTime(); q1->NextTime() && q2->NextTime();)
    {
      if (options.parameters.empty())
        difference = analyze_all_parameters_now(*q1, *q2);
      else
        difference = analyze_given_parameters_now(*q1, *q2);
      cout << q1->ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr() << ' ' << difference << endl;

      if (options.epsilon > 0 && difference > options.epsilon)
        throw runtime_error("Error limit exceeded");
    }
  }
  else
  {
    if (options.allparams)
    {
      bool ignoresubs = false;
      for (q1->ResetParam(), q2->ResetParam();
           q1->NextParam(ignoresubs) && q2->NextParam(ignoresubs);)
      {
        difference = analyze_this_parameter(*q1, *q2);
        cout << converter.ToString(q1->Param().GetParamIdent()) << '\t' << difference << endl;

        if (options.epsilon > 0 && difference > options.epsilon)
          throw runtime_error("Error limit exceeded");
      }
    }
    else if (options.parameters.empty())
    {
      difference = analyze_all_parameters(*q1, *q2);
      cout << difference << endl;
      if (options.epsilon > 0 && difference > options.epsilon)
        throw runtime_error("Error limit exceeded");
    }
    else
    {
      for (unsigned int i = 0; i < options.parameters.size(); i++)
      {
        if (!q1->Param(options.parameters[i]) || !q2->Param(options.parameters[i]))
          throw runtime_error("The files must contain the parameters given with -P");
        difference = analyze_this_parameter(*q1, *q2);
        cout << converter.ToString(q1->Param().GetParamIdent()) << '\t' << difference << endl;
        if (options.epsilon > 0 && difference > options.epsilon)
          throw runtime_error("Error limit exceeded");
      }
    }
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
