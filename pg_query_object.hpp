#ifndef PG_QUERY_OBJECT_HPP
#define PG_QUERY_OBJECT_HPP
#include <cassert>
#include <libpq-fe.h>
#include "reflection.hpp"

namespace pg_query_object
{

template<typename RNT_TYPE>
struct selectable
{
    selectable(std::string_view&& field, std::string_view&& tbl_name, std::string_view&& op) 
    : expr_(std::string(op) + "(" + std::string(field) + ")"), tbl_name_(tbl_name){};

    inline std::string to_string() const
    {
        return expr_;
    }

    inline std::string table_name() const
    {
        return tbl_name_;
    }

    RNT_TYPE return_type;

private:
    std::string expr_;
    std::string tbl_name_;
    
};

class expr
{
public:
    expr(std::string_view&& field, std::string_view&& tbl_name) : expr_(field), tbl_name_(tbl_name) {};

    template <typename T>
    expr make_expr(std::string&& op, T value)
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_array<U>::value || std::is_same_v<U, std::string> || std::is_same_v<U, const char*>)
            return expr (expr_ + " " + op + " " + "'" + value + "'", tbl_name_);
        else if constexpr (std::is_same_v<T, expr>)
            return expr (expr_ + " " + op + " " + value.expr_, tbl_name_);
        else
            return expr (expr_ + " " + op + " " + std::to_string(value), tbl_name_);
    }

    template <typename T>
    inline expr operator == (T value)
    {
        return expr(make_expr("=", value));
    }

    template <typename T>
    inline expr operator = (T value)
    {
        return expr(make_expr("=", value));
    }

    template <typename T>
    inline expr operator | (T value)
    {
        return expr(make_expr(",", value));
    }

    template <typename T>
    inline expr operator != (T value)
    {
        return expr(make_expr("!=", value));
    }

    template <typename T>
    inline expr operator < (T value)
    {
        return expr(make_expr("<", value));
    }

    template <typename T>
    inline expr operator > (T value)
    {
        return expr(make_expr(">", value));
    }

    template <typename T>
    inline expr operator <= (T value)
    {
        return expr(make_expr("<=", value));
    }

    template <typename T>
    inline expr operator >= (T value)
    {

        return expr(make_expr(">=", value));
    }

    inline expr operator % (std::string&& value)
    {
        return expr(make_expr("like", value));
    }

    inline expr operator ^ (std::string&& value)
    {
        return expr(make_expr("not like", value));
    }

    inline expr operator && (expr&& value)
    {
        return expr(make_expr("and", value));
    }

    inline expr operator || (expr&& value)
    {
        return expr(make_expr("or", value));
    }

    inline std::string to_string() const
    {
        return expr_;
    }

    inline std::string table_name() const
    {
        return tbl_name_;
    }

    inline std::string print() const
    {
        return tbl_name_ + ", "+ expr_;
    }

private:
    std::string expr_;
    std::string tbl_name_;
};

template <typename QueryResult>
class query_object
{
private:
    std::string select_sql_;
    std::string where_sql_;
    std::string group_by_sql_;
    std::string having_sql_;
    std::string order_by_sql_;
    std::string limit_sql_;
    std::string offset_sql_;
    std::string delete_sql_;
    std::string update_sql_;
    std::string set_sql_;

    std::string table_name_;
    QueryResult query_result_;
    PGconn* conn_;
    PGresult* res_;
    
public:

    query_object(PGconn* conn, std::string_view table_name) : conn_(conn), table_name_(table_name)
    {

    }

    query_object(PGconn* conn, std::string_view table_name, const std::string& delete_sql, const std::string& update_sql) 
    : conn_(conn), 
      table_name_(table_name), 
      delete_sql_(update_sql.empty() ? delete_sql + " from " + std::string(table_name): ""),
      update_sql_(delete_sql.empty() ? update_sql + " " + std::string(table_name): "")
    {

    }

    query_object(PGconn* conn, std::string_view table_name, QueryResult& query_result, 
                 const std::string& select_sql, const std::string& where_sql, const std::string& group_by_sql, 
                 const std::string& having_sql, const std::string& order_by_sql, const std::string& limit_sql, 
                 const std::string& offset_sql, const std::string& delete_sql, const std::string& update_sql, const std::string& set_sql) 
    : conn_(conn), 
      table_name_(table_name),
      query_result_(query_result),
      select_sql_(select_sql),
      where_sql_(where_sql),
      group_by_sql_(group_by_sql),
      having_sql_(having_sql),
      order_by_sql_(order_by_sql),
      limit_sql_(limit_sql),
      offset_sql_(offset_sql),
      delete_sql_(delete_sql),
      update_sql_(update_sql),
      set_sql_(set_sql)
    {

    }

    template<typename... Args>
    inline void select_impl(std::string& sql, Args&&... args)
    {
        constexpr auto size = sizeof...(Args);
        auto tp = std::make_tuple(std::forward<Args>(args)...);
        reflection::for_each(tp, [&](auto arg, auto i) {
            if constexpr (std::is_same_v<decltype(arg), selectable<decltype(arg.return_type)>>)
            {
                bool same_table = arg.table_name() == table_name_;
                assert(same_table);
                table_name_ = arg.table_name();
                sql += arg.to_string();
                if(i != size - 1)
                    sql += ", "; 
            }
        });
    }

    template<typename... Args>
    inline query_object<std::tuple<Args...>> new_query(std::tuple<Args...>&& query_result)
    {
        return query_object<std::tuple<Args...>>(conn_, table_name_, query_result,  
                                                select_sql_, where_sql_, group_by_sql_, 
                                                having_sql_, order_by_sql_, limit_sql_, 
                                                offset_sql_, delete_sql_, update_sql_, set_sql_);
    }

    
    template<typename... Args>
    inline auto select(Args&&... args)
    {
        std::string sql = "select ";
        if constexpr (sizeof...(Args) > 0)
            select_impl(sql, std::forward<Args>(args)...);
        else
            sql += " * ";
        sql += " from " + table_name_;
        (*this).select_sql_ = sql;
        return new_query(std::tuple<decltype(args.return_type)...>{});
    }

    inline query_object&& set(const expr& expression)
    {
        table_name_ = expression.table_name();
        (*this).set_sql_ = " set " + expression.to_string();
        return std::move(*this);
    }

    inline query_object&& where(const expr& expression)
    {
        table_name_ = expression.table_name();
        (*this).where_sql_ = " where (" + expression.to_string() + ")";
        return std::move(*this);
    }

    inline query_object&& group_by(const expr& expression)
    {
        (*this).group_by_sql_ = " group by (" + expression.to_string() + ")";
        return std::move(*this);
    }

    inline query_object&& having(const expr& expression)
    {
        (*this).having_sql_ = " having (" + expression.to_string() + ")";
        return std::move(*this);
    }

    inline query_object&& order_by(const expr& expression)
    {
        (*this).order_by_sql_ = " order by " + expression.to_string() + " asc";
        return std::move(*this);
    }

    inline query_object&& order_by_desc(const expr& expression)
    {
        (*this).order_by_sql_ = " order by " + expression.to_string() + " desc";
        return std::move(*this);
    }

    inline query_object&& limit(std::size_t n)
    {
        (*this).limit_sql_ = " limit " + std::to_string(n);
        return std::move(*this);
    }

    inline query_object&& offset(std::size_t n)
    {
        (*this).offset_sql_ = " offset " + std::to_string(n);
        return std::move(*this);
    }
    
    std::string to_string()
    {
        if (select_sql_.empty() && delete_sql_.empty() && update_sql_.empty())
        {
            select_sql_ = "select * from " + table_name_;
        }
        return update_sql_ + delete_sql_ + select_sql_ + set_sql_ + where_sql_ + group_by_sql_ + 
               having_sql_ + order_by_sql_ + limit_sql_ + offset_sql_ + ";";
    }

    template<typename T>
    constexpr void assign_value(T&& value, int row, int col)
    {
        using U = std::remove_const_t<std::remove_reference_t<T>>;
        if constexpr(std::is_integral<U>::value && !(std::is_same<U, int64_t>::value || std::is_same<U, uint64_t>::value))
        {
            // std::cout<<"smallint:"<<row<<","<<col<<std::endl;
            value = atoi(PQgetvalue(res_, row, col));
        }
        else if constexpr(std::is_floating_point<U>::value)
        {
            // std::cout<<"float:"<<row<<","<<col<<std::endl;
            value = atof(PQgetvalue(res_, row, col));
        }
        else if constexpr(std::is_same<U, int64_t>::value || std::is_same<U, uint64_t>::value)
        {
            // std::cout<<"bigint:"<<row<<","<<col<<std::endl;
            value = atoll(PQgetvalue(res_, row, col));
        }
        else if constexpr(std::is_same<U, std::string>::value)
        {
            // std::cout<<"string:"<<row<<","<<col<<std::endl;
            value = PQgetvalue(res_, row, col);
        }
        else if constexpr(std::is_array<U>::value && std::is_same<char, std::remove_pointer_t<std::decay_t<U>>>::value)
        {
            // std::cout<<"array:"<<row<<","<<col<<std::endl;
            auto ptr = PQgetvalue(res_, row, col);
            memcpy(value, ptr, sizeof(U));
        }
        else
        {
            std::cout<<"unsupported type:" << std::is_array<U>::value<<std::endl;
        }
    }

    template<typename T>
    constexpr std::enable_if_t<reflection::is_reflection<T>::value, std::vector<T>> query(const std::string& sql)
    {
        std::vector<T> ret_vector;
        std::cout<<"query:"<<sql<<std::endl;
        res_ = PQexec(conn_, sql.c_str());
        if (PQresultStatus(res_) != PGRES_TUPLES_OK) 
        {
            std::cout << PQresultErrorMessage(res_) << std::endl;
            PQclear(res_);
            return ret_vector;
        }
        // std::cout<<"num:"<<PQntuples(res_)<<std::endl;
        auto ntuples = PQntuples(res_);
        for (size_t i = 0; i < ntuples; i++)
        {
            T tp = {};
            reflection::for_each(tp, [this, &tp, &i](auto item, auto field, auto j){
                assign_value(tp.*item, i, (int)decltype(j)::value);
            });
            ret_vector.push_back(std::move(tp));
        }
        PQclear(res_);
        return ret_vector;
    }
    
    template<typename T>
    constexpr std::enable_if_t<!reflection::is_reflection<T>::value, std::vector<T>> query(const std::string& sql)
    {
        std::vector<T> ret_vector;
        std::cout<<"query:"<<sql<<std::endl;
        res_ = PQexec(conn_, sql.c_str());
        if (PQresultStatus(res_) != PGRES_TUPLES_OK) 
        {
            std::cout << PQresultErrorMessage(res_) << std::endl;
            PQclear(res_);
            return ret_vector;
        }
        // std::cout<<"num:"<<PQntuples(res_)<<std::endl;
        auto ntuples = PQntuples(res_);
        for (size_t i = 0; i < ntuples; i++)
        {
            T tp = {};
            int index = 0;
            reflection::for_each(tp, [this, &i, &index](auto& item, auto j){
                if constexpr(reflection::is_reflection_v<std::decay_t<decltype(item)>>)
                {
                    std::decay_t<decltype(item)> t = {};
                    reflection::for_each(t, [this, &index, &t](auto ele, auto field, auto j){
                        assign_value(t.*ele, j, index++);
                    });
                    item = std::move(t);
                }
                else
                {
                    assign_value(item, (int)i, index++);
                }
            });
            ret_vector.push_back(tp);
        }
        PQclear(res_);
        return ret_vector;
    }

    std::vector<QueryResult> to_vector()
    {
        return query<QueryResult>(to_string());
    }

    bool execute()
    {
        auto sql = to_string();
        std::cout<<"exec:"<<sql<<std::endl;
        res_ = PQexec(conn_, sql.data());
        return PQresultStatus(res_) == PGRES_COMMAND_OK;
        // return true;
    }


};



template <typename T>
struct field_attribute;

template <typename T, typename U>
struct field_attribute<U T::*> {
  using type = T;
  using return_type = U;
};

template <typename U>
constexpr std::string_view get_field_name(std::string_view full_name) 
{
  using T = typename field_attribute<U>::type;
  return full_name.substr(reflection::get_name<T>().length() + 2,
                          full_name.length());
}

template <typename U>
constexpr std::string_view get_table_name(std::string_view full_name) 
{
  using T = typename field_attribute<U>::type;
  return reflection::get_name<T>();
}

}

#define FD(field)                                                                 \
pg_query_object::expr(                                                            \
    pg_query_object::get_field_name<decltype(&field)>(std::string_view(#field)),  \
    pg_query_object::get_table_name<decltype(&field)>(std::string_view(#field)))  \

#define ORM_AGG(field, op, type)                                                  \
pg_query_object::selectable<type>(                                                \
    pg_query_object::get_field_name<decltype(&field)>(std::string_view(#field)),  \
    pg_query_object::get_table_name<decltype(&field)>(std::string_view(#field)),  \
    op)                                                                           \

#define RNT(field) ORM_AGG(field, "", pg_query_object::field_attribute<decltype(&field)>::return_type)
#define ORM_COUNT(field) ORM_AGG(field, "count", std::size_t)
#define ORM_SUM(field) ORM_AGG(field, "sum", pg_query_object::field_attribute<decltype(&field)>::return_type)
#define ORM_AVG(field) ORM_AGG(field, "avg", pg_query_object::field_attribute<decltype(&field)>::return_type)
#define ORM_MAX(field) ORM_AGG(field, "max", pg_query_object::field_attribute<decltype(&field)>::return_type)
#define ORM_MIN(field) ORM_AGG(field, "min", pg_query_object::field_attribute<decltype(&field)>::return_type)


#endif