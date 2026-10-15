#include "doctest/doctest.h"

#include "documents/AniDocument.h"
#include "documents/BoxDocument.h"
#include "documents/DocumentGenMeta.h"
#include "documents/LayerDocument.h"
#include "documents/MotionDocument.h"

#include "rapidjson/document.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
namespace fs = std::filesystem;

const char* const kMixedMeta = R"({
    "sourceHash": "sha256:0123456789abcdef",
    "count": 42,
    "negative": -7,
    "uint64Max": 18446744073709551615,
    "int64Min": -9223372036854775808,
    "uintAboveInt": 4294967295,
    "double": 0.1,
    "pi": 3.141592653589793,
    "tiny": 4.9406564584124654e-324,
    "huge": 1.7976931348623157e308,
    "oneDotZero": 1.0,
    "text": "中文 \"quoted\" \\ back\nline\ttab \u00e9 \ud83d\ude00 \u0001",
    "yes": true,
    "no": false,
    "nothing": null,
    "list": [1, -2, 3.5, "x", null, true, [], {}],
    "nested": {"z": {"inner": [0, {"k": "v"}]}, "a": 1, "m": -0.0}
})";

std::string readText(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeText(const fs::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

// Inserts `"_gen_meta_": <metaJson>` as the first member of a JSON object document.
std::string withMeta(const std::string& json, const std::string& metaJson)
{
    const std::size_t brace = json.find('{');
    return json.substr(0, brace + 1) + "\"" + editor::kDocumentGenMetaKey + "\": " + metaJson + "," +
           json.substr(brace + 1);
}

rapidjson::Document parse(const std::string& json)
{
    rapidjson::Document document;
    document.Parse<rapidjson::kParseFullPrecisionFlag>(json.c_str(), json.size());
    REQUIRE_FALSE(document.HasParseError());
    return document;
}

// Exact comparison: same JSON type, same numeric representation flags, bitwise-equal doubles, same key order.
bool sameJson(const rapidjson::Value& left, const rapidjson::Value& right)
{
    if (left.GetType() != right.GetType())
        return false;
    if (left.IsNumber())
    {
        if (left.IsInt() != right.IsInt() || left.IsUint() != right.IsUint() || left.IsInt64() != right.IsInt64() ||
            left.IsUint64() != right.IsUint64() || left.IsDouble() != right.IsDouble())
            return false;
        if (left.IsDouble())
        {
            const double a = left.GetDouble();
            const double b = right.GetDouble();
            return std::memcmp(&a, &b, sizeof(double)) == 0;
        }
        if (left.IsUint64())
            return left.GetUint64() == right.GetUint64();
        return left.GetInt64() == right.GetInt64();
    }
    if (left.IsString())
        return left.GetStringLength() == right.GetStringLength() &&
               std::memcmp(left.GetString(), right.GetString(), left.GetStringLength()) == 0;
    if (left.IsArray())
    {
        if (left.Size() != right.Size())
            return false;
        for (rapidjson::SizeType i = 0; i < left.Size(); ++i)
        {
            if (!sameJson(left[i], right[i]))
                return false;
        }
        return true;
    }
    if (left.IsObject())
    {
        if (left.MemberCount() != right.MemberCount())
            return false;
        for (auto l = left.MemberBegin(), r = right.MemberBegin(); l != left.MemberEnd(); ++l, ++r)
        {
            if (!sameJson(l->name, r->name) || !sameJson(l->value, r->value))
                return false;
        }
        return true;
    }
    return true;  // null, true, false: the type already matched
}

bool hasMetaKey(const std::string& json)
{
    const rapidjson::Document root = parse(json);
    return root.IsObject() && root.HasMember(editor::kDocumentGenMetaKey);
}

class TempDir
{
public:
    TempDir()
    {
        m_path = fs::temp_directory_path() /
                 ("axf_gen_meta_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::remove_all(m_path);
        fs::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code error;
        fs::remove_all(m_path, error);
    }
    fs::path file(const char* name) const { return m_path / name; }

private:
    fs::path m_path;
};

const char* const kMotionJson =
    R"({"type": "motion", "version": 1, "motions": [{"name": "idle", "animations": [)"
    R"({"id": "a", "type": "spine", "source": "hero.skel", "box": "idle.box"}]}]})";
const char* const kBoxJson =
    R"({"type": "combat", "version": 1, "duration": 100, "preview": {"kind": "none", "path": "", "atlasPath": "",)"
    R"( "animation": ""}, "tracks": [{"name": "body", "kind": "damage", "keys": [{"timeMs": 0, "box": null}]}],)"
    R"( "events": []})";
const char* const kAniJson = R"({"version": 1, "loop": true, "previewVars": {}, "frames": []})";
const char* const kLayerJson =
    R"({"type": "layer", "version": 2, "root": {"id": "root", "type": "Node", "name": "Root", "children": []},)"
    R"( "plugins": {"shape": {"selectedShapeId": "", "nextShapeSerial": 1, "shapes": []}}})";

// Per-type adapters so every check runs against all four editor JSON document types.
struct MotionCase
{
    using Document = editor::MotionDocument;
    static const char* json() { return kMotionJson; }
    static const char* fileName() { return "test.motion"; }
    static void edit(Document& document)
    {
        REQUIRE(document.beginEditTransaction());
        document.data().motions.front().animations.front().tag = "edited";
        document.commitEditTransaction();
    }
};

struct BoxCase
{
    using Document = editor::BoxDocument;
    static const char* json() { return kBoxJson; }
    static const char* fileName() { return "test.box"; }
    static void edit(Document& document)
    {
        REQUIRE(document.beginEditTransaction());
        document.data().duration = 250;
        document.commitEditTransaction();
    }
};

struct AniCase
{
    using Document = editor::AniDocument;
    static const char* json() { return kAniJson; }
    static const char* fileName() { return "test.ani"; }
    static void edit(Document& document)
    {
        REQUIRE(document.beginEditTransaction());
        document.animation().loop = false;
        document.commitEditTransaction();
    }
};

struct LayerCase
{
    using Document = editor::LayerDocument;
    static const char* json() { return kLayerJson; }
    static const char* fileName() { return "test.layer"; }
    static void edit(Document& document)
    {
        REQUIRE(document.beginUndoTransaction());
        document.root().name = "Edited";
        document.markDirty();
        document.commitUndoTransaction();
    }
};
}  // namespace

TEST_SUITE("DocumentGenMeta")
{
    TEST_CASE_TEMPLATE("mixed-type meta survives edit, undo and save", T, MotionCase, BoxCase, AniCase, LayerCase)
    {
        TempDir dir;
        const fs::path path = dir.file(T::fileName());
        writeText(path, withMeta(T::json(), kMixedMeta));
        const rapidjson::Document expected = parse(kMixedMeta);

        typename T::Document document;
        REQUIRE_MESSAGE(document.open(path), document.lastError());
        REQUIRE(document.genMeta().hasValue());
        CHECK(sameJson(document.genMeta().value(), expected));
        CHECK_FALSE(document.isDirty());
        CHECK_FALSE(document.canUndo());

        T::edit(document);
        CHECK(document.isDirty());
        REQUIRE(document.save());
        CHECK_FALSE(document.isDirty());

        const std::string saved = readText(path);
        const rapidjson::Document root = parse(saved);
        REQUIRE(root.HasMember(editor::kDocumentGenMetaKey));
        CHECK(sameJson(root[editor::kDocumentGenMetaKey], expected));
        // Stable position: the key is the last top-level member.
        CHECK(std::strcmp((root.MemberEnd() - 1)->name.GetString(), editor::kDocumentGenMetaKey) == 0);
        CHECK(saved.find("18446744073709551615") != std::string::npos);
        CHECK(saved.find("-9223372036854775808") != std::string::npos);

        // Undo back to the loaded state keeps the meta too; saving twice is stable.
        REQUIRE(document.undo());
        REQUIRE(document.save());
        const std::string savedAfterUndo = readText(path);
        CHECK(sameJson(parse(savedAfterUndo)[editor::kDocumentGenMetaKey], expected));
        REQUIRE(document.save());
        CHECK(readText(path) == savedAfterUndo);

        // Reopening the saved file yields the same meta.
        typename T::Document reopened;
        REQUIRE(reopened.open(path));
        CHECK(sameJson(reopened.genMeta().value(), expected));
    }

    TEST_CASE_TEMPLATE("non-object meta values are kept as-is", T, MotionCase, BoxCase, AniCase, LayerCase)
    {
        const char* values[] = {R"("sha1:da39a3ee5e6b4b0d3255bfef95601890afd80709")", "null", "false", "-12",
                                "2.5", R"([1, "two", {"three": 3}])"};
        for (const char* value : values)
        {
            CAPTURE(value);
            TempDir dir;
            const fs::path path = dir.file(T::fileName());
            writeText(path, withMeta(T::json(), value));

            typename T::Document document;
            REQUIRE(document.open(path));
            REQUIRE(document.genMeta().hasValue());
            T::edit(document);
            REQUIRE(document.save());

            const rapidjson::Document root = parse(readText(path));
            REQUIRE(root.HasMember(editor::kDocumentGenMetaKey));
            CHECK(sameJson(root[editor::kDocumentGenMetaKey], parse(value)));
        }
    }

    TEST_CASE_TEMPLATE("files without meta are saved without the key", T, MotionCase, BoxCase, AniCase, LayerCase)
    {
        TempDir dir;
        const fs::path path = dir.file(T::fileName());
        writeText(path, T::json());

        typename T::Document document;
        REQUIRE(document.open(path));
        CHECK_FALSE(document.genMeta().hasValue());
        T::edit(document);
        REQUIRE(document.save());
        const std::string saved = readText(path);
        CHECK_FALSE(hasMetaKey(saved));
        CHECK(saved.find(editor::kDocumentGenMetaKey) == std::string::npos);
    }

    TEST_CASE_TEMPLATE("reload refreshes meta from disk", T, MotionCase, BoxCase, AniCase, LayerCase)
    {
        TempDir dir;
        const fs::path path = dir.file(T::fileName());
        writeText(path, withMeta(T::json(), R"({"hash": "first"})"));

        typename T::Document document;
        REQUIRE(document.open(path));
        CHECK(sameJson(document.genMeta().value(), parse(R"({"hash": "first"})")));

        writeText(path, withMeta(T::json(), R"({"hash": "second", "n": 2})"));
        REQUIRE(document.open(path));
        CHECK(sameJson(document.genMeta().value(), parse(R"({"hash": "second", "n": 2})")));
        CHECK_FALSE(document.isDirty());

        writeText(path, T::json());
        REQUIRE(document.open(path));
        CHECK_FALSE(document.genMeta().hasValue());

        // A failed load leaves the previous state untouched.
        writeText(path, withMeta(T::json(), R"("kept")"));
        REQUIRE(document.open(path));
        writeText(path, "{ not json");
        CHECK_FALSE(document.open(path));
        CHECK(sameJson(document.genMeta().value(), parse(R"("kept")")));
    }

    TEST_CASE("new documents have no meta")
    {
        TempDir dir;
        std::string message;
        REQUIRE(editor::MotionDocument::createDefaultFile(dir.file("new.motion"), message));
        REQUIRE(editor::BoxDocument::createDefaultFile(dir.file("new.box"), message));
        REQUIRE(editor::AniDocument::createDefaultFile(dir.file("new.ani"), message));
        REQUIRE(editor::LayerDocument::createDefaultFile(dir.file("new.layer"), message));
        for (const char* name : {"new.motion", "new.box", "new.ani", "new.layer"})
        {
            CAPTURE(name);
            CHECK_FALSE(hasMetaKey(readText(dir.file(name))));
        }
    }

    TEST_CASE("DocumentGenMeta copies are deep")
    {
        const rapidjson::Document root = parse(withMeta(kAniJson, kMixedMeta));
        editor::DocumentGenMeta meta;
        meta.readFrom(root);
        editor::DocumentGenMeta copy(meta);
        meta.clear();
        CHECK_FALSE(meta.hasValue());
        REQUIRE(copy.hasValue());
        CHECK(sameJson(copy.value(), parse(kMixedMeta)));
    }
}
