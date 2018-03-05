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
#include <newbase/NFmiVPlaceDescriptor.h>
#include <spine/Exception.h>

#include <netcdfcpp.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
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

#include "NcFileExtended.h"
#include "nctools.h"

nctools::Options options;

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
    if (options.infiles.size() > 1) std::cout << std::endl;
    std::cout << "Input file: " << ncfile.path << std::endl;
    std::cout << "  x1 => " << x1 << std::endl;
    std::cout << "  y1 => " << y1 << std::endl;
    std::cout << "  x2 => " << x2 << std::endl;
    std::cout << "  y2 => " << y2 << std::endl;
    std::cout << "  nx => " << nx << std::endl;
    std::cout << "  ny => " << ny << std::endl;
    if (ncfile.xinverted()) std::cout << "  x-axis is inverted" << std::endl;
    if (ncfile.yinverted()) std::cout << "  y-axis is inverted" << std::endl;
    std::cout << "  x-scaling multiplier to meters => " << ncfile.x_scale() << std::endl;
    std::cout << "  y-scaling multiplier to meters => " << ncfile.y_scale() << std::endl;
    std::cout << "  latitude_origin => " << ncfile.latitudeOfProjectionOrigin << std::endl;
    std::cout << "  longitude_origin => " << ncfile.longitudeOfProjectionOrigin << std::endl;
    std::cout << "  grid_mapping => " << ncfile.grid_mapping() << std::endl;
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
 * Create time descriptor
 *
 * CF reference "4.4. Time Coordinate" is crap. We support only
 * the simple stuff we need.
 *
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(nctools::NcFileExtended& ncFile)
{
  NFmiTimeList tlist(ncFile.timeList());

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
    nctools::ParamInfo pinfo = nctools::parse_parameter(var, paramconvs, options.autoid);
    if (pinfo.id < 1)
    /*   	== kFmiBadParameter && pinfo.id < nctools::unknownParIdCounterBegin &&
           pinfo.id < 1) */
    {
      if (options.verbose)
        std::cout << "  Skipping unknown variable '" << nctools::get_name(var) << "'" << std::endl;
      continue;
    }
    else if (options.verbose)
      std::cout << "  Variable " << nctools::get_name(var) << " has id " << pinfo.id << " and name "
                << (nctools::get_enumconverter().ToString(pinfo.id).empty()
                        ? "undefined"
                        : nctools::get_enumconverter().ToString(pinfo.id))
                << std::endl;

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
    const nctools::ParamConversions paramconvs = nctools::read_netcdf_configs(options);

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
            std::make_shared<nctools::NcFileExtended>(infile, options.timeshift);
        ncfile->tolerance = options.tolerance;

        if (!ncfile->is_valid())
          throw SmartMet::Spine::Exception(
              BCP, "File '" + infile + "' does not contain valid NetCDF", nullptr);

        ncfile->require_conventions(&(options.conventions));
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
          tdesc = create_tdesc(*ncfile);
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
            throw SmartMet::Spine::Exception(BCP, "Files not joinable", nullptr);
          }

          NFmiHPlaceDescriptor newhdesc = create_hdesc(*ncfile);
          NFmiVPlaceDescriptor newvdesc = create_vdesc(*ncfile, ncfile->zmin(), ncfile->zmax(), nz);
          NFmiTimeDescriptor newtdesc = create_tdesc(*ncfile);

          if (!(newhdesc == hdesc))
            throw SmartMet::Spine::Exception(BCP, "Hdesc differs from " + ncfile1->path);
          if (!(newvdesc == vdesc))
            throw SmartMet::Spine::Exception(BCP, "Vdesc differs from " + ncfile1->path);

          tdesc = tdesc.Combine(newtdesc);
        }
        known_variables += add_to_pbag(*ncfile, paramconvs, pbag);
      }
      catch (...)
      {
        throw SmartMet::Spine::Exception(BCP, "File check failed on input " + infile, nullptr);
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
        if (options.verbose)
        {
          std::cout << "Copying data from input " << infile << std::endl;
        }
        // Default is to exit in some non fatal situations
        NcError errormode(NcError::silent_nonfatal);
        nctools::NcFileExtended ncfile(infile, options.timeshift);
        ncfile.tolerance = options.tolerance;

#if DEBUG_PRINT
        debug_output(ncfile);
#endif
        ncfile.copy_values(options, info, paramconvs);
      }
      catch (...)
      {
        throw SmartMet::Spine::Exception(BCP, "Operation failed on input " + infile, nullptr);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", nullptr);
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
    SmartMet::Spine::Exception e(BCP, "Operation failed!", nullptr);
    e.printError();
    return 1;
  }
}
