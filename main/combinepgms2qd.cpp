// ======================================================================
/*!
 * \file
 * \brief The main program for combinepgms2qd
 */
// ======================================================================

#include "Pgm2QueryData.h"

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>

#include <boost/shared_ptr.hpp>

#include <fstream>
#include <iostream>
#include <list>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: combinepgms2qd [options] inputfilepattern qdfileout" << endl
       << endl
       << "combinepgms2qd converts several RADAR formatted pgm files into" << endl
       << "single querydata, purpose is to make 3D queryDatas from radar slices" << endl
       << endl
       << "The available options are:" << endl
       << endl
       << "  -h\t\tprint this usage information" << endl
       << "  -v\t\tverbose mode" << endl
       << "  -p int,name\tset producer id and name (default = 1014,NRD)" << endl
       << "  -t timestepcount\thow many timesteps in result data (default = 0 (= all possible))"
       << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 */
// ----------------------------------------------------------------------

int parse_command_line(int argc, const char *argv[], FMI::RadContour::PgmReadOptions &theOptions)
{
  NFmiCmdLine cmdline(argc, argv, "hvp!t!");

  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  if (cmdline.NumberofParameters() != 2)
    throw runtime_error("Exactly two command line parameters are expected");

  theOptions.indata = cmdline.Parameter(1);
  theOptions.outdata = cmdline.Parameter(2);

  if (cmdline.isOption('v')) theOptions.verbose = true;

  if (cmdline.isOption('t'))
  {
    theOptions.maxtimesteps = NFmiStringTools::Convert<int>(cmdline.OptionValue('t'));
  }

  if (cmdline.isOption('p'))
  {
    vector<string> tmp = NFmiStringTools::Split(cmdline.OptionValue('p'));
    if (tmp.size() != 2) throw runtime_error("Invalid argument to option -p, int,name expected");
    theOptions.producer_number = NFmiStringTools::Convert<int>(tmp[0]);
    theOptions.producer_name = tmp[1];
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * The main program
 *
 * \param argc The number of arguments on the command line
 * \param argv The argument strings
 * \return Program exit code
 */
// ----------------------------------------------------------------------

int domain(int argc, const char *argv[])
{
  FMI::RadContour::PgmReadOptions options;

  // Parse the command line arguments
  if (!parse_command_line(argc, argv, options)) return 0;

  std::list<std::string> infiles = NFmiFileSystem::PatternFiles(options.indata);
  if (infiles.empty())
    throw std::runtime_error(std::string("There were no files from given file-pattern \n") +
                             options.indata + "\n, check the inputfilepattern, ending here...");

  // Now convert all files in the list to queryDatas and combine them
  std::string dirName = NFmiQueryDataUtil::GetFileFilterDirectory(
      options.indata);  // fileFilteristä pitää ottaa hakemisto irti, koska PatternFiles-funktio
                        // palautta vain tiedostojen nimet, ei polkua mukana
  std::vector<boost::shared_ptr<NFmiQueryData> > qDataList;
  std::list<std::string>::const_iterator it;
  for (it = infiles.begin(); it != infiles.end(); ++it)
  {
    NFmiQueryData *data = FMI::RadContour::Pgm2QueryData(dirName + *it, options, cout);
    if (data) qDataList.push_back(boost::shared_ptr<NFmiQueryData>(data));
  }

  // create combined qinfo
  boost::shared_ptr<NFmiQueryData> dummyBaseData;  // tätä ei käytetä, eli luodaan '0'-pointteri,
                                                   // koska NFmiQueryDataUtil::CombineQueryDatas
                                                   // vaatii tälläistä mahd. pohjadataa
  NFmiQueryData *data = NFmiQueryDataUtil::CombineQueryDatas(
      false,
      dummyBaseData,
      qDataList,
      false,
      options.maxtimesteps,
      0);  // false = yhdistely sliceista, 0 = ei lopetus funktoria käytössä
  if (data == 0)
    throw std::runtime_error(std::string("Error, unable to create combined queryData"));

  std::unique_ptr<NFmiQueryData> dataPtr(data);  // tuhoaa automaattisesti datan lopuksi
  NFmiStreamQueryData sQData;
  if (sQData.WriteData(options.outdata, data, static_cast<long>(data->InfoVersion())) == false)
    throw std::runtime_error(std::string("Error, unable to store combined queryData to file:\n") +
                             options.outdata);
  else
  {
    if (options.verbose) cout << std::string("Writing to file: ") + options.outdata << endl;
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * The main program
 *
 * \param argc The number of arguments on the command line
 * \param argv The argument strings
 * \return Program exit code
 */
// ----------------------------------------------------------------------

int main(int argc, const char *argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (exception &e)
  {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }
  catch (...)
  {
    cerr << "Error: Caught an unknown exception" << endl;
    return 1;
  }
}

// ======================================================================
