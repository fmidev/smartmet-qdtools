#pragma once

//h5pp bug workaround (missing include <algorithm> there)
#include <algorithm>
#include <stdexcept>

#ifndef H5PP_USE_FMT
#define H5PP_USE_FMT
#endif


#include <h5pp/h5pp.h>
#include <functional>
#include <iostream>
#include <map>

#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/NumericCast.h>
#include <macgyver/Join.h>

#include <macgyver/DebugTools.h>

bool is_group_attribute(const h5pp::File& file, const std::string& path, const std::string& name);

std::string get_attribute_string(const h5pp::File& file, const std::string& path, const std::string& name);

namespace detail
{

     constexpr const bool h5pp_support_endianess = false;

     H5T_order_t get_native_order();

     template <typename InfoType>
     std::enable_if_t<
          std::is_same<InfoType, h5pp::AttrInfo>::value or
          std::is_same<InfoType, h5pp::DsetInfo>::value, bool>
     requires_byte_swap(const InfoType& info)
     {
          // Check if the data is stored in a different endianness than the native one
          if (not info.h5Type)
               throw Fmi::Exception(BCP, "Attribute HDF5 type is not provided");
          static const H5T_order_t native_order = get_native_order();
          const H5T_order_t order = H5Tget_order(*info.h5Type);
          return order != native_order;
     }

     template <typename DataType, typename InfoType>
     std::enable_if_t<
          std::is_same<InfoType, h5pp::AttrInfo>::value or
          std::is_same<InfoType, h5pp::DsetInfo>::value, DataType>
     maybe_swap_bytes(const DataType& data, const InfoType& info)
     {
          if constexpr (h5pp_support_endianess)
          {
               // Already handled
          }
          else if constexpr (std::is_arithmetic<DataType>::value or std::is_floating_point<DataType>::value)
          {
               if (sizeof(data) > 1 and requires_byte_swap(info))
               {
                    union {
                         DataType value;
                         char bytes[sizeof(DataType)];
                    };
                    value = data;
                    std::reverse(std::begin(bytes), std::end(bytes));
                    return value;
               }
          }
          return data;
     }

     template <typename DataType, typename InfoType>
     std::enable_if_t<
          std::is_same<InfoType, h5pp::AttrInfo>::value or
          std::is_same<InfoType, h5pp::DsetInfo>::value, std::vector<DataType>>
     maybe_swap_bytes(const std::vector<DataType>& data, const InfoType& info)
     {
          std::vector<DataType> result;
          result.reserve(data.size());
          for (const DataType& value : data)
               result.push_back(maybe_swap_bytes(value, info));
          return result;
     }

     template <typename InfoType>
     std::enable_if_t<
          std::is_same<InfoType, h5pp::AttrInfo>::value or
          std::is_same<InfoType, h5pp::DsetInfo>::value, std::type_index>
     detect_type_index(const InfoType& info)
     {
          if (not info.cppTypeIndex or not info.h5Type or not info.cppTypeName or not info.cppTypeSize)
               throw Fmi::Exception(BCP, "No type information or it is incomplete");
          if (info.cppTypeIndex == std::type_index(typeid(decltype(nullptr))))
          {
               // h5pp has been unable to detect the type, so we need to do it ourselves if possible
               // and do endian related byte swaps also ourselves when necessary
               if (H5Tget_class(*info.h5Type) == H5T_INTEGER)
               {
                    switch (*info.cppTypeSize)
                    {
                    case 1:
                         return std::type_index(typeid(uint8_t));
                    case 2:
                         return std::type_index(typeid(uint16_t));
                    case 4:
                         return std::type_index(typeid(uint32_t));
                    case 8:
                         return std::type_index(typeid(uint64_t));
                    default:
                         throw Fmi::Exception(BCP, "Unsupported integer type with item size "
                             + Fmi::to_string(*info.cppTypeSize));
                    }
               }
               else
               {
                    // FIXME: support some other types like float and double with different endian
                    throw Fmi::Exception(BCP, "Unsupported type " + *info.cppTypeName);
               }
          }
          else
          {
               // h5pp has misdetected uint8_t as bool, so fix it
               if (*info.cppTypeIndex == std::type_index(typeid(bool)))
                    return std::type_index(typeid(uint8_t));

               // h5pp type detection was succesfull - so can use it
               return *info.cppTypeIndex;
          }
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

     template <typename Target, typename AttributeType>
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value ||
          std::is_same<Target, std::string>::value,
          Target>
     get_attribute_impl(const h5pp::File& file, const std::string& path, const std::string& name)
     {
          const h5pp::AttrInfo info = file.getAttributeInfo(path, name);
          if (not info.attrSize)
               throw Fmi::Exception(BCP, "Attribute " + name + " size is not provided");

          switch (*info.attrSize)
          {
          case 0:
               throw Fmi::Exception(BCP, "Attribute " + name + " is empty");
          case 1:
               if constexpr (std::is_same<Target, std::string>::value)
                    return file.readAttribute<AttributeType>(path, name);
               else
                    return Fmi::numeric_cast<Target>(
                         maybe_swap_bytes(
                              file.readAttribute<AttributeType>(path, name),
                              info
                         ));
          default:
               throw Fmi::Exception(BCP, "Attribute " + name + " has " + Fmi::to_string(int(*info.attrSize))
                    + " values, but only one is requested");
          }
     }

     template <typename Target, typename AttributeType>
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value ||
          std::is_same<Target, std::string>::value,
          std::vector<Target>>
     get_attribute_vect_impl(const h5pp::File& file, const std::string& path, const std::string& name)
     {
          const h5pp::AttrInfo info = file.getAttributeInfo(path, name);
          if constexpr (std::is_same<Target, std::string>::value)
               return file.readAttribute<std::vector<AttributeType>>(path, name);
          else
          {
               const std::vector<AttributeType> value =
                    maybe_swap_bytes(
                         file.readAttribute<std::vector<AttributeType>>(path, name), info);

               std::vector<Target> result;
               result.reserve(value.size());
               for (const auto& v : value)
                    result.push_back(Fmi::numeric_cast<Target>(v));
               return maybe_swap_bytes(result, info);
          }
     }

     template <typename Target, typename... SupportedAttributeTypes>
     class AttributeTypeMap
     {
     public:
          constexpr AttributeTypeMap()
          {
               ((converters[std::type_index(typeid(SupportedAttributeTypes))] = &get_attribute_impl<Target, SupportedAttributeTypes>), ...);
               ((converters_vect[std::type_index(typeid(std::vector<SupportedAttributeTypes>))] = &get_attribute_vect_impl<Target, SupportedAttributeTypes>), ...);
          }

          ~AttributeTypeMap() = default;

          Target get(const h5pp::File& file, const std::string& path, const std::string& name) const
          {
               using namespace std::literals;
               if (file.attributeExists(std::string_view(path), name))
               {
                    const auto info = file.getAttributeInfo(path, name);

                    const std::type_index ti = detect_type_index(info);

                    if (std::type_index(typeid(Target)) == ti)
                    {
                         return maybe_swap_bytes(file.readAttribute<Target>(path, name), info);
                    }

                    const auto it = converters.find(ti);
                    if (it != converters.end())
                    {
                         if (not info.attrSize)
                              throw Fmi::Exception(BCP, "Attribute " + name + " size is not provided");
                         else if (*info.attrSize == 1)
                              return it->second(file, path, name);
                         else
                              throw Fmi::Exception(BCP, "Attribute " + name + " has " + Fmi::to_string(int(*info.attrSize))
                                   + " values, but exactly one is requested");
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

          std::vector<Target> get_vect(const h5pp::File& file, const std::string& path, const std::string& name) const
          {
             using namespace std::literals;
             if (file.attributeExists(std::string_view(path), name))
               {
                    const auto info = file.getAttributeInfo(path, name);

                    const std::type_index ti = detect_type_index(info);

                    if (std::type_index(typeid(Target)) == ti)
                    {
                         return maybe_swap_bytes(file.readAttribute<std::vector<Target>>(path, name), info);
                    }

                    const auto it = converters_vect.find(ti);

                    if (it != converters_vect.end())
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

     private:
         typedef Target(*converter_t)(const h5pp::File&, const std::string&, const std::string&);
         typedef std::vector<Target>(*converter_vect_t)(const h5pp::File&, const std::string&, const std::string&);
         typedef std::map<std::type_index, converter_t> converter_map_t;
         typedef std::map<std::type_index, converter_vect_t> converter_vect_map_t;

         converter_map_t converters;
         converter_vect_map_t converters_vect;
     };

     template <typename Target, typename DataType>
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value,
          std::vector<Target>>
     read_dataset_impl(const h5pp::File& file, const std::string& path)
     {
          const h5pp::DsetInfo info = file.getDatasetInfo(path);
          auto tmp = file.readDataset<std::vector<DataType>>(path);
          if constexpr (std::is_same<Target, DataType>::value)
               return tmp;
          else
          {
               std::vector<Target> result;
               tmp = maybe_swap_bytes(tmp, info);
               result.reserve(tmp.size());
               for (const auto& v : tmp)
                    result.push_back(Fmi::numeric_cast<Target>(v));
               return result;
          }
     }
}

template <typename... SupportedTypes>
bool is_attribute(const h5pp::File &file, const std::string &path, const std::string &name)
{
  if (not is_group_attribute(file, path, name))
    return false;

  const h5pp::AttrInfo info = file.getAttributeInfo(path, name);
  if (not info.cppTypeIndex)
    throw Fmi::Exception(BCP, "Attribute " + name + " size is not provided");

  return (... or (*info.cppTypeIndex == std::type_index(typeid(SupportedTypes))));
}

template <typename Target>
typename std::optional<
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value or
          std::is_same<Target, std::string>::value,
          Target>>
get_optional_attribute(const h5pp::File& file, const std::string& path, const std::string& name)
{
     if (not file.attributeExists(std::string_view(path), name))
          return std::nullopt;

     const h5pp::AttrInfo info = file.getAttributeInfo(path, name);

     if (not info.cppTypeIndex)
          throw Fmi::Exception(BCP, "Attribute " + name + " has no type information");

     if constexpr (std::is_same<Target, std::string>::value)
     {
          if (*info.cppTypeIndex == std::type_index(typeid(std::string)))
          {
               return file.readAttribute<std::string>(path, name);
          }
          else
          {
               throw Fmi::Exception(BCP, "Attempt to read non-string attribute "
                    + path + "/" + name + " as std::string. Use read_string_attribute instead");
          }
     }
     else
     {
          static const detail::AttributeTypeMap<Target,
               int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> converters;
          return converters.get(file, path, name);
     }
}

template <typename Target>
typename std::optional<
     typename std::vector<typename std::enable_if_t<
          Fmi::is_numeric<Target>::value or
          std::is_same<Target, std::string>::value,
          Target>>>
get_optional_attribute_vect(const h5pp::File& file, const std::string& path, const std::string& name)
{
     if (not file.attributeExists(std::string_view(path), name))
          return std::nullopt;

     const h5pp::AttrInfo info = file.getAttributeInfo(path, name);

     if (not info.cppTypeIndex)
          throw Fmi::Exception(BCP, "Attribute " + name + " has no type information");

     if constexpr (std::is_same<Target, std::string>::value)
     {
          if (*info.cppTypeIndex == std::type_index(typeid(std::string)))
          {
               return file.readAttribute<std::vector<std::string>>(path, name);
          }
          else
          {
               throw Fmi::Exception(BCP, "Attempt to read non-string attribute "
                    + path + "/" + name + " as std::string. Use read_string_attribute instead");
          }
     }
     else
     {
          static detail::AttributeTypeMap<Target,
               int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> converters;
          return converters.get_vect(file, path, name);
     }
}

template <typename Target>
typename std::enable_if_t<(
     Fmi::is_numeric<Target>::value) ||
     std::is_same<Target, std::string>::value,
     Target>
get_attribute(const h5pp::File& file, const std::string& path, const std::string& name)
{
     const std::optional<Target> opt_value = get_optional_attribute<Target>(file, path, name);
     if (opt_value)
          return *opt_value;
     else
          throw Fmi::Exception(BCP, "Attribute " + name + " not found at path " + path);
}

template <typename Target>
typename std::vector<
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value
          || std::is_same<Target, std::string>::value,
          Target>>
get_attribute_vect(const h5pp::File& file, const std::string& path, const std::string& name)
{
     const auto opt_value = get_optional_attribute_vect<Target>(file, path, name);
     if (opt_value)
          return *opt_value;
     else
          throw Fmi::Exception(BCP, "Attribute " + name + " not found at path " + path);
}

template <typename Target>
typename std::optional<
     typename std::enable_if_t<
          Fmi::is_numeric<Target>::value
          or std::is_same<Target, std::string>::value
          , Target>>
get_optional_attribute_recursive(const h5pp::File& file, const std::string& parent_path_, const std::string& group, const std::string& attribute_name)
{
     namespace fs = std::filesystem;

     const std::string tmp = parent_path_.substr(0, 1) != "/" ? "/" + parent_path_ : parent_path_;
     std::filesystem::path parent_path(tmp);

     int cnt = 100;
     const fs::path root = parent_path.root_directory();
     do
     {
#pragma note "Currently no checking for presence of parent group. Causes problems for RHEL/RockyLinux 8"
          const std::filesystem::path group_path = parent_path / group;
          if (file.attributeExists(std::string_view(group_path.string()), attribute_name))
               return get_optional_attribute<Target>(file, group_path, attribute_name);
          else
          parent_path = parent_path.parent_path();
     }
     while (parent_path != root and cnt-- >= 0);

     return {};
}

template <typename Target>
typename std::optional<
     typename std::vector<
          typename std::enable_if_t<
               Fmi::is_numeric<Target>::value
               or std::is_same<Target, std::string>::value,
               Target>>>
get_optional_attribute_vect_recursive(const h5pp::File& file, const std::string& parent_path_, const std::string& group, const std::string& attribute_name)
{
     namespace fs = std::filesystem;

     const std::string tmp = parent_path_.substr(0, 1) != "/" ? "/" + parent_path_ : parent_path_;
     std::filesystem::path parent_path(tmp);

     int cnt = 100;
     const fs::path root = parent_path.root_directory();
     do
     {
#pragma note "Currently no checking for presence of parent group. Causes problems for RHEL/RockyLinux 8"
          const std::filesystem::path group_path = parent_path / group;
          if (file.attributeExists(std::string_view(group_path.string()), attribute_name))
               return get_optional_attribute_vect<Target>(file, group_path, attribute_name);
          else
               parent_path = parent_path.parent_path();
     }
     while (parent_path != root and cnt-- >= 0);

     return {};
}

template <typename Target>
typename std::enable_if_t<
     Fmi::is_numeric<Target>::value
     or std::is_same<Target, std::string>::value
     , Target>
get_attribute_recursive(const h5pp::File& file, const std::string& parent_path, const std::string& group, const std::string& name)
{
  const std::optional<Target> opt_value = get_optional_attribute_recursive<Target>(file, parent_path, group, name);
  if (opt_value)
    return *opt_value;
  else
    throw Fmi::Exception(BCP, "Attribute " + name + " not found at path (or parent paths)" + parent_path + group);
}

template <typename Target>
typename std::enable_if_t<
     Fmi::is_numeric<Target>::value
     or std::is_same<Target, std::string>::value
     , Target>
get_attribute_vect_recursive(const h5pp::File& file, const std::string& parent_path, const std::string& group, const std::string& name)
{
  const std::optional<Target> opt_value = get_optional_attribute_vect_recursive<Target>(file, parent_path, group, name);
  if (opt_value)
    return *opt_value;
  else
    throw Fmi::Exception(BCP, "Attribute " + name + " not found at path (or paremnt paths)" + parent_path + group);
}

/**
 * @brief Read dataset contents from HDF5 file
 * 
 * - Target - Target data type which the dataset will be converted to
 * - SupportedDataTypes - Supported data types of the dataset (only standard numeric types can be specified)
 */
template <typename Target, typename... SupportedDataTypes>
typename std::enable_if_t<
     Fmi::is_numeric<Target>::value,
     std::vector<Target>>
read_dataset(const h5pp::File& file, const std::string& path)
{
     using namespace std::literals;
     typedef std::vector<Target>(*converter_t)(const h5pp::File&, const std::string&);
     std::map<std::type_index, converter_t> converters;
     static_assert((... && Fmi::is_numeric<SupportedDataTypes>::value), "Only numeric types from dataset are supported");
     ((converters[std::type_index(typeid(SupportedDataTypes))] = detail::read_dataset_impl<Target, SupportedDataTypes>), ...);
     converters[std::type_index(typeid(uint8_t))] = detail::read_dataset_impl<Target, uint8_t>;
     converters[std::type_index(typeid(uint16_t))] = detail::read_dataset_impl<Target, uint16_t>;

     const std::string path_ = path.substr(0, 1) != "/" ? "/" + path : path;
     //std::cout << "Looking up datasets at path: " << path_ << std::endl;
     const std::vector<std::string> datasets = file.findDatasets("", path_);

     if (datasets.empty())
          throw Fmi::Exception(BCP, "Dataset " + path + " not found");

     //std::cout << "Found datasets: " << Fmi::join(datasets, ", ") << std::endl;

     if (datasets.size() > 1)
          throw Fmi::Exception(BCP, "Several dataset found at " + path);

     const std::string fn = path_ + "/" + datasets.at(0);
     const h5pp::DsetInfo info = file.getDatasetInfo(fn);
     //std::cout << "Dataset found: " << fn << " ("
     //     << (info.cppTypeName ? "type [" + *info.cppTypeName + "]" : "")
     //     << info.string() << ")" << std::endl;

     if (not info.cppTypeIndex or not info.cppTypeName)
          throw Fmi::Exception(BCP, "Dataset " + fn + " has no type information");

     std::type_index ti = detail::detect_type_index(info);

     const auto it = converters.find(ti);
     if (it != converters.end())
     {
          //std::cout << "Reading dataset " << fn << std::endl;
          std::vector<int> data = it->second(file, fn);
          return data;
     }
     else
          throw Fmi::Exception(BCP, "Dataset " + fn + " is of unsupported type "
               + (info.cppTypeName ? *info.cppTypeName : ""s));
}
