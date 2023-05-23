#ifndef TRAITS_UTILS_HPP
#define TRAITS_UTILS_HPP
#include <type_traits>
#include <tuple>

namespace traits_utils
{

template<typename T, typename Tuple>
struct has_type{};

template<typename T, typename... Args>
struct has_type<T, std::tuple<Args...>>
: std::disjunction<std::is_same<T, Args>...>{};

template<typename T>
struct identity{};


template<typename T>
struct array_size;

template<typename T, std::size_t N>
struct array_size<std::array<T, N>>
{
    static constexpr size_t value = N;
};

template<std::size_t N>
struct array_size<char[N]>
{
    static constexpr std::size_t value = N;
};


}

#endif