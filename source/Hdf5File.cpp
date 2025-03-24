#include "Hdf5File.h"

using namespace std::literals;

namespace Fmi
{
namespace HDF5
{
namespace detail
{
    H5T_order_t get_native_order()
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

    template <typename AttributeType>
    std::string get_attribute_string_impl(const h5pp::File& file, const std::string& path, const std::string& name)
    {
        const h5pp::AttrInfo info = file.getAttributeInfo(path, name);
        const std::vector<AttributeType> raw_value = file.readAttribute<std::vector<AttributeType>>(path, name);
        const std::vector<AttributeType> value = detail::maybe_swap_bytes(raw_value, info);
        if constexpr (std::is_same<AttributeType, std::string>::value)
            return Fmi::join(value, ", ");
        else
            return Fmi::join(value, [](const AttributeType& s) { return std::to_string(s); }, ", ");
    }
} // namespace detail


Hdf5File::Hdf5File(const std::string& path, h5pp::FileAccess fileAccess)
try
    : file(h5pp::File(path, fileAccess))
{
    // Get all groups
    const std::vector<std::string> t_groups = file.findGroups();
    for (const auto& group : t_groups)
        groups.insert(group);

    // Get all datasets
    const std::vector<std::string> t_datasets = file.findDatasets();
    for (const auto& dataset : t_datasets)
        datasets.insert(dataset);
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Failed to open HDF5 file");
    error.addParameter("Path", path);
    throw error;
}


bool Hdf5File::is_group(const h5pp::fs::path& path) const
{
    std::string name = path.string();
    if (name.substr(0, 1) == "/"s)
        name = name.substr(1);
    return groups.count(name);
}


std::set<std::string> Hdf5File::get_attribute_names(const h5pp::fs::path& path) const
{
    if (not groups.count(path))
        return {};

    const std::vector<std::string> attrNames = file.getAttributeNames(path.string());
    return std::set<std::string>(attrNames.begin(), attrNames.end());
}


h5pp::AttrInfo Hdf5File::get_attribute_info(const h5pp::fs::path& path, const std::string& name) const
{
    if (not groups.count(path))
        throw Fmi::Exception(BCP, "Group " + path.string() + " not found");

    if (not file.attributeExists(std::string_view(path.string()), name))
        throw Fmi::Exception(BCP, "Attribute " + name + " not found in group " + path.string());

    return file.getAttributeInfo(path.string(), name);
}

std::string Hdf5File::get_attribute_string(const std::string& path, const std::string& name) const
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

std::set<std::string> Hdf5File::get_top_names() const
{
    std::set<std::string> ret;
    const std::vector<std::string> names = file.findGroups(""s, "/"s, -1, 0);
    return std::set<std::string>(names.begin(), names.end());
}

} // namespace HDF5
}  // namespace Fm
