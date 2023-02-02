#pragma once

#include <system_error>  //  std::system_error
#include <string>  //  std::string
#include <vector>  //  std::vector
#include <functional>  //  std::reference_wrapper
#include <system_error>

#include "error_code.h"
#include "serializer_context.h"
#include "select_constraints.h"
#include "alias.h"
#include "serializing_util.h"
#include "util.h"

namespace sqlite_orm {

    namespace internal {

        template<class T, class I>
        std::string serialize(const T& t, const serializer_context<I>& context);

        template<class T, class SFINAE = void>
        struct column_names_getter {
            using expression_type = T;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& t, const Ctx& context) const {
                auto newContext = context;
                newContext.skip_table_name = false;
                auto columnName = serialize(t, newContext);
                if(!columnName.empty()) {
                    return {std::move(columnName)};
                } else {
                    throw std::system_error{orm_error_code::column_not_found};
                }
            }
        };

        template<class T, class Ctx>
        std::vector<std::string> get_column_names(const T& t, const Ctx& context) {
            column_names_getter<T> serializer;
            return serializer(t, context);
        }

        template<class T, class Ctx>
        std::vector<std::string> collect_table_column_names(bool definedOrder, const Ctx& context) {
            if(definedOrder) {
                std::vector<std::string> quotedNames;
                auto& table = pick_table<mapped_type_proxy_t<T>>(context.db_objects);
                quotedNames.reserve(table.count_columns_amount());
                table.for_each_column([&quotedNames](const column_identifier& column) {
                    if(std::is_base_of<alias_tag, T>::value) {
                        quotedNames.push_back(quote_identifier(alias_extractor<T>::extract()) + "." +
                                              quote_identifier(column.name));
                    } else {
                        quotedNames.push_back(quote_identifier(column.name));
                    }
                });
                return quotedNames;
            } else if(std::is_base_of<alias_tag, T>::value) {
                return {quote_identifier(alias_extractor<T>::extract()) + ".*"};
            } else {
                return {"*"};
            }
        }

        template<class T>
        struct column_names_getter<std::reference_wrapper<T>, void> {
            using expression_type = std::reference_wrapper<T>;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& expression, const Ctx& context) const {
                return get_column_names(expression.get(), context);
            }
        };

        template<class T>
        struct column_names_getter<asterisk_t<T>, match_if_not<is_column_alias, T>> {
            using expression_type = asterisk_t<T>;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& expression, const Ctx& context) const {
                return collect_table_column_names<T>(expression.defined_order, context);
            }
        };

        template<class A>
        struct column_names_getter<asterisk_t<A>, match_if<is_column_alias, A>> {
            using expression_type = asterisk_t<A>;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& expression, const Ctx& context) const {
                return collect_table_column_names<A>(expression.defined_order, context);
            }
        };

        template<class T>
        struct column_names_getter<object_t<T>, void> {
            using expression_type = object_t<T>;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& expression, const Ctx& context) const {
                return collect_table_column_names<T>(expression.defined_order, context);
            }
        };

        template<class... Args>
        struct column_names_getter<columns_t<Args...>, void> {
            using expression_type = columns_t<Args...>;

            template<class Ctx>
            std::vector<std::string> operator()(const expression_type& cols, const Ctx& context) const {
                std::vector<std::string> columnNames;
                columnNames.reserve(static_cast<size_t>(cols.count));
                auto newContext = context;
                newContext.skip_table_name = false;
                iterate_tuple(cols.columns, [&columnNames, &newContext](auto& m) {
                    columnNames.push_back(serialize(m, newContext));
                });
                return columnNames;
            }
        };

    }
}
