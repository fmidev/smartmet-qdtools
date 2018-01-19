// ======================================================================
/*!
 * \file
 * \brief Implementation of qdfilter - a querydata filter
 */
// ======================================================================
/*!
 * \page page_qdfilter qdfilter
 *
 * The \c qdfilter program allows one to calculate functions on
 * subsets of the querydata.
 *
 * Usage:
 * \code
 * qdfilter [options] startoffset endoffset function querydata
 * \endcode
 * The result is printed to standard output.
 *
 * The available functions are:
 *
 *   - min for min(x)
 *   - max for max(x)
 *   - meanabs for mean(abs(x))
 *   - mean for mean(x)
 *   - sum for sum(x)
 *   - median for median(x)
 *   - change for last(x) - first(x)
 *   - sdev for sample standard deviation
 *   - maxmean for mean(mean(x),max(x))
 *
 * The available options are:
 * <dl>
 * <dt>-h</dt>
 * <dd>
 * Display help on command line options.
 * </dd>
 * <dt>-Q</dt>
 * <dd>
 * Use all files in the directory (newbase multifile)
 * </dd>
 * <dt>-p [param1,param2,...]</dt>
 * <dd>
 * Define the parameters to be extracted. Normally all parameters
 * will be extracted.
 * </dd>
 * <dt>-a</dt>
 * <dd>Only the last time in the data file remains in output</dd>
 * <dt>-t [dt1,dt2,dt]</dt>
 * <dd>
 * Define the time interval to be extracted. Normally all original
 * times will be extracted.
 * </dd>
 * <dt>-T [dt1,dt2,dt]</dt>
 * <dd>
 * Define the time interval to be extracted. Normally all original
 * times will be extracted. This differs from -t in the interpretation
 * of the dt parameter, with -t it is local time, with -T it is UTC time
 * </dd>
 * <dt>-i [hour,...]</dt>
 * <dd>Define the hour to be extracted (local time)</dd>
 * <dt>-I [hour,...]</dt>
 * <dd>Define the hour to be extracted (UTC time)</dd>
 * </dd>
 * </dl>
 *
 * For example, to calculate daily maximum temperatures for 06-18 UTC,
 * one could use
 * \code
 * qdfilter -p Temperature -I 18 - -720 0 max /data/pal/querydata/pal/skandinavia/pinta > output.sqd
 * \endcode
 */
// ======================================================================

#include <newbase/NFmiCalculator.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiDataIntegrator.h>
#include <newbase/NFmiDataModifierAvg.h>
#include <newbase/NFmiDataModifierAvgAbs.h>
#include <newbase/NFmiDataModifierChange.h>
#include <newbase/NFmiDataModifierMax.h>
#include <newbase/NFmiDataModifierMaxMean.h>
#include <newbase/NFmiDataModifierMedian.h>
#include <newbase/NFmiDataModifierMin.h>
#include <newbase/NFmiDataModifierStandardDeviation.h>
#include <newbase/NFmiDataModifierSum.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace boost;

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdfilter [options] startoffset endoffset function querydata" << endl
       << endl
       << "qdfilter filters out data from the given querydata." << endl
       << endl
       << "Usage:" << endl
       << endl
       << "qdfilter [options] startoffset endoffset function querydata" << endl
       << endl
       << "Available functions:" << endl
       << endl
       << "\tmin for min(x)" << endl
       << "\tmax for max(x)" << endl
       << "\tmean for mean(x)" << endl
       << "\tmeanabs for mean(abs(x))" << endl
       << "\tchange for change(x)" << endl
       << "\tmedian for median(x)" << endl
       << "\tmaxmean for mean(mean(x),max(x))" << endl
       << endl
       << "Available options:" << endl
       << endl
       << "-Q" << endl
       << "\tUse all files in the directory (multifile mode)" << endl
       << "-o <outfile>" << endl
       << "\tThe output filename instead of standard output" << endl
       << endl
       << "-p <param1,param2,...,paramN>" << endl
       << endl
       << "\tThe parameters to be extracted as a comma separated list" << endl
       << "\tof parameter names, for example Temperature,Precipitation1h." << endl
       << "\tBy default all parameters are extracted." << endl
       << endl
       << "-l <level1,level2,...,levelN>" << endl
       << endl
       << "\tThe levels to be extracted as a comma separated list of" << endl
       << "\tlevel numbers. By default all levels are extracted." << endl
       << endl
       << "-a" << endl
       << endl
       << "\tOnly the final time step will remain in the data. The startoffset" << endl
       << "\tforced not to exceed the range of the data itself. For example," << endl
       << "\tusing startoffset -999999999 you should get the function value" << endl
       << "\tfor the entire data, but with -1440 only for the final 24 hours" << endl
       << endl
       << "-t <dt1,dt2,dt>" << endl
       << endl
       << "\tThe time interval to be extracted as offsets from the origin" << endl
       << "\ttime. For example parameters 24,48 would extract times" << endl
       << "\tbetween +24 and +48 hours from the origin time. dt1 may" << endl
       << "\tbe omitted, in which case it is assumed to be zero." << endl
       << "\tif the last dt parameter is given, it indicates the" << endl
       << "\tdesired time step (in local times)" << endl
       << endl
       << "-T <dt1,dt2,dt>" << endl
       << endl
       << "\tSame as -t, but dt is used in UTC-time mode" << endl
       << endl
       << "-i <hour>" << endl
       << "\tThe hour to be extracted (local time)" << endl
       << endl
       << "-I <hour>" << endl
       << "\tThe hour to be extracted (UTC time)" << endl
       << endl
       << "For example, to calculate 06-18 UTC temperature maximum use" << endl
       << endl
       << "\tqdfilter -p Temperature -I 18 - -720 0 max filename.sqd > tmax.sqd" << endl
       << endl
       << "To calculate 06-18 UTC precipitation sum use" << endl
       << endl
       << "\tqdfilter -p Precipitation1h -I 18 - -660 0 sum filename.sqd > rsum.sqd" << endl
       << endl
       << "Note the importance of using a difference endoffset when calculating" << endl
       << "the precipitation sum" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiParamDescriptor
 *
 * \param theQ The query info from which to construct
 * \param theParams The parameters to extract, empty if everything
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo& theQ, const vector<string>& theParams)
{
  if (theParams.empty()) return theQ.ParamDescriptor();

  NFmiParamBag pbag;

  NFmiEnumConverter converter;
  for (vector<string>::const_iterator it = theParams.begin(); it != theParams.end(); ++it)
  {
    FmiParameterName paramnum = FmiParameterName(converter.ToEnum(*it));
    if (paramnum == kFmiBadParameter)
      throw runtime_error("Parameter " + *it + " is not known to newbase");
    if (!theQ.Param(paramnum)) throw runtime_error("Source data does not contain parameter " + *it);
    pbag.Add(theQ.Param());
  }

  NFmiParamDescriptor pdesc(pbag);
  return pdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiTimeDescriptor
 *
 * \param theQ The query info from which to construct
 * \param lasttime True, if only the last time is to be kept
 * \param theTimes The time interval to extract, empty if everything
 * \param utc True, if UTC handling is desired
 * \param local_hours 0-23 to extract local hour, or empty
 * \param utc_hours 0-23 to extract utc hours, or empty
 * \param startoffset filter start offset in minutes
 * \param endoffset filter end offset in minutes
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor MakeTimeDescriptor(NFmiFastQueryInfo& theQ,
                                      bool lasttime,
                                      const list<int>& theTimes,
                                      bool utc,
                                      const std::vector<int>& local_hours,
                                      const std::vector<int>& utc_hours,
                                      int startoffset,
                                      int endoffset)
{
  if (lasttime)
  {
    NFmiMetTime origintime = theQ.OriginTime();
    NFmiTimeList datatimes;
    theQ.LastTime();
    datatimes.Add(new NFmiMetTime(theQ.ValidTime()));
    NFmiTimeDescriptor tdesc(origintime, datatimes);
    return tdesc;
  }

  if (theTimes.size() > 3)
    throw runtime_error("Cannot extract timeinterval containing more than 3 values");

  bool has_timestep = (theTimes.size() > 0);
  bool all_timesteps = (!has_timestep && local_hours.empty() && utc_hours.empty());

  int dt1 = 0;
  int dt2 = 0;
  int dt = 0;

  if (theTimes.size() == 1)
  {
    dt1 = 0;
    dt2 = theTimes.front();
    dt = 1;
  }
  else if (theTimes.size() == 2)
  {
    dt1 = theTimes.front();
    dt2 = theTimes.back();
    dt = 1;
  }
  else if (theTimes.size() == 3)
  {
    dt1 = theTimes.front();
    dt2 = *(++theTimes.begin());
    dt = theTimes.back();
  }

  if (has_timestep)
    if (dt < 0 || dt > 24 || 24 % dt != 0)
      throw runtime_error("Time step dt in option -t must divide 24");

  NFmiMetTime origintime = theQ.OriginTime();
  NFmiMetTime starttime = origintime;
  NFmiMetTime endtime = origintime;
  starttime.ChangeByHours(dt1);
  endtime.ChangeByHours(dt2);

  NFmiTimeList datatimes;
  for (theQ.ResetTime(); theQ.NextTime();)
  {
    bool ok = all_timesteps;

    NFmiMetTime t = theQ.ValidTime();

    if (has_timestep)
    {
      if (t.IsLessThan(starttime)) continue;
      if (endtime.IsLessThan(t)) continue;
    }

    if (!ok && has_timestep)
    {
      if (dt == 1)
        ok = true;
      else if (utc)
        ok = (t.GetHour() % dt == 0);
      else
        ok = (t.CorrectLocalTime().GetHour() % dt == 0);
    }

    // Accept desired local hours only

    if (!ok && !local_hours.empty())
    {
      NFmiTime tlocal = t.CorrectLocalTime();
      auto pos = find(local_hours.begin(), local_hours.end(), tlocal.GetHour());
      ok = (pos != local_hours.end());
    }

    // Accept desired UTC hours only
    if (!ok && !utc_hours.empty())
    {
      auto pos = find(utc_hours.begin(), utc_hours.end(), t.GetHour());
      ok = (pos != utc_hours.end());
    }

    if (!ok) continue;

    // Cannot accept a time for which the filter would go out of bounds

    NFmiMetTime t1 = t;
    NFmiMetTime t2 = t;
    t1.ChangeByMinutes(startoffset);
    t2.ChangeByMinutes(endoffset);

    ok = theQ.IsInside(t1);
    ok &= theQ.IsInside(t2);

    if (!ok) continue;

    datatimes.Add(new NFmiMetTime(t));
  }

  NFmiTimeDescriptor tdesc(origintime, datatimes);
  return tdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a new data modified
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiDataModifier> create_modifier(const string& theName)
{
  if (theName == "mean") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierAvg);
  if (theName == "meanabs") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierAvgAbs);
  if (theName == "max") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierMax);
  if (theName == "min") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierMin);
  if (theName == "sum") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierSum);
  if (theName == "change") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierChange);
  if (theName == "median") return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierMedian);
  if (theName == "maxmean")
    return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierMaxMean(0.5));
  if (theName == "sdev")
    return boost::shared_ptr<NFmiDataModifier>(new NFmiDataModifierStandardDeviation);

  throw runtime_error("Unknown function: '" + theName + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief The main work subroutine for the main program
 *
 * The main program traps all exceptions throw in here.
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  string opt_infile;              // the querydata to read
  vector<string> opt_parameters;  // the parameters to extract

  list<int> opt_times;  // the times to extract
  bool opt_utc = false;

  bool opt_multifile = false;        // option -Q
  bool opt_lasttime = false;         // option -a
  std::vector<int> opt_local_hours;  // the local hours to extract
  std::vector<int> opt_utc_hours;    // the UTC hours to extract

  int opt_startoffset = 0;
  int opt_endoffset = 0;
  string opt_function = "";
  string opt_outfile = "-";

  // Read command line arguments

  NFmiCmdLine cmdline(argc, argv, "hQap!t!T!i!I!o!");
  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help option must be checked before checking the number
  // of command line arguments

  if (cmdline.isOption('h'))
  {
    usage();
    return 0;
  }

  // extract command line parameters

  if (cmdline.NumberofParameters() != 4)
    throw runtime_error("Exactly 4 command line arguments are expected, not " +
                        lexical_cast<string>(cmdline.NumberofParameters()));

  opt_startoffset = NFmiStringTools::Convert<int>(cmdline.Parameter(1));
  opt_endoffset = NFmiStringTools::Convert<int>(cmdline.Parameter(2));
  opt_function = cmdline.Parameter(3);
  opt_infile = cmdline.Parameter(4);

  if (opt_infile.empty()) throw runtime_error("Input querydata filename cannot be empty");

  // extract command line options

  if (cmdline.isOption('Q')) opt_multifile = !opt_multifile;

  if (cmdline.isOption('p')) opt_parameters = NFmiStringTools::Split(cmdline.OptionValue('p'));

  if (cmdline.isOption('t') && cmdline.isOption('T'))
    throw runtime_error("Cannot use -t and -T simultaneously");

  if (cmdline.isOption('t'))
  {
    opt_utc = false;
    opt_times = NFmiStringTools::Split<list<int> >(cmdline.OptionValue('t'));
    if (opt_times.size() == 0 || opt_times.size() > 3)
      throw runtime_error("Option -t argument must have 1-3 values");
  }

  if (cmdline.isOption('T'))
  {
    opt_utc = true;
    opt_times = NFmiStringTools::Split<list<int> >(cmdline.OptionValue('T'));
    if (opt_times.size() == 0 || opt_times.size() > 3)
      throw runtime_error("Option -t argument must have 1-3 values");
  }

  if (cmdline.isOption('i'))
  {
    opt_local_hours = NFmiStringTools::Split<vector<int> >(cmdline.OptionValue('i'));
  }

  if (cmdline.isOption('I'))
  {
    opt_utc_hours = NFmiStringTools::Split<vector<int> >(cmdline.OptionValue('I'));
  }

  if (cmdline.isOption('o')) opt_outfile = cmdline.OptionValue('o');

  if (cmdline.isOption('a')) opt_lasttime = true;

  if (opt_lasttime && (cmdline.isOption('t') || cmdline.isOption('T') || cmdline.isOption('i') ||
                       cmdline.isOption('I')))
    throw runtime_error("Cannot use option -a with options -tTiI");

  // read the querydata

  std::unique_ptr<NFmiQueryData> qd;
  std::unique_ptr<NFmiFastQueryInfo> srcinfo;

  if (!opt_multifile)
  {
    qd.reset(new NFmiQueryData(opt_infile));
    srcinfo.reset(new NFmiFastQueryInfo(qd.get()));
  }
  else
  {
    srcinfo.reset(new NFmiMultiQueryInfo(opt_infile));
  }

  // create new descriptors for the new data

  NFmiHPlaceDescriptor hdesc(srcinfo->HPlaceDescriptor());
  NFmiVPlaceDescriptor vdesc(srcinfo->VPlaceDescriptor());
  NFmiParamDescriptor pdesc(MakeParamDescriptor(*srcinfo, opt_parameters));
  NFmiTimeDescriptor tdesc(MakeTimeDescriptor(*srcinfo,
                                              opt_lasttime,
                                              opt_times,
                                              opt_utc,
                                              opt_local_hours,
                                              opt_utc_hours,
                                              opt_startoffset,
                                              opt_endoffset));

  // now create new data based on the new descriptors

  NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  NFmiFastQueryInfo dstinfo(data.get());

  if (data.get() == 0) throw runtime_error("Could not allocate memory for result data");

  // Check that the output does not contain composite parameters

  for (dstinfo.ResetParam(); dstinfo.NextParam();)
  {
    FmiParameterName p = FmiParameterName(dstinfo.Param().GetParam()->GetIdent());
    if (p == kFmiWeatherAndCloudiness) throw runtime_error("Cannot filter WeatherAndCloudiness");
    if (p == kFmiTotalWindMS) throw runtime_error("Cannot filter TotalWindMS");
  }

  // If -a is given, we make sure the start offset is within the data range

  if (opt_lasttime)
  {
    srcinfo->LastTime();
    NFmiTime t2 = srcinfo->ValidTime();
    srcinfo->FirstTime();
    NFmiTime t1 = srcinfo->ValidTime();
    int minutes = t2.DifferenceInMinutes(t1);
    if (opt_startoffset < -minutes) opt_startoffset = -minutes;
  }

  // set the data modifier
  boost::shared_ptr<NFmiDataModifier> modifier = create_modifier(opt_function);

  for (dstinfo.ResetTime(); dstinfo.NextTime();)
  {
    NFmiMetTime starttime = dstinfo.ValidTime();
    NFmiMetTime endtime = dstinfo.ValidTime();

    starttime.ChangeByMinutes(opt_startoffset);
    endtime.ChangeByMinutes(opt_endoffset);

    for (dstinfo.ResetParam(); dstinfo.NextParam();)
    {
      if (!srcinfo->Param(dstinfo.Param())) throw runtime_error("Internal error in parameter loop");

      // We assume levels and locations are identical

      for (dstinfo.ResetLocation(), srcinfo->ResetLocation();
           dstinfo.NextLocation() && srcinfo->NextLocation();)
        for (dstinfo.ResetLevel(), srcinfo->ResetLevel();
             dstinfo.NextLevel() && srcinfo->NextLevel();)
        {
          modifier->Clear();
          float value = NFmiDataIntegrator::Integrate(*srcinfo, starttime, endtime, *modifier);
          dstinfo.FloatValue(value);
        }
    }
  }

  // finish up by printing the result

  data->Write(opt_outfile);

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 *
 * The main program is only an error trapping driver for run
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (const std::exception& e)
  {
    cerr << "Error: Caught an exception:" << endl << "--> " << e.what() << endl;
    return 1;
  }
}

// ======================================================================
