#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

int main()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    float lpwind_dir=343.3, wind_knots=121.2;

    writer.StartObject(); {
        writer.String("wind");
        writer.StartObject(); {
            writer.String("direction");
            writer.Double(lpwind_dir);
            writer.String("knots");
            writer.Double(wind_knots);
        }
        writer.EndObject();
    }
    writer.EndObject();

    std::string s = buffer.GetString();

    printf("hi %s\n", s.c_str());
}
