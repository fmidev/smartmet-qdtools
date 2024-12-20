#include "nctools.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
#include <macgyver/CsvReader.h>
#include <macgyver/Exception.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiStringTools.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <list>
#include <string>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

int nctools::unknownParIdCounterBegin = 30000;

namespace
{
// ----------------------------------------------------------------------
/*!
 * Newbase parameter names
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter;
std::map<std::string, int>
    unknownParIdMap;  // jos sallitaan tuntemattomien parametrien käyttä, ne talletetaan tähän
int unknownParIdCounter = nctools::unknownParIdCounterBegin;  // jos tuntematon paramtri, aloitetaan
                                                              // niiden id:t tästä ja kasvatetaan
                                                              // aina yhdellä kun tulee uusia
}  // namespace

namespace nctools
{
NFmiEnumConverter &get_enumconverter(void)
{
  return converter;
}
// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 *
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char *argv[], Options &options)
{
  namespace po = boost::program_options;
  namespace fs = std::filesystem;

  std::string producerinfo;

  std::string msg1 =
      "Replacement NetCDF CF standard name conversion table (default='" + options.configfile + "')";
  std::string tmpIgnoreUnitChangeParamsStr;
  std::string tmpExcludeParamsStr;
  std::string tmpCmdLineGlobalAttributesStr;
  std::string tmpAddParamsStr;

#ifdef UNIX
  struct winsize wsz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz);
  const int desc_width = (wsz.ws_col < 80 ? 80 : wsz.ws_col);
#else
  const int desc_width = 100;
#endif

  std::string default_tdim = (options.tdim ? *options.tdim : std::string());
  std::string default_xdim = (options.xdim ? *options.xdim : std::string());
  std::string default_ydim = (options.ydim ? *options.ydim : std::string());
  std::string default_zdim = (options.zdim ? *options.zdim : std::string());

  po::options_description desc("Allowed options", desc_width);
  desc.add_options()("help,h", "print out help message")(
      "config,c", po::value(&options.configfile), msg1.c_str())(
      "configs,C",
      po::value(&options.configs),
      "Extra NetCDF name conversions (take precedence over standard names)")(
      "conventions,n",
      po::value(&options.conventions),
      ("Minimum NetCDF conventions to verify or empty string if no check wanted (default: " +
       options.conventions + ")")
          .c_str())("debug,d", po::bool_switch(&options.debug), "enable debugging output")(
      "tdim",
      po::value<std::string>() /* (&options.tdim) */,
      ("name of T-dimension parameter (default=" + default_tdim + ")").c_str())(
      "xdim",
      po::value<std::string>() /* (&options.xdim) */,
      ("name of X-dimension parameter (default=" + default_xdim + ")").c_str())(
      "ydim",
      po::value<std::string>() /* (&options.ydim) */,
      ("name of Y-dimension parameter (default=" + default_ydim + ")").c_str())(
      "zdim",
      po::value<std::string>() /* (&options.zdim) */,
      ("name of Z-dimension parameter (default=" + default_zdim + ")").c_str())(
      "info", po::bool_switch(&options.info), "print information on data dimensions and exit")(
      "experimental", po::bool_switch(&options.experimental), "enable experimental features")(
      "infile,i", po::value(&options.infiles), "input netcdf file")(
      "globalAttributes,a",
      po::value(&tmpCmdLineGlobalAttributesStr),
      "netCdf data's cmd-line given global attributes")(
      "mmap", po::bool_switch(&options.memorymap), "memory map output file to save RAM")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "parameter,m",
      po::value(&options.parameters),
      "define parameter conversion(same format as in config)")(
      "producer,p", po::value(&producerinfo), "producer number,name")(
      "projection,P", po::value(&options.projection), "final data area projection")(
      "producernumber", po::value(&options.producernumber), "producer number")(
      "producername", po::value(&options.producername), "producer name")(
      "fixstaggered,s",
      po::bool_switch(&options.fixstaggered),
      "modifies staggered data to base form")("ignoreunitchangeparams,u",
                                              po::value(&tmpIgnoreUnitChangeParamsStr),
                                              "ignore unit change params")(
      "timeshift,t", po::value(&options.timeshift), "additional time shift in minutes")(
      "tolerance,T", po::value(&options.tolerance), "axis stepping tolerance")(
      "autoids,U",
      po::bool_switch(&options.autoid),
      ((std::string) "generate ids automatically for unknown parameters starting from id " +
       std::to_string((int)nctools::unknownParIdCounterBegin))
          .c_str())("verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "version,V", "display version number")(
      "excludeparams,x", po::value(&tmpExcludeParamsStr), "exclude params")(
      "addparams", po::value(&tmpAddParamsStr), "add parameters by calculating them");

  po::positional_options_description p;
  if (strstr(argv[0], "wrftoqd") != nullptr)
  {
    p.add("infile", 1);
    p.add("outfile", 1);
  }
  else
  {
    // We don't know beforehand whether there is an output file or multiple inputs
    p.add("infile", -1);
  }

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << argv[0] << " v1.3 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    if (strstr(argv[0], "wrftoqd") != nullptr)
    {
      std::cout << "Usage: wrfoqd [options] infile outfile" << std::endl
                << std::endl
                << "Converts Weather Research and Forecasting (WRF) Model NetCDF to querydata."
                << std::endl
                << std::endl
                << desc << std::endl;
    }
    else
    {
      std::cout << "Usage:\n"
                   "  nctoqd [options] infile outfile\n"
                   "  nctoqd [options] -o outfile infile ...\n"
                   "\n"
                   "Converts CF-1.4 conforming NetCDF to querydata.\n"
                   "\n"
                << desc
                << "\n"
                   "Parameters supported by --addparams: Humidity\n"
                << std::endl;
    }

    return false;
  }

  if (opt.count("tdim"))
    options.tdim = opt["tdim"].as<std::string>();
  if (opt.count("xdim"))
    options.xdim = opt["xdim"].as<std::string>();
  if (opt.count("ydim"))
    options.ydim = opt["ydim"].as<std::string>();
  if (opt.count("zdim"))
    options.zdim = opt["zdim"].as<std::string>();

  if (strstr(argv[0], "wrftoqd") != nullptr)
  {
    // Running wrftoqd
    if (opt.count("infile") == 0)
      throw std::runtime_error("Expecting input file as parameter 1");

    if (opt.count("infile") > 2)
      throw std::runtime_error("Multiple input files for wrtoqd not supported");

    if (!fs::exists(options.infiles[0]))
      throw std::runtime_error("Input file '" + options.infiles[0] + "' does not exist");
  }
  else
  {
    // Running nctoqd
    if (opt.count("infile") == 0)
      throw std::runtime_error("Expecting input file as parameter 1");

    if (!options.info)
    {
      if (opt.count("outfile") == 0)
      {
        // Output file not explicitly specified
        if (options.infiles.size() > 2)
          throw std::runtime_error(
              "You must specifify desired output file with -o parameter for multiple inputs");
        if (options.infiles.size() == 1)
          throw std::runtime_error(
              "Must specify output file either with the -o option or as the last parameter");
        options.outfile = options.infiles[1];
        options.infiles.pop_back();  // Remove the output element which was the last argument
      }
    }

    for (auto infile : options.infiles)
    {
      if (!fs::exists(infile))
        throw std::runtime_error("Input file '" + infile + "' does not exist");
    }
  }

  if (options.memorymap && options.outfile == "-")
    throw std::runtime_error("Cannot memory map standard output");

  // Parse parameter settings

  if (!producerinfo.empty())
  {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, producerinfo, boost::algorithm::is_any_of(","));
    if (parts.size() != 2)
      throw std::runtime_error("Option --producer expects a comma separated number,name argument");

    options.producernumber = std::stol(parts[0]);
    options.producername = parts[1];
  }

  if (!tmpIgnoreUnitChangeParamsStr.empty())
    options.ignoreUnitChangeParams =
        NFmiStringTools::Split<std::list<std::string>>(tmpIgnoreUnitChangeParamsStr, ",");

  if (!tmpExcludeParamsStr.empty())
    options.excludeParams =
        NFmiStringTools::Split<std::list<std::string>>(tmpExcludeParamsStr, ",");

  if (!tmpAddParamsStr.empty())
    options.addParams = NFmiStringTools::Split<std::list<std::string>>(tmpAddParamsStr, ",");

  if (!tmpCmdLineGlobalAttributesStr.empty())
  {
    // globaalit attribuutit annetaan muodossa -a DX=1356.3;DY=1265.3, eli eri attribuutit on
    // erotelty ;-merkeillä ja avain/arvot on eroteltu = -merkeillä
    std::list<std::string> attributeListParts =
        NFmiStringTools::Split<std::list<std::string>>(tmpCmdLineGlobalAttributesStr, ";");
    for (std::list<std::string>::iterator it = attributeListParts.begin();
         it != attributeListParts.end();
         ++it)
    {
      std::vector<std::string> attributeParts =
          NFmiStringTools::Split<std::vector<std::string>>(*it, "=");
      if (attributeParts.size() == 2)
        options.cmdLineGlobalAttributes.insert(
            std::make_pair(attributeParts[0], attributeParts[1]));
      else
        throw std::runtime_error(
            "Option -a (global attributes) was ilformatted, give option in following format:\n-a "
            "DX=1356.3;DY=1265.3");
    }
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the NetCDF parameter conversion file
 */
// ----------------------------------------------------------------------

ParamConversions read_netcdf_configs(const Options &options)
{
  using namespace std::placeholders;
  CsvParams csv(options);
  //  Parameter list is read starting from beginning so put most important ones first

  // Command line
  if (options.parameters.size() > 0)
    for (auto line : options.parameters)
      try
      {
        if (line.length() < 1)
          throw Fmi::Exception(BCP, "A parameter given on command line is of zero length");
        if (options.verbose)
          std::cout << "Adding parameter mapping " << line << std::endl;
        std::vector<std::string> row;
        std::size_t delimpos = line.find(',');
        if (delimpos < 1 || delimpos >= line.length() - 1)
          throw Fmi::Exception(BCP,
                               "Parameter from command line is not of correct format: " + line);
        row.push_back(line.substr(0, delimpos));
        row.push_back(line.substr(delimpos + 1));
        csv.paramconvs.push_back(row);
      }
      catch (...)
      {
        throw Fmi::Exception(
            BCP, "Adding parameter conversion " + line + " from command line failed", nullptr);
      }

  // Additional config files
  if (options.configs.size() > 0)
    for (auto file : options.configs)
      try
      {
        if (options.verbose)
          std::cout << "Reading " << file << std::endl;

        Fmi::CsvReader::read(file, std::bind(&CsvParams::add, &csv, _1));
      }
      catch (...)
      {
        throw Fmi::Exception(BCP, "Reading config file " + file + " failed", nullptr);
      }

  // Base config file
  if (!options.configfile.empty())
    try
    {
      if (options.verbose)
        std::cout << "Reading " << options.configfile << std::endl;

      Fmi::CsvReader::read(options.configfile, std::bind(&CsvParams::add, &csv, _1));
    }
    catch (...)
    {
      throw Fmi::Exception(
          BCP, "Reading base config file " + options.configfile + " failed", nullptr);
    }

  return csv.paramconvs;
}

CsvParams::CsvParams(const Options &optionsIn) : paramconvs(), options(optionsIn) {}
void CsvParams::add(const Fmi::CsvReader::row_type &row)
{
  if (row.size() == 0)
    return;
  if (row[0].substr(0, 1) == "#")
    return;

  if (row.size() != 2 && row.size() != 4)
    throw Fmi::Exception(
        BCP,
        (std::string) "Invalid config row of size " + std::to_string(row.size()) + ": " + row[0]);

  paramconvs.push_back(row);
}

// ----------------------------------------------------------------------
/*!
 * Parse parameter id
 *
 * Possible list structures:
 *
 *  netcdf,newbase
 *  netcdf_x,netcdf_y,newbase_speed,newbase_direction
 */
// ----------------------------------------------------------------------

static FmiParameterName getIdFromString(NFmiEnumConverter &converter, const std::string &name)
{
  // Convert numbers directly to int, other through the converter
  if (name.empty())
    return kFmiBadParameter;

  // Is it a name?
  if (name.find_first_not_of("012345678") != std::string::npos)
    return FmiParameterName(converter.ToEnum(name));

  // Fully numeric, just return the id
  return static_cast<FmiParameterName>(std::stoi(name));
}

ParamInfo parse_parameter(const std::string &name,
                          const ParamConversions &paramconvs,
                          bool useAutoGeneratedIds)
{
  ParamInfo info;

  for (const ParamConversions::value_type &vt : paramconvs)
  {
    if (vt.size() == 2)
    {
      if (name == vt[0])
      {
        info.id = getIdFromString(converter, vt[1]);
        info.name = vt[1];
        return info;
      }
    }
    else
    {
      if (name == vt[0])
      {
        info.id = getIdFromString(converter, vt[2]);
        info.name = vt[2];
        info.isregular = false;
        info.isspeed = true;
      }
      else if (name == vt[1])
      {
        info.id = getIdFromString(converter, vt[3]);
        info.name = vt[3];
        info.isregular = false;
        info.isspeed = false;
      }
      if (!info.isregular)
      {
        info.x_component = vt[0];
        info.y_component = vt[1];
        return info;
      }
    }
  }

  // Try newbase as fail safe
  info.id = getIdFromString(converter, name);
  if (info.id != kFmiBadParameter)
  {
    info.name = converter.ToString(info.id);
  }
  else
  {
    // We either have a numeric conversion done or should use autogenerated ids
    // Let's first check for already encountered ids
    std::map<std::string, int>::iterator it = unknownParIdMap.find(name);
    if (it != unknownParIdMap.end())
    {
      info.name = name;
      info.id = static_cast<FmiParameterName>(it->second);
    }
    else if (info.id > 0 || useAutoGeneratedIds)
    {
      // We either had a straight fixed numeric conversion or want autogenerated
      if (info.id < 1)  // Predefined(on cli or in config) have larger id, this is autogenerated
        info.id = static_cast<FmiParameterName>(unknownParIdCounter++);
      unknownParIdMap.insert(std::make_pair(name, info.id));
      info.name = name;
    }
  }

  return info;
}

#if DEBUG_PRINT
void print_att(const NcAtt &att)
{
  std::cerr << "\tname = " << att.name() << std::endl
            << "\ttype = " << int(att.type()) << std::endl
            << "\tvalid = " << att.is_valid() << std::endl
            << "\tnvals = " << att.num_vals() << std::endl;

  NcValues *values = att.values();

  switch (att.type())
  {
    case ncByte:
      for (int i = 0; i < att.num_vals(); i++)
        std::cerr << "\tBYTE " << i << ":" << values->as_char(i) << std::endl;
      break;
    case ncChar:
      std::cerr << "\tCHAR: '" << values->as_string(0) << "'" << std::endl;
      break;
    case ncShort:
      for (int i = 0; i < att.num_vals(); i++)
        std::cerr << "\tSHORT " << i << ":" << values->as_short(i) << std::endl;
      break;
    case ncInt:
      for (int i = 0; i < att.num_vals(); i++)
        std::cerr << "\tINT " << i << ":" << values->as_int(i) << std::endl;
      break;
    case ncFloat:
      for (int i = 0; i < att.num_vals(); i++)
        std::cerr << "\tFLOAT " << i << ":" << values->as_float(i) << std::endl;
      break;
    case ncDouble:
      for (int i = 0; i < att.num_vals(); i++)
        std::cerr << "\tDOUBLE " << i << ":" << values->as_double(i) << std::endl;
      break;
    default:
      break;
  }
}

void debug_output(const NcFile &ncfile)
{
  std::cerr << "num_dims: " << ncfile.num_dims() << std::endl
            << "num_vars: " << ncfile.num_vars() << std::endl
            << "num_atts: " << ncfile.num_atts() << std::endl;

  for (int i = 0; i < ncfile.num_dims(); i++)
  {
    NcDim *dim = ncfile.get_dim(i);
    std::cerr << "Dim " << i << "\tname = " << dim->name() << std::endl
              << "\tsize = " << dim->size() << std::endl
              << "\tvalid = " << dim->is_valid() << std::endl
              << "\tunlimited = " << dim->is_unlimited() << std::endl
              << "\tid = " << dim->id() << std::endl;
  }

  for (int i = 0; i < ncfile.num_atts(); i++)
  {
    NcAtt *att = ncfile.get_att(i);
    std::cerr << "Att " << i << std::endl;
    print_att(*att);
  }

  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar *var = ncfile.get_var(i);

    for (int j = 0; j < var->num_dims(); j++)
    {
      NcDim *dim = var->get_dim(j);
      std::cerr << "    Dim " << j << "\tname = " << dim->name() << std::endl
                << "\tsize = " << dim->size() << std::endl
                << "\tvalid = " << dim->is_valid() << std::endl
                << "\tunlimited = " << dim->is_unlimited() << std::endl
                << "\tid = " << dim->id() << std::endl;
    }
  }
}

#endif

}  // namespace nctools
