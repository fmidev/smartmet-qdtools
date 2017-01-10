// ======================================================================
/*!
 * \file
 * \brief Interface of namespace DataTransform
 */
// ======================================================================
/*!
 * \namespace RadContour::DataTransform
 *
 */
// ======================================================================

#ifndef FMI_RADCONTOUR_DATATRANSFORM_H
#define FMI_RADCONTOUR_DATATRANSFORM_H

#include <string>

namespace FMI
{
namespace RadContour
{
namespace DataTransform
{
double multiplier(const std::string& theParam);
double offset(const std::string& theParam);

}  // namespace DataTransform
}  // namespace RadContour
}  // namespace FMI

#endif  // FMI_RADCONTOUR_DATATRANSFORM_H

// ======================================================================
