// ======================================================================
/*!
 * \file
 * \brief Implementation of program combineHistory
 */
// ======================================================================
/*!
 * \page page_combineHistory combineHistory
 *
 * combineHistory takes as input a directory containing a large
 * number of forecast files, most likely with overlapping times,
 * and builds a new data file containing the shortest possible
 * forecast for each moment in time. The input files are processed
 * in reverse alphabetical order, which usually equals processing
 * newest file first, provided the queryfile names are equal
 * apart from a YYYYMMDDHHMI timestamp.
 *
 * Usage:
 * \code
 * combineHistory [options] <directory> [<directory2> ...] > output
 * combineHistory [options] -O outfile <directory> [<directory2> ...]
 * \endcode
 * The options are
 *
 *  - -p for maximum age into the past in hours (default is 48)
 *  - -f for maximum age into the future in hours (default is 8)
 *  - -v for verbose mode (written to standard error!)
 *  - -1 for taking only newest file from each input directory
 *  - -o require same origintime from each candidate
 *  - -O memory mapped output file
 *  - -r use oldest origin time instead of newest for output data
 *
 * If the set of times formed by the options is not available in
 * any forecast, all data for that moment will consist of missing values.
 *
 * It is assumed that the parameter, level and place descriptors
 * are equal in all input files in the directory.
 *
 * Error conditions:
 *
 *   - There are no query files
 *   - No queryfile has times in the requested range
 *
 * Example:
 * \code
 * combineHistory /data/pal/querydata/pal/skandinavia/pinta_xh > new.sqd
 * \endcode
 */
// ======================================================================

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiQueryInfo.h>
#include <newbase/NFmiTimeList.h>

#include <boost/lexical_cast.hpp>

#include <fstream>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Print short usage information
 */
// ----------------------------------------------------------------------

void Usage()
{
  cout << "combineHistory [options] <directory> [<directory2> ...] > output" << endl
       << "combineHistory [options] -O outfile <directory> [<directory2> ...]" << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-p <hours>\tmaximum age into the past (default 48)" << endl
       << "\t-f <hours>\tmaximum age into the future (default 8)" << endl
       << "\t-v\t\tverbose" << endl
       << "\t-o\t\trequire the same origin time" << endl
       << "\t-O <filename>\tmemory mapped output file" << endl
       << "\t-r\t\tpick oldest origin time instead of latest" << endl
       << "\t-t\t\ttry to make timebag instead of timelist" << endl
       << "\t-1\t\ttake only latest file from each directory" << endl
       << "\t-N <name>\tset new producer name" << endl
       << "\t-D <id>\t\tset new producer id" << endl
       << endl;
}

// tutkitaan onko mahdollista tehda listasta bagi
// eli ajat ovat peräkkäisiä ja tasavälisiä

static bool ConvertTimeList2TimeBag(NFmiTimeList &theTimeList, NFmiTimeBag &theTimeBag)
{
  if (theTimeList.NumberOfItems() >
      2)  // ei  tehdä yhdestä tai kahdesta ajasta bagiä vaikka se on mahdollista
  {
    theTimeList.First();
    theTimeList.Next();
    int resolution = theTimeList.CurrentResolution();
    for (; theTimeList.Next();)
    {
      if (resolution != theTimeList.CurrentResolution())
        return false;  // jos yhdenkin aikavälin resoluutio poikkeaa, ei voida tehdä bagia
    }
    theTimeBag = NFmiTimeBag(theTimeList.FirstTime(), theTimeList.LastTime(), resolution);
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 *
 * Algorithm:
 *
 *  -# read command line arguments
 *  -# check command line arguments
 *  -# establish all queryfiles
 *  -# read all queryfiles, starting from newest
 *     -# if firstnewest queryfile queryfile
 *         -# initialize new querydata with new time descriptor
 *            and copies for parameter etc descriptors
 *     -# for each time that is in the queryfile
 *         -# if the time is to be output and it has not been output yet,
 *            copy the grid to the output
 *  -# output the result
 *
 * \param argc The number of arguments
 * \param argv The arguments
 * \return The exit code
 */
// ----------------------------------------------------------------------

int main(int argc, const char *argv[])
{
  // The default options

  long maxPastAge = 48;      // maximum age in hours
  long maxFutureAge = 8;     // maximum future time in hours
  bool verbose = false;      // verbose mode
  bool latest = false;       // only latest files
  bool sameorigin = false;   // do not require same origintime
  bool newestorigin = true;  // pick newest origin time
  std::string outfile = "-";

  NFmiCmdLine cmdline(argc, argv, "vp!f!1otN!D!rO!");

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    Usage();
    return 1;
  }

  if (cmdline.NumberofParameters() < 1)
  {
    cerr << "Error: Directory argument is missing" << endl;
    Usage();
    return 1;
  }

  // Read the options

  list<string> datapaths;
  {
    for (int i = 1; i <= cmdline.NumberofParameters(); i++)
      datapaths.emplace_back(cmdline.Parameter(i));
  }

  if (cmdline.isOption('v'))
    verbose = true;

  if (cmdline.isOption('r'))
    newestorigin = !newestorigin;

  if (cmdline.isOption('p'))
    maxPastAge = boost::lexical_cast<long>(cmdline.OptionValue('p'));

  if (cmdline.isOption('f'))
    maxFutureAge = boost::lexical_cast<long>(cmdline.OptionValue('f'));

  if (cmdline.isOption('1'))
    latest = true;

  if (cmdline.isOption('o'))
    sameorigin = true;

  if (cmdline.isOption('O'))
    outfile = cmdline.OptionValue('O');

  // Check arguments

  for (list<string>::const_iterator it = datapaths.begin(); it != datapaths.end(); ++it)
  {
    if (!NFmiFileSystem::DirectoryExists(*it) && !NFmiFileSystem::FileReadable(*it))
    {
      cerr << "Error: Directory '" << *it << "' does not exist" << endl;
      return 1;
    }
  }

  // Establish the minimum and maximum times to be included
  // in the output.

  NFmiMetTime now;
  NFmiMetTime firsttime(now);
  NFmiMetTime lasttime(now);
  firsttime.ChangeByHours(-maxPastAge);
  lasttime.ChangeByHours(maxFutureAge);

  if (verbose)
  {
    cerr << "Timerange = " << firsttime.ToStr(kYYYYMMDDHHMM).CharPtr() << " - "
         << lasttime.ToStr(kYYYYMMDDHHMM).CharPtr() << endl;
  }

  // Initialize the time list

  NFmiTimeList timelist;

  // Establish the query files

  list<string> files;

  for (list<string>::const_iterator dir = datapaths.begin(); dir != datapaths.end(); ++dir)
  {
    if (NFmiFileSystem::FileReadable(*dir))
      files.push_back(*dir);

    else if (latest)
    {
      string newest = NFmiFileSystem::NewestFile(*dir);
      if (newest.empty())
      {
        cerr << "Directory '" + *dir + "' is empty" << endl;
        return 1;
      }
      files.push_back(*dir + '/' + newest);
    }
    else
    {
      list<string> dirfiles;

      dirfiles = NFmiFileSystem::DirectoryFiles(*dir);

      // Alphabetical order is assumed to be the time order
      // so that reverse alphabetical orders gives files
      // from newest to oldest

      dirfiles.sort();
      dirfiles.reverse();

      for (list<string>::const_iterator fit = dirfiles.begin(); fit != dirfiles.end(); ++fit)
      {
        files.push_back(*dir + '/' + *fit);
      }
    }
  }

  // We collect a list of files which contained accepted
  // timestamps to speed up the main loop. The files are
  // sorted based on the origin time

  multimap<NFmiMetTime, string> accepted_files;

  NFmiMetTime origintime;

  // First collect all times and parameters in the requested time range

  NFmiParamBag pbag;

  for (list<string>::const_iterator it = files.begin(); it != files.end(); ++it)
  {
    const string filename = *it;

    if (verbose)
      cerr << "Reading " << filename << " header" << endl;

    NFmiQueryInfo qi;
    try
    {
      ifstream in(filename.c_str(), ios::in | ios::binary);
      if (!in)
        continue;
      in >> qi;
      in.close();
    }
    catch (...)
    {
      continue;
    }

    // discard files with different origin time
    if (sameorigin && !accepted_files.empty())
    {
      if (origintime != qi.OriginTime())
      {
        if (verbose)
          cerr << ".. discared due to different origin time" << endl;
        continue;
      }
    }

    // Choose newest/oldest origin time of output

    if (accepted_files.empty())
      origintime = qi.OriginTime();
    else if (newestorigin && qi.OriginTime() > origintime)
      origintime = qi.OriginTime();
    else if (!newestorigin && qi.OriginTime() < origintime)
      origintime = qi.OriginTime();

    int accepted_count = 0;

    qi.FirstTime();
    do
    {
      if (qi.ValidTime() >= firsttime && qi.ValidTime() <= lasttime)
      {
        if (!timelist.Find(qi.ValidTime()))
        {
          if (verbose)
            cerr << "\taccepted " << qi.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr() << " ("
                 << qi.ValidTime().DifferenceInHours(now) << "h)" << endl;
          timelist.Add(new NFmiMetTime(qi.ValidTime()), false, false);
          ++accepted_count;
        }
      }
      else
      {
        if (verbose)
          cerr << "\tnot accepted " << qi.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr() << " ("
               << qi.ValidTime().DifferenceInHours(now) << "h)" << endl;
      }
    } while (qi.NextTime() && qi.ValidTime() <= lasttime);

    if (accepted_count > 0)
    {
      accepted_files.insert(make_pair(qi.OriginTime(), filename));
      pbag = pbag.Combine(*qi.ParamDescriptor().ParamBag());
    }
  }

  // It is an error if there are no timestamps in the desired range

  if (accepted_files.empty())
  {
    cerr << "Error: There is no data for the requested time range" << endl;
    return 1;
  }

  // Now a second pass reads all files which contained valid time stamps

  NFmiQueryData *outqd = nullptr;
  NFmiFastQueryInfo *outqi = nullptr;

  // This will contain all times that have already been handled
  set<NFmiMetTime> handled_times;

  for (auto it = accepted_files.rbegin(); it != accepted_files.rend(); ++it)
  {
    const string &filename = it->second;
    if (verbose)
      cerr << "Reading " << filename << endl;

    NFmiQueryData qd(filename);
    NFmiFastQueryInfo qi(&qd);

    // If first file, create output file

    if (outqd == nullptr)
    {
      NFmiTimeBag timeBag;
      bool fUseTimeBag = false;
      if (cmdline.isOption(
              't'))  // jos t-optio päällä, yritetään konvertoida timebagiksi jos mahdollista
        fUseTimeBag = ConvertTimeList2TimeBag(timelist, timeBag);  // muutetaan jos mahd.
      NFmiTimeDescriptor tmpTimeDesc = fUseTimeBag ? NFmiTimeDescriptor(origintime, timeBag)
                                                   : NFmiTimeDescriptor(origintime, timelist);
      tmpTimeDesc.OriginTime(origintime);
      NFmiQueryInfo tmpInfo(
          NFmiParamDescriptor(pbag), tmpTimeDesc, qi.HPlaceDescriptor(), qi.VPlaceDescriptor());
      tmpInfo.InfoVersion(qi.InfoVersion());

      // Change producer name if so requested
      NFmiProducer producer(*tmpInfo.Producer());
      if (cmdline.isOption('N'))
        producer.SetName(cmdline.OptionValue('N'));
      if (cmdline.isOption('D'))
        producer.SetIdent(NFmiStringTools::Convert<unsigned long>(cmdline.OptionValue('D')));
      if (cmdline.isOption('N') || cmdline.isOption('D'))
        tmpInfo.SetProducer(producer);

      if (outfile == "-")
        outqd = NFmiQueryDataUtil::CreateEmptyData(tmpInfo);
      else
        outqd = NFmiQueryDataUtil::CreateEmptyData(tmpInfo, outfile, true);
      outqi = new NFmiFastQueryInfo(outqd);
    }

    // Check whether point data must be combined slowly

    bool slowcopy = false;
    {
      if (!outqi->IsGrid() && !qi.IsGrid())
      {
        for (outqi->ResetLocation(), qi.ResetLocation();
             outqi->NextLocation() && qi.NextLocation();)
        {
          if (outqi->Location()->GetIdent() != qi.Location()->GetIdent())
          {
            slowcopy = true;
            break;
          }
        }
      }
    }

    if (verbose && slowcopy)
      cerr << "Must perform slow copy of point data, locations differ\n";

    // Collect time indexes which will be copied, from and to

    std::vector<unsigned long> input_time_indexes;
    std::vector<unsigned long> output_time_indexes;

    for (qi.ResetTime(); qi.NextTime();)
    {
      if (timelist.Find(qi.ValidTime()) &&
          handled_times.find(qi.ValidTime()) == handled_times.end() && outqi->Time(qi.ValidTime()))
      {
        input_time_indexes.push_back(qi.TimeIndex());
        output_time_indexes.push_back(outqi->TimeIndex());
        handled_times.insert(qi.ValidTime());
        if (verbose)
          cerr << "\ttaking " << qi.ValidTime().ToStr(kYYYYMMDDHHMM).CharPtr() << endl;
      }
    }

    // Copy

    for (qi.ResetParam(); qi.NextParam();)
    {
      if (outqi->Param(qi.Param()))
      {
        if (!slowcopy)
        {
          for (outqi->ResetLocation(), qi.ResetLocation();
               outqi->NextLocation() && qi.NextLocation();)
          {
            for (outqi->ResetLevel(), qi.ResetLevel(); outqi->NextLevel() && qi.NextLevel();)
            {
              for (size_t i = 0; i < input_time_indexes.size(); i++)
              {
                qi.TimeIndex(input_time_indexes[i]);
                outqi->TimeIndex(output_time_indexes[i]);
                outqi->FloatValue(qi.FloatValue());
              }
            }
          }
        }

        else  // slow copy
        {
          // Resolution changes cause slow copies until
          // old data has been forgotten
          for (outqi->ResetLocation(); outqi->NextLocation();)
          {
            if (qi.Location(outqi->Location()->GetIdent()))
            {
              for (outqi->ResetLevel(), qi.ResetLevel(); outqi->NextLevel() && qi.NextLevel();)
              {
                for (size_t i = 0; i < input_time_indexes.size(); i++)
                {
                  qi.TimeIndex(input_time_indexes[i]);
                  outqi->TimeIndex(output_time_indexes[i]);
                  outqi->FloatValue(qi.FloatValue());
                }
              }
            }
          }
        }
      }
      else if (verbose)
      {
        cerr << "Warning: Parameter " << qi.Param() << " is not available in all datas" << endl;
      }
    }
  }

  // Done

  if (outfile == "-")
    cout << *outqd;

  // Clean up memory, flush mmapped file to disk

  delete outqi;
  delete outqd;
}

// ======================================================================
