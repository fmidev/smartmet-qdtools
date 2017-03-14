// ======================================================================
/*!
 * \brief Extract point data from given grid data
 */
// ======================================================================

#include <newbase/NFmiCommentStripper.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiLocationBag.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>

#include <boost/program_options.hpp>

#include <stdexcept>
#include <string>

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  std::string infile;
  std::string outfile;
  std::string locationfile;
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options() : infile("-"), outfile("-"), locationfile("") {}
// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char* argv[])
{
  namespace po = boost::program_options;

  po::options_description desc("Available options");
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "infile,i", po::value(&options.infile), "input querydata")(
      "outfile,o", po::value(&options.outfile), "output querydata")(
      "locations,l", po::value(&options.locationfile), "location descriptions");

  po::positional_options_description p;
  p.add("locations", 1);
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "qdextract v1.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: qdextract [options] locations [inputdata [outputdata]] " << std::endl
              << std::endl
              << "Extract point data from gridded data or point data." << std::endl
              << "I/O can be specified in multiple ways:" << std::endl
              << std::endl
              << "   qdextract -i input.sqd -o output.sqd locations" << std::endl
              << "   qdextract locations input.sqd output.sqd" << std::endl
              << "   qdextract locations < input.sqd > output.sqd" << std::endl
              << std::endl
              << "Using '-' as input or output filename implies stdin/stdout" << std::endl
              << "Using a directory name or a filename enables memory mapping for efficiency"
              << std::endl
              << std::endl
              << desc << std::endl
              << std::endl
              << "Location files may contain C++ style comments. Once stripped, the file"
              << std::endl
              << "is expected to begin with a number describing the format of the locations"
              << std::endl
              << "as follows:" << std::endl
              << std::endl
              << "  1: a NFmiLocationBag C++ object follows" << std::endl
              << std::endl
              << "  2: a location list follows with rows of form" << std::endl
              << "     id" << std::endl
              << "     unused line" << std::endl
              << "     name" << std::endl
              << "     lon lat" << std::endl
              << std::endl
              << "  3: a free form location list with the following order of fields" << std::endl
              << "     id name lon lat" << std::endl
              << std::endl
              << "Using format 3 is recommended for clarity." << std::endl
              << std::endl;
    return false;
  }

  if (opt.count("locations") == 0)
    throw std::runtime_error("Excpecting locations file as parameter 1");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read location description from file
 */
// ----------------------------------------------------------------------

NFmiLocationBag ReadLocationsFromFile(const std::string& locationfile, NFmiFastQueryInfo& qi)
{
  using namespace std;

  NFmiCommentStripper stripComments;
  if (!stripComments.ReadAndStripFile(locationfile))
    throw std::runtime_error("Failed to read and preprocess file '" + locationfile + "'");

  NFmiLocationBag bag;

  // Mostly copied from old extractLocations.cpp

  stringstream strippedControlFile(stripComments.GetString());

  // jos tiedoston aluksi 1 on locationbag-file ja jos 2 on kyseessä> locationlist tyyppinen
  // tiedosto

  int locationFileType = 0;
  strippedControlFile >> locationFileType;
  if (locationFileType == 1)
  {
    strippedControlFile >> bag;
    if (strippedControlFile.fail())
      throw std::runtime_error("Failed to read a location bag from '" + locationfile + "'");
    return bag;
  }

  if (locationFileType == 2)
  {
    qi.First();
    NFmiProducer producer(*qi.Producer());
    char locName[512] = "";
    char buffer[512] = "";
    for (;;)
    {
      int locId = 0;
      strippedControlFile >> locId;
      strippedControlFile.getline(buffer, 511);
      strippedControlFile.getline(locName, 511);
      double lat, lon;
      strippedControlFile >> lon >> lat;
      if (!strippedControlFile.fail())
      {
        if (qi.Location(locId))
        {  // jos haluttu location löytyy sourcedatasta, otetaan se tähänkin locationbagiin
          bag.AddLocation(*qi.Location(), false);
        }
        else
        {
          NFmiStation location(locId, locName, lon, lat);
          bag.AddLocation(location, false);
        }
      }
      else  // kun tullaan eof:än, lopetetaan
        return bag;
    }
  }

  if (locationFileType == 3)
  {
    qi.First();

    NFmiProducer producer(*qi.Producer());
    int locId;
    string locName;
    double lat, lon;
    while (strippedControlFile >> locId >> locName >> lon >> lat)
    {
      if (qi.Location(locId))
      {
        // jos haluttu location löytyy sourcesta, otetaan se tähänkin locationbagiin
        bag.AddLocation(*qi.Location(), false);
      }
      else
      {
        NFmiStation location(locId, locName, lon, lat);
        bag.AddLocation(location, false);
      }
    }
    if (!strippedControlFile.eof())
      throw std::runtime_error("Syntax error when reading locations from '" + locationfile + "'");
    return bag;
  }

  throw std::runtime_error("Unknown location type. Allowed values: 1,2,3");
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv)) return 0;

  // Read input data

  NFmiQueryData qd(options.infile);
  NFmiFastQueryInfo qi(&qd);

  // Read locations

  NFmiLocationBag locations = ReadLocationsFromFile(options.locationfile, qi);

  NFmiQueryData* outqd = NFmiQueryDataUtil::ExtractLocations(&qd, locations, kLinearly);
  outqd->Write(options.outfile);

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char* argv[]) try
{
  return run(argc, argv);
}
catch (std::exception& e)
{
  std::cerr << "Error: " << e.what() << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Error: Caught an unknown exception" << std::endl;
  return 1;
}
