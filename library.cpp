#include <cstdio>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <cstring>

#include <curl/curl.h>
#include <json-c/json.h>

#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

/* Insert your extension code here */

std::optional<std::string> parse_name(const char* json_str) {
    json_object *json = json_tokener_parse(json_str);
    json_object *name_obj{};
    if (json_object_object_get_ex(json, "name", &name_obj)) {
        return {std::string{json_object_get_string(name_obj)}};
    }
    return {};
}

size_t write_callback(char* data, size_t size, size_t nmemb, std::string* str) {
    str->append(data, size * nmemb);
    return size * nmemb;
}

bool isValidHexChar(char c) {
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

std::optional<std::string> normalize_uuid(std::string_view uuid) {
    std::string out;
    out.reserve(36);
    int hyphens = 0;
    int hex_chars = 0;
    for (char c : uuid) {
        if (c == '-') {
            hyphens++;
            if (hyphens > 4) return {}; // Too many hyphens
        }
        else if (isValidHexChar(c)) {
            hex_chars++;
            if (hex_chars > 32) return {}; // Too many hex chars
            out.push_back(tolower(c));
            if (hex_chars == 8 || hex_chars == 12 || hex_chars == 16 || hex_chars == 20) {
                out.push_back('-');
            }
        } else {
            return {}; // invalid uuid
        }
    }
    return out;
}

std::optional<std::string> fetch_name(const char* uuid) {
    CURL *curl = curl_easy_init();
    assert(curl);
    std::string url = std::string{"https://sessionserver.mojang.com/session/minecraft/profile/"} + uuid;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    char error[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        puts(error);
        return {};
    } else {
        return parse_name(response.c_str());
    }
}

void update_cache(sqlite3* db, const char* uuid, const char* name, int64_t created_at) {
    const char *sql = "INSERT OR REPLACE INTO mc_uuid_v1(uuid, name, created_at) VALUES(?, ?, ?)";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, created_at);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void uuid_lookup_function(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    assert(argc == 1);
    sqlite3_value* arg1 = argv[0];
    const char* input = reinterpret_cast<const char *>(sqlite3_value_text(arg1));
    auto uuid = normalize_uuid(input);
    if (!uuid) {
        auto msg = "Invalid uuid";
        sqlite3_result_error(ctx, msg, strlen(msg));
        return;
    }

    sqlite3* db = sqlite3_context_db_handle(ctx);

    const char *sql = "SELECT name,created_at FROM mc_uuid_v1 WHERE uuid = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, uuid->c_str(), -1, SQLITE_TRANSIENT);

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    constexpr int64_t millis_in_hour = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours{1}).count();
    // Execute SQL statement
    auto result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        auto cached_name = (const char*) sqlite3_column_text(stmt, 0);
        int64_t created_at = sqlite3_column_int64(stmt, 1);
        if (now - created_at < millis_in_hour) {
            sqlite3_result_text(ctx, cached_name, strlen(cached_name), SQLITE_TRANSIENT);
        } else {
            // don't trust the cache
            auto name = fetch_name(uuid->c_str());
            if (name) {
                update_cache(db, uuid->c_str(), name->c_str(), created_at);
                sqlite3_result_text(ctx, name->c_str(), name->length(), SQLITE_TRANSIENT);
            } else {
                // request failed so just trust the cache
                sqlite3_result_text(ctx, cached_name, strlen(cached_name), SQLITE_TRANSIENT);
            }
        }
    } else {
        auto name = fetch_name(uuid->c_str());
        if (name) {
            update_cache(db, uuid->c_str(), name->c_str(), now);
            sqlite3_result_text(ctx, name->c_str(), name->length(), SQLITE_TRANSIENT);
        } else {
            auto msg = "Failed to do http request";
            sqlite3_result_error(ctx, msg, strlen(msg));
        }
    }
    sqlite3_finalize(stmt);
}


#ifdef _WIN32
__declspec(dllexport)
#endif
extern "C" int sqlite3_sqlitemcuuid_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi
){
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);

    const char *sql_stmt = "CREATE TABLE IF NOT EXISTS mc_uuid_v1 ("
                           "uuid TEXT PRIMARY KEY,"
                           "name TEXT NOT NULL,"
                           "created_at INTEGER NOT NULL);";
    char *err_msg = nullptr;
    auto err = sqlite3_exec(db, sql_stmt, nullptr, nullptr, &err_msg);
    if (err != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Failed to create mc_uuid_v1 table: %s", err_msg);
        sqlite3_free(err_msg);
        return SQLITE_ERROR;
    }

    err = sqlite3_create_function_v2(db, "username", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, uuid_lookup_function, nullptr, nullptr, nullptr);
    if (err != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Failed to create username function: %s", sqlite3_errstr(err));
        return SQLITE_ERROR;
    }

    return rc;
}
