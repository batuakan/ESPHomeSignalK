#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>
#include <ArduinoJson.h>

namespace esphome
{
namespace signalk
{

    class SignalKMetadata
    {
    public:
        using MetaValue = std::variant<std::string, double, bool,
                                        std::vector<std::string>>;

        // Store key/value metadata
        void add_metadata(const std::string &key, const std::string &value)
        {
            this->meta_[key] = value;
        }

        void add_metadata(const std::string &key, double value)
        {
            this->meta_[key] = value;
        }

        void add_metadata(const std::string &key, bool value)
        {
            this->meta_[key] = value;
        }

        void add_metadata(const std::string &key, const std::vector<std::string> &value)
        {
            this->meta_[key] = value;
        }

         /// Copy structure into ArduinoJson object
        void to_json(JsonVariant &meta) const {
            for (const auto &kv : meta_) {
                const auto &key = kv.first;
                const auto &val = kv.second;

                if (std::holds_alternative<std::string>(val))
                {
                    meta[key] = std::get<std::string>(val);
                }

                if (std::holds_alternative<double>(val))
                {
                    meta[key] = std::get<double>(val);
                }

                if (std::holds_alternative<bool>(val))
                {
                    meta[key] = std::get<bool>(val);
                }

                if (std::holds_alternative<std::vector<std::string>>(val))
                {
                    JsonArray arr = meta[key].to<JsonArray>(); // create array
                    for (const auto &s : std::get<std::vector<std::string>>(val))
                    {
                        arr.add(s);
                    }
                    return;
                }
            }
        }

    private:
        std::map<std::string, MetaValue> meta_;

        static void value_to_json(JsonVariant dst, const MetaValue &v)
        {
            if (std::holds_alternative<std::string>(v))
            {
                dst.set(std::get<std::string>(v));
                return;
            }

            if (std::holds_alternative<double>(v))
            {
                dst.set(std::get<double>(v));
                return;
            }

            if (std::holds_alternative<bool>(v))
            {
                dst.set(std::get<bool>(v));
                return;
            }

            if (std::holds_alternative<std::vector<std::string>>(v))
            {
                JsonArray arr = dst.to<JsonArray>(); // create array
                for (const auto &s : std::get<std::vector<std::string>>(v))
                {
                    arr.add(s);
                }
                return;
            }

            dst.set(nullptr); // default
        }
    };

} // namespace signalk
} // namespace esphome
