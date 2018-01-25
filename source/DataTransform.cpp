// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace DataTransform
 */
// ======================================================================

#include "DataTransform.h"
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
  if (theParam == "Precipitation1h")
    return 0.01;
  else if (theParam == "PrecipitationRate")
    return 0.01;
  else if (theParam == "CorrectedReflectivity")
    return 0.5;
  else if (theParam == "SurfaceWaterPhase")  // from 0-255 to 0-100
    return 100.0 / 255.0;
  else if (theParam == "EchoTop")
    return 0.1;
  else if (theParam == "Detectability")
    return -100.0 / 255;
  else if (theParam == "Precipitation3hF0")
    return 0.01;
  else if (theParam == "Precipitation3hF1")
    return 0.01;
  else if (theParam == "Precipitation3hF2")
    return 0.01;
  else if (theParam == "Precipitation3hF5")
    return 0.01;
  else if (theParam == "Precipitation3hF6")
    return 0.01;
  else if (theParam == "Precipitation3hF7")
    return 0.01;
  else if (theParam == "Precipitation3hF8")
    return 0.01;
  else if (theParam == "Precipitation3hF9")
    return 0.01;
  else if (theParam == "Precipitation3hF10")
    return 0.01;
  else if (theParam == "Precipitation3hF12")
    return 0.01;
  else if (theParam == "Precipitation3hF20")
    return 0.01;
  else if (theParam == "Precipitation3hF25")
    return 0.01;
  else if (theParam == "Precipitation3hF30")
    return 0.01;
  else if (theParam == "Precipitation3hF37")
    return 0.01;
  else if (theParam == "Precipitation3hF40")
    return 0.01;
  else if (theParam == "Precipitation3hF50")
    return 0.01;
  else if (theParam == "Precipitation3hF60")
    return 0.01;
  else if (theParam == "Precipitation3hF63")
    return 0.01;
  else if (theParam == "Precipitation3hF70")
    return 0.01;
  else if (theParam == "Precipitation3hF75")
    return 0.01;
  else if (theParam == "Precipitation3hF80")
    return 0.01;
  else if (theParam == "Precipitation3hF88")
    return 0.01;
  else if (theParam == "Precipitation3hF90")
    return 0.01;
  else if (theParam == "Precipitation3hF95")
    return 0.01;
  else if (theParam == "Precipitation3hF98")
    return 0.01;
  else if (theParam == "Precipitation3hF99")
    return 0.01;
  else if (theParam == "Precipitation3hF100")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h0mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h01mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h05mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h1mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h2mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h3mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h4mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h5mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h6mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h7mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h8mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h9mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h10mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h12mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h14mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h16mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h18mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h20mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h25mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h30mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h35mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h40mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h45mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h50mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h60mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h70mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h80mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h90mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h100mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h150mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h200mm")
    return 0.01;
  else if (theParam == "ProbabilityOfPrecipitation3h500mm")
    return 0.01;
  else
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
  else if (theParam == "EchoTop")
    return -0.1;
  else if (theParam == "Detectability")
    return 100;
  else
    return 0.0;
}
}  // namespace DataTransform
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
