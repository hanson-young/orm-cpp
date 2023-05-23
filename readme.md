
# 🌟 ORM-CPP: A Header-only Library for Modern C++17

ORM-CPP is a header-only library for modern C++17 that supports PostgreSQL CURD operations. It allows you to use LINQ syntax without the need to write any SQL queries.

## Features
- Header-only library
- Supports PostgreSQL database
- LINQ syntax for SQL queries
- No need to write raw SQL code
- Compile-time reflection can reduce runtime overhead.

## 🚀 Getting Started

### Installing

To use ORM-CPP, simply add the `*.hpp` header file to your project.Make sure you have installed the PostgreSQL client and server.  
You can use the command `g++ -o test test.cpp --std=c++17 -lpq` to compile the example program.

### Usage

Include the header file `pg_ormlite.hpp` and Create a struct and decorate the struct name and members with the REFLECTION_TEMPLATE macro.
```cpp
#include "pg_ormlite.hpp"
struct person {
    short id;
    char name[10];
    // std::string name;
    int age;
    float score;
}__attribute__((packed));
REFLECTION_TEMPLATE(person, id, name, age, score)
```
#### Connect
To use ORM-CPP, you need to first create a connection object to your PostgreSQL database, like so:

```cpp
pg_ormlite::pg_connection conn("xx.xx.xx.xx", "1234", "user", "password", "dbname");
```
#### Create
Once you have a connection object, you can create a table using the following statement
```cpp
pg_ormlite::key_map key_map_{"id"};
pg_ormlite::not_null_map not_null_map_;
not_null_map_.fields = {"id", "age"};
conn.create_table<person>(key_map_, not_null_map_);
// create table if not exists person(id smallint primary key, name varchar(10), age integer not null, score real);
```
#### Insert
You can use the insert method to insert a single person object or insert multiple objects in batches into the database table.
``` cpp
// insert single object one by one
person p1{1, "hxf1", 30, 101.1f};
person p2{2, "hxf2", 28, 102.2f};
person p3{3, "hxf3", 27, 103.3f};
person p4{4, "hxf4", 26, 104.4f};
person p5{5, "hxf1", 30, 108.1f};
person p6{6, "hxf3", 30, 109.1f};

conn.insert(p1);
// insert prepare:insert into person(id, name, age, score) values($1, $2, $3, $4);
conn.insert(p2);
conn.insert(p3);
conn.insert(p4);
conn.insert(p5);
conn.insert(p6);

// insert multiple objects in batches
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
```
#### Query 
Use ORM-CPP's LINQ syntax to query database. Directly return an array of structs.
``` cpp
auto pn1 = 
conn.query<person>()
    .where(FD(person::age) > 27 && FD(person::id) < 3)
    .limit(2)
    .to_vector();

for(auto it: pn1)
{
    std::cout<<it.id<<" "<<it.name<<" "<<it.age<<" "<<it.score<<std::endl;
}
// select * from person where (age > 27 and id < 3) limit 2;
// 1 hxf1 30 101.1
// 2 hxf2 28 102.2
```
If you only want to query certain fields, you can use the select method to filter them. In the end, it will return an array of `tuple` objects.

```cpp
auto pn2 = 
conn.query<person>()
    .select(RNT(person::id), RNT(person::name), RNT(person::age))
    .where(FD(person::age) >= 28 && FD(person::id) < 5)
    .to_vector();

for(auto it: pn2)
{
    std::apply([](auto&&... args) {
        ((std::cout << args << ' '), ...);
    }, it);
    std::cout<<std::endl;
}
// select (id), (name), (age) from person where (age >= 28 and id < 5);
// 1 hxf1 30 
// 2 hxf2 28 
```
To use the calculation engine of the database itself, you can use more complex operations such as group_by, order_by, etc. You can also add some aggregate functions to perform data statistics. The aggregate functions currently provided include count, sum, avg, max, and min.
```cpp
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
    std::apply([](auto&&... args) {
        ((std::cout << args << ' '), ...);
    }, it);
    std::cout<<std::endl;
}
// select (age), sum(score), count(name) from person where (age > 24 and id < 7) group by (age) order by age desc limit 3;
// 30 318.3 3 
// 28 102.2 1 
// 27 103.3 1 
```
#### Update 
The syntax for updating data is similar to that of querying data, you can do:

```cpp
auto res2 =
conn.update<person>()
    .set((FD(person::age) = 50) | (FD(person::name) = "hxf100"))
    .where(FD(person::age) > 29)
    .execute();
// update person set age = 50 , name = 'hxf100' where (age > 29);
```

#### Delete 
To delete data, you can use the del method.
```cpp
auto res = 
conn.del<person>()
    .where(FD(person::age) > 29)
    .execute();
// delete from person where (age > 29);
```

## 📖 Documentation

For more information on how to implement ORM-CPP, check out the [post](https://zhuanlan.zhihu.com/p/629445959).

## 🤝 Contributing

Contributions are welcome! If you find a bug or have a feature request, please open an issue on the [issue tracker](https://github.com/JohnDoe/orm-cpp/issues). If you want to contribute code, please fork the repository and submit a pull request.

## 📃 License

ORM-CPP is licensed under the [MIT License](https://github.com/JohnDoe/orm-cpp/blob/master/LICENSE).
