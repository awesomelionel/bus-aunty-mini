#include <fstream>
#include <sstream>
#include <string>

#include <unity.h>

#include "core/arrival_parser.h"
#include "net/bounded_stream.h"

namespace {

std::string readFixture(const char* name) {
    std::ifstream in(std::string("test/data/") + name);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

class MemoryStream : public Stream {
public:
    explicit MemoryStream(const std::string& data) : data_(data) {}

    int available() override {
        return static_cast<int>(data_.size() - pos_);
    }

    int read() override {
        if (pos_ >= data_.size()) {
            return -1;
        }
        return static_cast<unsigned char>(data_[pos_++]);
    }

    int peek() override {
        if (pos_ >= data_.size()) {
            return -1;
        }
        return static_cast<unsigned char>(data_[pos_]);
    }

    size_t write(uint8_t) override { return 0; }

private:
    const std::string& data_;
    size_t pos_ = 0;
};

}  // namespace

void test_real_fixture_parses_through_bounded_stream() {
    const std::string body = readFixture("stop_52109.json");
    TEST_ASSERT_TRUE(body.size() > 32);

    MemoryStream raw(body);
    BoundedStream capped(raw, body.size());
    capped.setTimeout(8000);
    const ParsedBusStop streamed = parseBusArrivalStream(capped, "52109");
    const ParsedBusStop direct = parseBusArrivalResponse(body, "52109");

    TEST_ASSERT_TRUE(streamed.valid);
    TEST_ASSERT_TRUE(direct.valid);
    TEST_ASSERT_EQUAL(direct.services.size(), streamed.services.size());
    TEST_ASSERT_EQUAL(direct.rows.size(), streamed.rows.size());
    TEST_ASSERT_EQUAL_INT64(direct.updatedAtEpoch, streamed.updatedAtEpoch);
    for (size_t i = 0; i < direct.rows.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(direct.rows[i].serviceNo.c_str(),
                                 streamed.rows[i].serviceNo.c_str());
        TEST_ASSERT_EQUAL_STRING(direct.rows[i].label.c_str(),
                                 streamed.rows[i].label.c_str());
        TEST_ASSERT_EQUAL_STRING(direct.rows[i].visitNumber.c_str(),
                                 streamed.rows[i].visitNumber.c_str());
    }

    MemoryStream shortRaw(body);
    BoundedStream tiny(shortRaw, 32);
    tiny.setTimeout(8000);
    const ParsedBusStop cut = parseBusArrivalStream(tiny, "52109");
    TEST_ASSERT_FALSE(cut.valid);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_real_fixture_parses_through_bounded_stream);
    return UNITY_END();
}
