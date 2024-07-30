#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/type_visitor.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/capi/capi_internal.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parser/parsed_data/create_aggregate_function_info.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

struct CAggregateFunctionInfo : public AggregateFunctionInfo {
	~CAggregateFunctionInfo() override {
		if (extra_info && delete_callback) {
			delete_callback(extra_info);
		}
		extra_info = nullptr;
		delete_callback = nullptr;
	}

	duckdb_aggregate_update_t update = nullptr;
	duckdb_aggregate_combine_t combine = nullptr;
	duckdb_aggregate_finalize_t finalize = nullptr;
	duckdb_aggregate_destroy_t destroy = nullptr;
	duckdb_function_info extra_info = nullptr;
	duckdb_delete_callback_t delete_callback = nullptr;
	bool success = true;
	string error;
};

struct CAggregateFunctionBindData : public FunctionData {
	explicit CAggregateFunctionBindData(CAggregateFunctionInfo &info) : info(info) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<CAggregateFunctionBindData>(info);
	}
	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<CAggregateFunctionBindData>();
		return info.extra_info == other.info.extra_info && info.update == other.info.update &&
		       info.combine == other.info.combine && info.finalize == other.info.finalize;
	}

	CAggregateFunctionInfo &info;
};

duckdb::AggregateFunction &GetCAggregateFunction(duckdb_aggregate_function function) {
	return *reinterpret_cast<duckdb::AggregateFunction *>(function);
}

unique_ptr<FunctionData> CAPIAggregateBind(ClientContext &context, AggregateFunction &function,
                                           vector<unique_ptr<Expression>> &arguments) {
	auto &info = function.function_info->Cast<CAggregateFunctionInfo>();
	return make_uniq<CAggregateFunctionBindData>(info);
}

void CAPIAggregateUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count, Vector &state,
                         idx_t count) {
	DataChunk chunk;
	for (idx_t c = 0; c < input_count; c++) {
		inputs[c].Flatten(count);
		chunk.data.emplace_back(inputs[c]);
	}
	chunk.SetCardinality(count);

	auto &bind_data = aggr_input_data.bind_data->Cast<CAggregateFunctionBindData>();
	auto state_data = FlatVector::GetData<duckdb_aggregate_state>(state);
	auto c_input_chunk = reinterpret_cast<duckdb_data_chunk>(&chunk);
	auto c_function_info = reinterpret_cast<duckdb_function_info>(&bind_data.info);
	bind_data.info.update(c_function_info, c_input_chunk, state_data);
	if (!bind_data.info.success) {
		throw InvalidInputException(bind_data.info.error);
	}
}

void CAPIAggregateCombine(Vector &state, Vector &combined, AggregateInputData &aggr_input_data, idx_t count) {
	state.Flatten(count);
	auto &bind_data = aggr_input_data.bind_data->Cast<CAggregateFunctionBindData>();
	auto input_state_data = FlatVector::GetData<duckdb_aggregate_state>(state);
	auto result_state_data = FlatVector::GetData<duckdb_aggregate_state>(combined);
	auto c_function_info = reinterpret_cast<duckdb_function_info>(&bind_data.info);
	bind_data.info.combine(c_function_info, input_state_data, result_state_data, count);
	if (!bind_data.info.success) {
		throw InvalidInputException(bind_data.info.error);
	}
}

// typedef void (*duckdb_aggregate_finalize_t)(duckdb_function_info info, duckdb_aggregate_state *source, duckdb_vector
// result, idx_t count);

void CAPIAggregateFinalize(Vector &state, AggregateInputData &aggr_input_data, Vector &result, idx_t count,
                           idx_t offset) {
	state.Flatten(count);
	auto &bind_data = aggr_input_data.bind_data->Cast<CAggregateFunctionBindData>();
	auto input_state_data = FlatVector::GetData<duckdb_aggregate_state>(state);
	auto result_vector = reinterpret_cast<duckdb_vector>(&result);
	auto c_function_info = reinterpret_cast<duckdb_function_info>(&bind_data.info);
	bind_data.info.finalize(c_function_info, input_state_data, result_vector, count, offset);
	if (!bind_data.info.success) {
		throw InvalidInputException(bind_data.info.error);
	}
}

void CAPIAggregateDestructor(Vector &state, AggregateInputData &aggr_input_data, idx_t count) {
	auto &bind_data = aggr_input_data.bind_data->Cast<CAggregateFunctionBindData>();
	auto input_state_data = FlatVector::GetData<duckdb_aggregate_state>(state);
	bind_data.info.destroy(input_state_data, count);
}

} // namespace duckdb

using duckdb::GetCAggregateFunction;

duckdb_aggregate_function duckdb_create_aggregate_function() {
	auto function = new duckdb::AggregateFunction("", {}, duckdb::LogicalType::INVALID, nullptr, nullptr,
	                                              duckdb::CAPIAggregateUpdate, duckdb::CAPIAggregateCombine,
	                                              duckdb::CAPIAggregateFinalize, nullptr, duckdb::CAPIAggregateBind);
	function->function_info = duckdb::make_shared_ptr<duckdb::CAggregateFunctionInfo>();
	return reinterpret_cast<duckdb_aggregate_function>(function);
}

void duckdb_destroy_aggregate_function(duckdb_aggregate_function *function) {
	if (function && *function) {
		auto aggregate_function = reinterpret_cast<duckdb::AggregateFunction *>(*function);
		delete aggregate_function;
		*function = nullptr;
	}
}

void duckdb_aggregate_function_set_name(duckdb_aggregate_function function, const char *name) {
	if (!function || !name) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	aggregate_function.name = name;
}

void duckdb_aggregate_function_add_parameter(duckdb_aggregate_function function, duckdb_logical_type type) {
	if (!function || !type) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	auto logical_type = reinterpret_cast<duckdb::LogicalType *>(type);
	aggregate_function.arguments.push_back(*logical_type);
}

void duckdb_aggregate_function_set_return_type(duckdb_aggregate_function function, duckdb_logical_type type) {
	if (!function || !type) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	auto logical_type = reinterpret_cast<duckdb::LogicalType *>(type);
	aggregate_function.return_type = *logical_type;
}

void duckdb_aggregate_function_set_functions(duckdb_aggregate_function function, duckdb_aggregate_state_size state_size,
                                             duckdb_aggregate_init_t state_init, duckdb_aggregate_update_t update,
                                             duckdb_aggregate_combine_t combine, duckdb_aggregate_finalize_t finalize) {
	if (!function || !state_size || !state_init || !update || !combine || !finalize) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	aggregate_function.initialize = reinterpret_cast<duckdb::aggregate_initialize_t>(state_init);
	aggregate_function.state_size = state_size;
	auto &function_info = aggregate_function.function_info->Cast<duckdb::CAggregateFunctionInfo>();
	function_info.update = update;
	function_info.combine = combine;
	function_info.finalize = finalize;
}

void duckdb_aggregate_function_set_destructor(duckdb_aggregate_function function, duckdb_aggregate_destroy_t destroy) {
	if (!function || !destroy) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	auto &function_info = aggregate_function.function_info->Cast<duckdb::CAggregateFunctionInfo>();
	function_info.destroy = destroy;
	aggregate_function.destructor = duckdb::CAPIAggregateDestructor;
}

duckdb_state duckdb_register_aggregate_function(duckdb_connection connection, duckdb_aggregate_function function) {
	if (!connection || !function) {
		return DuckDBError;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	auto &info = aggregate_function.function_info->Cast<duckdb::CAggregateFunctionInfo>();

	if (aggregate_function.name.empty() || !info.update || !info.combine || !info.finalize) {
		return DuckDBError;
	}
	if (duckdb::TypeVisitor::Contains(aggregate_function.return_type, duckdb::LogicalTypeId::INVALID) ||
	    duckdb::TypeVisitor::Contains(aggregate_function.return_type, duckdb::LogicalTypeId::ANY)) {
		return DuckDBError;
	}
	for (const auto &argument : aggregate_function.arguments) {
		if (duckdb::TypeVisitor::Contains(argument, duckdb::LogicalTypeId::INVALID)) {
			return DuckDBError;
		}
	}

	try {
		auto con = reinterpret_cast<duckdb::Connection *>(connection);
		con->context->RunFunctionInTransaction([&]() {
			auto &catalog = duckdb::Catalog::GetSystemCatalog(*con->context);
			duckdb::CreateAggregateFunctionInfo sf_info(aggregate_function);
			catalog.CreateFunction(*con->context, sf_info);
		});
	} catch (...) {
		return DuckDBError;
	}
	return DuckDBSuccess;
}

void duckdb_aggregate_function_set_extra_info(duckdb_aggregate_function function, void *extra_info,
                                              duckdb_delete_callback_t destroy) {
	if (!function || !extra_info) {
		return;
	}
	auto &aggregate_function = GetCAggregateFunction(function);
	auto &function_info = aggregate_function.function_info->Cast<duckdb::CAggregateFunctionInfo>();
	function_info.extra_info = static_cast<duckdb_function_info>(extra_info);
	function_info.delete_callback = destroy;
}

duckdb::CAggregateFunctionInfo &GetCAggregateFunctionInfo(duckdb_function_info info) {
	D_ASSERT(info);
	return *reinterpret_cast<duckdb::CAggregateFunctionInfo *>(info);
}

void *duckdb_aggregate_function_get_extra_info(duckdb_function_info info_p) {
	auto &info = GetCAggregateFunctionInfo(info_p);
	return info.extra_info;
}

void duckdb_aggregate_function_set_error(duckdb_function_info info_p, const char *error) {
	auto &info = GetCAggregateFunctionInfo(info_p);
	info.error = error;
	info.success = false;
}
