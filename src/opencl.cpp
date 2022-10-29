#include "opencl.hpp"

#include <new>
#include <string>
#include <zlib.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <type_traits>
#include <optional>
#include <tuple>

using namespace std::literals;

namespace
{

inline constexpr auto CHUNK = 16384ul;
inline constexpr auto LEVEL = 6;

template<typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>>
write_bin(std::ofstream& os, T const& val)
{
    os.write(reinterpret_cast<char const*>(&val), sizeof(val));
}

void write_bin(std::ofstream& os, std::string const& val)
{
    write_bin(os, val.length());
    os.write(val.data(), val.length());
}

template<typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, std::optional<T>>
read_bin(std::ifstream& is)
{
    T ret;
    is.read(reinterpret_cast<char*>(&ret), sizeof(ret));
    if (is.gcount() != sizeof(ret))
    {
        return std::nullopt;
    }
    return ret;
}

template<typename T>
std::enable_if_t<std::is_same_v<std::string, T>, std::optional<T>>
read_bin(std::ifstream& is)
{
    std::optional<std::string> ret;
    auto const size = read_bin<decltype(ret->size())>(is);
    if (!size || *size >= 1024)
    {
        // Size too large
        return ret;
    }
    ret = std::string(*size, '\0');
    is.read(ret->data(), ret->length());
    if (static_cast<size_t>(is.gcount()) != ret->length())
    {
        ret = std::nullopt;
    }
    return ret;
}

template<typename T>
bool check_header(std::ifstream& is, T const& compare_to)
{
    auto const read = read_bin<T>(is);
    if (!read.has_value())
    {
        return false;
    }
    return *read == compare_to;
}

} // namespace

bool save_voxelized_mesh_to_disk(
	std::string const& path,
	Memory<unsigned char> const& flags,
    Device const& device,
	float3 const box_size,
	float3 const center,
	float3x3 const& rotation,
	float const size)
{
    std::ofstream out_file{path, std::ios::binary | std::ios::out | std::ios::trunc};
    if (!out_file.good())
    {
        print_info(path + ": could not open file to write");
        return false;
    }

    // Write header
    write_bin(out_file, device.info.name);
    write_bin(out_file, box_size);
    write_bin(out_file, center);
    write_bin(out_file, rotation);
    write_bin(out_file, size);

    struct raii : public z_stream {
        raii() : z_stream{}
        {
            if (deflateInit(this, LEVEL) != Z_OK)
            {
                throw std::bad_alloc{};
            }
        }

        ~raii() noexcept
        {
            deflateEnd(this);
        }
    } strm{};

    std::array<uint8_t, CHUNK> out;

    auto remaining = flags.length();
    int flush;
    while (out_file.good() && remaining > 0) {
        auto const next_chunk_size = std::min(remaining, CHUNK);
        strm.avail_in = next_chunk_size;
        flush = remaining <= next_chunk_size ? Z_FINISH : Z_NO_FLUSH;
        // For some reason, next_in is a pointer to non-const, hence the evil const_cast
        strm.next_in = const_cast<unsigned char*>(&flags.data()[flags.length() - remaining]);

        do {
            strm.avail_out = out.size();
            strm.next_out = out.data();
            auto const result = deflate(&strm, flush);
            if (result != Z_OK && result != Z_STREAM_END)
            {
                print_info("Error compressing memory: "s + std::to_string(result));
                return false;
            }
            auto const have = out.size() - strm.avail_out;
            out_file.write(reinterpret_cast<char const*>(out.data()), have);
        } while (strm.avail_out == 0 && out_file.good());

        remaining -= next_chunk_size;
    }

    return out_file.good();
}

bool load_voxelized_mesh_from_disk(
	std::string const& path,
	Memory<unsigned char>& flags,
    Device const& device,
	float3 const box_size,
	float3 const center,
	float3x3 const& rotation,
	float const size)
{
    std::ifstream in_file{path, std::ios::binary | std::ios::in};
    if (!in_file.good())
    {
        print_info(path + ": could not open file to read");
        return false;
    }

    if (!check_header(in_file, device.info.name) ||
        !check_header(in_file, box_size) ||
        !check_header(in_file, center) ||
        !check_header(in_file, rotation) ||
        !check_header(in_file, size))
    {
        print_info("Header does not match");
        return false;
    }

    struct raii : public z_stream {
        raii() : z_stream{}
        {
            if (inflateInit(this) != Z_OK)
            {
                throw std::bad_alloc{};
            }
        }

        ~raii() noexcept
        {
            inflateEnd(this);
        }
    } strm{};

    std::array<uint8_t, CHUNK> in;

    auto remaining = flags.length();
    while (in_file && remaining > 0) {
        in_file.read(reinterpret_cast<char*>(in.data()), in.size());
        strm.avail_in = in_file.gcount();
        if (strm.avail_in == 0) {
            continue;
        }

        strm.next_in = in.data();
        strm.avail_out = remaining;
        strm.next_out = &flags.data()[flags.length() - remaining];
        auto const result = inflate(&strm, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END)
        {
            print_info("Error decompressing memory: "s + std::to_string(result));
            return false;
        }
        auto const have = remaining - strm.avail_out;
        if (have > remaining)
        {
            print_info("Buffer overflow after decompressing data!");
            std::fill_n(flags.data(), flags.length(), '\0');
            return false;
        }
        remaining -= have;
    }

    if (remaining > 0)
    {
        print_info("Buffer not complete after decompressing data!");
        std::fill_n(flags.data(), flags.length(), '\0');
        return false;
    }

    return true;
}
