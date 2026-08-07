#pragma once

#include <string>
#include <string_view>

namespace editor
{
class SignatureBuilder
{
public:
    SignatureBuilder& appendRaw(std::string_view value);
    SignatureBuilder& appendString(std::string_view key, std::string_view value);
    SignatureBuilder& appendInt(std::string_view key, int value);
    SignatureBuilder& appendBool(std::string_view key, bool value);
    SignatureBuilder& appendFloat(std::string_view key, float value);

    const std::string& payload() const;
    std::string hash() const;

private:
    std::string floatToken(float value);

    SignatureBuilder& appendField(char type, std::string_view key, std::string_view value);

    std::string m_payload;
};
}  // namespace editor
