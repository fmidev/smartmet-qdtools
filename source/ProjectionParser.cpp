// ======================================================================
/*!
 * \file
 * \brief Implementation of class ProjectionParser
 */
// ======================================================================

#include "ProjectionParser.h"

#include "Projection.h"
#include "ProjectionStore.h"

#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <cmath>

#include <iostream>
#include <stdexcept>
#include <string>

namespace FMI
{
namespace RadContour
{
namespace
{
// Which projection settings have been given
struct ProjectionFlags
{
  std::string proj_type;
  bool got_type = false;
  bool got_bottomleft = false;
  bool got_topright = false;
  bool got_center = false;
  bool got_scale = false;
  bool got_central_longitude = false;
  bool got_central_latitude = false;
  bool got_true_latitude = false;
};

// Read the projection settings up to the closing brace
void read_projection_settings(std::istream& is,
                              Projection& projection,
                              ProjectionFlags& flags,
                              const std::string& proj_name)
{
  std::string token;
  while (is.good())
  {
    is >> token;
    if (!is.good())
      break;

    if (token == "}")
      break;
    if (token == "type")
    {
      is >> token;
      projection.type(token);
      flags.proj_type = token;
      flags.got_type = true;
    }
    else if (token == "centrallatitude")
    {
      float lat = NAN;
      is >> lat;
      projection.centralLatitude(lat);
      flags.got_central_latitude = true;
    }
    else if (token == "centrallongitude")
    {
      float lon = NAN;
      is >> lon;
      projection.centralLongitude(lon);
      flags.got_central_longitude = true;
    }
    else if (token == "truelatitude")
    {
      float lat = NAN;
      is >> lat;
      projection.trueLatitude(lat);
      flags.got_true_latitude = true;
    }
    else if (token == "bottomleft")
    {
      float lon = NAN;
      float lat = NAN;
      is >> lon >> lat;
      projection.bottomLeft(lon, lat);
      flags.got_bottomleft = true;
    }
    else if (token == "topright")
    {
      float lon = NAN;
      float lat = NAN;
      is >> lon >> lat;
      projection.topRight(lon, lat);
      flags.got_topright = true;
    }
    else if (token == "center")
    {
      float lon = NAN;
      float lat = NAN;
      is >> lon >> lat;
      projection.center(lon, lat);
      flags.got_center = true;
    }
    else if (token == "scale")
    {
      float scale = NAN;
      is >> scale;
      projection.scale(scale);
      flags.got_scale = true;
    }
    else
    {
      std::string msg = "Unrecognized token ";
      msg += token;
      msg += " in projection ";
      msg += proj_name;
      throw std::runtime_error(msg);
    }
  }

  if (!is.good())
  {
    std::string msg;
    if (is.eof())
      msg = "Closing brace missing from projection ";
    else
      msg = "Failed to read projection ";
    msg += proj_name;
    throw std::runtime_error(msg);
  }
}

// Validate the given projection settings
// Validate the projection type specific settings
void validate_projection_parameters(const ProjectionFlags& flags, const std::string& error_prefix)
{
  if (flags.proj_type == "latlon" || flags.proj_type == "ykj" || flags.proj_type == "mercator")
  {
    if (flags.got_central_latitude)
      throw std::runtime_error(error_prefix + " does not need a centrallatitude");
    if (flags.got_central_longitude)
      throw std::runtime_error(error_prefix + " does not need a centrallongitude");
  }
  else
  {
    if (!flags.got_central_latitude)
      throw std::runtime_error(error_prefix + " requires a centrallatitude");
    if (!flags.got_central_longitude)
      throw std::runtime_error(error_prefix + " requires a centrallongitude");
  }

  if (flags.proj_type == "latlon" || flags.proj_type == "ykj" || flags.proj_type == "mercator" ||
      flags.proj_type == "equidist")
  {
    if (flags.got_true_latitude)
      throw std::runtime_error(error_prefix + " does not need a truelatitude");
  }
  else
  {
    if (!flags.got_true_latitude)
      throw std::runtime_error(error_prefix + " requires a truelatitude");
  }
}

void validate_projection(const ProjectionFlags& flags, const std::string& proj_name)
{
  std::string error_prefix = "projection ";
  error_prefix += proj_name;

  bool got_corners = (flags.got_bottomleft && flags.got_topright);

  if (!flags.got_type)
    throw std::runtime_error(error_prefix + " must specify projection type");

  if ((flags.got_bottomleft && !flags.got_topright) ||
      (!flags.got_bottomleft && flags.got_topright))
    throw std::runtime_error(error_prefix + " should have both bottomleft and topright");

  if ((flags.got_center && !flags.got_scale) || (!flags.got_center && flags.got_scale))
    throw std::runtime_error(error_prefix + " should have both center and scale");

  if (got_corners && flags.got_center)
    throw std::runtime_error(error_prefix + " must not specify center and corners simultaneously");

  validate_projection_parameters(flags, error_prefix);
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * Parse a projection specification from the given input stream
 * and store it.
 *
 * This method throws a std::runtime_error if the specification is
 * incomplete or incorrect.
 *
 * \param is The input stream to read from
 * \param theStore The store for the projection
 */
// ----------------------------------------------------------------------

void ProjectionParser::parse(std::istream& is, ProjectionStore& theStore)
{
  // An initial safety check
  if (!is.good())
    throw std::runtime_error(
        "The input stream is not good when trying to read a projection specification");

  std::string proj_name;
  is >> proj_name;
  if (!is.good() || proj_name == "{")
    throw std::runtime_error("Projection name is missing");

  std::string token;
  is >> token;
  if (token != "{")
    throw std::runtime_error("The projection specification must start with a {");

  // The projection info

  Projection projection;

  ProjectionFlags flags;
  read_projection_settings(is, projection, flags, proj_name);

  // Now build the projection

  validate_projection(flags, proj_name);

  theStore.add(proj_name, projection);
}
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
