#include "HdfTools.h"

using namespace std::literals;

bool is_group_attribute(const h5pp::File& file, const std::string& path, const std::string& name)
{
  return file.attributeExists(std::string_view(path), name);
}

H5T_order_t detail::get_native_order()
{
     // Perhaps would be unneeded for x86_64, but somebody may try to buildata on ARM
     // Could also use H5Tget_order(H5T_NATIVE_INT32) or similar
     union {
          uint32_t value;
          uint8_t bytes[sizeof(uint32_t)];
     };
     value = 0xdeadbeef;
     return bytes[0] == 0xef ? H5T_ORDER_LE : H5T_ORDER_BE;
}

std::string get_attribute_string(const h5pp::File& file, const std::string& path, const std::string& name)
{
  static const std::map<std::type_index, std::string(*)(const h5pp::File&, const std::string&, const std::string&)> converters = {
      {std::type_index(typeid(int8_t)), detail::get_attribute_string_impl<int8_t>},
      {std::type_index(typeid(uint8_t)), detail::get_attribute_string_impl<uint8_t>},
      {std::type_index(typeid(int16_t)), detail::get_attribute_string_impl<int16_t>},
      {std::type_index(typeid(uint16_t)), detail::get_attribute_string_impl<uint16_t>},
      {std::type_index(typeid(int32_t)), detail::get_attribute_string_impl<int32_t>},
      {std::type_index(typeid(uint32_t)), detail::get_attribute_string_impl<uint32_t>},
      {std::type_index(typeid(int64_t)), detail::get_attribute_string_impl<int64_t>},
      {std::type_index(typeid(uint64_t)), detail::get_attribute_string_impl<uint64_t>},
      {std::type_index(typeid(float)), detail::get_attribute_string_impl<float>},
      {std::type_index(typeid(double)), detail::get_attribute_string_impl<double>},
      {std::type_index(typeid(std::string)), [](const h5pp::File& file, const std::string& path, const std::string& name) {
         return file.readAttribute<std::string>(path, name);
      }}};

  if (file.attributeExists(std::string_view(path), name))
  {
    const auto info = file.getAttributeInfo(path, name);
    if (not info.cppTypeName)
      throw Fmi::Exception(BCP, "Attribute " + name + " has no type information");
    const auto it = converters.find(*info.cppTypeIndex);
    if (it != converters.end())
    {
      return it->second(file, path, name);
    }
    else
    {
      throw Fmi::Exception(BCP, "Variable " + name + " is of unsupported type "
        + (info.cppTypeName ? *info.cppTypeName : ""s));
    }
  }
  else
  {
    throw std::runtime_error("Attribute " + name + " not found at path " + path);
  }
}
