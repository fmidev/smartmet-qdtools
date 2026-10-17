// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace DataTransform
 */
// ======================================================================

#include "DataTransform.h"
#include <map>
#include <stdexcept>

using namespace std;

namespace FMI
{
namespace RadContour
{
namespace DataTransform
{
// ----------------------------------------------------------------------
/*!
 * \brief Return the raw data multiplier
 *
 * Throws for unknown parameters
 *
 * \param theParam The parameter name
 * \return The multiplier for the parameter
 */
// ----------------------------------------------------------------------

double multiplier(const std::string& theParam)
{
  static const std::map<std::string, double> multipliers = {
      {"Precipitation1h", 0.01},
      {"PrecipitationRate", 0.01},
      {"CorrectedReflectivity", 0.5},
      {"SurfaceWaterPhase", 100.0 / 255.0},  // from 0-255 to 0-100
      {"EchoTop", 0.1},
      {"Detectability", -100.0 / 255},
      {"Precipitation3hF0", 0.01},
      {"Precipitation3hF1", 0.01},
      {"Precipitation3hF2", 0.01},
      {"Precipitation3hF5", 0.01},
      {"Precipitation3hF6", 0.01},
      {"Precipitation3hF7", 0.01},
      {"Precipitation3hF8", 0.01},
      {"Precipitation3hF9", 0.01},
      {"Precipitation3hF10", 0.01},
      {"Precipitation3hF12", 0.01},
      {"Precipitation3hF20", 0.01},
      {"Precipitation3hF25", 0.01},
      {"Precipitation3hF30", 0.01},
      {"Precipitation3hF37", 0.01},
      {"Precipitation3hF40", 0.01},
      {"Precipitation3hF50", 0.01},
      {"Precipitation3hF60", 0.01},
      {"Precipitation3hF63", 0.01},
      {"Precipitation3hF70", 0.01},
      {"Precipitation3hF75", 0.01},
      {"Precipitation3hF80", 0.01},
      {"Precipitation3hF88", 0.01},
      {"Precipitation3hF90", 0.01},
      {"Precipitation3hF95", 0.01},
      {"Precipitation3hF98", 0.01},
      {"Precipitation3hF99", 0.01},
      {"Precipitation3hF100", 0.01},
      {"ProbabilityOfPrecipitation3h0mm", 0.01},
      {"ProbabilityOfPrecipitation3h01mm", 0.01},
      {"ProbabilityOfPrecipitation3h05mm", 0.01},
      {"ProbabilityOfPrecipitation3h1mm", 0.01},
      {"ProbabilityOfPrecipitation3h2mm", 0.01},
      {"ProbabilityOfPrecipitation3h3mm", 0.01},
      {"ProbabilityOfPrecipitation3h4mm", 0.01},
      {"ProbabilityOfPrecipitation3h5mm", 0.01},
      {"ProbabilityOfPrecipitation3h6mm", 0.01},
      {"ProbabilityOfPrecipitation3h7mm", 0.01},
      {"ProbabilityOfPrecipitation3h8mm", 0.01},
      {"ProbabilityOfPrecipitation3h9mm", 0.01},
      {"ProbabilityOfPrecipitation3h10mm", 0.01},
      {"ProbabilityOfPrecipitation3h12mm", 0.01},
      {"ProbabilityOfPrecipitation3h14mm", 0.01},
      {"ProbabilityOfPrecipitation3h16mm", 0.01},
      {"ProbabilityOfPrecipitation3h18mm", 0.01},
      {"ProbabilityOfPrecipitation3h20mm", 0.01},
      {"ProbabilityOfPrecipitation3h25mm", 0.01},
      {"ProbabilityOfPrecipitation3h30mm", 0.01},
      {"ProbabilityOfPrecipitation3h35mm", 0.01},
      {"ProbabilityOfPrecipitation3h40mm", 0.01},
      {"ProbabilityOfPrecipitation3h45mm", 0.01},
      {"ProbabilityOfPrecipitation3h50mm", 0.01},
      {"ProbabilityOfPrecipitation3h60mm", 0.01},
      {"ProbabilityOfPrecipitation3h70mm", 0.01},
      {"ProbabilityOfPrecipitation3h80mm", 0.01},
      {"ProbabilityOfPrecipitation3h90mm", 0.01},
      {"ProbabilityOfPrecipitation3h100mm", 0.01},
      {"ProbabilityOfPrecipitation3h150mm", 0.01},
      {"ProbabilityOfPrecipitation3h200mm", 0.01},
      {"ProbabilityOfPrecipitation3h500mm", 0.01}};

  const auto it = multipliers.find(theParam);
  if (it != multipliers.end())
    return it->second;
  return 1.0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the raw data offset
 *
 * Throws for unknown parameters
 *
 * \param theParam The parameter name
 * \return The offset for the parameter
 */
// ----------------------------------------------------------------------

double offset(const std::string& theParam)
{
  if (theParam == "CorrectedReflectivity")
    return -32;
  if (theParam == "EchoTop")
    return -0.1;
  if (theParam == "Detectability")
    return 100;
  return 0.0;
}
}  // namespace DataTransform
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
