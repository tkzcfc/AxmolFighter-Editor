#include "core/SignatureUtils.h"

#include "xxhash.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace editor
{
namespace
{
constexpr double kSignatureFloatScale = 10000.0;
}

std::string SignatureBuilder::floatToken(float value)
{
    if (std::isnan(value))
        return "nan";
    if (std::isinf(value))
        return value > 0.0f ? "+inf" : "-inf";

    const double scaled = static_cast<double>(value) * kSignatureFloatScale;
    if (scaled >= static_cast<double>(std::numeric_limits<long long>::max()))
        return "+max";
    if (scaled <= static_cast<double>(std::numeric_limits<long long>::min()))
        return "-max";

    const long long quantized = std::llround(scaled);
    if (quantized == 0)
        return "0";
    return std::to_string(quantized);
}

SignatureBuilder& SignatureBuilder::appendRaw(std::string_view value)
{
    m_payload.append("R");
    m_payload.append(std::to_string(value.size()));
    m_payload.append(":");
    m_payload.append(value);
    m_payload.append(";");
    return *this;
}

SignatureBuilder& SignatureBuilder::appendString(std::string_view key, std::string_view value)
{
    return appendField('S', key, value);
}

SignatureBuilder& SignatureBuilder::appendInt(std::string_view key, int value)
{
    return appendField('I', key, std::to_string(value));
}

SignatureBuilder& SignatureBuilder::appendBool(std::string_view key, bool value)
{
    return appendField('B', key, value ? "1" : "0");
}

SignatureBuilder& SignatureBuilder::appendFloat(std::string_view key, float value)
{
    return appendField('F', key, floatToken(value));
}

const std::string& SignatureBuilder::payload() const
{
    return m_payload;
}

std::string SignatureBuilder::hash() const
{
    const XXH64_hash_t hash = XXH64(m_payload.empty() ? nullptr : m_payload.data(), m_payload.size(), 0);
    std::ostringstream stream;
    stream << std::hex << std::nouppercase << std::setfill('0') << std::setw(16)
           << static_cast<unsigned long long>(hash);
    return stream.str();
}

SignatureBuilder& SignatureBuilder::appendField(char type, std::string_view key, std::string_view value)
{
    m_payload.push_back(type);
    m_payload.append(std::to_string(key.size()));
    m_payload.append(":");
    m_payload.append(key);
    m_payload.append("=");
    m_payload.append(std::to_string(value.size()));
    m_payload.append(":");
    m_payload.append(value);
    m_payload.append(";");
    return *this;
}
}  // namespace editor
