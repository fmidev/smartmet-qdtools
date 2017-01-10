// ======================================================================
/*!
 * \brief NetCDF to querydata conversion for CF-conforming data
 *
 * http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/
 */
// ======================================================================

#include "NFmiEnumConverter.h"
#include "NFmiFastQueryInfo.h"
#include "NFmiQueryData.h"
#include "NFmiQueryDataUtil.h"
#include "NFmiLatLonArea.h"
#include "NFmiStereographicArea.h"
#include "NFmiHPlaceDescriptor.h"
#include "NFmiVPlaceDescriptor.h"
#include "NFmiTimeDescriptor.h"
#include "NFmiTimeList.h"
#include "NFmiParamDescriptor.h"
#include "NFmiAreaFactory.h"

#include "nctools.h"
#include <CsvReader.h>
#include <String.h>
#include <TimeParser.h>

#include <netcdfcpp.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include <cmath>
#include <limits>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <functional>

nctools::Options options;

// ----------------------------------------------------------------------
/*!
 * Validate the data conforms to CF
 */
// ----------------------------------------------------------------------

void require_conventions(const NcFile& ncfile, const std::string& reference, int sz)
{
  NcAtt* att = ncfile.get_att("Conventions");
  if (att == 0) throw std::runtime_error("The NetCDF file is missing the Conventions attribute");

  if (att->type() != ncChar) throw std::runtime_error("The Conventions attribute must be a string");

  std::string ref = att->values()->as_string(0);

  if (ref.substr(0, sz) != reference.substr(0, sz))
    throw std::runtime_error("The file must conform to " + reference + ", not to " + ref);

  // We do not test the version numerically at all, since it is
  // possible the version will some day be of the form x.y.z
}

// ----------------------------------------------------------------------
/*!
 * Find variable for the desired axis
 */
// ----------------------------------------------------------------------

NcVar* find_axis(const NcFile& ncfile, const std::string& axisname)
{
  std::string axis = boost::algorithm::to_lower_copy(axisname);

  NcVar* var = 0;
  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    var = ncfile.get_var(i);
    for (int j = 0; j < var->num_atts(); j++)
    {
      NcAtt* att = var->get_att(j);
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
 * Find dimension of given axis
 */
// ----------------------------------------------------------------------

int find_dimension(const NcFile& ncfile, const std::string& varname)
{
  std::string dimname = boost::algorithm::to_lower_copy(varname);
  for (int i = 0; i < ncfile.num_dims(); i++)
  {
    NcDim* dim = ncfile.get_dim(i);
    std::string name = dim->name();
    boost::algorithm::to_lower(name);
    if (name == dimname) return dim->size();
  }
  throw std::runtime_error(std::string("Could not find dimension of axis ") + varname);
}

// ----------------------------------------------------------------------
/*!
 * Find axis bounds
 */
// ----------------------------------------------------------------------

void find_axis_bounds(NcVar* var, int n, double* x1, double* x2, const char* name)
{
  if (var == NULL) return;

  NcValues* values = var->values();

  // Verify monotonous coordinates

  for (int i = 1; i < var->num_vals(); i++)
  {
    if (values->as_double(i) <= values->as_double(i - 1))
      throw std::runtime_error(std::string(name) + "-axis is not monotonously increasing");
  }

  // Min&max is now easy

  *x1 = values->as_double(0);
  *x2 = values->as_double(var->num_vals() - 1);

  // Verify stepsize is even

  if (n <= 2) return;

  double step = ((*x2) - (*x1)) / (n - 1);
  double tolerance = 1e-3;

  for (int i = 1; i < var->num_vals(); i++)
  {
    double s = values->as_double(i) - values->as_double(i - 1);

    if (std::abs(s - step) > tolerance * step)
      throw std::runtime_error(std::string(name) + "-axis is not regular with tolerance 1e-3");
  }
}

void find_lonlat_bounds(
    const NcFile& ncfile, double& lon1, double& lat1, double& lon2, double& lat2)
{
  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar* ncvar = ncfile.get_var(i);

    NcAtt* att = ncvar->get_att("standard_name");
    if (att != 0)
    {
      std::string attributeStandardName(att->values()->as_string(0));
      if (attributeStandardName == "longitude" || attributeStandardName == "latitude")
      {
        NcValues* ncvals = ncvar->values();
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

// ----------------------------------------------------------------------
/*!
 * Check X-axis units
 */
// ----------------------------------------------------------------------

void check_xaxis_units(NcVar* var)
{
  NcAtt* att = var->get_att("units");
  if (att == 0) throw std::runtime_error("X-axis has no units attribute");

  std::string units = att->values()->as_string(0);

  // Ref: CF conventions section 4.2 Longitude Coordinate
  if (units == "degrees_east") return;
  if (units == "degree_east") return;
  if (units == "degree_E") return;
  if (units == "degrees_E") return;
  if (units == "degreeE") return;
  if (units == "degreesE") return;
  if (units == "100  km") return;
  if (units == "m") return;

  throw std::runtime_error("X-axis has unknown units: " + units);
}

// ----------------------------------------------------------------------
/*!
 * Check Y-axis units
 */
// ----------------------------------------------------------------------

void check_yaxis_units(NcVar* var)
{
  NcAtt* att = var->get_att("units");
  if (att == 0) throw std::runtime_error("Y-axis has no units attribute");

  std::string units = att->values()->as_string(0);

  // Ref: CF conventions section 4.1 Latitude Coordinate
  if (units == "degrees_north") return;
  if (units == "degree_north") return;
  if (units == "degree_N") return;
  if (units == "degrees_N") return;
  if (units == "degreeN") return;
  if (units == "degreesN") return;
  if (units == "100  km") return;
  if (units == "m") return;

  throw std::runtime_error("Y-axis has unknown units: " + units);
}

// ----------------------------------------------------------------------
/*!
 * Create horizontal descriptor
 */
// ----------------------------------------------------------------------
NFmiHPlaceDescriptor create_hdesc(double x1,
                                  double y1,
                                  double x2,
                                  double y2,
                                  int nx,
                                  int ny,
                                  double centralLongitude,
                                  const std::string& grid_mapping)
{
  if (options.verbose)
  {
    std::cout << "x1 => " << x1 << std::endl;
    std::cout << "y1 => " << y1 << std::endl;
    std::cout << "x2 => " << x2 << std::endl;
    std::cout << "y2 => " << y2 << std::endl;
    std::cout << "nx => " << nx << std::endl;
    std::cout << "ny => " << ny << std::endl;
    std::cout << "grid_mapping => " << grid_mapping << std::endl;
  }

  NFmiArea* area;
  if (grid_mapping == POLAR_STEREOGRAPHIC)
    area = new NFmiStereographicArea(NFmiPoint(x1, y1), NFmiPoint(x2, y2), centralLongitude);
  else
    area = new NFmiLatLonArea(NFmiPoint(x1, y1), NFmiPoint(x2, y2));

  NFmiGrid grid(area, nx, ny);
  NFmiHPlaceDescriptor hdesc(grid);

  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * Create vertical descriptor
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc(const NcFile& /* ncfile */,
                                  double /* z1 */,
                                  double /* z2 */,
                                  int /* nz */)
{
  NFmiLevelBag bag(kFmiAnyLevelType, 0, 0, 0);
  NFmiVPlaceDescriptor vdesc(bag);
  return vdesc;
}

// ----------------------------------------------------------------------
/*!
 * Parse unit information from time attributes
 */
// ----------------------------------------------------------------------

void parse_time_units(NcVar* t, boost::posix_time::ptime* origintime, long* timeunit)
{
  NcAtt* att = t->get_att("units");
  if (att == 0) throw std::runtime_error("Time axis has no defined units");
  if (att->type() != ncChar) throw std::runtime_error("Time axis units must be a string");

  // "units since date [time] [tz]"

  std::string units = att->values()->as_string(0);

  std::vector<std::string> parts;
  boost::algorithm::split(parts, units, boost::algorithm::is_any_of(" "));

  if (parts.size() < 3 || parts.size() > 5)
    throw std::runtime_error("Invalid time units string: '" + units + "'");

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
    throw std::runtime_error("Unknown unit in time axis: '" + unit + "'");

  if (boost::algorithm::to_lower_copy(parts[1]) != "since")
    throw std::runtime_error("Invalid time units string: '" + units + "'");

  std::string datestr = parts[2];
  std::string timestr = (parts.size() >= 4 ? parts[3] : "00:00:00");

  *origintime = Fmi::TimeParser::parse_iso(datestr + "T" + timestr);

  if (parts.size() == 5) *origintime += boost::posix_time::duration_from_string(parts[4]);
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct NFmiMetTime from posix time
 */
// ----------------------------------------------------------------------

NFmiMetTime tomettime(const boost::posix_time::ptime& t)
{
  return NFmiMetTime(static_cast<short>(t.date().year()),
                     static_cast<short>(t.date().month()),
                     static_cast<short>(t.date().day()),
                     static_cast<short>(t.time_of_day().hours()),
                     static_cast<short>(t.time_of_day().minutes()),
                     static_cast<short>(t.time_of_day().seconds()),
                     1);
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
    throw std::runtime_error("Invalid time unit used: " + unit_str);
  }
}

NFmiTimeList get_tlist(const NcFile& ncfile,
                       std::string varName = "time",
                       std::string unitAttrName = "units")
{
  NcVar* ncvar = ncfile.get_var(varName.c_str());
  NcAtt* units_att = ncvar->get_att(unitAttrName.c_str());

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

  NFmiTimeList tlist;
  NcValues* ncvals = ncvar->values();
  for (int k = 0; k < ncvar->num_vals(); k++)
  {
    boost::posix_time::ptime timestep(t +
                                      boost::posix_time::seconds(ncvals->as_long(k) * unit_secs));
    tlist.Add(new NFmiMetTime(tomettime(timestep)));
  }

  return tlist;
}

// ----------------------------------------------------------------------
/*!
 * Create time descriptor
 *
 * CF reference "4.4. Time Coordinate" is crap. We support only
 * the simple stuff we need.
 *
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(const NcFile& ncFile)
{
  NFmiTimeList tlist(get_tlist(ncFile));

  return NFmiTimeDescriptor(tlist.FirstTime(), tlist);
}

NFmiTimeDescriptor create_tdesc(const NcFile& /* ncfile */, NcVar* t)
{
  using boost::posix_time::ptime;

  ptime origintime;
  long timeunit;
  parse_time_units(t, &origintime, &timeunit);

  // Note the use of longs and units greater than seconds when possible
  // to avoid integer arithmetic overflows.

  NFmiTimeList tlist;
  NcValues* values = t->values();
  for (int i = 0; i < t->num_vals(); i++)
  {
    long timeoffset = values->as_int(i);

    ptime validtime = origintime + boost::posix_time::minutes(options.timeshift);

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

    tlist.Add(new NFmiMetTime(tomettime(validtime)));
  }

  return NFmiTimeDescriptor(tlist.FirstTime(), tlist);
}

std::string find_projection(const NcFile& ncfile, double& longitudeOfProjectionOrigin)
{
  std::string projection_var_name;

  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar* var = ncfile.get_var(i);
    if (var == 0) continue;

    NcAtt* att = var->get_att("grid_mapping");
    if (att == 0) continue;

    projection_var_name = att->values()->as_string(0);
    break;
  }

  std::string projection_name;

  if (!projection_var_name.empty())
  {
    for (int i = 0; i < ncfile.num_vars(); i++)
    {
      NcVar* var = ncfile.get_var(i);
      if (var == 0) continue;

      if (var->name() == projection_var_name)
      {
        NcAtt* name_att = var->get_att("grid_mapping_name");
        if (name_att != 0) projection_name = name_att->values()->as_string(0);

        NcAtt* lon_att = var->get_att("longitude_of_projection_origin");
        if (lon_att != 0) longitudeOfProjectionOrigin = lon_att->values()->as_double(0);
        break;
      }
    }
  }

  return projection_name;
}

// ----------------------------------------------------------------------
/*!
 * Create parameter descriptor
 *
 * We extract all parameters which are recognized by newbase.
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor create_pdesc(const NcFile& ncfile, const nctools::ParamConversions& paramconvs)
{
  NFmiParamBag pbag;

  const float minvalue = kFloatMissing;
  const float maxvalue = kFloatMissing;
  const float scale = kFloatMissing;
  const float base = kFloatMissing;
  const NFmiString precision = "%.1f";
  const FmiInterpolationMethod interpolation = kLinearly;

  // Note: We loop over variables the same way as in copy_values

  for (int i = 0; i < ncfile.num_vars(); i++)
  {
    NcVar* var = ncfile.get_var(i);
    if (var == 0) continue;

    // Here we need to know only the id
    nctools::ParamInfo pinfo = nctools::parse_parameter(var, paramconvs);
    if (pinfo.id == kFmiBadParameter)
    {
      if (options.verbose)
        std::cout << "Skipping unknown variable '" << nctools::get_name(var) << "'" << std::endl;
      continue;
    }

    NFmiParam param(pinfo.id,
                    nctools::get_enumconverter().ToString(pinfo.id),
                    minvalue,
                    maxvalue,
                    scale,
                    base,
                    precision,
                    interpolation);
    NFmiDataIdent ident(param);
    pbag.Add(ident);
  }

  return NFmiParamDescriptor(pbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv, options)) return 0;

  // Default is to exit in some non fatal situations
  NcError errormode(NcError::silent_nonfatal);
  NcFile ncfile(options.infile.c_str(), NcFile::ReadOnly);

  if (!ncfile.is_valid())
    throw std::runtime_error("File '" + options.infile + "' does not contain valid NetCDF");

  // Parameter conversions

  nctools::ParamConversions paramconvs = nctools::read_netcdf_config(options);

#if DEBUG_PRINT
  debug_output(ncfile);
#endif

  require_conventions(ncfile, "CF-1.0", 3);
  double centralLongitude(0);
  std::string grid_mapping(find_projection(ncfile, centralLongitude));
  bool isStereographicProjection = (grid_mapping == POLAR_STEREOGRAPHIC);

  NcVar* x = find_axis(ncfile, "x");
  NcVar* y = find_axis(ncfile, "y");
  NcVar* z = find_axis(ncfile, "z");
  NcVar* t = (isStereographicProjection ? 0 : find_axis(ncfile, "T"));

  if (x == 0) throw std::runtime_error("Failed to find X-axis variable");
  if (y == 0) throw std::runtime_error("Failed to find Y-axis variable");
  // if (z == 0) throw std::runtime_error("Failed to find Z-axis variable");
  if (!isStereographicProjection && t == 0)
    throw std::runtime_error("Failed to find T-axis variable");

  if (x->num_vals() < 1) throw std::runtime_error("X-axis has no values");
  if (y->num_vals() < 1) throw std::runtime_error("Y-axis has no values");
  if (z != NULL && z->num_vals() < 1) throw std::runtime_error("Z-axis has no values");
  if (!isStereographicProjection && t->num_vals() < 1)
    throw std::runtime_error("T-axis has no values");

  check_xaxis_units(x);
  check_yaxis_units(y);

  int nx = find_dimension(ncfile, x->name());
  int ny = find_dimension(ncfile, y->name());
  int nz = (z == NULL ? 1 : find_dimension(ncfile, z->name()));
  int nt = (isStereographicProjection ? 0 : find_dimension(ncfile, t->name()));

  if (nx == 0) throw std::runtime_error("X-dimension is of size zero");
  if (ny == 0) throw std::runtime_error("Y-dimension is of size zero");
  if (nz == 0) throw std::runtime_error("Z-dimension is of size zero");
  if (!isStereographicProjection && nt == 0)
    throw std::runtime_error("T-dimension is of size zero");

  if (nz != 1)
    throw std::runtime_error(
        "Z-dimension <> 1 is not supported (yet), sample file is needed first");

  double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0;
  if (isStereographicProjection)
  {
    find_lonlat_bounds(ncfile, x1, y1, x2, y2);
  }
  else
  {
    find_axis_bounds(x, nx, &x1, &x2, "x");
    find_axis_bounds(y, ny, &y1, &y2, "y");
  }
  find_axis_bounds(z, nz, &z1, &z2, "z");

  NFmiHPlaceDescriptor hdesc = create_hdesc(x1, y1, x2, y2, nx, ny, centralLongitude, grid_mapping);
  NFmiVPlaceDescriptor vdesc = create_vdesc(ncfile, z1, z2, nz);
  NFmiTimeDescriptor tdesc =
      (isStereographicProjection ? create_tdesc(ncfile) : create_tdesc(ncfile, t));
  NFmiParamDescriptor pdesc = create_pdesc(ncfile, paramconvs);

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  std::auto_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  NFmiFastQueryInfo info(data.get());

  if (data.get() == 0) throw std::runtime_error("Could not allocate memory for result data");

  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  nctools::copy_values(options, ncfile, info, paramconvs);

  // TODO: Handle unit conversions too!

  if (options.outfile == "-")
    data->Write();
  else
    data->Write(options.outfile);

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
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
}
