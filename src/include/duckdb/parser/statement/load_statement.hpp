//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/statement/set_statement.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/parser/sql_statement.hpp"
#include "duckdb/parser/parsed_data/load_info.hpp"

namespace duckdb {

class LoadStatement : public SQLStatement {
public:
	LoadStatement();

public:
	unique_ptr<SQLStatement> Copy() const override;

	unique_ptr<LoadInfo> info;
};
} // namespace duckdb
