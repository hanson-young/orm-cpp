#ifndef PG_ORMLITE_HPP
#define PG_ORMLITE_HPP
#include <set>
#include <iostream>
#include <cstring>
#include "reflection.hpp"
#include "traits_utils.hpp"
#include "pg_query_object.hpp"

namespace pg_ormlite
{

struct key_map
{
    std::string fields;
};

struct auto_key_map
{
    std::string fields;
};

struct not_null_map
{
    std::set<std::string> fields;
};

template<typename... Args>
constexpr auto sort_tuple(const std::tuple<Args...>& t)
{
    if constexpr (sizeof...(Args) == 2)
    {
        auto [a, b] = t;
        if constexpr (!std::is_same_v<decltype(a), key_map> && !std::is_same_v<decltype(a), auto_key_map>)
        {
            return std::make_tuple(b, a);
        }
    }
    return t;
}


class pg_connection
{
private:
    template<typename Tuple1, typename Tuple2, size_t... Idx>
    std::string generate_connect_sql(const Tuple1& t1, const Tuple2& t2,std::index_sequence<Idx...>)
    {
        std::stringstream os;
        auto serialize = [&os](const std::string& key, const auto& val)
        {
            os << key << "="<< val<< " ";
        };
        
        int unused[] = {0, (serialize(std::get<Idx>(t1), std::get<Idx>(t2)), 0)...};
        (void) unused;
        return os.str();
    }

public:
    template<typename... Args> 
    pg_connection(Args&&... args)
    {
        auto args_tp = std::make_tuple(std::forward<Args>(args)...);
        auto index =  std::make_index_sequence<sizeof...(Args)>();
        constexpr size_t size = std::tuple_size<decltype(args_tp)>::value;
        std::string sql;
        if constexpr(size == 5)
        {
            auto fields = std::make_tuple("host", "port", "user", "password", "dbname");
            sql = generate_connect_sql(fields, args_tp, index);
        }
        if constexpr(size == 6)
        {
            auto fields = std::make_tuple("host", "port", "user", "password", "dbname", "connect_timeout");
            sql = generate_connect_sql(fields, args_tp, index);
        }
        
        std::cout<<"connect:"<<sql<<std::endl;
        conn_ = PQconnectdb(sql.data());
        if (PQstatus(conn_) != CONNECTION_OK)
        {
            std::cout<< PQerrorMessage(conn_) <<std::endl;
        }
    }

    template<typename T>
    bool prepare(const std::string& sql)
    {
        res_ = PQprepare(conn_, "", sql.data(), (int)reflection::get_value<T>(), nullptr);
        if (PQresultStatus(res_) != PGRES_COMMAND_OK)
        {
            std::cout<< PQerrorMessage(conn_) <<std::endl;
            return false;
        }
        PQclear(res_);
        return true;
    }

    template<typename T, typename... Args>
    bool create_table(Args&&... args)
    {
        std::string sql = generate_create_table_sql<T>(std::forward<Args>(args)...);
        std::cout<<"create:"<<sql<<std::endl;
        res_ = PQexec(conn_, sql.data());
        if (PQresultStatus(res_) != PGRES_COMMAND_OK)
        {
            std::cout<< PQerrorMessage(conn_) <<std::endl;
            return false;
        }
        PQclear(res_);
        return true;
    }

    template<typename T>
    constexpr auto generate_insert_sql(bool replace)
    {
        std::string sql = replace ? "replace into " : "insert into ";
        std::string table_name = reflection::get_name<T>().data();
        std::string field_name_pack = reflection::get_field<T>().data();
        sql += table_name + "(" + field_name_pack + ") values(";
        constexpr auto field_size = reflection::get_value<T>();
        for (size_t i = 0; i < field_size; i++)
        {
            sql += "$";
            sql += std::to_string(i + 1);
            if (i != field_size - 1)
            {
                sql += ", ";
            }
        }
        sql += ");";
        return sql;
    }

    template<typename T>
    constexpr void set_param_values(std::vector<std::vector<char>>& param_values, T&& value)
    {
        using U = std::remove_const_t<std::remove_reference_t<T>>;
        if constexpr(std::is_same_v<U, int64_t> || std::is_same_v<U, uint64_t>)  
        { 
            std::vector<char> temp(65, 0);
            auto v_str = std::to_string(value);
            memcpy(temp.data(), v_str.data(), v_str.size());
            param_values.push_back(temp); 
        }
        else if constexpr(std::is_integral_v<U> || std::is_floating_point_v<U>)
        {
            std::vector<char> temp(20, 0);
            auto v_str = std::to_string(value);
            memcpy(temp.data(), v_str.data(), v_str.size());
            param_values.push_back(std::move(temp)); 
        }
        else if constexpr(std::is_enum_v<U>)
        {
            std::vector<char> temp(20, 0);
            auto v_str = std::to_string(static_cast<std::underlying_type_t<U>>(value));
            memcpy(temp.data(), v_str.data(), v_str.size());
            param_values.push_back(std::move(temp)); 
        }
        else if constexpr(std::is_same_v<U, std::string>)
        {
            std::vector<char> temp = {};
            std::copy(value.data(), value.data() + value.size() + 1, std::back_inserter(temp));
            param_values.push_back(std::move(temp)); 
        }
        else if constexpr(std::is_array<U>::value)
        {
            std::vector<char> temp = {};
            std::copy(value, value + traits_utils::array_size<U>::value, std::back_inserter(temp));
            param_values.push_back(std::move(temp));
        }
    }

    template<typename T>
    bool insert_impl(std::string& sql, T&& t)
    {
        std::vector<std::vector<char>> param_values;
        reflection::for_each(t, [&](auto& item, auto field, auto j){
            set_param_values(param_values, t.*item);
        });
        if (param_values.empty())
        {
            return false;
        }
        std::vector<const char *> param_values_buf;
        for (auto &item : param_values) 
        {
            param_values_buf.push_back(item.data());
        }

        std::cout<<"params:";
        for (size_t i = 0; i < param_values_buf.size(); i++)
        {
            std::cout << i << "="<<*(param_values_buf.data() + i)<<", ";
        }
        std::cout<<std::endl;
        res_ = PQexecPrepared(conn_, "", (int)param_values.size(),
                            param_values_buf.data(), NULL, NULL, 0);

        if (PQresultStatus(res_) != PGRES_COMMAND_OK) 
        {
            std::cout << PQresultErrorMessage(res_) << std::endl;
            PQclear(res_);
            return false;
        }

        PQclear(res_);
        return true;
    }

    template<typename T>
    int insert(T&& t)
    {
        std::string sql = generate_insert_sql<T>(false);
        std::cout<<"insert prepare:"<<sql<<std::endl;
        if (!prepare<T>(sql))
            return false;
        return insert_impl(sql, std::forward<T>(t));
    }

    template<typename T>
    int insert(std::vector<T>& t)
    {
        std::string sql = generate_insert_sql<T>(false);
        std::cout<<"insert prepare:"<<sql<<std::endl;
        if (!prepare<T>(sql))
            return 0;

        for (auto& item : t)
        {
            if(!insert_impl(sql, item))
            {
                execute("rollback;");
                return 0;
            }
        }
        if (!execute("commit;"))
            return 0;
        return t.size();
    }

    template <typename T>
    constexpr auto get_type_names()
    {
        constexpr auto field_size = reflection::get_value<T>();
        std::array<std::string, field_size> field_types;
        reflection::for_each(T{}, [&](auto& item, auto field, auto j){
            constexpr auto Idx = decltype(j)::value;
            using U = std::remove_reference_t<decltype(reflection::get<Idx>(std::declval<T>()))>;
            if constexpr(std::is_same_v<U, bool> || 
                         std::is_same_v<U, int> || 
                         std::is_same_v<U, int32_t> || 
                         std::is_same_v<U, uint32_t> ||
                         std::is_enum_v<U>)
                { field_types[Idx] = "integer"; return; }
            if constexpr(std::is_same_v<U, int8_t> || std::is_same_v<U, uint8_t> || 
                         std::is_same_v<U, int16_t> || std::is_same_v<U, uint16_t>)  
                { field_types[Idx] = "smallint"; return; }
            if constexpr(std::is_same_v<U, int64_t> || std::is_same_v<U, uint64_t>)  
                { field_types[Idx] = "bigint"; return; }
            if constexpr(std::is_same_v<U, float>)  
                { field_types[Idx] = "real"; return; }
            if constexpr(std::is_same_v<U, double>)
                { field_types[Idx] = "double precision"; return; }
            if constexpr(std::is_same_v<U, std::string>)
                { field_types[Idx] = "text"; return; }
            if constexpr(std::is_array<U>::value)
                { field_types[Idx] = "varchar(" + std::to_string(traits_utils::array_size<U>::value) + ")"; return; }
        });
        return field_types;
    }
    template<typename T, typename... Args>
    std::string generate_create_table_sql(Args&&... args)
    {
        auto table_name = reflection::get_name<T>();
        std::string sql = std::string("create table if not exists ") + table_name.data() + "(";
        auto field_names = reflection::get_array<T>();
        auto field_types = get_type_names<T>();
        using TT = std::tuple<std::decay_t<Args>...>;
        if constexpr (sizeof...(Args) > 0)
        {
            
            static_assert(!(traits_utils::has_type<key_map, TT>::value && traits_utils::has_type<auto_key_map, TT>::value), 
                        "key_map and auto_key_map cannot be used together");

        }

        auto tp = sort_tuple(std::make_tuple(std::forward<Args>(args)...));
        constexpr auto field_size = reflection::get_value<T>();
        static_assert(field_size == field_names.size(), "field_size != field_names.size");
        for (size_t i = 0; i < field_size; i++)
        {
            std::string field_name = field_names[i].data();
            std::string field_type = field_types[i].data();
            bool has_add = false;
            reflection::for_each(
                tp,
                [&](auto item, auto j){
                    if constexpr (std::is_same_v<decltype(item), not_null_map>)
                    {
                        // 如果字段不存在在not_null_map里面，则直接返回，进行普通填充
                        if(item.fields.find(field_name) == item.fields.end())
                            return;
                    }
                    else
                    {
                        // 如果字段不是主键，则直接返回，进行普通填充
                        if(item.fields != field_name)
                            return;
                    }

                    if constexpr (std::is_same_v<decltype(item), auto_key_map>)
                    {
                        if(!has_add)
                        {
                            sql += field_name + " " + field_type + " serial primary key";
                            has_add = true;
                        }
                    }
                    else if constexpr (std::is_same_v<decltype(item), key_map>)
                    {
                        if(!has_add)
                        {
                            sql += field_name + " " + field_type + " primary key";
                            has_add = true;
                        }
                    }
                    else if constexpr (std::is_same_v<decltype(item), not_null_map>)
                    {
                        if(!has_add)
                        {
                            if(item.fields.find(field_name) == item.fields.end())
                                return;
                            sql += field_name + " " + field_type + " not null";
                            has_add = true;
                        }
                    }
                }
            );

            if(!has_add)
            {
                sql += field_name + " " + field_type;
                has_add = true;
            }

            if( i < field_size-1)
                sql += ", ";
        }
        sql += ");";
        return sql;
    }

    template<typename T>
    constexpr typename std::enable_if<reflection::is_reflection<T>::value, pg_query_object::query_object<T>>::type query()
    {
        return pg_query_object::query_object<T>(conn_, reflection::get_name<T>());
    }

    template<typename T>
    constexpr typename std::enable_if<reflection::is_reflection<T>::value, pg_query_object::query_object<T>>::type del()
    {
        return pg_query_object::query_object<T>(conn_, reflection::get_name<T>(), "delete", "");
    }

    template<typename T>
    constexpr typename std::enable_if<reflection::is_reflection<T>::value, pg_query_object::query_object<T>>::type update()
    {
        return pg_query_object::query_object<T>(conn_, reflection::get_name<T>(), "", "update");
    }

    ~pg_connection()
    {
        if(conn_ != nullptr)
        {
            std::cout<<"release pg conn"<<std::endl;
            PQfinish(conn_);
            conn_ = nullptr;
        } 
    }

    bool execute(const std::string& sql)
    {
        res_ = PQexec(conn_, sql.data());
        return PQresultStatus(res_) == PGRES_COMMAND_OK;
    }

private:
    PGresult *res_ = nullptr;
    PGconn *conn_ = nullptr;
};

}



#endif