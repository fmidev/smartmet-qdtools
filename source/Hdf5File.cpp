#include "Hdf5File.h"

#include <gdal_priv.h>
#include <algorithm>
#include <stdexcept>

namespace Fmi
{
namespace HDF5
{

// Convert HDF5 path to GDAL metadata key prefix.
// Strips leading '/' and replaces remaining '/' with '_'.
// E.g.  "/dataset1/what"  →  "dataset1_what"
//        "/what"           →  "what"
//        "/"  or  ""       →  ""
std::string Hdf5File::to_key_prefix(const std::string& path)
{
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    while (!p.empty() && p.back() == '/') p.pop_back();
    std::replace(p.begin(), p.end(), '/', '_');
    return p;
}

std::string Hdf5File::to_key(const std::string& path, const std::string& attr)
{
    std::string prefix = to_key_prefix(path);
    if (prefix.empty()) return attr;
    return prefix + "_" + attr;
}

bool Hdf5File::try_parse_double(const std::string& s, double& out) noexcept
{
    try
    {
        std::size_t idx = 0;
        out = std::stod(s, &idx);
        while (idx < s.size() && std::isspace(static_cast<unsigned char>(s[idx]))) ++idx;
        return idx == s.size();
    }
    catch (...)
    {
        return false;
    }
}

Hdf5File::Hdf5File(const std::string& path)
try
{
    GDALAllRegister();

    gdal_ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!gdal_ds)
        throw std::runtime_error("GDAL failed to open HDF5 file: " + path);

    // Load all attribute metadata (GDAL flattens HDF5 group attributes)
    char** papszMD = gdal_ds->GetMetadata("");
    if (papszMD != nullptr)
    {
        for (int i = 0; papszMD[i] != nullptr; ++i)
        {
            std::string entry(papszMD[i]);
            auto pos = entry.find('=');
            if (pos != std::string::npos)
                metadata[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
    }

    // Load subdatasets (multi-dataset HDF5 files like PVOL)
    char** papszSub = gdal_ds->GetMetadata("SUBDATASETS");
    if (papszSub != nullptr)
    {
        for (int i = 0; papszSub[i] != nullptr; ++i)
        {
            std::string entry(papszSub[i]);
            // Entries look like:
            //   SUBDATASET_N_NAME=HDF5:"path/to/file.h5"://dataset1/data1/data
            auto eq = entry.find("_NAME=");
            if (eq == std::string::npos) continue;
            std::string gdal_name = entry.substr(eq + 6);

            // Extract HDF5 internal path: everything after the last "://"
            // e.g.  HDF5:"file.h5"://dataset1/data1/data  →  /dataset1/data1/data
            auto sep = gdal_name.rfind("://");
            if (sep == std::string::npos) continue;
            // After "://" the next char is '/' (the HDF5 path starts with '/')
            std::string hdf5_path = gdal_name.substr(sep + 2);  // keep the leading '/'
            if (hdf5_path.empty() || hdf5_path.front() != '/')
                hdf5_path = "/" + hdf5_path;
            subdatasets[hdf5_path] = gdal_name;
        }
    }
}
catch (...)
{
    if (gdal_ds) { GDALClose(gdal_ds); gdal_ds = nullptr; }
    throw;
}

Hdf5File::~Hdf5File()
{
    if (gdal_ds)
        GDALClose(gdal_ds);
}

bool Hdf5File::is_group(const std::string& path) const
{
    // Normalise
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    if (p.empty() || p == "/") return true;  // root is always a group

    std::string prefix = to_key_prefix(p) + "_";
    // A group exists if at least one metadata key starts with its prefix
    auto it = metadata.lower_bound(prefix);
    return it != metadata.end() && it->first.substr(0, prefix.size()) == prefix;
}

std::set<std::string> Hdf5File::get_top_names() const
{
    // Top-level group names are the first underscore-separated segment of metadata keys
    std::set<std::string> result;
    for (const auto& [k, v] : metadata)
    {
        auto pos = k.find('_');
        if (pos != std::string::npos)
            result.insert(k.substr(0, pos));
    }
    return result;
}

std::set<std::string> Hdf5File::get_attribute_names(const std::string& path) const
{
    if (!is_group(path)) return {};
    std::string prefix = to_key_prefix(path) + "_";
    std::set<std::string> result;
    for (const auto& [k, v] : metadata)
    {
        if (k.size() > prefix.size() && k.substr(0, prefix.size()) == prefix)
            result.insert(k.substr(prefix.size()));
    }
    return result;
}

std::string Hdf5File::get_attribute_string(const std::string& path, const std::string& name) const
{
    auto it = metadata.find(to_key(path, name));
    if (it == metadata.end())
        throw std::runtime_error("Attribute " + name + " not found at " + path);
    return it->second;
}

std::set<std::string> Hdf5File::get_groups() const
{
    // Reconstruct an approximation of group paths from metadata keys.
    // For ODIM HDF5, group names don't contain underscores, so this works well.
    std::set<std::string> result;
    result.insert("/");
    for (const auto& [k, v] : metadata)
    {
        std::string prefix;
        std::size_t pos = 0;
        while (pos < k.size())
        {
            auto next = k.find('_', pos);
            if (next == std::string::npos) break;
            if (!prefix.empty()) prefix += '/';
            prefix += k.substr(pos, next - pos);
            result.insert('/' + prefix);
            pos = next + 1;
        }
    }
    return result;
}

std::set<std::string> Hdf5File::get_datasets() const
{
    std::set<std::string> result;
    for (const auto& [p, n] : subdatasets)
        result.insert(p);
    return result;
}

std::vector<char> Hdf5File::read_dataset_raw(
    const std::string& path, GDALDataType dtype, size_t elem_size) const
{
    // Normalize: ensure leading '/'
    std::string normalized = path;
    if (normalized.empty() || normalized.front() != '/')
        normalized = "/" + normalized;

    GDALDataset* src_ds = nullptr;
    bool opened_sub = false;

    if (!subdatasets.empty())
    {
        // Multi-dataset file (e.g. PVOL): find the unique raster under path
        // (may be a direct child or a deeper descendant, e.g. /dataset1/data1/data)
        std::string prefix = normalized + "/";
        std::string match_name;

        for (const auto& [hdf5_path, gdal_name] : subdatasets)
        {
            if (hdf5_path.size() > prefix.size() &&
                hdf5_path.substr(0, prefix.size()) == prefix)
            {
                if (!match_name.empty())
                    throw Fmi::Exception(BCP, "Multiple datasets found under " + path);
                match_name = gdal_name;
            }
        }

        if (match_name.empty())
            throw Fmi::Exception(BCP, "No dataset found under " + path);

        src_ds = static_cast<GDALDataset*>(GDALOpen(match_name.c_str(), GA_ReadOnly));
        if (!src_ds)
            throw Fmi::Exception(BCP, "Failed to open HDF5 subdataset: " + match_name);
        opened_sub = true;
    }
    else
    {
        // Single-image file (HDF5Image driver): the main dataset IS the raster
        src_ds = gdal_ds;
    }

    GDALRasterBand* band = src_ds->GetRasterBand(1);
    if (!band)
    {
        if (opened_sub) GDALClose(src_ds);
        throw Fmi::Exception(BCP, "No raster band in dataset at " + path);
    }

    int width  = src_ds->GetRasterXSize();
    int height = src_ds->GetRasterYSize();
    std::size_t total = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    std::vector<char> raw(total * elem_size);
    CPLErr err = band->RasterIO(
        GF_Read, 0, 0, width, height,
        raw.data(), width, height,
        dtype, 0, 0);

    if (opened_sub)
        GDALClose(src_ds);

    if (err != CE_None)
        throw Fmi::Exception(BCP, "RasterIO failed reading dataset at " + path);

    return raw;
}

}  // namespace HDF5
}  // namespace Fmi
