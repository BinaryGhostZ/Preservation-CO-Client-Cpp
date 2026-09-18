#include "assets.hpp"
#include "tq_archive_hash.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

namespace sandbox {

static bool decode_dds(const std::vector<uint8_t> &bytes, Image &img) {
  if (bytes.size() < 128)
    return false;
  uint32_t magic;
  std::memcpy(&magic, bytes.data(), 4);
  if (magic != 0x20534444)
    return false; // "DDS "

  uint32_t height, width, linear_size, mipmap_count;
  std::memcpy(&height, bytes.data() + 12, 4);
  std::memcpy(&width, bytes.data() + 16, 4);
  std::memcpy(&linear_size, bytes.data() + 20, 4);
  std::memcpy(&mipmap_count, bytes.data() + 28, 4);

  uint32_t flags, fourcc, rgb_bitcount, r_bitmask, g_bitmask, b_bitmask,
      a_bitmask;
  std::memcpy(&flags, bytes.data() + 76 + 4, 4);
  std::memcpy(&fourcc, bytes.data() + 76 + 8, 4);
  std::memcpy(&rgb_bitcount, bytes.data() + 76 + 12, 4);
  std::memcpy(&r_bitmask, bytes.data() + 76 + 16, 4);
  std::memcpy(&g_bitmask, bytes.data() + 76 + 20, 4);
  std::memcpy(&b_bitmask, bytes.data() + 76 + 24, 4);
  std::memcpy(&a_bitmask, bytes.data() + 76 + 28, 4);

  img.width = width;
  img.height = height;
  img.channels = 4;
  img.data.resize(width * height * 4, 255);

  const uint8_t *compressed = bytes.data() + 128;
  int blocks_x = (width + 3) / 4;
  int blocks_y = (height + 3) / 4;

  auto decode_blocks = [&](auto bcdec_func, size_t block_size) {
    for (int y = 0; y < blocks_y; ++y) {
      for (int x = 0; x < blocks_x; ++x) {
        uint8_t decompressed_block[4 * 4 * 4];
        bcdec_func(compressed, decompressed_block, 4 * 4);
        compressed += block_size;
        for (int by = 0; by < 4; ++by) {
          for (int bx = 0; bx < 4; ++bx) {
            if (x * 4 + bx < width && y * 4 + by < height) {
              int px = x * 4 + bx;
              int py = y * 4 + by;
              std::memcpy(&img.data[(py * width + px) * 4],
                          &decompressed_block[(by * 4 + bx) * 4], 4);
            }
          }
        }
      }
    }
  };

  if (fourcc == 0x31545844) { // DXT1
    decode_blocks(bcdec_bc1, BCDEC_BC1_BLOCK_SIZE);
    return true;
  } else if (fourcc == 0x33545844) { // DXT3
    decode_blocks(bcdec_bc2, BCDEC_BC2_BLOCK_SIZE);
    return true;
  } else if (fourcc == 0x35545844) { // DXT5
    decode_blocks(bcdec_bc3, BCDEC_BC3_BLOCK_SIZE);
    return true;
  } else if (fourcc == 0 && rgb_bitcount > 0) {
    if (rgb_bitcount == 32) {
      for (uint32_t py = 0; py < height; ++py) {
        for (uint32_t px = 0; px < width; ++px) {
          if (compressed - bytes.data() + 4 <= bytes.size()) {
            uint8_t b = compressed[0];
            uint8_t g = compressed[1];
            uint8_t r = compressed[2];
            uint8_t a = compressed[3];
            img.data[(py * width + px) * 4 + 0] = r_bitmask == 0xFF0000 ? r : b;
            img.data[(py * width + px) * 4 + 1] = g;
            img.data[(py * width + px) * 4 + 2] = r_bitmask == 0xFF0000 ? b : r;
            img.data[(py * width + px) * 4 + 3] = a;
            compressed += 4;
          }
        }
      }
      return true;
    } else if (rgb_bitcount == 16) {
      for (uint32_t py = 0; py < height; ++py) {
        for (uint32_t px = 0; px < width; ++px) {
          if (compressed - bytes.data() + 2 <= bytes.size()) {
            uint16_t c;
            std::memcpy(&c, compressed, 2);
            compressed += 2;
            // RGB565 or ARGB1555 based on a_bitmask
            if (a_bitmask) {
              uint8_t r = (c >> 10) & 0x1F;
              uint8_t g = (c >> 5) & 0x1F;
              uint8_t b = c & 0x1F;
              uint8_t a = (c >> 15) ? 255 : 0;
              img.data[(py * width + px) * 4 + 0] = (r << 3) | (r >> 2);
              img.data[(py * width + px) * 4 + 1] = (g << 3) | (g >> 2);
              img.data[(py * width + px) * 4 + 2] = (b << 3) | (b >> 2);
              img.data[(py * width + px) * 4 + 3] = a;
            } else {
              uint8_t r = (c >> 11) & 0x1F;
              uint8_t g = (c >> 5) & 0x3F;
              uint8_t b = c & 0x1F;
              img.data[(py * width + px) * 4 + 0] = (r << 3) | (r >> 2);
              img.data[(py * width + px) * 4 + 1] = (g << 2) | (g >> 4);
              img.data[(py * width + px) * 4 + 2] = (b << 3) | (b >> 2);
              img.data[(py * width + px) * 4 + 3] = 255;
            }
          }
        }
      }
      return true;
    }
  }
  return false;
}

Assets::Assets(const std::filesystem::path &root)
    : root_(std::filesystem::canonical(root)) {
  if (!std::filesystem::is_directory(root_)) {
    throw std::runtime_error("--assets must name an installation directory");
  }

  const char *archive_names[] = {"data.wdf", "c3.wdf"};
  for (const char *name : archive_names) {
    auto path = loose(name);
    if (path) {
      std::ifstream file(*path, std::ios::binary);
      if (!file)
        continue;

      file.seekg(0, std::ios::end);
      uint64_t length = file.tellg();
      file.seekg(0, std::ios::beg);

      uint8_t header_bytes[12];
      file.read(reinterpret_cast<char *>(header_bytes), 12);
      if (file.gcount() != 12)
        continue;

      auto header = sandbox_data::WdfHeader::parse(header_bytes);
      size_t size = header.directory_length();
      if (size > 64 * 1024 * 1024)
        throw std::runtime_error("WDF directory exceeds safety limit");

      file.seekg(header.directory_offset, std::ios::beg);
      std::vector<uint8_t> bytes(size);
      file.read(reinterpret_cast<char *>(bytes.data()), size);

      archives_.emplace_back(std::move(file), sandbox_data::WdfDirectory::parse(
                                                  header, bytes, length));
    }
  }
}

std::optional<std::filesystem::path> Assets::loose(const std::string &name) {
  if (resolved_.find(name) != resolved_.end()) {
    return resolved_[name];
  }

  std::filesystem::path current = root_;

  // Split name by '/' or '\\'
  size_t start = 0;
  while (start < name.length()) {
    size_t end = name.find_first_of("/\\", start);
    if (end == std::string::npos)
      end = name.length();

    std::string component = name.substr(start, end - start);
    if (!component.empty()) {
      std::filesystem::path exact = current / component;
      if (std::filesystem::exists(exact)) {
        current = exact;
      } else {
        bool found = false;
        std::string comp_lower = component;
        std::transform(comp_lower.begin(), comp_lower.end(), comp_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (std::filesystem::exists(current) &&
            std::filesystem::is_directory(current)) {
          for (const auto &entry :
               std::filesystem::directory_iterator(current)) {
            std::string entry_name = entry.path().filename().string();
            std::string entry_lower = entry_name;
            std::transform(entry_lower.begin(), entry_lower.end(),
                           entry_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (comp_lower == entry_lower) {
              current = entry.path();
              found = true;
              break;
            }
          }
        }
        if (!found)
          return std::nullopt;
      }
    }
    start = end + 1;
  }

  current = std::filesystem::canonical(current);
  std::string root_str = root_.string();
  std::string curr_str = current.string();
  if (curr_str.find(root_str) != 0) {
    throw std::runtime_error("asset symlink leaves installation");
  }

  if (!std::filesystem::is_regular_file(current)) {
    return std::nullopt;
  }

  resolved_[name] = current;
  return current;
}

std::vector<uint8_t> Assets::read(const std::string &name) {
  auto path = loose(name);
  if (path) {
    std::ifstream file(*path, std::ios::binary | std::ios::ate);
    if (!file)
      throw std::runtime_error("Missing loose asset: " + name);

    std::streamsize size = file.tellg();
    if (size > 128 * 1024 * 1024)
      throw std::runtime_error("asset exceeds 128 MiB safety limit");

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char *>(buffer.data()), size)) {
      return buffer;
    }
  }

  uint32_t id = sandbox_data::path_id(name, "");
  for (auto &[file, directory] : archives_) {
    auto entry = directory.get(id);
    if (entry) {
      if (entry->size > 128 * 1024 * 1024)
        throw std::runtime_error("archive entry exceeds safety limit");

      file.seekg(entry->offset, std::ios::beg);
      std::vector<uint8_t> buffer(entry->size);
      if (file.read(reinterpret_cast<char *>(buffer.data()), entry->size)) {
        return buffer;
      }
    }
  }

  throw std::runtime_error("Missing installed asset: " + name);
}

Image Assets::image(const std::string &name) {
  std::vector<uint8_t> bytes = read(name);

  Image img;
  if (decode_dds(bytes, img)) {
    return img;
  }

  int w, h, c;
  stbi_uc *pixels = stbi_load_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), &w, &h, &c, 4);
  if (!pixels) {
    throw std::runtime_error("Failed to decode image: " + name);
  }

  img.width = w;
  img.height = h;
  img.channels = 4;
  size_t size = static_cast<size_t>(w) * h * 4;
  img.data.assign(pixels, pixels + size);

  stbi_image_free(pixels);
  return img;
}

} // namespace sandbox
