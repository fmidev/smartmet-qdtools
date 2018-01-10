
#include "nctools.h"

#include <macgyver/TimeParser.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiStringTools.h>
#include <spine/Exception.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

namespace
{
// ----------------------------------------------------------------------
/*!
 * Newbase parameter names
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter;
std::map<std::string, int>
    unknownParIdMap;  // jos sallitaan tuntemattomien parametrien k�ytt�, ne talletetaan t�h�n
int unknownParIdCounter = 1200;  // jos tuntematon paramtri, aloitetaan niiden id:t t�st� ja
                                 // kasvatetaan aina yhdell� kun tulee uusia
}  // namespace

namespace nctools
{
// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options()
    : verbose(false),
      outfile("-"),
#ifdef UNIX
      configfile("/usr/share/smartmet/formats/netcdf.conf"),
#else
      configfile("netcdf.conf"),
#endif
      producername("UNKNOWN"),
      producernumber(0),
      timeshift(0),
      memorymap(false),
      fixstaggered(false),
      ignoreUnitChangeParams(),
      excludeParams(),
      projection(),
      cmdLineGlobalAttributes()
{
  debug = false;
  experimental = false;
}

NFmiEnumConverter &get_enumconverter(void) { return converter; }
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
  namespace fs = boost::filesystem;

  std::string producerinfo;

  std::string msg1 =
      "NetCDF CF standard name conversion table (default='" + options.configfile + "')";
  std::string tmpIgnoreUnitChangeParamsStr;
  std::string tmpExcludeParamsStr;
  std::string tmpCmdLineGlobalAttributesStr;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")(
      "debug,d", po::bool_switch(&options.debug), "enable debugging output")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "version,V", "display version number")(
      "experimental,x", po::bool_switch(&options.experimental), "enable experimental features")(
      "infile,i", po::value(&options.infiles), "input netcdf file")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "mmap", po::bool_switch(&options.memorymap), "memory map output file to save RAM")(
      "config,c", po::value(&options.configfile), msg1.c_str())(
      "timeshift,t", po::value(&options.timeshift), "additional time shift in minutes")(
      "producer,p", po::value(&producerinfo), "producer number,name")(
      "producernumber", po::value(&options.producernumber), "producer number")(
      "producername", po::value(&options.producername), "producer name")(
      "fixstaggered,s",
      po::bool_switch(&options.fixstaggered),
      "modifies staggered data to base form")("ignoreunitchangeparams,u",
                                              po::value(&tmpIgnoreUnitChangeParamsStr),
                                              "ignore unit change params")(
      "excludeparams,x", po::value(&tmpExcludeParamsStr), "exclude params")(
      "projection,P", po::value(&options.projection), "final data area projection")(
      "globalAttributes,a",
      po::value(&tmpCmdLineGlobalAttributesStr),
      "netCdf data's cmd-line given global attributes");

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
    if (strstr(argv[0], "wrftoqd") != NULL)
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
      std::cout << "Usage: " << std::endl
                << "  nctoqd [options] infile outfile " << std::endl
                << "  nctoqd [options] -o outfile infile ..." << std::endl
                << "Converts CF-1.4 conforming NetCDF to querydata." << std::endl
                << "Only features in known use are supported." << std::endl
                << std::endl
                << desc << std::endl;
    }

    return false;
  }

  if (strstr(argv[0], "wrftoqd") != NULL)
  {
    // Running wrftoqd
    if (opt.count("infile") == 0) throw std::runtime_error("Expecting input file as parameter 1");

    if (opt.count("infile") > 2)
      throw std::runtime_error("Multiple input files for wrtoqd not supported");

    if (!fs::exists(options.infiles[0]))
      throw std::runtime_error("Input file '" + options.infiles[0] + "' does not exist");
  }
  else
  {
    // Running nctoqd
    if (opt.count("infile") == 0) throw std::runtime_error("Expecting input file as parameter 1");

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
  {
    options.ignoreUnitChangeParams =
        NFmiStringTools::Split<std::list<std::string>>(tmpIgnoreUnitChangeParamsStr, ",");
  }

  if (!tmpExcludeParamsStr.empty())
  {
    options.excludeParams =
        NFmiStringTools::Split<std::list<std::string>>(tmpExcludeParamsStr, ",");
  }

  if (!tmpCmdLineGlobalAttributesStr.empty())
  {
    // globaalit attribuutit annetaan muodossa -a DX=1356.3;DY=1265.3, eli eri attribuutit on
    // erotelty ;-merkeill� ja avain/arvot on eroteltu = -merkeill�
    std::list<std::string> attributeListParts =
        NFmiStringTools::Split<std::list<std::string>>(tmpCmdLineGlobalAttributesStr, ";");
    for (std::list<std::string>::iterator it = attributeListParts.begin();
         it != attributeListParts.end();
         ++it)
    {
      std::vector<std::string> attributeParts =
          NFmiStringTools::Split<std::vector<std::string>>(*it, "=");
      if (attributeParts.size() == 2)
      {
        options.cmdLineGlobalAttributes.insert(
            std::make_pair(attributeParts[0], attributeParts[1]));
      }
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

ParamConversions read_netcdf_config(const Options &options)
{
  CsvParams csv(options);
  if (!options.configfile.empty())
  {
    if (options.verbose) std::cout << "Reading " << options.configfile << std::endl;
    Fmi::CsvReader::read(options.configfile, boost::bind(&CsvParams::add, &csv, _1));
  }
  return csv.paramconvs;
}

CsvParams::CsvParams(const Options &optionsIn) : paramconvs(), options(optionsIn) {}
void CsvParams::add(const Fmi::CsvReader::row_type &row)
{
  if (row.size() == 0) return;
  if (row[0].substr(0, 1) == "#") return;

  if (row.size() != 2 && row.size() != 4)
  {
    std::ostringstream msg;
    msg << "Invalid row of size " << row.size() << " in '" << options.configfile << "':";
    std::ostream_iterator<std::string> out_it(msg, ",");
    std::copy(row.begin(), row.end(), out_it);
    throw std::runtime_error(msg.str());
  }

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

ParamInfo parse_parameter(const std::string &name,
                          const ParamConversions &paramconvs,
                          bool useAutoGeneratedIds)
{
  ParamInfo info;

  BOOST_FOREACH (const ParamConversions::value_type &vt, paramconvs)
  {
    if (vt.size() == 2)
    {
      if (name == vt[0])
      {
        info.id = FmiParameterName(converter.ToEnum(vt[1]));
        info.name = vt[1];
        return info;
      }
    }
    else
    {
      if (name == vt[0])
      {
        info.id = FmiParameterName(converter.ToEnum(vt[2]));
        info.name = vt[2];
        info.isregular = false;
        info.isspeed = true;
      }
      else if (name == vt[1])
      {
        info.id = FmiParameterName(converter.ToEnum(vt[3]));
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
  info.id = FmiParameterName(converter.ToEnum(name));
  if (info.id != kFmiBadParameter) info.name = converter.ToString(info.id);

  if (info.id == kFmiBadParameter && useAutoGeneratedIds)
  {  // Lis�t��n haluttaessa my�s automaattisesti generoitu par-id
    std::map<std::string, int>::iterator it = unknownParIdMap.find(name);
    if (it != unknownParIdMap.end())
    {
      info.name = name;
      info.id = static_cast<FmiParameterName>(it->second);
    }
    else
    {
      unknownParIdMap.insert(std::make_pair(name, unknownParIdCounter));
      info.name = name;
      info.id = static_cast<FmiParameterName>(unknownParIdCounter);
      unknownParIdCounter++;
    }
  }

  return info;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract NetCDF parameter name
 */
// ----------------------------------------------------------------------

std::string get_name(NcVar *var)
{
  NcAtt *att = var->get_att("standard_name");
  if (att == 0) return var->name();

  if (att->type() != ncChar)
    throw std::runtime_error("The standard_name attribute must be a string for variable " +
                             std::string(var->name()));

  return att->values()->as_string(0);
}

// ----------------------------------------------------------------------
/*!
 * Parse parameter id
 */
// ----------------------------------------------------------------------

ParamInfo parse_parameter(NcVar *var, const ParamConversions &paramconvs, bool useAutoGeneratedIds)
{
  if (var->num_dims() < 3 || var->num_dims() > 4) return ParamInfo();

  return parse_parameter(get_name(var), paramconvs, useAutoGeneratedIds);
}

// ----------------------------------------------------------------------
/*!
 * \brief Find variable with given name
 */
// ----------------------------------------------------------------------

NcVar *NcFileExtended::find_variable(const std::string &name)
{
  for (int i = 0; i < this->num_vars(); i++)
  {
    NcVar *var = this->get_var(i);
    if (var == nullptr) continue;
    if (get_name(var) == name) return var;
  }
  return NULL;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the missing value
 */
// ----------------------------------------------------------------------

float get_missingvalue(NcVar *var)
{
  NcAtt *att = var->get_att("_FillValue");
  if (att != 0) return att->values()->as_float(0);
  // Fillvalue can be also fill_value
  att = var->get_att("fill_value");
  if (att != 0)
    return att->values()->as_float(0);
  else
    return std::numeric_limits<float>::quiet_NaN();
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the scale value
 */
// ----------------------------------------------------------------------

float get_scale(NcVar *var)
{
  NcAtt *att = var->get_att("scale_factor");
  if (att != 0)
    return att->values()->as_float(0);
  else
    return 1.0f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the offset value
 */
// ----------------------------------------------------------------------

float get_offset(NcVar *var)
{
  NcAtt *att = var->get_att("add_offset");
  if (att != 0)
    return att->values()->as_float(0);
  else
    return 0.0f;
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform well known unit conversions
 *
 * Kelvin --> Celsius
 * Pascal --> hectoPascal
 */
// ----------------------------------------------------------------------

float normalize_units(float value, const std::string &units)
{
  if (value == kFloatMissing)
    return value;
  else if (units == "K")
    return value - 273.15f;
  else if (units == "Pa")
    return value / 100.0f;
  else
    return value;
}

// ----------------------------------------------------------------------
/*!
 * Report unit conversions if they are done
 */
// ----------------------------------------------------------------------

void report_units(NcVar *var,
                  const std::string &units,
                  const Options &options,
                  bool ignoreUnitChange)
{
  if (!options.verbose) return;

  if (units == "K")
  {
    if (ignoreUnitChange)
      std::cout << "Note: " << get_name(var) << " units convertion ignored from K to C"
                << std::endl;
    else
      std::cout << "Note: " << get_name(var) << " units converted from K to C" << std::endl;
  }
  else if (units == "Pa")
  {
    if (ignoreUnitChange)
      std::cout << "Note: " << get_name(var) << " units convertion ignored from pa to hPa"
                << std::endl;
    else
      std::cout << "Note: " << get_name(var) << " units converted from pa to hPa" << std::endl;
  }
}

bool IsMissingValue(float value, float ncMissingValue)
{
  const float extraMissingValueLimit = 9.99e034f;  // joskus datassa on outoja isoja (ja
                                                   // vaihtelevia) lukuja esim. vuoriston kohdalla,
  // jotka pit�� mielest�ni tulkita puuttuviksi (en tied� mit� muutakaan voi tehd�)
  if (value != ncMissingValue && value < extraMissingValueLimit)
    return false;
  else
    return true;
}

bool is_name_in_list(const std::list<std::string> &nameList, const std::string name)
{
  if (!nameList.empty())
  {
    std::list<std::string>::const_iterator it = std::find(nameList.begin(), nameList.end(), name);
    if (it != nameList.end()) return true;
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * Copy regular variable data into querydata
 */
// ----------------------------------------------------------------------

void NcFileExtended::copy_values(const Options &options, NcVar *var, NFmiFastQueryInfo &info)
{
  std::string name = var->name();
  std::string units = "";
  NcAtt *att = var->get_att("units");
  if (att != 0) units = att->values()->as_string(0);

  if (options.debug) std::cerr << "debug: starting copy for variable " << name << std::endl;

  // joskus metatiedot valehtelevat, t�ll�in ei saa muuttaa parametrin yksik�it�
  bool ignoreUnitChange = is_name_in_list(options.ignoreUnitChangeParams, name);

  report_units(var, units, options);

  float missingvalue = get_missingvalue(var);
  float scale = get_scale(var);
  float offset = get_offset(var);

  // NetCDF data ordering: time, level, rows from bottom row to top row, left-right order in row, if
  // none of the axises are inverted We have to calculate the actual position for inverted axises.
  // They will be non-inverted in the result data.
  int timeindex = 0;
  for (info.ResetTime(); info.NextTime(); ++timeindex)
  {
    unsigned long level = 0;

    // Only copy to correct time index
    if (*timeList().Time(timeindex) == info.Time())
    {
      // must delete
      NcValues *vals = var->get_rec(timeindex);
      for (info.ResetLevel(); info.NextLevel(); ++level)
      {
        // Outer loop is just the level - multiple levels are not supported yet
        unsigned long xcounter = (this->xinverted() ? xsize() - 1 : 0);  // Current x-coordinate
        // Calculating every point by multiplication is slow so saving the starting point of current
        // row Further improvement when both axises are non-inverted does not improve performance
        unsigned long ystart = (this->yinverted() ? (ysize() - 1) * xsize() : 0);

        if (options.debug)
          std::cerr << "debug: before copy loop, timeindex=" << timeindex << " level=" << level
                    << " xcounter=" << xcounter << " ystart=" << ystart << std::endl;

        // Inner loop contains all of the x,y values on this level
        for (info.ResetLocation(); info.NextLocation();)
        {
          float value = vals->as_float(ystart + xcounter);
          if (!IsMissingValue(value, missingvalue))
          {
            if (!ignoreUnitChange) value = normalize_units(scale * value + offset, units);
            info.FloatValue(value);
          }

          // Next row?
          if (xcounter == (xinverted() ? 0 : xsize() - 1))
          {
            // Yes, increase the y counter and reset x
            ystart += (yinverted() ? -xsize() : +xsize());
            xcounter = (xinverted() ? xsize() - 1 : 0);
          }
          else
          {
            // No, just increase x (or decrease if inverted )
            (this->xinverted() ? xcounter-- : xcounter++);
          }
        }
        if (options.debug)
          std::cerr << "debug: after copy loop, timeindex=" << timeindex << " level=" << level
                    << " xcounter=" << xcounter << " ystart=" << ystart << std::endl;
      }
      delete vals;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * Copy speed/direction variable data into querydata
 */
// ----------------------------------------------------------------------

void NcFileExtended::copy_values(NFmiFastQueryInfo &info,
                                 const ParamInfo &pinfo,
                                 const nctools::Options *options)
{
  const float pi = 3.14159265358979326f;

  NcVar *xvar = find_variable(pinfo.x_component);
  NcVar *yvar = find_variable(pinfo.y_component);

  if (xvar == NULL || yvar == NULL) return;

  float xmissingvalue = get_missingvalue(xvar);
  float xscale = get_scale(xvar);
  float xoffset = get_offset(xvar);

  float ymissingvalue = get_missingvalue(yvar);
  float yscale = get_scale(yvar);
  float yoffset = get_offset(yvar);

  // NetCDF data ordering: time, level, y, x
  int timeindex = 0;
  for (info.ResetTime(); info.NextTime(); ++timeindex)
  {
    if (*timeList().Time(timeindex) == info.Time())
    {
      // must delete
      NcValues *xvals = xvar->get_rec(timeindex);
      NcValues *yvals = yvar->get_rec(timeindex);
      if (options != nullptr && options->debug)
      {
        std::cerr << (std::string) "debug: x-component has " + std::to_string(xvals->num()) +
                         " elements\n";
        std::cerr << (std::string) "debug: y-component has " + std::to_string(yvals->num()) +
                         " elements\n";
      }
      long counter = 0;
      for (info.ResetLevel(); info.NextLevel();)
        for (info.ResetLocation(); info.NextLocation();)
        {
          if (xinverted() || yinverted())
            throw SmartMet::Spine::Exception(
                BCP, std::string("Inverted axises not implemented here yet"));
          float x = xvals->as_float(counter);
          float y = yvals->as_float(counter);
          if (x != xmissingvalue && y != ymissingvalue)
          {
            x = xscale * x + xoffset;
            y = yscale * y + yoffset;

            // We assume everything is in m/s here and all is fine

            if (pinfo.isspeed)
              info.FloatValue(sqrt(x * x + y * y));
            else
              info.FloatValue(180 * atan2(x, y) / pi);
          }
          ++counter;
        }
      if (options != nullptr && options->debug)
        std::cerr << "debug: counter went through " + std::to_string(counter) + " elements\n";

      delete xvals;
      delete yvals;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * Copy raw NetCDF data into querydata
 */
// ----------------------------------------------------------------------

void NcFileExtended::copy_values(const Options &options,
                                 NFmiFastQueryInfo &info,
                                 const ParamConversions &paramconvs,
                                 bool useAutoGeneratedIds)
{
  // Note: We loop over variables the same way as in create_pdesc
  for (int i = 0; i < num_vars(); i++)
  {
    NcVar *var = get_var(i);
    if (var == nullptr) continue;

    ParamInfo pinfo = parse_parameter(var, paramconvs, useAutoGeneratedIds);
    if (pinfo.id == kFmiBadParameter) continue;

    if (info.Param(pinfo.id))
    {
      // Now we handle differently the two cases of a regular parameter
      // and one calculated from X- and Y-components

      if (pinfo.isregular)
        copy_values(options, var, info);
      else
        copy_values(info, pinfo, &options);
    }
  }
}

NcFileExtended::NcFileExtended(std::string path,
                               long timeshift,
                               FileMode fm,
                               size_t *bufrsizeptr,
                               size_t initialsize,
                               FileFormat ff)
    : NcFile(path.c_str(), fm, bufrsizeptr, initialsize, ff),
      path(path),
      longitudeOfProjectionOrigin(0),
      latitudeOfProjectionOrigin(0),
      projectionName(nullptr),
      x(nullptr),
      y(nullptr),
      z(nullptr),
      t(nullptr),
      minmaxfound(false),
      _xmin(0),
      _xmax(0),
      _ymin(0),
      _ymax(0),
      _zmin(0),
      _zmax(0),
      _xinverted(false),
      _yinverted(false),
      _zinverted(false),
      x_units(nullptr),
      y_units(nullptr),
      z_units(nullptr),
      xscale(0),
      yscale(0),
      zscale(0),
      timelist(nullptr)
{
  this->timeshift = timeshift;
}

std::string NcFileExtended::grid_mapping()
{
  if (projectionName != nullptr) return *projectionName;  // Do not rescan projection unnecessarily

  std::string projection_var_name;

  for (int i = 0; i < num_vars(); i++)
  {
    NcVar *var = get_var(i);
    if (var == nullptr) continue;

    NcAtt *att = var->get_att("grid_mapping");
    if (att == nullptr) continue;

    projection_var_name = att->values()->as_string(0);
    break;
  }

  if (!projection_var_name.empty())
  {
    for (int i = 0; i < num_vars(); i++)
    {
      NcVar *var = get_var(i);
      if (var == nullptr) continue;

      if (var->name() == projection_var_name)
      {
        NcAtt *name_att = var->get_att("grid_mapping_name");
        if (name_att != 0)
          projectionName = std::make_shared<std::string>(name_att->values()->as_string(0));

        NcAtt *lon_att = var->get_att("longitude_of_projection_origin");
        if (lon_att != 0) longitudeOfProjectionOrigin = lon_att->values()->as_double(0);
        NcAtt *lat_att = var->get_att("latitude_of_projection_origin");
        if (lat_att != 0) latitudeOfProjectionOrigin = lat_att->values()->as_double(0);
        break;
      }
    }
  }

  if (projectionName == nullptr) projectionName = std::make_shared<std::string>(LATITUDE_LONGITUDE);

  return *projectionName;
}

// ----------------------------------------------------------------------
/*!
 * Find variable for the desired axis
 */
// ----------------------------------------------------------------------

NcVar *NcFileExtended::axis(const std::string &axisname)
{
  std::string axis = boost::algorithm::to_lower_copy(axisname);

  NcVar *var = 0;
  for (int i = 0; i < num_vars(); i++)
  {
    var = get_var(i);
    for (int j = 0; j < var->num_atts(); j++)
    {
      NcAtt *att = var->get_att(j);
      if (att->type() == ncChar && att->num_vals() > 0)
      {
        std::string name = att->values()->as_string(0);
        boost::algorithm::to_lower(name);
        if (name == axis) return var;
      }
    }
  }
  return NULL;
}

// ----------------------------------------------------------------------
/*!
 * Try various names to find x axis
 */
// ----------------------------------------------------------------------
NcVar *NcFileExtended::x_axis()
{
  if (x != nullptr) return x;

  x = axis("x");
  if (x == nullptr) x = axis("degree_east");
  if (x == nullptr) x = axis("degrees_east");
  if (x == nullptr) x = axis("degree_E");
  if (x == nullptr) x = axis("degrees_E");
  if (x == nullptr) x = axis("degreeE");
  if (x == nullptr) x = axis("degreesE");
  /* Really? I think these are units ... pernu 2017-12-07
  if (x == nullptr) x = axis("100  km");
  if (x == nullptr) x = axis("m"); */
  if (x == nullptr) x = axis("projection_x_coordinate");
  if (x == nullptr) throw SmartMet::Spine::Exception(BCP, "X-axis type unsupported");

  return x;
}

// ----------------------------------------------------------------------
/*!
 * Try various names to find y axis
 */
// ----------------------------------------------------------------------
NcVar *NcFileExtended::y_axis()
{
  if (y != nullptr) return y;

  y = axis("y");
  if (y == nullptr) y = axis("degree_north");
  if (y == nullptr) y = axis("degrees_north");
  if (y == nullptr) y = axis("degree_N");
  if (y == nullptr) y = axis("degrees_N");
  if (y == nullptr) y = axis("degreeN");
  if (y == nullptr) y = axis("degreesN");
  /* Really? I think these are units ... pernu 2017-12-07
  if (y == nullptr) y = axis("100  km");
  if (y == nullptr) y = axis("m"); */
  if (y == nullptr) y = axis("projection_y_coordinate");
  if (y == nullptr) throw SmartMet::Spine::Exception(BCP, "Y-axis type unsupported");

  return y;
}

// ----------------------------------------------------------------------
/*!
 * Try various names to find z axis
 */
// ----------------------------------------------------------------------
NcVar *NcFileExtended::z_axis()
{
  if (z != nullptr) return z;
  z = axis("z");
  if (z == nullptr) z = axis("projection_z_coordinate");

  // It is okay for z-axis to be null: there might only be one level(=no z-axis)
  return z;
}

// ----------------------------------------------------------------------
/*!
 * Try various names to find t axis
 */
// ----------------------------------------------------------------------
NcVar *NcFileExtended::t_axis()
{
  if (t != nullptr) return t;
  t = (isStereographic() ? nullptr : axis("T"));

  // Alternate names
  if (t == nullptr) t = axis("time");

  // It is okay for t-axis to be null: there might be only one time for the whole file
  return t;
}

bool NcFileExtended::isStereographic()
{
  if (grid_mapping() == POLAR_STEREOGRAPHIC) return true;
  return false;
}

// ----------------------------------------------------------------------
/*!
 * Find dimension of given axis
 */
// ----------------------------------------------------------------------

unsigned long NcFileExtended::axis_size(NcVar *axis)
{
  if (axis == nullptr)
    throw SmartMet::Spine::Exception(BCP,
                                     std::string("Dimensions for axis null cannot be retrieved"));
  std::string varname = axis->name();

  std::string dimname = boost::algorithm::to_lower_copy(varname);
  for (int i = 0; i < num_dims(); i++)
  {
    NcDim *dim = get_dim(i);
    std::string name = dim->name();
    boost::algorithm::to_lower(name);
    if (name == dimname) return dim->size();
  }
  throw SmartMet::Spine::Exception(BCP, std::string("Could not find dimension of axis ") + varname);
}

unsigned long NcFileExtended::xsize() { return axis_size(x_axis()); }

unsigned long NcFileExtended::ysize() { return axis_size(y_axis()); }

unsigned long NcFileExtended::zsize()
{
  auto z = z_axis();
  return (z == nullptr ? 1 : axis_size(z));
}

unsigned long NcFileExtended::tsize() { return (isStereographic() ? 0 : axis_size(t_axis())); }

/*
 * Get list of times in this NetCDF file
 */
NFmiTimeList NcFileExtended::timeList(std::string varName, std::string unitAttrName)
{
  if (this->timelist != nullptr) return *(this->timelist);

  std::shared_ptr<NFmiTimeList> tlist = std::make_shared<NFmiTimeList>();
  if (isStereographic())
  {
    NcVar *ncvar = get_var(varName.c_str());
    NcAtt *units_att = ncvar->get_att(unitAttrName.c_str());

    std::string unit_val_value(units_att->as_string(0));
    delete units_att;

    std::vector<std::string> tokens;
    boost::split(tokens, unit_val_value, boost::algorithm::is_any_of(" "));

    // convert unit to seconds: day == 86400, hour == 3600, ...
    unsigned long unit_secs(get_units_in_seconds(tokens[0]));
    std::string date_str(tokens[2]);
    if (date_str.find('-') != std::string::npos)
    {
      if (isdigit(date_str[5]) && !isdigit(date_str[6])) date_str.insert(5, "0");
      if (isdigit(date_str[8]) && !isdigit(date_str[9])) date_str.insert(8, "0");
    }

    boost::posix_time::ptime t = Fmi::TimeParser::parse(date_str);

    NcValues *ncvals = ncvar->values();
    for (int k = 0; k < ncvar->num_vals(); k++)
    {
      boost::posix_time::ptime timestep(t +
                                        boost::posix_time::seconds(ncvals->as_long(k) * unit_secs));
      tlist->Add(new NFmiMetTime(tomettime(timestep)));
    }
  }
  else
  {
    using boost::posix_time::ptime;
    ptime origintime;
    long timeunit;
    parse_time_units(t_axis(), &origintime, &timeunit);

    NcValues *values = t->values();
    for (int i = 0; i < t->num_vals(); i++)
    {
      long timeoffset = values->as_int(i);

      ptime validtime = origintime + boost::posix_time::minutes(timeshift);

      if (timeunit == 1)
        validtime += boost::posix_time::seconds(timeoffset);
      else if (timeunit == 60)
        validtime += boost::posix_time::minutes(timeoffset);
      else if (timeunit == 60 * 60)
        validtime += boost::posix_time::hours(timeoffset);
      else if (timeunit == 24 * 60 * 60)
        validtime += boost::posix_time::hours(24 * timeoffset);
      else
        validtime += boost::posix_time::seconds(timeoffset * timeunit);

      tlist->Add(new NFmiMetTime(nctools::tomettime(validtime)));
    }
  }

  this->timelist = tlist;
  return *this->timelist;
}

unsigned long get_units_in_seconds(std::string unit_str)
{
  if (unit_str == "day" || unit_str == "days" || unit_str == "d")
    return 86400;
  else if (unit_str == "hour" || unit_str == "hours" || unit_str == "h")
    return 3600;
  else if (unit_str == "minute" || unit_str == "minutes" || unit_str == "min" || unit_str == "mins")
    return 60;
  else if (unit_str == "second" || unit_str == "seconds" || unit_str == "sec" ||
           unit_str == "secs" || unit_str == "s")
    return 1;
  else
  {
    throw SmartMet::Spine::Exception(BCP, "Invalid time unit used: " + unit_str);
  }
}

// ----------------------------------------------------------------------
/*!
 * Parse unit information from time attributes
 */
// ----------------------------------------------------------------------

void parse_time_units(NcVar *t, boost::posix_time::ptime *origintime, long *timeunit)
{
  NcAtt *att = t->get_att("units");
  if (att == 0) throw SmartMet::Spine::Exception(BCP, "Time axis has no defined units");
  if (att->type() != ncChar)
    throw SmartMet::Spine::Exception(BCP, "Time axis units must be a string");

  // "units since date [time] [tz]"

  std::string units = att->values()->as_string(0);

  std::vector<std::string> parts;
  boost::algorithm::split(parts, units, boost::algorithm::is_any_of(" "));

  if (parts.size() < 3 || parts.size() > 5)
    throw SmartMet::Spine::Exception(BCP, "Invalid time units string: '" + units + "'");

  std::string unit = boost::algorithm::to_lower_copy(parts[0]);

  if (unit == "second" || unit == "seconds" || unit == "sec" || unit == "secs" || unit == "s")
    *timeunit = 1;
  else if (unit == "minute" || unit == "minutes" || unit == "min" || unit == "mins")
    *timeunit = 60;
  else if (unit == "hour" || unit == "hours" || unit == "hr" || unit == "h")
    *timeunit = 60 * 60;
  else if (unit == "day" || unit == "days" || unit == "d")
    *timeunit = 24 * 60 * 60;
  else
    throw SmartMet::Spine::Exception(BCP, "Unknown unit in time axis: '" + unit + "'");

  if (boost::algorithm::to_lower_copy(parts[1]) != "since")
    throw SmartMet::Spine::Exception(BCP, "Invalid time units string: '" + units + "'");

  std::string datestr = parts[2];
  std::string timestr = (parts.size() >= 4 ? parts[3] : "00:00:00");

  *origintime = Fmi::TimeParser::parse_iso(datestr + "T" + timestr);

  if (parts.size() == 5 && boost::iequals(parts[4], "UTC") == false)
    *origintime += boost::posix_time::duration_from_string(parts[4]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct NFmiMetTime from posix time
 */
// ----------------------------------------------------------------------

NFmiMetTime tomettime(const boost::posix_time::ptime &t)
{
  return NFmiMetTime(static_cast<short>(t.date().year()),
                     static_cast<short>(t.date().month()),
                     static_cast<short>(t.date().day()),
                     static_cast<short>(t.time_of_day().hours()),
                     static_cast<short>(t.time_of_day().minutes()),
                     static_cast<short>(t.time_of_day().seconds()),
                     1);
}

// ----------------------------------------------------------------------
/*!
 * Find axis bounds
 */
// ----------------------------------------------------------------------

void NcFileExtended::find_axis_bounds(
    NcVar *var, int n, double *x1, double *x2, const char *name, bool *isdescending)
{
  if (var == NULL) return;

  NcValues *values = var->values();
  *isdescending = false;  // Set to true if we detect decreasing instead of increasing values

  // Verify monotonous coordinates
  if (var->num_vals() >= 2 && values->as_double(1) < values->as_double(0)) *isdescending = true;

  for (int i = 1; i < var->num_vals(); i++)
  {
    if (*isdescending == false && values->as_double(i) <= values->as_double(i - 1))
      throw SmartMet::Spine::Exception(BCP,
                                       std::string(name) + "-axis is not monotonously increasing");
    if (*isdescending == true && values->as_double(i) >= values->as_double(i - 1))
      throw SmartMet::Spine::Exception(BCP,
                                       std::string(name) + "-axis is not monotonously decreasing");
  }

  // Min&max is now easy
  if (*isdescending == false)
  {
    *x1 = values->as_double(0);
    *x2 = values->as_double(var->num_vals() - 1);
  }
  else
  {
    *x2 = values->as_double(0);
    *x1 = values->as_double(var->num_vals() - 1);
  }

  // Verify stepsize is even
  if (n <= 2) return;

  double step = ((*x2) - (*x1)) / (n - 1);
  double tolerance = 1e-3;

  for (int i = 1; i < var->num_vals(); i++)
  {
    double s;
    if (*isdescending == false)
      s = values->as_double(i) - values->as_double(i - 1);
    else
      s = values->as_double(i - 1) - values->as_double(i);

    if (std::abs(s - step) > tolerance * step)
      throw SmartMet::Spine::Exception(
          BCP, std::string(name) + "-axis is not regular with tolerance 1e-3");
  }
}

void NcFileExtended::find_lonlat_bounds(double &lon1, double &lat1, double &lon2, double &lat2)
{
  for (int i = 0; i < num_vars(); i++)
  {
    NcVar *ncvar = get_var(i);

    NcAtt *att = ncvar->get_att("standard_name");
    if (att != 0)
    {
      std::string attributeStandardName(att->values()->as_string(0));
      if (attributeStandardName == "longitude" || attributeStandardName == "latitude")
      {
        NcValues *ncvals = ncvar->values();
        if (attributeStandardName == "longitude")
        {
          lon1 = ncvals->as_double(0);
          lon2 = ncvals->as_double(ncvar->num_vals() - 1);
        }
        else
        {
          lat1 = ncvals->as_double(0);
          lat2 = ncvals->as_double(ncvar->num_vals() - 1);
        }
        delete ncvals;
      }
    }
  }
}

void NcFileExtended::find_bounds()
{
  if (isStereographic())
  {
    find_lonlat_bounds(_xmin, _ymin, _xmax, _ymax);
  }
  else
  {
    find_axis_bounds(x, xsize(), &_xmin, &_xmax, "x", &_xinverted);
    find_axis_bounds(y, ysize(), &_ymin, &_ymax, "y", &_yinverted);
  }
  find_axis_bounds(z, zsize(), &_zmin, &_zmax, "z", &_zinverted);
  if (_zinverted == true && _zmin != _zmax)
    throw SmartMet::Spine::Exception(BCP, "z-axis is inverted: this is not supported(yet?)");
  minmaxfound = true;
}

bool NcFileExtended::xinverted()
{
  if (minmaxfound == false) find_bounds();
  return _xinverted;
}
bool NcFileExtended::yinverted()
{
  if (minmaxfound == false) find_bounds();
  return _yinverted;
}

double NcFileExtended::xmin()
{
  if (minmaxfound == false) find_bounds();
  return _xmin;
}
double NcFileExtended::xmax()
{
  if (minmaxfound == false) find_bounds();
  return _xmax;
}
double NcFileExtended::ymin()
{
  if (minmaxfound == false) find_bounds();
  return _ymin;
}
double NcFileExtended::ymax()
{
  if (minmaxfound == false) find_bounds();
  return _ymax;
}
double NcFileExtended::zmin()
{
  if (minmaxfound == false) find_bounds();
  return _zmin;
}
double NcFileExtended::zmax()
{
  if (minmaxfound == false) find_bounds();
  return _zmax;
}

/**
 * Handle axis units and scaling
 *
 *
 */
std::shared_ptr<std::string> NcFileExtended::get_axis_units(NcVar *axis)
{
  // String presentation of a particular units on an axis
  NcAtt *att = axis->get_att("units");
  if (att == 0)
    throw SmartMet::Spine::Exception(
        BCP, (std::string)axis->name() + (std::string) "-axis has no units attribute");

  std::shared_ptr<std::string> units = std::make_shared<std::string>(att->values()->as_string(0));

  // Ref: CF conventions section 4.1 Latitude Coordinate
  /*	  if (units == "degrees_north") return;
            if (units == "degree_north") return;
            if (units == "degree_N") return;
            if (units == "degrees_N") return;
            if (units == "degreeN") return;
            if (units == "degreesN") return;
            if (units == "100  km") return;
            if (units == "m") return;
            if (units == "km") return;
  */
  //	  throw SmartMet::Spine::Exception(BCP, "Y-axis has unknown units: " + units);
  return units;
}

double NcFileExtended::get_axis_scale(NcVar *axis,
                                      std::shared_ptr<std::string> *source_units,
                                      std::string *target_units)
{  // Get scaling multiplier for target
   // units, default target being meters
  *source_units = get_axis_units(axis);
  if (target_units != nullptr && target_units->compare("m") != 0)
    throw SmartMet::Spine::Exception(BCP,
                                     "Sorry: do not know how to convert " + **source_units +
                                         " to " + *target_units + " on axis " + axis->name());

  if (**source_units == "100  km") return 100 * 1000;
  if (**source_units == "m") return 1;
  if (**source_units == "km") return 1000;
  return 1;
}

double NcFileExtended::x_scale()
{
  if (x_units == nullptr)
  {
    std::shared_ptr<std::string> tmp = nullptr;
    xscale = get_axis_scale(x_axis(), &tmp);
    x_units = tmp;
  }
  return xscale;
}

double NcFileExtended::y_scale()
{
  if (y_units == nullptr)
  {
    std::shared_ptr<std::string> tmp = nullptr;
    yscale = get_axis_scale(y_axis(), &tmp);
    y_units = tmp;
  }
  return yscale;
}

double NcFileExtended::z_scale()
{
  if (z_units == nullptr)
  {
    std::shared_ptr<std::string> tmp = nullptr;
    zscale = get_axis_scale(z_axis(), &tmp);
    z_units = tmp;
  }
  return zscale;
}

/**
 * Miscellaneous
 */
bool NcFileExtended::joinable(NcFileExtended &ncfile, std::vector<std::string> *failreasons)
{
  bool ok = true;
  if (failreasons != nullptr) failreasons->clear();

  if (this->grid_mapping() != ncfile.grid_mapping())
  {
    ok = false;
    failreasons->push_back("projection is different");
  }
  if (this->xsize() != ncfile.xsize())
  {
    ok = false;
    failreasons->push_back("x-axis dimension is different");
  }
  if (this->ysize() != ncfile.ysize())
  {
    ok = false;
    failreasons->push_back("y-axis dimension is different");
  }
  if (this->zsize() != ncfile.zsize())
  {
    ok = false;
    failreasons->push_back("z-axis dimension is different");
  }
  if (this->longitudeOfProjectionOrigin != ncfile.longitudeOfProjectionOrigin)
  {
    ok = false;
    failreasons->push_back("origin(longitude) is different");
  }
  if (this->latitudeOfProjectionOrigin != ncfile.latitudeOfProjectionOrigin)
  {
    ok = false;
    failreasons->push_back("origin(latitude) is different");
  }
  if (this->xinverted() != ncfile.xinverted())
  {
    ok = false;
    failreasons->push_back("x-axis inversion is different");
  }
  if (this->yinverted() != ncfile.yinverted())
  {
    ok = false;
    failreasons->push_back("y-axis inversion is different");
  }
  if (this->x_scale() != ncfile.x_scale())
  {
    ok = false;
    failreasons->push_back("x-axis units are different");
  }
  if (this->y_scale() != ncfile.y_scale())
  {
    ok = false;
    failreasons->push_back("y-axis units are different");
  }
  if (this->isStereographic() != ncfile.isStereographic())
  {
    ok = false;
    failreasons->push_back("both files are not stereographic");
  }

  // TODO: Possibly z-axis units , stepping ? , whatever, time formats etc .
  return ok;
}

// namespace nctools

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

  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar *var = ncfile.get_var(i);
    std::cerr << "Var " << i << "\tname = " << var->name() << std::endl
              << "\ttype = " << int(var->type()) << std::endl
              << "\tvalid = " << var->is_valid() << std::endl
              << "\tdims = " << var->num_dims() << std::endl
              << "\tatts = " << var->num_atts() << std::endl
              << "\tvals = " << var->num_vals() << std::endl;

    for (int j = 0; j < var->num_atts(); j++)
    {
      NcAtt *att = var->get_att(j);
      std::cerr << "\tAtt " << j << std::endl;
      print_att(*att);
    }
  }

  for (int i = 0; i < ncfile.num_atts(); i++)
  {
    NcAtt *att = ncfile.get_att(i);
    std::cerr << "\tAtt " << i << std::endl;
    print_att(*att);
  }
}

#endif

}  // namespace nctools
