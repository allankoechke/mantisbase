#include "db_wrap.h"
#include <nlohmann/json.hpp>
#include <soci/soci.h>

using json = nlohmann::json;

Napi::FunctionReference DbWrap::s_constructor;

static Napi::Value JsonToNapi(Napi::Env env, const json& j) {
    if (j.is_null()) return env.Null();
    if (j.is_boolean()) return Napi::Boolean::New(env, j.get<bool>());
    if (j.is_number_integer()) return Napi::Number::New(env, static_cast<double>(j.get<int64_t>()));
    if (j.is_number_float()) return Napi::Number::New(env, j.get<double>());
    if (j.is_string()) return Napi::String::New(env, j.get<std::string>());
    if (j.is_array()) {
        auto arr = Napi::Array::New(env, j.size());
        for (size_t i = 0; i < j.size(); ++i) {
            arr.Set(static_cast<uint32_t>(i), JsonToNapi(env, j[i]));
        }
        return arr;
    }
    if (j.is_object()) {
        auto obj = Napi::Object::New(env);
        for (auto it = j.begin(); it != j.end(); ++it) {
            obj.Set(it.key(), JsonToNapi(env, it.value()));
        }
        return obj;
    }
    return env.Undefined();
}

static json NapiToJson(Napi::Value val) {
    if (val.IsNull() || val.IsUndefined()) return nullptr;
    if (val.IsBoolean()) return val.As<Napi::Boolean>().Value();
    if (val.IsNumber()) {
        double d = val.As<Napi::Number>().DoubleValue();
        if (d == static_cast<int64_t>(d) && d >= -9007199254740992.0 && d <= 9007199254740992.0) {
            return static_cast<int64_t>(d);
        }
        return d;
    }
    if (val.IsString()) return val.As<Napi::String>().Utf8Value();
    if (val.IsArray()) {
        auto arr = val.As<Napi::Array>();
        json j = json::array();
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            j.push_back(NapiToJson(arr.Get(i)));
        }
        return j;
    }
    if (val.IsObject()) {
        auto obj = val.As<Napi::Object>();
        json j = json::object();
        auto names = obj.GetPropertyNames();
        for (uint32_t i = 0; i < names.Length(); ++i) {
            std::string key = names.Get(i).As<Napi::String>().Utf8Value();
            j[key] = NapiToJson(obj.Get(key));
        }
        return j;
    }
    return nullptr;
}

static json SociRowToJson(const soci::row& row) {
    json obj = json::object();
    for (size_t i = 0; i < row.size(); ++i) {
        const auto& props = row.get_properties(i);
        const std::string& name = props.get_name();

        if (row.get_indicator(i) == soci::i_null) {
            obj[name] = nullptr;
            continue;
        }

        switch (props.get_db_type()) {
            case soci::db_string:
            case soci::db_xml:
                obj[name] = row.get<std::string>(i);
                break;
            case soci::db_double:
                obj[name] = row.get<double>(i);
                break;
            case soci::db_int8:
                obj[name] = static_cast<int64_t>(row.get<int8_t>(i));
                break;
            case soci::db_uint8:
                obj[name] = static_cast<int64_t>(row.get<uint8_t>(i));
                break;
            case soci::db_int16:
                obj[name] = static_cast<int64_t>(row.get<int16_t>(i));
                break;
            case soci::db_uint16:
                obj[name] = static_cast<int64_t>(row.get<uint16_t>(i));
                break;
            case soci::db_int32:
                obj[name] = static_cast<int64_t>(row.get<int32_t>(i));
                break;
            case soci::db_uint32:
                obj[name] = static_cast<int64_t>(row.get<uint32_t>(i));
                break;
            case soci::db_int64:
                obj[name] = static_cast<int64_t>(row.get<int64_t>(i));
                break;
            case soci::db_uint64:
                obj[name] = static_cast<int64_t>(row.get<uint64_t>(i));
                break;
            case soci::db_date: {
                std::tm t = row.get<std::tm>(i);
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
                obj[name] = std::string(buf);
                break;
            }
            default:
                obj[name] = row.get<std::string>(i);
                break;
        }
    }
    return obj;
}

class DbQueryWorker : public Napi::AsyncWorker {
public:
    DbQueryWorker(Napi::Env env, mb::Database* db,
                  std::string sql, soci::values binds, bool hasBinds,
                  Napi::Promise::Deferred deferred)
        : Napi::AsyncWorker(env)
        , m_db(db)
        , m_sql(std::move(sql))
        , m_binds(std::move(binds))
        , m_hasBinds(hasBinds)
        , m_deferred(deferred) {}

    void Execute() override {
        try {
            auto session = m_db->session();

            soci::row data_row;
            soci::statement st = m_hasBinds
                ? (session->prepare << m_sql, soci::use(m_binds), soci::into(data_row))
                : (session->prepare << m_sql, soci::into(data_row));
            st.execute();

            m_results = json::array();
            while (st.fetch()) {
                m_results.push_back(SociRowToJson(data_row));
            }
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::HandleScope scope(env);

        if (m_results.empty()) {
            m_deferred.Resolve(Napi::Array::New(env, 0));
        } else {
            m_deferred.Resolve(JsonToNapi(env, m_results));
        }
    }

    void OnError(const Napi::Error& error) override {
        m_deferred.Reject(error.Value());
    }

private:
    mb::Database* m_db;
    std::string m_sql;
    soci::values m_binds;
    bool m_hasBinds;
    Napi::Promise::Deferred m_deferred;
    json m_results;
};

Napi::Function DbWrap::GetClass(Napi::Env env) {
    auto func = DefineClass(env, "Database", {
        InstanceMethod<&DbWrap::Query>("query"),
        InstanceAccessor<&DbWrap::IsConnected>("connected"),
    });

    s_constructor = Napi::Persistent(func);
    s_constructor.SuppressDestruct();

    return func;
}

DbWrap::DbWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<DbWrap>(info) {

    if (info.Length() > 0 && info[0].IsExternal()) {
        m_db = info[0].As<Napi::External<mb::Database>>().Data();
    }
}

Napi::Object DbWrap::NewInstance(Napi::Env env, mb::Database* db) {
    return s_constructor.New({Napi::External<mb::Database>::New(env, db)});
}

Napi::Value DbWrap::Query(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (sql: string, ...params: object)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string sql = info[0].As<Napi::String>().Utf8Value();

    soci::values binds;
    bool hasBinds = false;

    for (size_t i = 1; i < info.Length(); ++i) {
        if (!info[i].IsObject() || info[i].IsNull() || info[i].IsArray()) {
            Napi::TypeError::New(env, "Bind parameters must be plain objects")
                .ThrowAsJavaScriptException();
            return env.Undefined();
        }

        hasBinds = true;
        Napi::Object obj = info[i].As<Napi::Object>();
        auto names = obj.GetPropertyNames();

        for (uint32_t k = 0; k < names.Length(); ++k) {
            std::string key = names.Get(k).As<Napi::String>().Utf8Value();
            Napi::Value val = obj.Get(key);

            if (val.IsNull() || val.IsUndefined()) {
                std::optional<int> nullVal;
                binds.set(key, nullVal, soci::i_null);
            } else if (val.IsString()) {
                binds.set(key, val.As<Napi::String>().Utf8Value());
            } else if (val.IsNumber()) {
                double d = val.As<Napi::Number>().DoubleValue();
                if (d == static_cast<int64_t>(d) && d >= -9007199254740992.0 && d <= 9007199254740992.0) {
                    binds.set(key, static_cast<int>(d));
                } else {
                    binds.set(key, d);
                }
            } else if (val.IsBoolean()) {
                binds.set(key, val.As<Napi::Boolean>().Value());
            } else {
                json j = NapiToJson(val);
                binds.set(key, j);
            }
        }
    }

    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new DbQueryWorker(env, m_db, std::move(sql), std::move(binds), hasBinds, deferred);
    worker->Queue();

    return deferred.Promise();
}

Napi::Value DbWrap::IsConnected(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), m_db && m_db->isConnected());
}
