
#include "nctools.h"

#include <newbase/NFmiFastQueryInfo.h>

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
    unknownParIdMap;  // jos sallitaan tuntemattomien parametrien käyttö, ne talletetaan tähän
int unknownParIdCounter = 1200;  // jos tuntematon paramtri, aloitetaan niiden id:t tästä ja
                                 // kasvatetaan aina yhdellä kun tulee uusia
}

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
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "version,V", "display version number")(
      "infile,i", po::value(&options.infile), "input netcdf file")(
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
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "nctoqd v1.2 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
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
      std::cout << "Usage: nctoqd [options] infile outfile" << std::endl
                << std::endl
                << "Converts CF-1.4 conforming NetCDF to querydata." << std::endl
                << "Only features in known use are supported." << std::endl
                << std::endl
                << desc << std::endl;
    }

    return false;
  }

  if (opt.count("infile") == 0) throw std::runtime_error("Expecting input file as parameter 1");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input file '" + options.infile + "' does not exist");

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
        NFmiStringTools::Split<std::list<std::string> >(tmpIgnoreUnitChangeParamsStr, ",");
  }

  if (!tmpExcludeParamsStr.empty())
  {
    options.excludeParams =
        NFmiStringTools::Split<std::list<std::string> >(tmpExcludeParamsStr, ",");
  }

  if (!tmpCmdLineGlobalAttributesStr.empty())
  {
    // globaalit attribuutit annetaan muodossa -a DX=1356.3;DY=1265.3, eli eri attribuutit on
    // erotelty ;-merkeillä ja avain/arvot on eroteltu = -merkeillä
    std::list<std::string> attributeListParts =
        NFmiStringTools::Split<std::list<std::string> >(tmpCmdLineGlobalAttributesStr, ";");
    for (std::list<std::string>::iterator it = attributeListParts.begin();
         it != attributeListParts.end();
         ++it)
    {
      std::vector<std::string> attributeParts =
          NFmiStringTools::Split<std::vector<std::string> >(*it, "=");
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
  {  // Lisätään haluttaessa myös automaattisesti generoitu par-id
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

NcVar *find_variable(const NcFile &ncfile, const std::string &name)
{
  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar *var = ncfile.get_var(i);
    if (var == 0) continue;
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
  // jotka pitää mielestäni tulkita puuttuviksi (en tiedä mitä muutakaan voi tehdä)
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

void copy_values(const Options &options, NcVar *var, NFmiFastQueryInfo &info)
{
  std::string name = var->name();
  std::string units = "";
  NcAtt *att = var->get_att("units");
  if (att != 0) units = att->values()->as_string(0);

  // joskus metatiedot valehtelevat, tällöin ei saa muuttaa parametrin yksiköitä
  bool ignoreUnitChange = is_name_in_list(options.ignoreUnitChangeParams, name);

  report_units(var, units, options);

  float missingvalue = get_missingvalue(var);
  float scale = get_scale(var);
  float offset = get_offset(var);

  // NetCDF data ordering: time, level, rows from bottom row to top row, left-right order in row
  int timeindex = 0;
  for (info.ResetTime(); info.NextTime(); ++timeindex)
  {
    // must delete
    NcValues *vals = var->get_rec(timeindex);

    long counter = 0;
    for (info.ResetLevel(); info.NextLevel();)
    {
      for (info.ResetLocation(); info.NextLocation();)
      {
        float value = vals->as_float(counter);
        if (!IsMissingValue(value, missingvalue))
        {
          if (!ignoreUnitChange) value = normalize_units(scale * value + offset, units);
          info.FloatValue(value);
        }
        ++counter;
      }
    }
    delete vals;
  }
}

// ----------------------------------------------------------------------
/*!
 * Copy speed/direction variable data into querydata
 */
// ----------------------------------------------------------------------

void copy_values(const NcFile &ncfile, NFmiFastQueryInfo &info, const ParamInfo &pinfo)
{
  const float pi = 3.14159265358979326f;

  NcVar *xvar = find_variable(ncfile, pinfo.x_component);
  NcVar *yvar = find_variable(ncfile, pinfo.y_component);

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
    // must delete
    NcValues *xvals = xvar->get_rec(timeindex);
    NcValues *yvals = yvar->get_rec(timeindex);

    long counter = 0;
    for (info.ResetLevel(); info.NextLevel();)
      for (info.ResetLocation(); info.NextLocation();)
      {
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

    delete xvals;
    delete yvals;
  }
}

// ----------------------------------------------------------------------
/*!
 * Copy raw NetCDF data into querydata
 */
// ----------------------------------------------------------------------

void copy_values(const Options &options,
                 const NcFile &ncfile,
                 NFmiFastQueryInfo &info,
                 const ParamConversions &paramconvs,
                 bool useAutoGeneratedIds)
{
  // Note: We loop over variables the same way as in create_pdesc

  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar *var = ncfile.get_var(i);
    if (var == 0) continue;

    ParamInfo pinfo = parse_parameter(var, paramconvs, useAutoGeneratedIds);
    if (pinfo.id == kFmiBadParameter) continue;

    if (info.Param(pinfo.id))
    {
      // Now we handle differently the two cases of a regular parameter
      // and one calculated from X- and Y-components

      if (pinfo.isregular)
        copy_values(options, var, info);
      else
        copy_values(ncfile, info, pinfo);
    }
  }
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
