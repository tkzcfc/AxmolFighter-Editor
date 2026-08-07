#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace editor::npk
{

inline constexpr char NpkFlag[]   = "NeoplePack_Bill";
inline constexpr char ImgFlag[]   = "Neople Img File";
inline constexpr char ImageFlag[] = "Neople Image File";

enum ColorBits
{
    ARGB_1555 = 0x0e,
    ARGB_4444 = 0x0f,
    ARGB_8888 = 0x10,
    LINK      = 0x11,
    DXT_1     = 0x12,
    DXT_3     = 0x13,
    DXT_5     = 0x14,
    INVALID   = 0x00,
};

enum class CompressMode
{
    ZLIB     = 0x06,
    NONE     = 0x05,
    DDS_ZLIB = 0x07,
    UNKNOWN  = 0x01,
};

struct TextureInfo
{
    int32_t type         = INVALID;
    int32_t compressMode = 0;
    int32_t width        = 0;
    int32_t height       = 0;
    int32_t length       = 0;
    int32_t x            = 0;
    int32_t y            = 0;
    int32_t canvasWidth  = 0;
    int32_t canvasHeight = 0;
    int32_t index        = -1;
    int32_t linkIndex    = -1;
    std::vector<uint8_t> data;
};

struct Album
{
    int32_t version     = -1;
    int64_t indexLength = 0;
    int32_t count       = 0;
    int64_t dataOffset  = 0;
    std::vector<TextureInfo> textures;
    std::vector<std::vector<uint32_t>> colorDataList;
};

struct NpkIndex
{
    int32_t offset = 0;
    int32_t size   = 0;
    std::string name;
    std::unique_ptr<Album> album;

    NpkIndex() = default;
    NpkIndex(const NpkIndex&) = delete;
    NpkIndex& operator=(const NpkIndex&) = delete;
    NpkIndex(NpkIndex&&) noexcept = default;
    NpkIndex& operator=(NpkIndex&&) noexcept = default;
};

// Decodes a DXT/BC payload into tightly packed RGBA8888 pixels.
bool decodeDxtToRgba(int32_t type,
                    int32_t width,
                    int32_t height,
                    const uint8_t* compressedData,
                    std::size_t compressedLength,
                    std::vector<uint8_t>& rgba);

class Npk
{
public:
    Npk();
    ~Npk();

    Npk(const Npk&) = delete;
    Npk& operator=(const Npk&) = delete;

    bool load(const std::filesystem::path& filePath);
    bool loadTexturesFromImgFile(std::string_view filePath);
    bool loadTexturesFromImgFile(NpkIndex& index);

    // Reads the IMG header only. This lets the exporter calculate an exact
    // frame total without decoding all image pixels twice.
    bool readTextureCount(NpkIndex& index, int32_t& count);

    const std::vector<NpkIndex>& getIndexList() const { return m_indexList; }
    std::vector<NpkIndex>& getIndexList() { return m_indexList; }
    const std::filesystem::path& getNpkFilePath() const { return m_npkFilePath; }
    const std::string& lastError() const { return m_lastError; }

private:
    bool loadTexturesFromAlbum(Album& album);
    bool readAlbumVer2(Album& album);
    bool readAlbumVer4(Album& album);
    bool readBitmapTexture(Album& album, TextureInfo& texture);
    bool decompressTextureData(TextureInfo& texture, std::vector<uint8_t>& unpackedData);
    bool readInt32(int32_t& value);
    bool readInt64(int64_t& value);
    bool readCString(std::string& value);
    bool readEncryptedPath(std::string& value);
    bool seek(std::int64_t offset);
    bool fail(std::string message);

    std::unique_ptr<std::ifstream> m_fileStream;
    std::vector<NpkIndex> m_indexList;
    std::filesystem::path m_npkFilePath;
    std::string m_lastError;
};

}  // namespace editor::npk
