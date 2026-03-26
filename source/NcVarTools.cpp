#include "NcVarTools.h"
#include <macgyver/Exception.h>
#include <ncAtt.h>
#include <ncType.h>
#include <string>

namespace nctools
{

std::string get_att_string_value(const netCDF::NcAtt& att)
{
  try
  {
    std::string value;
    att.getValues(value);
    return value;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace nctools
