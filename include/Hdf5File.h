#pragma once

//h5pp bug workaround (missing include <algorithm> there)
#include <algorithm>
#include <stdexcept>

#ifndef H5PP_USE_FMT
#define H5PP_USE_FMT
#endif

#include <h5pp/h5pp.h>
#include <macgyver/Exception.h>
#include <macgyver/Join.h>
#include <macgyver/NumericCast.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <iostream>
#include <map>
#include <set>

namespace Fmi
{
namespace HDF5
{

namespace detail
{
    // h5pp versions before 1.11.3 do not handle endianness, so we need to do it ourselves when necessary
    constexpr const bool h5pp_support_endianess = (H5PP_VERSION >= 11103);
}

class Hdf5File
{
    h5pp::File file;
    std::set<std::string> groups;
    std::set<std::string> datasets;

public:
    Hdf5File(const std::string& path, h5pp::FileAccess fileAccess = h5pp::FileAccess::READONLY);
    Hdf5File(const Hdf5File&) = delete;
    Hdf5File& operator=(const Hdf5File&) = delete;
    Hdf5File(Hdf5File&&) = default;
    Hdf5File& operator=(Hdf5File&&) = default;
    virtual ~Hdf5File() = default;

    const h5pp::File& get() const { return file; }

    bool is_group(const h5pp::fs::path& path) const;

    template <typename AttrType>
    bool is_attribute(const h5pp::fs::path& path, const std::string& name) const;

    std::set<std::string> get_attribute_names(const h5pp::fs::path& path) const;

    h5pp::AttrInfo get_attribute_info(const h5pp::fs::path& path, const std::string& name) const;

    template <typename Target>
    std::optional<Target>
    get_optional_attribute(const h5pp::fs::path& path, const std::string& name) const;

    template <typename Target>
    std::optional<std::vector<Target>>
    get_optional_attribute_vect(const h5pp::fs::path& path, const std::string& name) const;

    template <typename AttrType>
    AttrType get_attribute(const h5pp::fs::path& path, const std::string& name) const;

    template <typename AttrType>
    std::vector<AttrType> get_attribute_vect(const h5pp::fs::path& path, const std::string& name) const;

    template <typename AttrType>
    std::optional<AttrType>
    get_optional_attribute_recursive(
        const h5pp::fs::path& parent_path,
        const std::string& group,const std::string& attribute_name) const;

    template <typename AttrType>
    std::optional<std::vector<AttrType>>
    get_optional_vect_attribute_recursive(
        const h5pp::fs::path& parent_path,
        const std::string& group,
        const std::string& attribute_name) const;

    template <typename AttrType>
    AttrType get_attribute_recursive(
        const h5pp::fs::path& parent_path,
        const std::string& group,
        const std::string& name) const;

    template <typename AttrType>
    std::vector<AttrType> get_attribute_vect_recursive(
        const h5pp::fs::path& parent_path,
        const std::string& group,
        const std::string& name) const;

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
    read_dataset(const std::string& path) const;

    std::string get_attribute_string(const std::string& path, const std::string& name) const;

    std::set<std::string> get_groups() const { return groups; }
    std::set<std::string> get_datasets() const { return datasets; }
    std::set<std::string> get_top_names() const;
};


namespace detail
{

    H5T_order_t get_native_order();

    template <typename Target, typename... SupportedAttributeType>
    class AttributeTypeMap
    {
        typedef Target(*converter_t)(const h5pp::File&, const std::string&, const std::string&);
        typedef std::vector<Target>(*converter_vect_t)(const h5pp::File&, const std::string&, const std::string&);
        typedef std::map<std::type_index, converter_t> converter_map_t;
        typedef std::map<std::type_index, converter_vect_t> converter_vect_map_t;

        converter_map_t converters;
        converter_vect_map_t converters_vect;
   public:
        AttributeTypeMap();
        Target get(const h5pp::File& file, const h5pp::fs::path& path, const std::string& name) const;
        std::vector<Target> get_vect(const h5pp::File& file, const h5pp::fs::path& path, const std::string& name) const;
    };

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

    template<typename Target, typename... SupportedAttributeType>
    AttributeTypeMap<Target, SupportedAttributeType...>::AttributeTypeMap()
    {
        ((converters[std::type_index(typeid(SupportedAttributeType))] = &get_attribute_impl<Target, SupportedAttributeType>), ...);
        ((converters_vect[std::type_index(typeid(std::vector<SupportedAttributeType>))] = &get_attribute_vect_impl<Target, SupportedAttributeType>), ...);
    }


    template<typename Target, typename... SupportedAttributeType>
    Target AttributeTypeMap<Target, SupportedAttributeType...>::get(
        const h5pp::File& file,
        const h5pp::fs::path& path,
        const std::string& name) const
    {
        const h5pp::AttrInfo info = file.getAttributeInfo(path.string(), name);
        const std::type_index& ti = detect_type_index(info);
        if (not info.attrSize or *info.attrSize != 1)
            throw Fmi::Exception(BCP, "Attribute " + name + " size mismatch (or size not available)");

        if (std::type_index(typeid(Target)) == ti)
            return maybe_swap_bytes(file.readAttribute<Target>(path.string(), name), info);

        const auto it = converters.find(ti);
        if (it == converters.end())
        {
            Fmi::Exception error(BCP, "No conversion defined from attribute of type " +
                Fmi::demangle_cpp_type_name(ti.name()) + " to " + Fmi::demangle_cpp_type_name(typeid(Target).name()));
            error.addParameter("path", path);
            error.addParameter("name", name);
            throw error;
        }

        return it->second(file, path, name);
    }


    template<typename Target, typename... SupportedAttributeType>
    std::vector<Target>
    detail::AttributeTypeMap<Target, SupportedAttributeType...>::get_vect(
        const h5pp::File& file,
        const h5pp::fs::path& path,
        const std::string& name) const
    {
        const h5pp::AttrInfo info = file.getAttributeInfo(path.string(), name);
        const std::type_index& ti = detect_type_index(info);
        if (not info.cppTypeSize or *info.cppTypeSize == 0)
            throw Fmi::Exception(BCP, "Attribute " + name + " size mismatch (or size not available)");

        if (std::type_index(typeid(Target)) == ti)
            return maybe_swap_bytes(file.readAttribute<std::vector<Target>>(path.string(), name), info);

        const auto it = converters_vect.find(ti);
        if (it == converters_vect.end())
        {
            Fmi::Exception error(BCP, "No conversion defined from attribute of type " +
                Fmi::demangle_cpp_type_name(ti.name()) + " to " + Fmi::demangle_cpp_type_name(typeid(Target).name()));
            error.addParameter("path", path);
            error.addParameter("name", name);
            throw error;
        }

        return it->second(file, path, name);
    }


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

    template <typename Target, typename... SupportedDataTypes>
    std::map<std::type_index, std::vector<Target>(*)(const h5pp::File&, const std::string&)>
    get_dataset_converters()
    {
        std::map<std::type_index, std::vector<Target>(*)(const h5pp::File&, const std::string&)> converters;
        static_assert((... && Fmi::is_numeric<SupportedDataTypes>::value), "Only numeric types from dataset are supported");
        ((converters[std::type_index(typeid(SupportedDataTypes))] = read_dataset_impl<Target, SupportedDataTypes>), ...);
        return converters;
    }

}  // namespace detail

template <typename AttrType>
bool Hdf5File::is_attribute(const h5pp::fs::path& path, const std::string& name) const
{
    if (not is_group(path))
        return false;

    if (not file.attributeExists(std::string_view(path.string()), name))
        return false;

    const h5pp::AttrInfo info = file.getAttributeInfo(std::string_view(path.string()), name);
    if (not info.cppTypeIndex)
        throw Fmi::Exception(BCP, "Attribute " + name + " size is not provided");

    return *info.cppTypeIndex == std::type_index(typeid(AttrType));
}

template <typename Target>
std::optional<Target>
Hdf5File::get_optional_attribute(const h5pp::fs::path& path, const std::string& name) const
try
{
    if (not is_group(path))
        return std::nullopt;

    if  (not file.attributeExists(std::string_view(path.string()), name))
        return std::nullopt;

    const h5pp::AttrInfo info = file.getAttributeInfo(std::string_view(path.string()), name);
    const std::type_index& ti = detail::detect_type_index(info);

    if constexpr (std::is_same<Target, std::string>::value)
    {
        if (ti == std::type_index(typeid(std::string)))
        {
            return file.readAttribute<std::string>(std::string_view(path.string()), name);
        }
        else
        {
            throw Fmi::Exception(BCP, "Attempt to read non-string attribute "
                + path.string() + "/" + name + " as std::string. Use read_string_attribute instead");
        }
    }
    else
    {
        static const detail::AttributeTypeMap<Target,
            int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> converters;
        return converters.get(file, path, name);
    }
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("path", path);
    error.addParameter("name", name);
    throw error;
}


template <typename Target>
std::optional<std::vector<Target>>
Hdf5File::get_optional_attribute_vect(const h5pp::fs::path& path, const std::string& name) const
try
{
    if (not is_group(path))
        return std::nullopt;

    if  (not file.attributeExists(std::string_view(path.string()), name))
        return std::nullopt;

    const h5pp::AttrInfo info = file.getAttributeInfo(std::string_view(path.string()), name);
    const std::type_index& ti = detail::detect_type_index(info);

    if constexpr (std::is_same<Target, std::string>::value)
    {
        if (ti == std::type_index(typeid(std::string)))
        {
            return file.readAttribute<std::vector<std::string>>(path.string(), name);
        }
        else
        {
            throw Fmi::Exception(BCP, "Attempt to read non-string attribute "
                + path.string() + "/" + name + " as std::string. Use read_string_attribute instead");
        }
    }
    else
    {
        static detail::AttributeTypeMap<Target,
            int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double> converters;
        return converters.get_vect(file, path, name);
    }
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("path", path);
    error.addParameter("name", name);
    throw error;
}


template <typename AttrType>
AttrType Hdf5File::get_attribute(const h5pp::fs::path& path, const std::string& name) const
try
{
    const std::optional<AttrType> opt_value = get_optional_attribute<AttrType>(path, name);
    if (opt_value)
        return *opt_value;
    else
        throw std::runtime_error("Attribute " + name + " not found in group " + path.string());
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    throw error;
}

template <typename AttrType>
std::vector<AttrType> Hdf5File::get_attribute_vect(const h5pp::fs::path& path, const std::string& name) const
try
{
    const std::optional<std::vector<AttrType>> opt_value = get_optional_attribute_vect<AttrType>(path, name);
    if (opt_value)
        return *opt_value;
    else
        throw std::runtime_error("Attribute " + name + " not found in group " + path.string());
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    throw error;
}


template <typename AttrType>
std::optional<AttrType>
Hdf5File::get_optional_attribute_recursive(
    const h5pp::fs::path& parent_path,
    const std::string& group,
    const std::string& attribute_name) const
try
{
    int cnt = 100;
    h5pp::fs::path path = parent_path;
    const h5pp::fs::path root = parent_path.root_directory();
    do
    {
        const h5pp::fs::path group_path = path / group;
        if (is_group(group_path))
        {
            if (file.attributeExists(std::string_view(group_path.string()), attribute_name))
                return get_optional_attribute<AttrType>(group_path, attribute_name);
        }
        // Group does not exist or attribute not found - try parent group next
        path = path.parent_path();
    } while (parent_path != root and cnt-- >= 0);

    return {};
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("parent_path", parent_path);
    error.addParameter("group", group);
    error.addParameter("attribute_name", attribute_name);
    throw error;
}


template <typename AttrType>
std::optional<std::vector<AttrType>>
Hdf5File::get_optional_vect_attribute_recursive(
    const h5pp::fs::path& parent_path,
    const std::string& group,
    const std::string& attribute_name) const
try
{
    int cnt = 100;
    h5pp::fs::path path = parent_path;
    const h5pp::fs::path root = parent_path.root_directory();
    do
    {
        const h5pp::fs::path group_path = path / group;
        if (is_group(group_path))
        {
            if (file.attributeExists(std::string_view(group_path.string()), attribute_name))
                return get_optional_attribute_vect<AttrType>(std::string_view(group_path.string()), attribute_name);
        }
        path = path.parent_path();
    } while (parent_path != root and cnt-- >= 0);

    return {};
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("parent_path", parent_path);
    error.addParameter("group", group);
    error.addParameter("attribute_name", attribute_name);
    throw error;
}


template <typename AttrType>
AttrType Hdf5File::get_attribute_recursive(
    const h5pp::fs::path& parent_path,
    const std::string& group,
    const std::string& name) const
try
{
    const std::optional<AttrType> opt_value = get_optional_attribute_recursive<AttrType>(parent_path, group, name);
    if (opt_value)
        return *opt_value;
    else
        throw std::runtime_error("Attribute " + name + " not found in group " + (parent_path / group).string()
            + " or parent groups");
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    throw error;
}


template <typename AttrType>
std::vector<AttrType>
Hdf5File::get_attribute_vect_recursive(
    const h5pp::fs::path& parent_path,
    const std::string& group,
    const std::string& name) const
try
{
    const std::optional<std::vector<AttrType>> opt_value =
        get_optional_vect_attribute_recursive<AttrType>(parent_path, group, name);
    if (opt_value)
        return *opt_value;
    else
        throw std::runtime_error("Attribute " + name + " not found in group " + (parent_path / group).string()
            + " or parent groups");
}
catch (...)
{
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    throw error;
}


template <typename Target, typename... SupportedDataTypes>
typename std::enable_if_t<
     Fmi::is_numeric<Target>::value,
     std::vector<Target>>
Hdf5File::read_dataset(const std::string& path) const
{
     using namespace std::literals;

     static auto converters =
        detail::get_dataset_converters<Target,
            uint8_t, uint16_t, uint32_t, uint64_t,
            int8_t, int16_t, int32_t, int64_t,
            float, double>();

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


}  // namespace HDF5
}  // namespace Fmi
