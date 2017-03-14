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
#include <iostream>
#include <stdexcept>
#include <string>

namespace FMI
{
namespace RadContour
{
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
  if (!is.good() || proj_name == "{") throw std::runtime_error("Projection name is missing");

  std::string token;
  is >> token;
  if (token != "{") throw std::runtime_error("The projection specification must start with a {");

  // The projection info

  Projection projection;
  std::string proj_type = "";
  bool got_type = false;
  bool got_bottomleft = false;
  bool got_topright = false;
  bool got_center = false;
  bool got_scale = false;
  bool got_central_longitude = false;
  bool got_central_latitude = false;
  bool got_true_latitude = false;

  while (is.good())
  {
    is >> token;
    if (!is.good()) break;

    if (token == "}")
      break;
    else if (token == "type")
    {
      is >> token;
      projection.type(token);
      proj_type = token;
      got_type = true;
    }
    else if (token == "centrallatitude")
    {
      float lat;
      is >> lat;
      projection.centralLatitude(lat);
      got_central_latitude = true;
    }
    else if (token == "centrallongitude")
    {
      float lon;
      is >> lon;
      projection.centralLongitude(lon);
      got_central_longitude = true;
    }
    else if (token == "truelatitude")
    {
      float lat;
      is >> lat;
      projection.trueLatitude(lat);
      got_true_latitude = true;
    }
    else if (token == "bottomleft")
    {
      float lon, lat;
      is >> lon >> lat;
      projection.bottomLeft(lon, lat);
      got_bottomleft = true;
    }
    else if (token == "topright")
    {
      float lon, lat;
      is >> lon >> lat;
      projection.topRight(lon, lat);
      got_topright = true;
    }
    else if (token == "center")
    {
      float lon, lat;
      is >> lon >> lat;
      projection.center(lon, lat);
      got_center = true;
    }
    else if (token == "scale")
    {
      float scale;
      is >> scale;
      projection.scale(scale);
      got_scale = true;
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

  // Now build the projection

  std::string error_prefix = "projection ";
  error_prefix += proj_name;

  bool got_corners = (got_bottomleft && got_topright);

  if (!got_type) throw std::runtime_error(error_prefix + " must specify projection type");

  if ((got_bottomleft && !got_topright) || (!got_bottomleft && got_topright))
    throw std::runtime_error(error_prefix + " should have both bottomleft and topright");

  if ((got_center && !got_scale) || (!got_center && got_scale))
    throw std::runtime_error(error_prefix + " should have both center and scale");

  if (got_corners && got_center)
    throw std::runtime_error(error_prefix + " must not specify center and corners simultaneously");

  if (proj_type == "latlon" || proj_type == "ykj" || proj_type == "mercator")
  {
    if (got_central_latitude)
      throw std::runtime_error(error_prefix + " does not need a centrallatitude");
    if (got_central_longitude)
      throw std::runtime_error(error_prefix + " does not need a centrallongitude");
  }
  else
  {
    if (!got_central_latitude)
      throw std::runtime_error(error_prefix + " does not need a centrallatitude");
    if (!got_central_longitude)
      throw std::runtime_error(error_prefix + " does not need a centrallongitude");
  }

  if (proj_type == "latlon" || proj_type == "ykj" || proj_type == "mercator" ||
      proj_type == "equidist")
  {
    if (got_true_latitude) throw std::runtime_error(error_prefix + " does not need a truelatitude");
  }
  else
  {
    if (!got_true_latitude) throw std::runtime_error(error_prefix + " requires a truelatitude");
  }

  theStore.add(proj_name, projection);
}
}
}  // namespace FMI::RadContour

// ======================================================================
