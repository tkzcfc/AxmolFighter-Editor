#include "npk/Npk.h"

#include "base/ZipUtils.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace editor::npk
{
namespace
{

std::array<uint8_t, 256> makeNpkKey()
{
    std::array<uint8_t, 256> key{};
    constexpr char prefix[] = "puchikon@neople dungeon and fighter DNF";
    constexpr std::size_t prefixLength = sizeof(prefix) - 1;
    std::memcpy(key.data(), prefix, prefixLength);
    for (std::size_t i = prefixLength; i < key.size(); ++i)
    {
        switch ((i - prefixLength) % 3)
        {
        case 0:
            key[i] = 'D';
            break;
        case 1:
            key[i] = 'N';
            break;
        default:
            key[i] = 'F';
            break;
        }
    }
    return key;
}

const std::array<uint8_t, 256>& npkKey()
{
    static const std::array<uint8_t, 256> key = makeNpkKey();
    return key;
}

uint16_t readU16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t readU32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

void rgb565(uint16_t value, uint8_t& red, uint8_t& green, uint8_t& blue)
{
    red   = static_cast<uint8_t>(((value >> 11) & 0x1f) * 255 / 31);
    green = static_cast<uint8_t>(((value >> 5) & 0x3f) * 255 / 63);
    blue  = static_cast<uint8_t>((value & 0x1f) * 255 / 31);
}

void decodeColorBlock(const uint8_t* block, bool dxt1, uint8_t colors[4][4])
{
    const uint16_t color0 = readU16(block);
    const uint16_t color1 = readU16(block + 2);
    rgb565(color0, colors[0][0], colors[0][1], colors[0][2]);
    rgb565(color1, colors[1][0], colors[1][1], colors[1][2]);
    colors[0][3] = 255;
    colors[1][3] = 255;

    if (!dxt1 || color0 > color1)
    {
        for (int channel = 0; channel < 3; ++channel)
        {
            colors[2][channel] = static_cast<uint8_t>((2 * colors[0][channel] + colors[1][channel]) / 3);
            colors[3][channel] = static_cast<uint8_t>((colors[0][channel] + 2 * colors[1][channel]) / 3);
        }
        colors[2][3] = colors[3][3] = 255;
    }
    else
    {
        for (int channel = 0; channel < 3; ++channel)
            colors[2][channel] = static_cast<uint8_t>((colors[0][channel] + colors[1][channel]) / 2);
        colors[2][3] = 255;
        colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
    }
}

void writePixel(std::vector<uint8_t>& rgba, int width, int height, int x, int y, const uint8_t color[4])
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
    std::memcpy(rgba.data() + offset, color, 4);
}

std::size_t dxtBlockSize(int32_t type)
{
    return type == DXT_1 ? 8u : 16u;
}

std::size_t dxtDataSize(int32_t type, int32_t width, int32_t height)
{
    const std::size_t blocksWide = (static_cast<std::size_t>(width) + 3) / 4;
    const std::size_t blocksHigh = (static_cast<std::size_t>(height) + 3) / 4;
    return blocksWide * blocksHigh * dxtBlockSize(type);
}

bool isDxt(int32_t type)
{
    return type == DXT_1 || type == DXT_3 || type == DXT_5;
}

bool pixelBufferSize(int32_t width, int32_t height, std::size_t& size)
{
    if (width <= 0 || height <= 0)
        return false;
    constexpr std::size_t kBytesPerPixel = 4;
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h)
        return false;
    const std::size_t pixels = w * h;
    if (pixels > std::numeric_limits<std::size_t>::max() / kBytesPerPixel)
        return false;
    size = pixels * kBytesPerPixel;
    return true;
}

}  // namespace

bool decodeDxtToRgba(int32_t type,
                    int32_t width,
                    int32_t height,
                    const uint8_t* compressedData,
                    std::size_t compressedLength,
                    std::vector<uint8_t>& rgba)
{
    if (!isDxt(type) || compressedData == nullptr || width <= 0 || height <= 0)
        return false;

    const std::size_t expectedLength = dxtDataSize(type, width, height);
    if (compressedLength < expectedLength)
        return false;

    std::size_t rgbaSize = 0;
    if (!pixelBufferSize(width, height, rgbaSize))
        return false;
    rgba.assign(rgbaSize, 0);

    const int blocksWide = (width + 3) / 4;
    const int blocksHigh = (height + 3) / 4;
    const std::size_t blockSize = dxtBlockSize(type);

    for (int blockY = 0; blockY < blocksHigh; ++blockY)
    {
        for (int blockX = 0; blockX < blocksWide; ++blockX)
        {
            const uint8_t* block = compressedData +
                                    (static_cast<std::size_t>(blockY) * blocksWide + blockX) * blockSize;
            uint8_t colors[4][4]{};
            decodeColorBlock(block + (type == DXT_1 ? 0 : 8), type == DXT_1, colors);

            uint8_t alpha[16];
            std::fill(std::begin(alpha), std::end(alpha), 255);
            if (type == DXT_3)
            {
                for (int i = 0; i < 16; ++i)
                {
                    const uint8_t nibble = (block[i / 2] >> ((i % 2) * 4)) & 0x0f;
                    alpha[i] = static_cast<uint8_t>(nibble * 17);
                }
            }
            else if (type == DXT_5)
            {
                const uint8_t alpha0 = block[0];
                const uint8_t alpha1 = block[1];
                uint8_t alphaTable[8]{};
                alphaTable[0] = alpha0;
                alphaTable[1] = alpha1;
                if (alpha0 > alpha1)
                {
                    for (int i = 2; i < 8; ++i)
                        alphaTable[i] = static_cast<uint8_t>(((8 - i) * alpha0 + (i - 1) * alpha1) / 7);
                }
                else
                {
                    for (int i = 2; i < 6; ++i)
                        alphaTable[i] = static_cast<uint8_t>(((6 - i) * alpha0 + (i - 1) * alpha1) / 5);
                    alphaTable[6] = 0;
                    alphaTable[7] = 255;
                }

                uint64_t alphaBits = 0;
                for (int i = 0; i < 6; ++i)
                    alphaBits |= static_cast<uint64_t>(block[2 + i]) << (i * 8);
                for (int i = 0; i < 16; ++i)
                    alpha[i] = alphaTable[(alphaBits >> (i * 3)) & 0x07];
            }

            const uint32_t colorBits = readU32(block + (type == DXT_1 ? 4 : 12));
            for (int localY = 0; localY < 4; ++localY)
            {
                for (int localX = 0; localX < 4; ++localX)
                {
                    const int pixelIndex = localY * 4 + localX;
                    const uint8_t colorIndex = (colorBits >> (pixelIndex * 2)) & 0x03;
                    uint8_t color[4] = {colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2],
                                        alpha[pixelIndex]};
                    writePixel(rgba, width, height, blockX * 4 + localX, blockY * 4 + localY, color);
                }
            }
        }
    }
    return true;
}

Npk::Npk() = default;
Npk::~Npk() = default;

bool Npk::fail(std::string message)
{
    m_lastError = std::move(message);
    return false;
}

bool Npk::readInt32(int32_t& value)
{
    std::array<uint8_t, 4> bytes{};
    if (!m_fileStream || !m_fileStream->read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        return fail("unexpected end of NPK while reading int32");
    value = static_cast<int32_t>(readU32(bytes.data()));
    return true;
}

bool Npk::readInt64(int64_t& value)
{
    std::array<uint8_t, 8> bytes{};
    if (!m_fileStream || !m_fileStream->read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        return fail("unexpected end of NPK while reading int64");
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i)
        result |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    value = static_cast<int64_t>(result);
    return true;
}

bool Npk::readCString(std::string& value)
{
    value.clear();
    if (!m_fileStream)
        return fail("NPK stream is not open");

    char character = 0;
    while (m_fileStream->get(character))
    {
        if (character == '\0')
            return true;
        value.push_back(character);
        if (value.size() > 4096)
            return fail("NPK string is too long");
    }
    return fail("unexpected end of NPK while reading string");
}

bool Npk::readEncryptedPath(std::string& value)
{
    std::array<uint8_t, 256> bytes{};
    if (!m_fileStream || !m_fileStream->read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        return fail("unexpected end of NPK while reading IMG path");
    const auto& key = npkKey();
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] ^= key[i];

    const auto end = std::find(bytes.begin(), bytes.end(), 0);
    value.assign(reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(end - bytes.begin()));
    return true;
}

bool Npk::seek(std::int64_t offset)
{
    if (!m_fileStream)
        return fail("NPK stream is not open");
    m_fileStream->clear();
    m_fileStream->seekg(offset, std::ios::beg);
    if (!*m_fileStream)
        return fail("failed to seek in NPK");
    return true;
}

bool Npk::load(const std::filesystem::path& filePath)
{
    m_lastError.clear();
    m_npkFilePath = filePath;
    m_indexList.clear();
    m_fileStream = std::make_unique<std::ifstream>(filePath, std::ios::binary);
    if (!m_fileStream || !*m_fileStream)
        return fail("failed to open NPK: " + filePath.generic_string());

    std::string flag;
    if (!readCString(flag) || flag != NpkFlag)
        return fail("invalid NPK flag: " + filePath.generic_string());

    int32_t indexCount = 0;
    if (!readInt32(indexCount) || indexCount < 0 || indexCount > 1000000)
        return fail("invalid NPK index count");

    m_indexList.reserve(static_cast<std::size_t>(indexCount));
    for (int32_t i = 0; i < indexCount; ++i)
    {
        NpkIndex index;
        if (!readInt32(index.offset) || !readInt32(index.size) || !readEncryptedPath(index.name))
            return false;
        if (index.offset < 0 || index.size < 0)
            return fail("invalid NPK index range for IMG: " + index.name);
        m_indexList.emplace_back(std::move(index));
    }
    return true;
}

bool Npk::readTextureCount(NpkIndex& index, int32_t& count)
{
    count = 0;
    if (!seek(index.offset))
        return false;

    std::string flag;
    if (!readCString(flag))
        return false;
    if (flag == ImgFlag)
    {
        int64_t indexLength = 0;
        int32_t version = 0;
        if (!readInt64(indexLength) || !readInt32(version) || !readInt32(count))
            return false;
        if ((version != 2 && version != 4) || count < 0 || count > 1000000)
            return fail("unsupported IMG header for: " + index.name);
        return true;
    }
    return fail("unsupported IMG flag for: " + index.name);
}

bool Npk::loadTexturesFromImgFile(std::string_view filePath)
{
    for (auto& index : m_indexList)
    {
        if (index.name == filePath)
            return loadTexturesFromImgFile(index);
    }
    return fail("IMG file not found in NPK: " + std::string(filePath));
}

bool Npk::loadTexturesFromImgFile(NpkIndex& index)
{
    if (index.album == nullptr)
    {
        index.album = std::make_unique<Album>();
        if (!seek(index.offset))
            return false;

        std::string flag;
        if (!readCString(flag))
            return false;
        if (flag == ImgFlag)
        {
            if (!readInt64(index.album->indexLength) || !readInt32(index.album->version) ||
                !readInt32(index.album->count))
                return false;
        }
        else if (flag == ImageFlag)
        {
            index.album->version = 1;
        }
        else
        {
            index.album->version = 0;
            if (!seek(index.offset))
                return false;
        }
        index.album->dataOffset = m_fileStream->tellg();
    }
    return loadTexturesFromAlbum(*index.album);
}

bool Npk::loadTexturesFromAlbum(Album& album)
{
    if (album.version < 0)
        return fail("invalid IMG album");
    if (!album.textures.empty())
        return true;
    if (!seek(album.dataOffset))
        return false;

    if (album.version == 2)
        return readAlbumVer2(album);
    if (album.version == 4)
        return readAlbumVer4(album);
    return fail("unsupported IMG version: " + std::to_string(album.version));
}

bool Npk::readAlbumVer2(Album& album)
{
    album.textures.reserve(static_cast<std::size_t>(album.count));
    for (int32_t i = 0; i < album.count; ++i)
    {
        TextureInfo texture;
        texture.index = i;
        if (!readInt32(texture.type))
            return false;
        if (texture.type == LINK)
        {
            if (!readInt32(texture.linkIndex))
                return false;
            album.textures.emplace_back(std::move(texture));
            continue;
        }
        if (!readInt32(texture.compressMode) || !readInt32(texture.width) || !readInt32(texture.height) ||
            !readInt32(texture.length) || !readInt32(texture.x) || !readInt32(texture.y) ||
            !readInt32(texture.canvasWidth) || !readInt32(texture.canvasHeight))
            return false;
        album.textures.emplace_back(std::move(texture));
    }

    for (const auto& texture : album.textures)
    {
        if (texture.type == LINK &&
            (texture.linkIndex < 0 || texture.linkIndex >= static_cast<int32_t>(album.textures.size())))
            return fail("invalid IMG link index");
    }

    for (auto& texture : album.textures)
    {
        if (texture.type == LINK)
            continue;
        if (!readBitmapTexture(album, texture))
            return false;
    }
    return true;
}

bool Npk::readAlbumVer4(Album& album)
{
    int32_t colorSize = 0;
    if (!readInt32(colorSize) || colorSize < 0 || colorSize > 1000000)
        return false;
    std::vector<uint32_t> colors;
    colors.reserve(static_cast<std::size_t>(colorSize));
    for (int32_t i = 0; i < colorSize; ++i)
    {
        int32_t color = 0;
        if (!readInt32(color))
            return false;
        colors.push_back(static_cast<uint32_t>(color));
    }
    album.colorDataList.emplace_back(std::move(colors));
    return readAlbumVer2(album);
}

bool Npk::decompressTextureData(TextureInfo& texture, std::vector<uint8_t>& unpackedData)
{
    if (texture.data.empty())
        return fail("empty compressed texture data");
    unsigned char* output = nullptr;
    const ssize_t unpackedLength = ax::ZipUtils::inflateMemory(
        texture.data.data(), static_cast<ssize_t>(texture.data.size()), &output);
    if (unpackedLength <= 0 || output == nullptr)
    {
        delete[] output;
        return fail("failed to decompress texture data");
    }
    unpackedData.assign(output, output + unpackedLength);
    delete[] output;
    return true;
}

bool Npk::readBitmapTexture(Album&, TextureInfo& texture)
{
    if (texture.width <= 0 || texture.height <= 0)
        return fail("invalid texture dimensions");

    const bool dxt = isDxt(texture.type);
    const std::size_t rawLength = dxt
                                      ? dxtDataSize(texture.type, texture.width, texture.height)
                                      : static_cast<std::size_t>(texture.width) * texture.height *
                                            (texture.type == ARGB_8888 ? 4u : 2u);
    if (rawLength == 0 || rawLength > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
        return fail("invalid texture data length");

    const auto compression = static_cast<CompressMode>(texture.compressMode);
    const std::size_t encodedLength = compression == CompressMode::NONE ? rawLength :
                                      texture.length > 0 ? static_cast<std::size_t>(texture.length) : 0;
    if (encodedLength == 0 || encodedLength > 256u * 1024u * 1024u)
        return fail("invalid encoded texture length");

    texture.data.resize(encodedLength);
    if (!m_fileStream->read(reinterpret_cast<char*>(texture.data.data()), static_cast<std::streamsize>(encodedLength)))
        return fail("failed to read texture data");

    if (compression == CompressMode::ZLIB || compression == CompressMode::DDS_ZLIB)
    {
        std::vector<uint8_t> unpacked;
        if (!decompressTextureData(texture, unpacked))
            return false;
        texture.data = std::move(unpacked);
    }
    else if (compression != CompressMode::NONE)
    {
        return fail("unsupported texture compression mode");
    }

    if (dxt)
    {
        std::vector<uint8_t> rgba;
        if (!decodeDxtToRgba(texture.type, texture.width, texture.height, texture.data.data(), texture.data.size(),
                             rgba))
            return fail("failed to decode DXT texture");
        texture.data = std::move(rgba);
        texture.length = static_cast<int32_t>(texture.data.size());
        return true;
    }

    std::size_t rgbaSize = 0;
    if (!pixelBufferSize(texture.width, texture.height, rgbaSize) || texture.data.size() < rawLength)
        return fail("invalid decoded texture size");
    std::vector<uint8_t> rgba(rgbaSize, 0);

    if (texture.type == ARGB_8888)
    {
        for (std::size_t i = 0; i < static_cast<std::size_t>(texture.width) * texture.height; ++i)
        {
            rgba[i * 4 + 0] = texture.data[i * 4 + 2];
            rgba[i * 4 + 1] = texture.data[i * 4 + 1];
            rgba[i * 4 + 2] = texture.data[i * 4 + 0];
            rgba[i * 4 + 3] = texture.data[i * 4 + 3];
        }
    }
    else if (texture.type == ARGB_1555)
    {
        for (std::size_t i = 0; i < static_cast<std::size_t>(texture.width) * texture.height; ++i)
        {
            const std::size_t source = i * 2;
            const std::size_t target = i * 4;
            rgba[target + 0] = static_cast<uint8_t>(((texture.data[source + 1] & 127) >> 2) << 3);
            rgba[target + 1] = static_cast<uint8_t>(
                (((texture.data[source + 1] & 0x0003) << 3) | ((texture.data[source] >> 5) & 0x0007)) << 3);
            rgba[target + 2] = static_cast<uint8_t>((texture.data[source] & 0x003f) << 3);
            rgba[target + 3] = (texture.data[source + 1] >> 7) == 0 ? 0 : 255;
        }
    }
    else if (texture.type == ARGB_4444)
    {
        for (std::size_t i = 0; i < static_cast<std::size_t>(texture.width) * texture.height; ++i)
        {
            const std::size_t source = i * 2;
            const std::size_t target = i * 4;
            rgba[target + 0] = static_cast<uint8_t>((texture.data[source + 1] & 0x0f) << 4);
            rgba[target + 1] = static_cast<uint8_t>((texture.data[source] & 0xf0) >> 4 << 4);
            rgba[target + 2] = static_cast<uint8_t>((texture.data[source] & 0x0f) << 4);
            rgba[target + 3] = static_cast<uint8_t>((texture.data[source + 1] & 0xf0) >> 4 << 4);
        }
    }
    else
    {
        return fail("unsupported texture color type");
    }

    texture.data = std::move(rgba);
    texture.length = static_cast<int32_t>(texture.data.size());
    return true;
}

}  // namespace editor::npk
