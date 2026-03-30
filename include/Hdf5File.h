#pragma once

#include <gdal.h>
#include <macgyver/Exception.h>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <cstring>

// Forward declaration - full definition in gdal_priv.h (included in Hdf5File.cpp)
class GDALDataset;

namespace Fmi
{
namespace HDF5
{

namespace detail
{

template <typename T>
GDALDataType gdal_type()
{
    if constexpr (std::is_same_v<T, int8_t>)
        return GDT_Int8;
    else if constexpr (std::is_same_v<T, uint8_t>)
        return GDT_Byte;
    else if constexpr (std::is_same_v<T, int16_t>)
        return GDT_Int16;
    else if constexpr (std::is_same_v<T, uint16_t>)
        return GDT_UInt16;
    else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, int>)
        return GDT_Int32;
    else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, unsigned int>)
        return GDT_UInt32;
    else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, long>)
        return GDT_Int64;
    else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, unsigned long>)
        return GDT_UInt64;
    else if constexpr (std::is_same_v<T, float>)
        return GDT_Float32;
    else if constexpr (std::is_same_v<T, double>)
        return GDT_Float64;
    else
    {
        static_assert(sizeof(T) == 0, "Unsupported type for GDAL data type mapping");
        return GDT_Unknown;
    }
}

}  // namespace detail


class Hdf5File
{
    GDALDataset* gdal_ds{nullptr};

    // All GDAL metadata flattened: "group1_group2_attr" → "value"
    // Derived from HDF5 path by replacing '/' with '_' and stripping leading '/'
    // e.g. /what/date → what_date, /dataset1/what/gain → dataset1_what_gain
    std::map<std::string, std::string> metadata;

    // HDF5 internal path → full GDAL subdataset open string
    // e.g. "/dataset1/data1/data" → "HDF5:\"file.h5\"://dataset1/data1/data"
    std::map<std::string, std::string> subdatasets;

    // Convert HDF5 group path + attribute name to GDAL metadata key
    // E.g. ("/dataset1/what", "gain") → "dataset1_what_gain"
    static std::string to_key(const std::string& path, const std::string& attr);

    // Convert HDF5 group path to the metadata key prefix (without trailing '_')
    // E.g. "/dataset1/what" → "dataset1_what"
    static std::string to_key_prefix(const std::string& path);

    // Try to parse a string as a double; returns false on failure
    static bool try_parse_double(const std::string& s, double& out) noexcept;

    // Core non-template dataset read: returns raw bytes in the requested GDAL type
    std::vector<char> read_dataset_raw(
        const std::string& path, GDALDataType dtype, size_t elem_size) const;

public:
    explicit Hdf5File(const std::string& path);
    ~Hdf5File();
    Hdf5File(const Hdf5File&) = delete;
    Hdf5File& operator=(const Hdf5File&) = delete;
    Hdf5File(Hdf5File&&) = default;
    Hdf5File& operator=(Hdf5File&&) = default;

    // Check whether path refers to an existing HDF5 group (as seen by GDAL)
    bool is_group(const std::string& path) const;

    // Return direct attribute names for the given group path
    std::set<std::string> get_attribute_names(const std::string& path) const;

    // Return string representation of attribute (for verbose output)
    std::string get_attribute_string(const std::string& path, const std::string& name) const;

    // Return all known group paths (approximation from metadata keys)
    std::set<std::string> get_groups() const;

    // Return all known dataset (raster) paths from subdataset list
    std::set<std::string> get_datasets() const;

    // Return top-level group names (immediate children of root)
    std::set<std::string> get_top_names() const;

    // True if attribute exists and its value is compatible with type T
    template <typename T>
    bool is_attribute(const std::string& path, const std::string& name) const
    {
        if (!is_group(path)) return false;
        auto it = metadata.find(to_key(path, name));
        if (it == metadata.end()) return false;
        if constexpr (std::is_same_v<T, std::string>)
            return true;
        else
        {
            double dummy;
            return try_parse_double(it->second, dummy);
        }
    }

    // Read attribute as a vector; returns nullopt if group or attribute not present
    template <typename T>
    std::optional<std::vector<T>> get_optional_attribute_vect(
        const std::string& path, const std::string& name) const
    {
        if (!is_group(path)) return std::nullopt;
        auto it = metadata.find(to_key(path, name));
        if (it == metadata.end()) return std::nullopt;

        if constexpr (std::is_same_v<T, std::string>)
        {
            return std::vector<std::string>{it->second};
        }
        else
        {
            // GDAL exposes numeric arrays as space- or comma-separated strings
            std::vector<T> result;
            std::istringstream iss(it->second);
            std::string token;
            while (iss >> token)
            {
                if (!token.empty() && token.back() == ',')
                    token.pop_back();
                if (token.empty()) continue;
                try
                {
                    if constexpr (std::is_floating_point_v<T>)
                        result.push_back(static_cast<T>(std::stod(token)));
                    else
                        result.push_back(static_cast<T>(std::stoll(token)));
                }
                catch (...)
                {
                    throw Fmi::Exception(BCP,
                        "Cannot parse attribute " + name + " at " + path +
                        " value '" + it->second + "' as numeric");
                }
            }
            if (result.empty()) return std::nullopt;
            return result;
        }
    }

    // Read scalar attribute; returns nullopt if not present; throws if >1 value
    template <typename T>
    std::optional<T> get_optional_attribute(
        const std::string& path, const std::string& name) const
    {
        auto vect = get_optional_attribute_vect<T>(path, name);
        if (!vect) return std::nullopt;
        if (vect->empty())
            throw Fmi::Exception(BCP, "Attribute " + name + " at " + path + " is empty");
        if (vect->size() > 1)
            throw Fmi::Exception(BCP,
                "Attribute " + name + " at " + path + " has " +
                std::to_string(vect->size()) + " values, but exactly one expected");
        return (*vect)[0];
    }

    // Read required scalar attribute; throws if not present
    template <typename T>
    T get_attribute(const std::string& path, const std::string& name) const
    {
        auto opt = get_optional_attribute<T>(path, name);
        if (!opt)
            throw std::runtime_error("Attribute " + name + " not found at " + path);
        return *opt;
    }

    // Read required attribute vector; throws if not present
    template <typename T>
    std::vector<T> get_attribute_vect(const std::string& path, const std::string& name) const
    {
        auto opt = get_optional_attribute_vect<T>(path, name);
        if (!opt)
            throw std::runtime_error("Attribute " + name + " not found at " + path);
        return *opt;
    }

    // Walk up from parent_path checking parent_path/group, parent_path/../group, etc.
    template <typename T>
    std::optional<T> get_optional_attribute_recursive(
        const std::string& parent_path,
        const std::string& group,
        const std::string& attr) const
    {
        namespace fs = std::filesystem;
        fs::path path = parent_path;
        while (true)
        {
            fs::path group_path = path / group;
            if (is_group(group_path.string()))
            {
                auto result = get_optional_attribute<T>(group_path.string(), attr);
                if (result) return result;
            }
            fs::path parent = path.parent_path();
            if (parent == path) break;
            path = parent;
        }
        return std::nullopt;
    }

    template <typename T>
    T get_attribute_recursive(
        const std::string& parent_path,
        const std::string& group,
        const std::string& attr) const
    {
        auto opt = get_optional_attribute_recursive<T>(parent_path, group, attr);
        if (!opt)
            throw std::runtime_error(
                "Attribute " + attr + " not found in " + parent_path + "/" + group +
                " or parent groups");
        return *opt;
    }

    template <typename T>
    std::optional<std::vector<T>> get_optional_vect_attribute_recursive(
        const std::string& parent_path,
        const std::string& group,
        const std::string& attr) const
    {
        namespace fs = std::filesystem;
        fs::path path = parent_path;
        while (true)
        {
            fs::path group_path = path / group;
            if (is_group(group_path.string()))
            {
                auto result = get_optional_attribute_vect<T>(group_path.string(), attr);
                if (result) return result;
            }
            fs::path parent = path.parent_path();
            if (parent == path) break;
            path = parent;
        }
        return std::nullopt;
    }

    template <typename T>
    std::vector<T> get_attribute_vect_recursive(
        const std::string& parent_path,
        const std::string& group,
        const std::string& attr) const
    {
        auto opt = get_optional_vect_attribute_recursive<T>(parent_path, group, attr);
        if (!opt)
            throw std::runtime_error(
                "Attribute " + attr + " not found in " + parent_path + "/" + group +
                " or parent groups");
        return *opt;
    }

    // Read HDF5 dataset as a flat vector of T; path is the containing group
    // (e.g., "/dataset1/data1" finds the raster dataset within that group)
    template <typename T>
    std::vector<T> read_dataset(const std::string& path) const
    {
        auto raw = read_dataset_raw(path, detail::gdal_type<T>(), sizeof(T));
        std::vector<T> result(raw.size() / sizeof(T));
        std::memcpy(result.data(), raw.data(), raw.size());
        return result;
    }
};

}  // namespace HDF5
}  // namespace Fmi
