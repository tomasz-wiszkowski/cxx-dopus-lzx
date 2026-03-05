#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <vector>

extern "C" {
#include "vfs plugins.h"
}

/// @brief A class representing single Opus custom column definition, including its metadata and value retrieval logic.
/// @tparam T The type of the entry this column is associated with / produces data for.
template <typename T>
class CustomColumn {
 public:
  /// @brief Constructs a CustomColumn content with the given parameters.
  using ValueFunc =
      std::function<size_t(const T& /* entry */, wchar_t* /* nullable_buffer */, size_t /* buffer_length */)>;

  /// @brief Instantiates a CustomColumn with the given parameters.
  /// @param id The next available column ID for this column.
  /// @param name The display name of the column.
  /// @param key The key used to store the column value in VFSFILEDATA.
  /// @param flags The display flags for the column (e.g. alignment).
  /// @param value_func The function to call to retrieve the column value for a given directory entry.
  CustomColumn(int id, const wchar_t* name, const wchar_t* key, DWORD flags, ValueFunc value_func)
      : column_info_{.cbSize = sizeof(VFSCUSTOMCOLUMN), .lpNext = nullptr, .lpszLabel = const_cast<wchar_t*>(name), .lpszKey = const_cast<wchar_t*>(key), .dwFlags = flags, .iID = id},
        value_func_(std::move(value_func)) {}

  /// @brief Links this column to the next column in the list.
  void link_column(const CustomColumn<T>* next) {
    column_info_.lpNext = const_cast<VFSCUSTOMCOLUMN*>(next ? &next->column_info_ : nullptr);
  }

  /// @return The column ID for this custom column.
  int get_id() const { return column_info_.iID; }

  /// @return The VFSCUSTOMCOLUMN structure for this custom column.
  const VFSCUSTOMCOLUMN* get_info() const { return &column_info_; }

  /// @brief Get formatted column data for an entry.
  /// @param heap The heap to allocate the returned string on.
  /// @param entry The entry to retrieve the column value for.
  /// @return A pointer to a heap-allocated string containing the column value for the given entry, or nullptr if no
  /// value.
  wchar_t* get_value(HANDLE heap, const T& entry) const {
    size_t length = value_func_(entry, nullptr, 0);
    if (length == 0)
      return nullptr;

    wchar_t* buffer = static_cast<wchar_t*>(HeapAlloc(heap, 0, (length + 1) * sizeof(wchar_t)));
    value_func_(entry, buffer, length + 1);
    return buffer;
  }

 private:
  VFSCUSTOMCOLUMN column_info_;
  ValueFunc value_func_;
};

/// @brief A manager class for handling multiple custom columns, including their definitions and value retrieval.
/// @tparam T The type of the entry the columns are associated with / produce data for.
template <typename T>
class CustomColumnManager {
 public:
  /// @brief Populates the lpvfsColumnData field of a VFSFILEDATA structure with the custom column values for a given
  /// directory entry.
  /// @param data Pointer to the VFSFILEDATA structure to populate.
  /// @param heap Handle to the heap for memory allocation.
  /// @param entry The directory entry to retrieve column values for.
  void populate_custom_column_data(LPVFSFILEDATAW data, HANDLE heap, const T& entry) const {
    std::vector<std::pair<int /* column_id */, wchar_t* /* content */>> column_values;

    for (const auto& column : columns_) {
      wchar_t* value = column->get_value(heap, entry);
      if (!value)
        continue;
      column_values.emplace_back(column->get_id(), value);
    }

    data->iNumColumns = static_cast<int>(column_values.size());
    data->lpvfsColumnData = nullptr;

    if (column_values.empty())
      return;

    data->lpvfsColumnData =
        static_cast<LPVFSFILEDATACOLUMNW>(HeapAlloc(heap, 0, column_values.size() * sizeof(VFSFILEDATACOLUMNW)));
    for (size_t i = 0; i < column_values.size(); ++i) {
      data->lpvfsColumnData[i].iColumnId = column_values[i].first;
      data->lpvfsColumnData[i].lpszValue = column_values[i].second;
    }
  }

  const VFSCUSTOMCOLUMN* get_columns() const { return columns_.empty() ? nullptr : columns_[0]->get_info(); }

  /// @brief Add a custom column definition to the manager.
  /// @param name Display name of the column.
  /// @param key Map key for the column (used in VFSFILEDATA).
  /// @param flags Display flags for the column (e.g. alignment).
  /// @param value_func Method to retrieve the column value for a given directory entry.
  void add_custom_column(
      const wchar_t* name,
      const wchar_t* key,
      DWORD flags,
      typename CustomColumn<T>::ValueFunc value_func) {
    auto new_column =
        std::make_unique<CustomColumn<T>>(static_cast<int>(columns_.size()), name, key, flags, std::move(value_func));
    if (!columns_.empty()) {
      columns_.back()->link_column(new_column.get());
    }
    columns_.push_back(std::move(new_column));
  }

 private:
  std::vector<std::unique_ptr<CustomColumn<T>>> columns_;
};
