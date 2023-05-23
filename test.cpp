// g++ -o test test.cpp --std=c++17 -lpq

#include <type_traits>
#include <string>
#include <iostream>
#include <tuple>
#include <vector>
#include <sstream>
#include <cstring>
#include "pg_ormlite.hpp"

struct person {
    short id;
    char name[10];
    // std::string name;
    int age;
    float score;
}__attribute__((packed));
REFLECTION_TEMPLATE(person, id, name, age, score)

int main() {
    // 连接数据库
    pg_ormlite::pg_connection conn("xx.xx.xx.xx", "1234", "user", "password", "dbname");
    // 删除表
    conn.execute("drop table person;");
    // 创建表
    pg_ormlite::key_map key_map_{"id"};
    pg_ormlite::not_null_map not_null_map_;
    not_null_map_.fields = {"id", "age"};
    // create table if not exists person(id smallint primary key, name varchar(10), age integer not null, score real);
    conn.create_table<person>(key_map_, not_null_map_);

    // 插入数据
    person p1{1, "hxf1", 30, 101.1f};
    person p2{2, "hxf2", 28, 102.2f};
    person p3{3, "hxf3", 27, 103.3f};
    person p4{4, "hxf4", 26, 104.4f};
    person p5{5, "hxf1", 30, 108.1f};
    person p6{6, "hxf3", 30, 109.1f};

    conn.insert(p1);
    conn.insert(p2);
    conn.insert(p3);
    conn.insert(p4);
    conn.insert(p5);
    conn.insert(p6);

    std::vector<person> persons;
    for (size_t i = 6; i < 10; i++)
    {
        person p;
        p.id = i + 1;
        std::string name = "hxf" + std::to_string(i + 1);
        strcpy(p.name, name.c_str());
        p.age = 30 + i;
        p.score = 101.1f + i;
        persons.push_back(p);
    }
    conn.insert(persons);
    
    //查询数据 效果1 直接返回json结构体
    auto pn1 = 
    conn.query<person>()
        .where(FD(person::age) > 27 && FD(person::id) < 3)
        .limit(2)
        .to_vector();

    for(auto it: pn1)
    {
        // select * from person where (age > 27 and id < 3) limit 2;
        // 1 hxf1 30 101.1
        // 2 hxf2 28 102.2
        std::cout<<it.id<<" "<<it.name<<" "<<it.age<<" "<<it.score<<std::endl;
    }

    //查询数据 效果2 返回tuple对象
    auto pn2 = 
    conn.query<person>()
        .select(RNT(person::id), RNT(person::name), RNT(person::age))
        .where(FD(person::age) >= 28 && FD(person::id) < 5)
        .to_vector();

    for(auto it: pn2)
    {
        // select (id), (name), (age) from person where (age >= 28 and id < 5);
        // 1 hxf1 30 
        // 2 hxf2 28 
        std::apply([](auto&&... args) {
            ((std::cout << args << ' '), ...);
        }, it);
        std::cout<<std::endl;
    }

    //查询数据 效果3 返回tuple对象，使用了group_by等操作
    auto pn3 = 
    conn.query<person>()
        .select(RNT(person::age), ORM_SUM(person::score), ORM_COUNT(person::name))
        .where(FD(person::age) > 24 && FD(person::id) < 7)
        .limit(3)
        .group_by(FD(person::age))
        .order_by_desc(FD(person::age))
        .to_vector();

    for(auto it: pn3)
    {
        // select (age), sum(score), count(name) from person where (age > 24 and id < 7) group by (age) order by age desc limit 3;
        // 30 318.3 3 
        // 28 102.2 1 
        // 27 103.3 1 
        std::apply([](auto&&... args) {
            ((std::cout << args << ' '), ...);
        }, it);
        std::cout<<std::endl;
    }

    // 修改数据
    auto res2 =
    conn.update<person>()
        .set((FD(person::age) = 50) | (FD(person::name) = "hxf100"))
        .where(FD(person::age) > 29)
        .execute();

    //查询全部数据 效果4
    auto pn4 = 
    conn.query<person>()
        .to_vector();

    for(auto it: pn4)
    {
        // select * from person;
        // 2 hxf2 28 102.2
        // 3 hxf3 27 103.3
        // 4 hxf4 26 104.4
        // 1 hxf100 50 101.1
        // 5 hxf100 50 108.1
        // 6 hxf100 50 109.1
        // 7 hxf100 50 107.1
        // 8 hxf100 50 108.1
        // 9 hxf100 50 109.1
        // 10 hxf100 50 110.1
        std::cout<<it.id<<" "<<it.name<<" "<<it.age<<" "<<it.score<<std::endl;
    }

    // 删除数据
    // delete from person where (age > 29);
    auto res = 
    conn.del<person>()
        .where(FD(person::age) > 29)
        .execute();
    return 0;
}
