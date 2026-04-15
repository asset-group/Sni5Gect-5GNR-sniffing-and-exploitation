#include "shadower/comp/workers/ws_filter.h"
#include <epan/dfilter/dfilter.h>
#include <epan/ftypes/ftypes.h>
#include <epan/proto.h>
#include <limits>

namespace {

GPtrArray* find_finfos(epan_dissect_t* epan_dissect, int hf_id, bool& caller_owns_array)
{
  caller_owns_array = false;

  GPtrArray* finfos = proto_get_finfo_ptr_array(epan_dissect->tree, hf_id);
  if (finfos != nullptr) {
    return finfos;
  }

  finfos            = proto_find_finfo(epan_dissect->tree, hf_id);
  caller_owns_array = true;
  return finfos;
}

} // namespace

ws_filter_t::ws_filter_t(std::string filter_str_) : filter_str(std::move(filter_str_)), compiled(false), match(false)
{
  if (!compile()) {
    fprintf(stderr, "Failed to compile filter: %s\n", filter_str.c_str());
  }
}

ws_filter_t::~ws_filter_t()
{
  if (compiled_filter != nullptr) {
    dfilter_free(compiled_filter);
    compiled_filter = nullptr;
  }
  clear_filter_error();
}

void ws_filter_t::clear_filter_error()
{
  if (filter_error != nullptr) {
    df_error_free(&filter_error);
    filter_error = nullptr;
  }
}

bool ws_filter_t::compile()
{
  if (compiled && compiled_filter != nullptr) {
    return true;
  }
  if (compiled_filter != nullptr) {
    dfilter_free(compiled_filter);
    compiled_filter = nullptr;
  }
  if (!dfilter_compile(filter_str.c_str(), &compiled_filter, &filter_error)) {
    clear_filter_error();
    compiled_filter = nullptr;
    return false;
  }
  compiled = true;
  return compiled;
}

ws_field_t::ws_field_t(std::string field_name_, field_type_t field_type_) :
  field_name(std::move(field_name_)),
  field_type(field_type_),
  compiled(false),
  has_uint32(false),
  uint32_value(0),
  has_string(false),
  string_value()
{
  if (!compile()) {
    fprintf(stderr, "Failed to compile field: %s\n", field_name.c_str());
  }
}

void ws_field_t::reset_extracted_values()
{
  has_uint32   = false;
  uint32_value = 0;
  has_string   = false;
  string_value.clear();
}

bool ws_field_t::compile()
{
  if (compiled && !hf_ids.empty()) {
    return true;
  }

  reset_extracted_values();

  if (field_name.empty()) {
    return false;
  }

  for (header_field_info* hfinfo = proto_registrar_get_byname(field_name.c_str()); hfinfo != nullptr;
       hfinfo                    = hfinfo->same_name_next) {
    bool type_match = false;
    switch (field_type) {
      case FIELD_UINT32:
        type_match = FT_IS_UINT(hfinfo->type);
        break;
      case FIELD_STRING:
        type_match = FT_IS_STRING(hfinfo->type);
        break;
      default:
        break;
    }

    if (!type_match) {
      continue;
    }

    hf_ids.push_back(hfinfo->id);
  }

  compiled = !hf_ids.empty();
  return compiled;
}

bool ws_field_t::read_first_uint32_field(epan_dissect_t* epan_dissect)
{
  if (epan_dissect == nullptr || epan_dissect->tree == nullptr || !compiled || field_type != FIELD_UINT32) {
    return false;
  }

  for (int hf_id : hf_ids) {
    bool       free_finfos = false;
    GPtrArray* finfos      = find_finfos(epan_dissect, hf_id, free_finfos);
    if (finfos == nullptr || finfos->len == 0) {
      if (free_finfos && finfos != nullptr) {
        g_ptr_array_free(finfos, true);
      }
      continue;
    }

    for (int i = static_cast<int>(finfos->len) - 1; i >= 0; --i) {
      field_info* finfo = static_cast<field_info*>(g_ptr_array_index(finfos, i));
      if (finfo == nullptr || finfo->value == nullptr) {
        continue;
      }

      uint64_t raw_value = fvalue_get_uinteger64(finfo->value);
      if (raw_value > std::numeric_limits<uint32_t>::max()) {
        continue;
      }

      uint32_value = static_cast<uint32_t>(raw_value);
      if (free_finfos) {
        g_ptr_array_free(finfos, true);
      }
      return true;
    }

    if (free_finfos) {
      g_ptr_array_free(finfos, true);
    }
  }

  return false;
}

bool ws_field_t::read_first_string_field(epan_dissect_t* epan_dissect)
{
  if (epan_dissect == nullptr || epan_dissect->tree == nullptr || !compiled || field_type != FIELD_STRING) {
    return false;
  }

  for (int hf_id : hf_ids) {
    bool       free_finfos = false;
    GPtrArray* finfos      = find_finfos(epan_dissect, hf_id, free_finfos);
    if (finfos == nullptr || finfos->len == 0) {
      if (free_finfos && finfos != nullptr) {
        g_ptr_array_free(finfos, true);
      }
      continue;
    }

    for (int i = static_cast<int>(finfos->len) - 1; i >= 0; --i) {
      field_info* finfo = static_cast<field_info*>(g_ptr_array_index(finfos, i));
      if (finfo == nullptr || finfo->value == nullptr) {
        continue;
      }

      const char* text = fvalue_get_string(finfo->value);
      if (text == nullptr) {
        continue;
      }

      string_value = text;
      if (free_finfos) {
        g_ptr_array_free(finfos, true);
      }
      return true;
    }

    if (free_finfos) {
      g_ptr_array_free(finfos, true);
    }
  }

  return false;
}
