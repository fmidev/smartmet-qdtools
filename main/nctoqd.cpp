// ======================================================================
/*!
 * \brief NetCDF to querydata conversion for CF-conforming data
 *
 * http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/
 */
// ======================================================================

#include <macgyver/CsvReader.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiLambertEqualArea.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiParam.h>
#include <newbase/NFmiParamBag.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStereographicArea.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>
#include <spine/Exception.h>

#include <netcdfcpp.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "nctools.h"

nctools::Options options;

// ----------------------------------------------------------------------
/*!
 * Validate the data conforms to CF
 */
// ----------------------------------------------------------------------

void require_conventions(const NcFile& ncfile, const std::string& reference, int sz)
{
  NcAtt* att = ncfile.get_att("Conventions");
  if (att == 0)
    throw SmartMet::Spine::Exception(BCP, "The NetCDF file is missing the Conventions attribute");

  if (att->type() != ncChar)
    throw SmartMet::Spine::Exception(BCP, "The Conventions attribute must be a string");

  std::string ref = att->values()->as_string(0);

  if (ref.substr(0, sz) != reference.substr(0, sz))
    throw SmartMet::Spine::Exception(BCP,
                                     "The file must conform to " + reference + ", not to " + ref);

  // We do not test the version numerically at all, since it is
  // possible the version will some day be of the form x.y.z
}
// ----------------------------------------------------------------------
/*!
 * Check X-axis units
 */
// ----------------------------------------------------------------------

void check_xaxis_units(NcVar* var)
{
  NcAtt* att = var->get_att("units");
  if (att == 0) throw SmartMet::Spine::Exception(BCP, "X-axis has no units attribute");

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
  if (units == "km") return;

  throw SmartMet::Spine::Exception(BCP, "X-axis has unknown units: " + units);
}

// ----------------------------------------------------------------------
/*!
 * Check Y-axis units
 */
// ----------------------------------------------------------------------

void check_yaxis_units(NcVar* var)
{
  NcAtt* att = var->get_att("units");
  if (att == 0) throw SmartMet::Spine::Exception(BCP, "Y-axis has no units attribute");

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
  if (units == "km") return;

  throw SmartMet::Spine::Exception(BCP, "Y-axis has unknown units: " + units);
}

// ----------------------------------------------------------------------
/*!
 * Create horizontal descriptor
 */
// ----------------------------------------------------------------------
NFmiHPlaceDescriptor create_hdesc(nctools::NcFileExtended& ncfile)
{
  double x1 = ncfile.xmin();
  double y1 = ncfile.ymin();
  double x2 = ncfile.xmax();
  double y2 = ncfile.ymax();
  double nx = ncfile.xsize();
  double ny = ncfile.ysize();
  double centralLongitude = ncfile.longitudeOfProjectionOrigin;

  if (options.verbose)
  {
    std::cout << "x1 => " << x1 << std::endl;
    std::cout << "y1 => " << y1 << std::endl;
    std::cout << "x2 => " << x2 << std::endl;
    std::cout << "y2 => " << y2 << std::endl;
    std::cout << "nx => " << nx << std::endl;
    std::cout << "ny => " << ny << std::endl;
    if (ncfile.xinverted()) std::cout << "x-axis is inverted" << std::endl;
    if (ncfile.yinverted()) std::cout << "y-axis is inverted" << std::endl;
    std::cout << "x-scaling multiplier to meters => " << ncfile.x_scale() << std::endl;
    std::cout << "y-scaling multiplier to meters => " << ncfile.y_scale() << std::endl;
    std::cout << "latitude_origin => " << ncfile.latitudeOfProjectionOrigin << std::endl;
    std::cout << "longitude_origin => " << ncfile.longitudeOfProjectionOrigin << std::endl;
    std::cout << "grid_mapping => " << ncfile.grid_mapping() << std::endl;
  }

  NFmiArea* area;
  if (ncfile.grid_mapping() == POLAR_STEREOGRAPHIC)
    area = new NFmiStereographicArea(NFmiPoint(x1, y1), NFmiPoint(x2, y2), centralLongitude);
  else if (ncfile.grid_mapping() == LAMBERT_CONFORMAL_CONIC)
    throw SmartMet::Spine::Exception(BCP, "Lambert conformal conic projection not supported");
  else if (ncfile.grid_mapping() == LAMBERT_AZIMUTHAL)
  {
    NFmiLambertEqualArea tmp(NFmiPoint(-90, 0),
                             NFmiPoint(90, 0),
                             ncfile.longitudeOfProjectionOrigin,
                             NFmiPoint(0, 0),
                             NFmiPoint(1, 1),
                             ncfile.latitudeOfProjectionOrigin);
    NFmiPoint bottomleft =
        tmp.WorldXYToLatLon(NFmiPoint(ncfile.x_scale() * x1, ncfile.y_scale() * y1));
    NFmiPoint topright =
        tmp.WorldXYToLatLon(NFmiPoint(ncfile.x_scale() * x2, ncfile.y_scale() * y2));
    area = new NFmiLambertEqualArea(bottomleft,
                                    topright,
                                    ncfile.longitudeOfProjectionOrigin,
                                    NFmiPoint(0, 0),
                                    NFmiPoint(1, 1),
                                    ncfile.latitudeOfProjectionOrigin);
  }
  else if (ncfile.grid_mapping() == LATITUDE_LONGITUDE)
    area = new NFmiLatLonArea(NFmiPoint(x1, y1), NFmiPoint(x2, y2));
  else
    throw SmartMet::Spine::Exception(BCP,
                                     "Projection " + ncfile.grid_mapping() + " is not supported");

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
    throw SmartMet::Spine::Exception(BCP, "Invalid time unit used: " + unit_str);
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

// ----------------------------------------------------------------------
/*!
 * Create parameter descriptor
 *
 * We extract all parameters which are recognized by newbase.
 */
// ----------------------------------------------------------------------

int add_to_pbag(const NcFile& ncfile,
                const nctools::ParamConversions& paramconvs,
                NFmiParamBag& pbag)
{
  unsigned int added_variables = 0;

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
    if (pbag.Add(ident, true)) added_variables++;
  }

  return added_variables;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  try
  {
    // Parse options
    if (!parse_options(argc, argv, options)) return 0;

    // Parameter conversions
    const nctools::ParamConversions paramconvs = nctools::read_netcdf_config(options);

    // Prepare empty target querydata
    std::unique_ptr<NFmiQueryData> data;

    int counter = 0;
    NFmiHPlaceDescriptor hdesc;
    NFmiVPlaceDescriptor vdesc;
    NFmiTimeDescriptor tdesc;
    NFmiParamBag pbag;
    std::shared_ptr<nctools::NcFileExtended> ncfile1;
    unsigned int known_variables = 0;

    // Loop through the files once to check and to prepare the descriptors first
    for (std::string infile : options.infiles)
    {
      try
      {
        NcError errormode(NcError::silent_nonfatal);
        std::shared_ptr<nctools::NcFileExtended> ncfile =
            std::make_shared<nctools::NcFileExtended>(infile, NcFile::ReadOnly);

        if (!ncfile->is_valid())
          throw SmartMet::Spine::Exception(
              BCP, "File '" + infile + "' does not contain valid NetCDF", NULL);

        require_conventions(*ncfile, "CF-1.0", 3);
        std::string grid_mapping(ncfile->grid_mapping());
        NcVar* x = ncfile->x_axis();
        NcVar* y = ncfile->y_axis();
        NcVar* z = ncfile->z_axis();
        NcVar* t = ncfile->t_axis();

        if (!ncfile->isStereographic() && t == nullptr)
          throw SmartMet::Spine::Exception(BCP, "Failed to find T-axis variable");
        if (x->num_vals() < 1) throw SmartMet::Spine::Exception(BCP, "X-axis has no values");
        if (y->num_vals() < 1) throw SmartMet::Spine::Exception(BCP, "Y-axis has no values");
        if (z != nullptr && z->num_vals() < 1)
          throw SmartMet::Spine::Exception(BCP, "Z-axis has no values");
        if (!ncfile->isStereographic() && t->num_vals() < 1)
          throw SmartMet::Spine::Exception(BCP, "T-axis has no values");

        check_xaxis_units(x);
        check_yaxis_units(y);

        unsigned long nx = ncfile->xsize();
        unsigned long ny = ncfile->ysize();
        unsigned long nz = ncfile->zsize();
        unsigned long nt = ncfile->tsize();

        if (nx == 0) throw SmartMet::Spine::Exception(BCP, "X-dimension is of size zero");
        if (ny == 0) throw SmartMet::Spine::Exception(BCP, "Y-dimension is of size zero");
        if (nz == 0) throw SmartMet::Spine::Exception(BCP, "Z-dimension is of size zero");
        if (!ncfile->isStereographic() && nt == 0)
          throw SmartMet::Spine::Exception(BCP, "T-dimension is of size zero");

        if (nz != 1)
          throw SmartMet::Spine::Exception(
              BCP, "Z-dimension <> 1 is not supported (yet), sample file is needed first");

        // We don't do comparison for the first one but instead initialize the param descriptors
        if (counter == 0)
        {
          ncfile1 = ncfile;

          hdesc = create_hdesc(*ncfile);
          vdesc = create_vdesc(*ncfile, ncfile->zmin(), ncfile->zmax(), nz);
          tdesc = (ncfile->isStereographic() ? create_tdesc(*ncfile) : create_tdesc(*ncfile, t));
        }
        else
        {
          std::vector<std::string> failreasons;
          if (ncfile->joinable(*ncfile1, &failreasons) == false)
          {
            std::cerr << "Unable to combine " << ncfile1->path << " and " << infile << ":"
                      << std::endl;
            for (auto error : failreasons)
            {
              std::cerr << "  " << error << std::endl;
            }
            throw SmartMet::Spine::Exception(BCP, "Files not joinable", NULL);
          }

          NFmiHPlaceDescriptor newhdesc = create_hdesc(*ncfile);
          NFmiVPlaceDescriptor newvdesc = create_vdesc(*ncfile, ncfile->zmin(), ncfile->zmax(), nz);
          NFmiTimeDescriptor newtdesc =
              (ncfile->isStereographic() ? create_tdesc(*ncfile) : create_tdesc(*ncfile, t));
          if (!(newhdesc == hdesc))
            throw SmartMet::Spine::Exception(BCP, "Hdesc differs from " + ncfile1->path);
          if (!(newvdesc == vdesc))
            throw SmartMet::Spine::Exception(BCP, "Vdesc differs from " + ncfile1->path);
          if (!(newtdesc == tdesc))
            throw SmartMet::Spine::Exception(BCP, "Tdesc differs from " + ncfile1->path);
        }
        known_variables += add_to_pbag(*ncfile, paramconvs, pbag);
      }
      catch (...)
      {
        throw SmartMet::Spine::Exception(BCP, "File check failed on input " + infile, NULL);
      }
      counter++;
    }

    // Check parameters
    if (known_variables == 0)
      throw SmartMet::Spine::Exception(BCP,
                                       "inputs do not contain any convertible variables. Do you "
                                       "need to define some conversion in config?");

    // Create querydata structures and target file
    NFmiParamDescriptor pdesc(pbag);
    NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
    if (options.memorymap)
    {
      data.reset(NFmiQueryDataUtil::CreateEmptyData(qi, options.outfile, true));
    }
    else
    {
      data.reset(NFmiQueryDataUtil::CreateEmptyData(qi));
    }
    NFmiFastQueryInfo info(data.get());
    info.SetProducer(NFmiProducer(options.producernumber, options.producername));

    // Copy data from input files
    counter = 0;
    for (std::string infile : options.infiles)
    {
      try
      {
        // Default is to exit in some non fatal situations
        NcError errormode(NcError::silent_nonfatal);
        nctools::NcFileExtended ncfile(infile, NcFile::ReadOnly);

#if DEBUG_PRINT
        debug_output(ncfile);
#endif
        ncfile.copy_values(options, info, paramconvs);
      }
      catch (...)
      {
        throw SmartMet::Spine::Exception(BCP, "Operation failed on input " + infile, NULL);
      }
      counter++;
    }

    if (options.outfile == "-")
      data->Write();
    else if (!options.memorymap)
      data->Write(options.outfile);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
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
  catch (...)
  {
    SmartMet::Spine::Exception e(BCP, "Operation failed!", NULL);
    e.printError();
    return 1;
  }
}
