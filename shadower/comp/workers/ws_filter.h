#pragma once
#include <cstdint>
#include <epan/dfilter/dfilter.h>
#include <epan/epan_dissect.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ws_filter_t
{
public:
  explicit ws_filter_t(std::string filter_str_);

  ~ws_filter_t();

  bool compile();

  static std::shared_ptr<ws_filter_t> make_filter(const std::string& str) { return std::make_shared<ws_filter_t>(str); }

  void clear_filter_error();

  std::string filter_str;
  dfilter_t*  compiled_filter = nullptr;
  bool        match         = false;
  bool        compiled        = false;

private:
  df_error_t* filter_error = nullptr;
};

enum field_type_t {
  FIELD_UINT32,
  FIELD_STRING,
};

class ws_field_t
{
public:
  explicit ws_field_t(std::string field_name_, field_type_t field_type_);

  ~ws_field_t() = default;

  void reset_extracted_values();

  bool compile();

  static std::shared_ptr<ws_field_t> make_field_uint32(const std::string& field_name)
  {
    return std::make_shared<ws_field_t>(field_name, FIELD_UINT32);
  }

  static std::shared_ptr<ws_field_t> make_field_string(const std::string& field_name)
  {
    return std::make_shared<ws_field_t>(field_name, FIELD_STRING);
  }

  std::string      field_name;
  field_type_t     field_type = FIELD_UINT32;
  std::vector<int> hf_ids;
  bool             compiled = false;

  bool     has_uint32   = false;
  uint32_t uint32_value = 0;

  bool        has_string = false;
  std::string string_value;

  bool read_first_uint32_field(epan_dissect_t* epan_dissect);

  bool read_first_string_field(epan_dissect_t* epan_dissect);
};
