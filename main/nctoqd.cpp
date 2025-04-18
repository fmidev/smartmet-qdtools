// ======================================================================
/*!
 * \brief NetCDF to querydata conversion for CF-conforming data
 *
 * http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/
 */
// ======================================================================

#include "NcFileExtended.h"
#include "nctools.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <macgyver/CsvReader.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiAreaTools.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiParam.h>
#include <newbase/NFmiParamBag.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiVPlaceDescriptor.h>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

nctools::Options options;

using Fmi::Exception;

// ----------------------------------------------------------------------
/*!
 * Check X-axis units
 */
// ----------------------------------------------------------------------

void check_xaxis_units(const netCDF::NcVar& var)
{
  const netCDF::NcVarAtt att = var.getAtt("units");
  if (att.isNull())
    throw Exception(BCP, "X-axis has no units attribute");

  std::string units = nctools::get_att_string_value(att);

  // Ref: CF conventions section 4.2 Longitude Coordinate
  if (units == "degrees_east")
    return;
  if (units == "degree_east")
    return;
  if (units == "degree_E")
    return;
  if (units == "degrees_E")
    return;
  if (units == "degreeE")
    return;
  if (units == "degreesE")
    return;
  if (units == "100  km")
    return;
  if (units == "m")
    return;
  if (units == "km")
    return;

  if (units == "Meter")
    return;

  throw Exception(BCP, "X-axis has unknown units: " + units);
}

// ----------------------------------------------------------------------
/*!
 * Check Y-axis units
 */
// ----------------------------------------------------------------------

void check_yaxis_units(const netCDF::NcVar& var)
{
  const netCDF::NcVarAtt att = var.getAtt("units");
  if (att.isNull())
    throw Exception(BCP, "Y-axis has no units attribute");

  std::string units = nctools::get_att_string_value(att);

  // Ref: CF conventions section 4.1 Latitude Coordinate
  if (units == "degrees_north")
    return;
  if (units == "degree_north")
    return;
  if (units == "degree_N")
    return;
  if (units == "degrees_N")
    return;
  if (units == "degreeN")
    return;
  if (units == "degreesN")
    return;
  if (units == "100  km")
    return;
  if (units == "m")
    return;
  if (units == "km")
    return;

  if (units == "Meter")
    return;

  throw Exception(BCP, "Y-axis has unknown units: " + units);
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

  if (options.verbose)
  {
    if (options.infiles.size() > 1)
      std::cout << std::endl;
    std::cout << "Input file: " << ncfile.path << std::endl;
    std::cout << "  x1 => " << x1 << std::endl;
    std::cout << "  y1 => " << y1 << std::endl;
    std::cout << "  x2 => " << x2 << std::endl;
    std::cout << "  y2 => " << y2 << std::endl;
    std::cout << "  nx => " << nx << std::endl;
    std::cout << "  ny => " << ny << std::endl;
    if (ncfile.xinverted())
      std::cout << "  x-axis is inverted" << std::endl;
    if (ncfile.yinverted())
      std::cout << "  y-axis is inverted" << std::endl;
    std::cout << "  x-scaling multiplier to meters => " << ncfile.x_scale() << std::endl;
    std::cout << "  y-scaling multiplier to meters => " << ncfile.y_scale() << std::endl;
    std::cout << "  latitude_origin => " << ncfile.latitudeOfProjectionOrigin << std::endl;
    std::cout << "  longitude_origin => " << ncfile.longitudeOfProjectionOrigin << std::endl;
    std::cout << "  grid_mapping => " << ncfile.grid_mapping() << std::endl;
  }

  NFmiArea* area = nullptr;

  if (ncfile.grid_mapping() == POLAR_STEREOGRAPHIC)
  {
    double true_lat = 60;  // for historical reasons
    area = NFmiAreaTools::CreateLegacyStereographicArea(NFmiPoint(x1, y1),
                                                        NFmiPoint(x2, y2),
                                                        ncfile.longitudeOfProjectionOrigin,
                                                        ncfile.latitudeOfProjectionOrigin,
                                                        true_lat);
  }
  else if (ncfile.grid_mapping() == LAMBERT_AZIMUTHAL)
  {
    auto proj4 =
        fmt::format("+datum=WGS84 +proj=laea +lat_0={} +lon_0={} +k=1 +x_0=0 +y_0=0 +units=m",
                    ncfile.latitudeOfProjectionOrigin,
                    ncfile.longitudeOfProjectionOrigin);
    area = NFmiArea::CreateFromBBox(proj4,
                                    NFmiPoint(ncfile.x_scale() * x1, ncfile.y_scale() * y1),
                                    NFmiPoint(ncfile.x_scale() * x2, ncfile.y_scale() * y2));
  }
  else if (ncfile.grid_mapping() == LATITUDE_LONGITUDE)
  {
    area = NFmiAreaTools::CreateLegacyLatLonArea(NFmiPoint(x1, y1), NFmiPoint(x2, y2));
  }
  else
    throw Exception(BCP, "Projection " + ncfile.grid_mapping() + " is not supported");

  NFmiGrid grid(area, nx, ny);
  NFmiHPlaceDescriptor hdesc(grid);

  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * Create vertical descriptor
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc(const nctools::NcFileExtended& ncfile)
{
  const netCDF::NcVar& z = ncfile.z_axis();

  // Defaults if there are no levels
  if (z.isNull())
  {
    if (options.verbose)
      std::cerr << "  Extracting default level only\n";
    NFmiLevelBag bag(kFmiAnyLevelType, 0, 0, 0);
    return NFmiVPlaceDescriptor(bag);
  }

  // Guess level type from z-axis units

  auto leveltype = kFmiAnyLevelType;

  const netCDF::NcVarAtt units_att = z.getAtt("units");
  if (!units_att.isNull())
  {
    std::string units = nctools::get_att_string_value(units_att);
    if (units == "Pa" || units == "hPa" || units == "mb")
      leveltype = kFmiPressureLevel;
    else if (units == "cm")
      leveltype = kFmiDepth;
    else if (units == "m" || units == "km" || units == "Meter")
      leveltype = kFmiHeight;
  }

  // Otherwise collect all levels

  NFmiLevelBag bag;

  const std::vector<long> zvalues = nctools::get_values<long>(z);

  if (options.verbose)
    std::cerr << "  Extracting " << zvalues.size() << " levels:";

  for (std::size_t i = 0; i < zvalues.size(); i++)
  {
    auto value = zvalues.at(i);
    if (options.verbose)
      std::cerr << " " << value << std::flush;
    NFmiLevel level(leveltype, value);
    bag.AddLevel(level);
  }

  if (options.verbose)
    std::cout << std::endl;

  return NFmiVPlaceDescriptor(bag);
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

NFmiTimeDescriptor create_tdesc(nctools::NcFileExtended& ncfile)
{
  NFmiTimeList tlist(ncfile.timeList());

  return NFmiTimeDescriptor(tlist.FirstTime(), tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add calculated parameters to the param descriptor
 */
// ----------------------------------------------------------------------

void add_calculated_params_to_pbag(NFmiParamBag& pbag)
{
  for (const auto& name : options.addParams)
  {
    auto id = nctools::get_enumconverter().ToEnum(name);

    if (id == kFmiBadParameter)
      throw std::runtime_error("Unknown parameter name '" + name + "'");

    NFmiParam param(id, name);

    switch (id)
    {
      case kFmiHumidity:
      {
        param.InterpolationMethod(kLinearly);
        param.MinValue(0);
        param.MaxValue(100);
        break;
      }
      default:
        throw Exception(BCP, "Calculating '" + name + "' from other parameters is not supported");
    }

    NFmiDataIdent ident(param);
    const bool check_duplicates = true;

    if (!pbag.Add(ident, check_duplicates))
      throw Exception(BCP, "Failed to add calculated parameter to parameter descriptor")
          .addParameter("parameter", name);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate humidity
 */
// ----------------------------------------------------------------------

void calculate_humidity(NFmiFastQueryInfo& info)
{
  if (!info.Param(kFmiHumidity))
    throw Exception(BCP, "Humidity has not been added to the data, internal error");

  const auto rh_idx = info.ParamIndex();

  if (!info.Param(kFmiTemperature))
    throw Exception(BCP, "Temperature is needed for calculating Humidity");

  const auto t_idx = info.ParamIndex();

  if (info.Param(kFmiDewPoint))
  {
    const auto td_idx = info.ParamIndex();

    for (info.ResetLevel(); info.NextLevel();)
      for (info.ResetLocation(); info.NextLocation();)
        for (info.ResetTime(); info.NextTime();)
        {
          info.ParamIndex(t_idx);
          auto t = info.FloatValue();
          info.ParamIndex(td_idx);
          auto td = info.FloatValue();
          if (t != kFloatMissing && td != kFloatMissing)
          {
            // t -= 273.15;  // we assume Celsius
            // td -= 273.15;
            auto rh = 100 * (exp(1.8 + 17.27 * (td / (td + 237.3)))) /
                      (exp(1.8 + 17.27 * (t / (t + 237.3))));
            rh = std::max(0.0, std::min(100.0, rh));
            info.ParamIndex(rh_idx);
            info.FloatValue(rh);
          }
        }
  }
  else if (info.Param(kFmiSpecificHumidity))
  {
    const auto q_idx = info.ParamIndex();
    int p_idx = -1;

    if (info.Param(kFmiPressure))
      p_idx = info.ParamIndex();
    else
    {
      info.FirstLevel();
      if (info.Level()->LevelType() != kFmiPressureLevel)
        throw Exception(BCP, "Pressure data is required for calculating humidity");
    }

    for (info.ResetLevel(); info.NextLevel();)
      for (info.ResetLocation(); info.NextLocation();)
        for (info.ResetTime(); info.NextTime();)
        {
          float p = kFloatMissing;
          if (p_idx >= 0)
          {
            info.ParamIndex(p_idx);
            p = info.FloatValue();  // we assume hPa
          }
          else
            p = info.Level()->LevelValue();  // we assume hPa

          info.ParamIndex(t_idx);
          auto t = info.FloatValue();
          info.ParamIndex(q_idx);
          auto q = info.FloatValue();

          if (t != kFloatMissing && q != kFloatMissing && p != kFloatMissing)
          {
            // t -= 273.15;  // we assume Celsius
            // p *= 100;     // we assume hPa
            q /= 1000;  // g/kg to kg/kg
            auto e = (t > -5) ? (6.107 * pow(10, 7.5 * t / (237 + t)))
                              : (6.107 * pow(10, 9.5 * t / (265.5 + t)));
            auto rh = 100 * (p * q) / (0.622 * e) * (p - e) / (p - q * p / 0.622);
            rh = std::max(0.0, std::min(100.0, rh));

            info.ParamIndex(rh_idx);
            info.FloatValue(rh);
          }
        }
  }
  else
    throw Exception(
        BCP, "DewPoint or Pressure and SpecificHumidity is required for calculating humidity");
}

// ----------------------------------------------------------------------
/*!
 * \brief Add calculated parameters
 */
// ----------------------------------------------------------------------

void calculate_added_params(NFmiFastQueryInfo& info)
{
  for (const auto& name : options.addParams)
  {
    if (name == "Humidity")
      calculate_humidity(info);
    else
      throw Exception(BCP, "Calculating '" + name + "' from other parameters is not supported");
  }
}

// ----------------------------------------------------------------------
/*!
 * Create parameter descriptor
 *
 * We extract all parameters which are recognized by newbase and use the
 * axes established by the command line options --xdim etc, or which
 * have been guessed based on standard names.
 */
// ----------------------------------------------------------------------

int add_to_pbag(const nctools::NcFileExtended& ncfile,
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

  // Number of dimensions the parameter must have
  int wanted_dims = 0;
  if (!ncfile.x_axis().isNull())
    ++wanted_dims;
  if (!ncfile.y_axis().isNull())
    ++wanted_dims;
  if (!ncfile.z_axis().isNull())
    ++wanted_dims;
  if (!ncfile.t_axis().isNull())
    ++wanted_dims;

  // Note: We loop over variables the same way as in copy_values

  const std::multimap<std::string, netCDF::NcVar> vars = ncfile.getVars();
  for (const auto& item : vars)
  {
    const netCDF::NcVar& var = item.second;
    if (var.isNull())
      continue;

    // Skip dimension variables
    if (ncfile.is_dim(var.getName()))
      continue;

    // Check dimensions

    if (!ncfile.axis_match(var))
    {
      if (options.verbose)
        std::cout << "  Skipping variable " << nctools::get_name(var)
                  << " for not having requested dimensions\n";
      continue;
    }

    // Here we need to know only the id
    nctools::ParamInfo pinfo =
        nctools::parse_parameter(nctools::get_name(var), paramconvs, options.autoid);
    if (pinfo.id < 1)
    {
      if (options.verbose)
        std::cout << "  Skipping unknown variable '" << nctools::get_name(var) << "'\n";
      continue;
    }
    else if (options.verbose)
    {
      std::cout << "  Variable " << nctools::get_name(var) << " has id " << pinfo.id << " and name "
                << (nctools::get_enumconverter().ToString(pinfo.id).empty()
                        ? "undefined"
                        : nctools::get_enumconverter().ToString(pinfo.id))
                << std::endl;
    }

    // Check dimensions match

    NFmiParam param(pinfo.id,
                    nctools::get_enumconverter().ToString(pinfo.id),
                    minvalue,
                    maxvalue,
                    scale,
                    base,
                    precision,
                    interpolation);
    NFmiDataIdent ident(param);
    if (pbag.Add(ident, true))
      added_variables++;
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
    if (!parse_options(argc, argv, options))
      return 0;

    //NcError netcdf_error_handling(options.verbose ? NcError::verbose_nonfatal
    //                                              : NcError::silent_nonfatal);

    // Parameter conversions
    const nctools::ParamConversions paramconvs = nctools::read_netcdf_configs(options);

    // Prepare empty target querydata
    std::unique_ptr<NFmiQueryData> data;

    int file_counter = 0;
    NFmiHPlaceDescriptor hdesc;
    NFmiVPlaceDescriptor vdesc;
    NFmiTimeDescriptor tdesc;
    NFmiParamBag pbag;

    using NcFileExtendedPtr = std::shared_ptr<nctools::NcFileExtended>;
    using NcFileExtendedList = std::vector<NcFileExtendedPtr>;

    NcFileExtendedPtr first_ncfile;
    NcFileExtendedList ncfilelist;

    unsigned int known_variables = 0;

    // Loop through the files once to check and to prepare the descriptors first

    for (std::string infile : options.infiles)
    {
      ++file_counter;

      try
      {
        //NcError errormode(NcError::silent_nonfatal);
        auto ncfile = std::make_shared<nctools::NcFileExtended>(infile, options.timeshift);
        ncfile->setOptions(options);
        ncfile->setWRF(false);

        if (ncfile->isNull())
          throw Exception(BCP, "File '" + infile + "' does not contain valid NetCDF", nullptr);

        // When --info is given we only print useful metadata instead of generating anything
        if (options.info)
        {
          ncfile->printInfo();
          continue;
        }

        // Verify convention requirement
        ncfile->require_conventions(&(options.conventions));

        // Establish wanted axis parameters, this throws if unsuccesful
        ncfile->initAxis(options.xdim, options.ydim, options.zdim, options.tdim);

        // Save initialized state for further processing
        ncfilelist.push_back(ncfile);

        std::string grid_mapping(ncfile->grid_mapping());

        // FIXME: check number of value below
        if (ncfile->x_axis().isNull())
          throw Exception(BCP, "X-axis has no values");
        if (ncfile->y_axis().isNull())
          throw Exception(BCP, "Y-axis has no values");
        if (!ncfile->z_axis().isNull() && false)
          throw Exception(BCP, "Z-axis has no values");
        if (!ncfile->t_axis().isNull() && false)
          throw Exception(BCP, "T-axis has no values");

        check_xaxis_units(ncfile->x_axis());
        check_yaxis_units(ncfile->y_axis());

        if (ncfile->xsize() == 0)
          throw Exception(BCP, "X-dimension is of size zero");
        if (ncfile->ysize() == 0)
          throw Exception(BCP, "Y-dimension is of size zero");
        if (ncfile->zsize() == 0)
          throw Exception(BCP, "Z-dimension is of size zero");

        // Crate initial descriptors based on the first NetCDF file
        if (file_counter == 1)
        {
          first_ncfile = ncfile;

          tdesc = create_tdesc(*ncfile);
          hdesc = create_hdesc(*ncfile);
          vdesc = create_vdesc(*ncfile);
        }
        else
        {
          // Try to merge times and parameters from other files with the same grid and levels
          std::vector<std::string> failreasons;
          if (ncfile->joinable(*first_ncfile, &failreasons) == false)
          {
            std::cerr << "Unable to combine " << first_ncfile->path << " and " << infile << ":"
                      << std::endl;
            for (auto error : failreasons)
              std::cerr << "  " << error << std::endl;

            throw Exception(BCP, "Files not joinable", nullptr);
          }

          auto new_hdesc = create_hdesc(*ncfile);
          auto new_vdesc = create_vdesc(*ncfile);
          auto new_tdesc = create_tdesc(*ncfile);

          if (!(new_hdesc == hdesc))
            throw Exception(BCP, "Hdesc differs from " + first_ncfile->path);
          if (!(new_vdesc == vdesc))
            throw Exception(BCP, "Vdesc differs from " + first_ncfile->path);

          tdesc = tdesc.Combine(new_tdesc);
        }
        known_variables += add_to_pbag(*ncfile, paramconvs, pbag);
      }
      catch (...)
      {
        throw Exception(BCP, "File check failed on input " + infile, nullptr);
      }
    }

    if (options.info)
      return 0;

    // Check parameters
    if (known_variables == 0)
      throw Exception(BCP,
                      "No known parameters defined by conversion tables found from input file(s)");

    // Create querydata structures and target file
    add_calculated_params_to_pbag(pbag);
    NFmiParamDescriptor pdesc(pbag);
    NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);

    if (options.memorymap)
      data.reset(NFmiQueryDataUtil::CreateEmptyData(qi, options.outfile, true));
    else
      data.reset(NFmiQueryDataUtil::CreateEmptyData(qi));

    NFmiFastQueryInfo info(data.get());
    info.SetProducer(NFmiProducer(options.producernumber, options.producername));

    // Copy data from input files
    for (auto i = 0ul; i < options.infiles.size(); i++)
    {
      try
      {
        const auto& ncfile = ncfilelist[i];
        bool useAutoGeneratedIds = false;
        ncfile->copy_values(options, info, paramconvs, useAutoGeneratedIds);
      }
      catch (...)
      {
        throw Exception(BCP, "Operation failed on input " + options.infiles[i], nullptr);
      }
    }

    calculate_added_params(info);

    // Save output
    if (options.outfile == "-")
      data->Write();
    else if (!options.memorymap)
      data->Write(options.outfile);
  }
  catch (...)
  {
    throw Exception(BCP, "Operation failed!", nullptr);
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
    Exception e(BCP, "Operation failed!", nullptr);
    e.printError();
    return 1;
  }
}
