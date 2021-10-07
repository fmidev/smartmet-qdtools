// ======================================================================
/*!
 * \file
 * \brief The main program for pgm2qd
 */
// ======================================================================

#include "Pgm2QueryData.h"

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiStringTools.h>

#include <fstream>
#include <iostream>
#include <list>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * Change original suffix to the given one. If there is no original
 * suffix, one will be added.
 *
 * \param theName The original name
 * \param theSuffix The desired suffix
 * \return The changed name
 */
// ----------------------------------------------------------------------

std::string changesuffix(const std::string &theName, const std::string &theSuffix)
{
  string::size_type pos = theName.rfind('.');
  if (pos == string::npos)
    return theName + '.' + theSuffix;
  else
    return theName.substr(0, pos + 1) + theSuffix;
}

// ----------------------------------------------------------------------
/*!
 * \brief Global instance of command line options
 */
// ----------------------------------------------------------------------

FMI::RadContour::PgmReadOptions options;

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: pgm2qd [options] inputdata outputdata\n"
          "\n"
          "pgm2qd converts RADAR formatted pgm files into querydata\n"
          "\n"
          "The available options are:\n"
          "\n"
          "  -h\t\tprint this usage information\n"
          "  -v\t\tverbose mode\n"
          "  -f\t\tforce rewrite if already converted\n"
          "  -e ellipsoid\treference ellipsoid, default=intl (see 'cs2cs -le' for a list of "
          "ellipsoids)\n"
          "  -t dir\ttemporary work directory to use (default is /tmp)\n"
          "  -p int,name\tset producer id and name (default = 1014,NRD)\n"
          "  -l minutes\tset age limit for input files (default = none)\n"
          "\n";
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the command line
 */
// ----------------------------------------------------------------------

int parse_command_line(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "hvfp!t!l!");

  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  if (cmdline.isOption('h'))
  {
    usage();
    return false;
  }

  if (cmdline.NumberofParameters() != 2)
    throw runtime_error("Exactly two command line parameters are expected");

  options.indata = cmdline.Parameter(1);
  options.outdata = cmdline.Parameter(2);

  if (cmdline.isOption('v'))
    options.verbose = true;

  if (cmdline.isOption('f'))
    options.force = true;

  if (cmdline.isOption('t'))
    options.tmpdir = cmdline.OptionValue('t');

  if (cmdline.isOption('p'))
  {
    vector<string> tmp = NFmiStringTools::Split(cmdline.OptionValue('p'));
    if (tmp.size() != 2)
      throw runtime_error("Invalid argument to option -p, int,name expected");
    options.producer_number = NFmiStringTools::Convert<int>(tmp[0]);
    options.producer_name = tmp[1];
  }

  if (cmdline.isOption('l'))
  {
    options.agelimit = NFmiStringTools::Convert<int>(cmdline.OptionValue('l'));
    if (options.agelimit <= 0)
      throw runtime_error("Age limit given by -l must be positive");
  }

  if (cmdline.isOption('e'))
    options.ellipsoid = cmdline.OptionValue('e');

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

int run(int argc, const char *argv[])
{
  // Parse the command line arguments
  if (!parse_command_line(argc, argv))
    return 0;

  std::list<std::string> infiles;

  if (NFmiFileSystem::DirectoryExists(options.indata))
  {
    infiles = NFmiFileSystem::DirectoryFiles(options.indata);
    std::list<std::string>::iterator it;
    for (it = infiles.begin(); it != infiles.end(); ++it)
      *it = options.indata + '/' + (*it);
  }
  else
  {
    if (!NFmiFileSystem::FileExists(options.indata))
      throw runtime_error("Filename " + options.indata + " does not exist");
    infiles.push_back(options.indata);
  }

  // Now convert all files in the list
  // If the output file already exists, no conversion is done
  // unless -f was used

  std::list<std::string>::const_iterator it;
  for (it = infiles.begin(); it != infiles.end(); ++it)
  {
    NFmiQueryData *data = FMI::RadContour::Pgm2QueryData(*it, options, cout);
    if (data)
    {
      // Establish the output name for the file
      std::string outname = options.outdata + '/';
      outname += NFmiStringTools::Basename(changesuffix(*it, "sqd"));

      if (NFmiFileSystem::FileExists(outname) && !options.force)
      {
        if (options.verbose)
          cout << "Already converted " << *it << " to " << outname << endl;
        continue;
      }

      // Now we have a NFmiFastQueryInfo in variable info for
      // saving in file outname

      // We must use a temporary file name to prevent triggers

      string tmpname = options.tmpdir + '/' + NFmiStringTools::Basename(*it);

      std::ofstream tmpfile(tmpname.c_str(), ios::binary | ios::out);
      if (!tmpfile)
      {
        cout << "Could not write " << tmpname << endl;
        continue;
      }

      if (options.verbose)
        cout << "writing " << outname << endl;
      data->UseBinaryStorage(true);
      tmpfile << *data;
      tmpfile.close();

      if (!NFmiFileSystem::RenameFile(tmpname, outname))
      {
        cout << "Could not rename " << tmpname << " to outname" << endl;
        continue;
      }
    }
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
try
{
  return run(argc, argv);
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

// ======================================================================
