#ifndef __SYLAR_UTIL_JSON_UTIL_H__
#define __SYLAR_UTIL_JSON_UTIL_H__

#include <json/json.h>
#include <iostream>
#include <string>

namespace sylar {

class JsonUtil {
 public:
  static bool NeedEscape(const std::string& v);
  static std::string Escape(const std::string& v);
  static std::string GetString(const Json::Value& json, const std::string& name,
                               const std::string& default_value = "");
  static double GetDouble(const Json::Value& json, const std::string& name,
                          double default_value = 0);
  static int32_t GetInt32(const Json::Value& json, const std::string& name,
                          int32_t default_value = 0);
  static uint32_t GetUint32(const Json::Value& json, const std::string& name,
                            uint32_t default_value = 0);
  static int64_t GetInt64(const Json::Value& json, const std::string& name,
                          int64_t default_value = 0);
  static uint64_t GetUint64(const Json::Value& json, const std::string& name,
                            uint64_t default_value = 0);

  static std::string GetStringValue(const Json::Value& json,
                                    const std::string& default_value = "");
  static double GetDoubleValue(const Json::Value& json,
                               double default_value = 0);
  static int32_t GetInt32Value(const Json::Value& json,
                               int32_t default_value = 0);
  static uint32_t GetUint32Value(const Json::Value& json,
                                 uint32_t default_value = 0);
  static int64_t GetInt64Value(const Json::Value& json,
                               int64_t default_value = 0);
  static uint64_t GetUint64Value(const Json::Value& json,
                                 uint64_t default_value = 0);

  static bool FromString(Json::Value& json, const std::string& v);
  static std::string ToString(const Json::Value& json, bool emit_utf8 = true);
};

}  // namespace sylar

#endif
