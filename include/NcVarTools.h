#pragma once
// netCDF C++ helper utilities - used by wrftoqd which directly uses the netCDF C++ API

#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <macgyver/TypeTraits.h>
#include <ncAtt.h>
#include <ncDim.h>
#include <ncVar.h>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

namespace nctools
{

// Implemented in NcVarTools.cpp
std::string get_att_string_value(const netCDF::NcAtt& att);

template <typename ReturnType, typename ObjectType>
typename std::enable_if<std::is_base_of<netCDF::NcAtt, ObjectType>::value,
                        std::vector<ReturnType>>::type
get_att_vector_value(const ObjectType& att)
{
  try
  {
    const auto length = att.getAttLength();
    if (length == 0)
      return std::vector<ReturnType>();

    std::vector<ReturnType> values(length);
    att.getValues(values.data());
    return values;
  }
  catch (...)
  {
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("Attribute name", att.getName());
    error.addParameter("Return type", Fmi::demangle_cpp_type_name(typeid(ReturnType).name()));
    error.addParameter("Attribute length", std::to_string(att.getAttLength()));
    error.addParameter("Attribute type", att.getType().getName());
    throw error;
  }
}

template <typename ReturnType>
typename std::enable_if<Fmi::is_numeric<ReturnType>::value, ReturnType>::type
get_att_value(const netCDF::NcAtt& att, std::size_t index)
{
  try
  {
    const auto length = att.getAttLength();
    if (length < index + 1)
      throw std::runtime_error("The attribute doesn not have element " +
                               Fmi::to_string(index));

    std::vector<ReturnType> values(length);
    att.getValues(values.data());
    return values.at(index);
  }
  catch (...)
  {
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("Attribute name", att.getName());
    error.addParameter("Return type", Fmi::demangle_cpp_type_name(typeid(ReturnType).name()));
    error.addParameter("Attribute length", std::to_string(att.getAttLength()));
    error.addParameter("Attribute type", att.getType().getName());
    error.addParameter("Index", std::to_string(index));
    throw error;
  }
}

template <typename ReturnType>
typename std::enable_if<Fmi::is_numeric<ReturnType>::value ||
                            std::is_same<ReturnType, std::string>::value,
                        std::vector<ReturnType>>::type
get_values(const netCDF::NcVar& var)
{
  try
  {
    const auto dims = var.getDims();
    if (dims.empty())
      return std::vector<ReturnType>();

    const std::size_t length =
        std::accumulate(dims.begin(),
                        dims.end(),
                        1,
                        [](std::size_t a, const netCDF::NcDim& b) { return a * b.getSize(); });
    std::vector<ReturnType> values(length);
    var.getVar(values.data());
    return values;
  }
  catch (...)
  {
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("Variable name", var.getName());
    error.addParameter("Return type", Fmi::demangle_cpp_type_name(typeid(ReturnType).name()));
    error.addParameter("Variable length", std::to_string(var.getDimCount()));
    error.addParameter("Variable type", var.getType().getName());
    throw error;
  }
}

}  // namespace nctools
