// database_util.cpp
#include "../include/database.hpp"
#include <memory>
#include <vector>

DatabaseUtil::DatabaseUtil(MySQLConnectionPool &pool) : pool_(pool) {}

void DatabaseUtil::execute_query(const std::string &query,
                                 const std::vector<ParamType> &params,
                                 ResultHandler handler) {
    auto conn = pool_.get_connection();
    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        throw std::runtime_error("Statement initialization failed");
    }

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt_guard(
        stmt, &mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    std::vector<std::unique_ptr<char[]>> str_buffers;
    auto binds = bind_parameters(params, str_buffers);

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt)) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    if (handler) {
        process_results(stmt, handler);
    }
}

bool DatabaseUtil::exists(const std::string &query,
                          const std::vector<ParamType> &params) {
    bool found = false;
    execute_query(query, params, [&found](auto, auto) { found = true; });
    return found;
}

uint64_t DatabaseUtil::insert(const std::string &query,
                              const std::vector<ParamType> &params) {
    auto conn = pool_.get_connection();
    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        throw std::runtime_error("Statement initialization failed");
    }

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt_guard(
        stmt, &mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    std::vector<std::unique_ptr<char[]>> str_buffers;
    auto binds = bind_parameters(params, str_buffers);

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt)) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    return mysql_stmt_insert_id(stmt);
}

std::vector<MYSQL_BIND> DatabaseUtil::bind_parameters(
    const std::vector<ParamType> &params,
    std::vector<std::unique_ptr<char[]>> &str_buffers) {
    std::vector<MYSQL_BIND> binds(params.size());
    memset(binds.data(), 0, sizeof(MYSQL_BIND) * binds.size());

    for (size_t i = 0; i < params.size(); ++i) {
        std::visit(
            [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, uint64_t>) {
                    binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
                    binds[i].buffer = const_cast<uint64_t *>(&arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    str_buffers.emplace_back(new char[arg.size()]);
                    memcpy(str_buffers.back().get(), arg.data(), arg.size());
                    binds[i].buffer_type = MYSQL_TYPE_STRING;
                    binds[i].buffer = str_buffers.back().get();
                    binds[i].buffer_length = arg.size();
                } else if constexpr (std::is_same_v<T, double>) {
                    binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
                    binds[i].buffer = const_cast<double *>(&arg);
                } else {
                    binds[i].buffer_type = MYSQL_TYPE_NULL;
                }
            },
            params[i]);
    }

    return binds;
}

void DatabaseUtil::process_results(MYSQL_STMT *stmt, ResultHandler handler) {
    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (!meta)
        return;

    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> meta_guard(
        meta, &mysql_free_result);

    unsigned num_fields = mysql_num_fields(meta);
    std::vector<MYSQL_BIND> result_binds(num_fields);
    std::vector<char> buffer(num_fields * 1024);
    std::vector<unsigned long> lengths(num_fields);

    for (unsigned i = 0; i < num_fields; ++i) {
        result_binds[i].buffer_type = MYSQL_TYPE_STRING;
        result_binds[i].buffer = buffer.data() + i * 1024;
        result_binds[i].buffer_length = 1024;
        result_binds[i].length = &lengths[i];
    }

    if (mysql_stmt_bind_result(stmt, result_binds.data())) {
        throw std::runtime_error(mysql_stmt_error(stmt));
    }

    mysql_stmt_store_result(stmt);
    while (!mysql_stmt_fetch(stmt)) {
        handler(reinterpret_cast<MYSQL_ROW>(buffer.data()), lengths.data());
    }
}