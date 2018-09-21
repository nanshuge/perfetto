/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/window_table.h"

#include "src/trace_processor/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

namespace {
using namespace sqlite_utils;
}  // namespace

WindowTable::WindowTable(sqlite3*, const TraceStorage*) {}

void WindowTable::RegisterTable(sqlite3* db, const TraceStorage* storage) {
  Table::Register<WindowTable>(db, storage, "window", true);
}

std::string WindowTable::CreateTableStmt(int, const char* const*) {
  return "CREATE TABLE x("
         "rowid HIDDEN UNSIGNED BIG INT, "
         "quantum HIDDEN UNSIGNED BIG INT, "
         "window_start HIDDEN UNSIGNED BIG INT, "
         "window_dur HIDDEN UNSIGNED BIG INT, "
         "ts UNSIGNED BIG INT, "
         "dur UNSIGNED BIG INT, "
         "cpu UNSIGNED INT, "
         "quantized_group UNSIGNED BIG INT, "
         "PRIMARY KEY(rowid)"
         ") WITHOUT ROWID;";
}

std::unique_ptr<Table::Cursor> WindowTable::CreateCursor() {
  uint64_t window_end = window_start_ + window_dur_;
  uint64_t step_size = quantum_ == 0 ? window_dur_ : quantum_;
  return std::unique_ptr<Table::Cursor>(
      new Cursor(this, window_start_, window_end, step_size));
}

int WindowTable::BestIndex(const QueryConstraints&, BestIndexInfo*) {
  return SQLITE_OK;
}

int WindowTable::Update(int argc, sqlite3_value** argv, sqlite3_int64*) {
  // We only support updates to ts and dur. Disallow deletes (argc == 1) and
  // inserts (argv[0] == null).
  if (argc < 2 || sqlite3_value_type(argv[0]) == SQLITE_NULL)
    return SQLITE_READONLY;

  quantum_ = static_cast<uint64_t>(sqlite3_value_int64(argv[3]));
  window_start_ = static_cast<uint64_t>(sqlite3_value_int64(argv[4]));
  window_dur_ = static_cast<uint64_t>(sqlite3_value_int64(argv[5]));

  return SQLITE_OK;
}

WindowTable::Cursor::Cursor(const WindowTable* table,
                            uint64_t window_start,
                            uint64_t window_end,
                            uint64_t step_size)
    : window_start_(window_start),
      window_end_(window_end),
      step_size_(step_size),
      table_(table) {}

int WindowTable::Cursor::Column(sqlite3_context* context, int N) {
  switch (N) {
    case Column::kQuantum: {
      sqlite3_result_int64(context,
                           static_cast<sqlite_int64>(table_->quantum_));
      break;
    }
    case Column::kWindowStart: {
      sqlite3_result_int64(context,
                           static_cast<sqlite_int64>(table_->window_start_));
      break;
    }
    case Column::kWindowDur: {
      sqlite3_result_int(context, static_cast<int>(table_->window_dur_));
      break;
    }
    case Column::kTs: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(current_ts_));
      break;
    }
    case Column::kDuration: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(step_size_));
      break;
    }
    case Column::kCpu: {
      sqlite3_result_int(context, static_cast<int>(current_cpu_));
      break;
    }
    case Column::kQuantizedGroup: {
      sqlite3_result_int64(context,
                           static_cast<sqlite_int64>(quantized_group_));
      break;
    }
    case Column::kRowId: {
      sqlite3_result_int64(context, static_cast<sqlite_int64>(row_id_));
      break;
    }
    default: {
      PERFETTO_FATAL("Unknown column %d", N);
      break;
    }
  }
  return SQLITE_OK;
}

int WindowTable::Cursor::Filter(const QueryConstraints& qc, sqlite3_value** v) {
  current_ts_ = window_start_;
  current_cpu_ = 0;
  quantized_group_ = 0;
  row_id_ = 0;
  return_first = qc.constraints().size() == 1 &&
                 qc.constraints()[0].iColumn == Column::kRowId &&
                 sqlite3_value_int(v[0]) == 0;
  return SQLITE_OK;
}

int WindowTable::Cursor::Next() {
  // If we're only returning the first row, set the values to EOF.
  if (return_first) {
    current_cpu_ = base::kMaxCpus;
    current_ts_ = window_end_;
    return SQLITE_OK;
  }

  if (++current_cpu_ == base::kMaxCpus && current_ts_ < window_end_) {
    current_cpu_ = 0;
    current_ts_ += step_size_;
    quantized_group_++;
  }
  row_id_++;
  return SQLITE_OK;
}

int WindowTable::Cursor::Eof() {
  return current_cpu_ == base::kMaxCpus && current_ts_ >= window_end_;
}

}  // namespace trace_processor
}  // namespace perfetto