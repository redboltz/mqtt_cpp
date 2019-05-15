#ifndef MQTT_TEST_CHECKER_HPP
#define MQTT_TEST_CHECKER_HPP

#include <boost/test/unit_test.hpp>

#include <vector>
#include <string>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/variant.hpp>

#define MQTT_CHK(...)  { BOOST_TEST(chk(__VA_ARGS__)); }

namespace mi = boost::multi_index;

struct null_stream : public std::ostream {
public:
    null_stream() : std::ostream(&sb_) {}
private:
    struct null_buf : public std::streambuf {
        int overflow(int c) {
            return c;
        }
    };
    null_buf sb_;
};

struct checker {
    struct cond_var;
    struct cond;
    struct op_and;
    struct op_or;
    struct op_not;
    struct lt_true;
    struct lt_cont;

    struct entry {
        entry(std::string self, cond_var&& cv)
            :self(std::move(self)), cv(std::make_shared<cond_var>(std::move(cv))) {}
        std::string self;
        std::shared_ptr<cond_var> cv;
        mutable bool passed = false;
    };

    using mi_entries = mi::multi_index_container<
        entry,
        mi::indexed_by<
            mi::ordered_unique<
                mi::member<
                    entry,
                    std::string,
                    &entry::self
                >
            >
        >
    >;

    struct eval_visitor {
        eval_visitor(mi_entries const& me, std::ostream& os):me(me), os(os) {}
        template <typename T>
        bool operator()(T const& v) const {
            return v(me, os);
        }
        mi_entries const& me;
        std::ostream& os;
    };

    struct cond {
        cond(std::string&& s):val(std::move(s)) {}
        bool operator()(mi_entries const& es, std::ostream& os) const {
            auto it = es.find(val);
            if (it == es.end()) {
                os << val << " is not found" << std::endl;
            }
            else if (it->passed) {
                os << val << " has been passed" << std::endl;
            }
            else {
                os << val << " has not been passed" << std::endl;
            }
            return it->passed;
        }
        std::string val;
    };

    struct cond_var : boost::variant<
        cond,
        boost::recursive_wrapper<op_and>,
        boost::recursive_wrapper<op_or>,
        boost::recursive_wrapper<op_not>,
        boost::recursive_wrapper<lt_true>,
        boost::recursive_wrapper<lt_cont>
    > {
        // inheriting constructor
        using boost::variant<
            cond,
            boost::recursive_wrapper<op_and>,
            boost::recursive_wrapper<op_or>,
            boost::recursive_wrapper<op_not>,
            boost::recursive_wrapper<lt_true>,
            boost::recursive_wrapper<lt_cont>
        >::variant;

        bool operator()(mi_entries const& es, std::ostream& os) const {
            return boost::apply_visitor(eval_visitor(es, os), *this);
        }
    };


    struct op_and {
        op_and(cond_var&& lhs, cond_var&& rhs)
            :lhs(std::move(lhs)), rhs(std::move(rhs)) {}
        bool operator()(mi_entries const& es, std::ostream& os) const {
            auto l = boost::apply_visitor(eval_visitor(es, os), lhs);
            auto r = boost::apply_visitor(eval_visitor(es, os), rhs);
            return l && r;
        }
        cond_var lhs;
        cond_var rhs;
    };

    struct op_or {
        op_or(cond_var&& lhs, cond_var&& rhs)
            :lhs(std::move(lhs)), rhs(std::move(rhs)) {}
        bool operator()(mi_entries const& es, std::ostream& os) const {
            auto l = boost::apply_visitor(eval_visitor(es, os), lhs);
            auto r = boost::apply_visitor(eval_visitor(es, os), rhs);
            return l || r;
        }
        cond_var lhs;
        cond_var rhs;
    };

    struct op_not {
        op_not(cond_var&& exp)
            :exp(std::move(exp)) {}
        bool operator()(mi_entries const& es, std::ostream& os) const {
            return !boost::apply_visitor(eval_visitor(es, os), exp);
        }
        cond_var exp;
    };

    struct lt_true {
        bool operator()(mi_entries const&, std::ostream&) const {
            return true;
        }
    };

    struct lt_cont {
        bool operator()(mi_entries const&, std::ostream&) const {
            return true;
        }
    };

    checker(std::initializer_list<entry>&& entries) {
        std::string prev;
        for (auto&& e : entries) {
            if (boost::get<lt_cont>(e.cv.get())) {
                if (prev.empty()) {
                    entries_.insert(entry(std::move(e.self), lt_true()));
                }
                else {
                    entries_.insert(entry(std::move(e.self), cond(std::move(prev))));
                }
                prev = e.self;
            }
            else {
                prev = e.self;
                entries_.insert(std::move(e));
            }
        }
        create_graph();
        if (!validate_graph()) {
            throw std::runtime_error("loop detected in checker graph");
        }
    }

    // cannot use BOOST_TEST here
    ~checker() {
        if (!all_called_) {
            bool ret = true;
            for (auto const& e : entries_) {
                if (!e.passed) {
                    std::cout << e.self + " has not been passed" << std::endl;
                    ret = false;
                }
            }
            BOOST_ASSERT(ret);
        }
    }

    bool operator()(std::string&& s) {
        auto it = entries_.find(s);
        if (it == entries_.end()) {
            BOOST_ERROR(s + " is not found");
            return false;
        }

        if (it->passed) {
            BOOST_ERROR(s + " has already been passed");
            return false;
        }
        it->passed = true;

        std::stringstream ss;
        auto ret = (*it->cv)(entries_, ss);
        if (!ret) {
            BOOST_ERROR(s + "'s precondition is not satisfiled");
            BOOST_ERROR("--- begin detail report");
            BOOST_ERROR(ss.str());
            BOOST_ERROR("--- end   detail report");
        }
        return ret;
    }

    bool passed(std::string s) const {
        null_stream ns;
        return cond_var(cond(std::move(s)))(entries_, ns);
    }

    bool passed(cond const& cv) const {
        null_stream ns;
        return cv(entries_, ns);
    }

    template <typename... StrFuncs>
    bool match(StrFuncs&&... strfuncs) const {
        std::vector<std::tuple<std::string, std::function<void()>>> exps;
        make_exps(exps, std::forward<StrFuncs>(strfuncs)...);

        std::sort(
            exps.begin(),
            exps.end(),
            [&]
            (auto const& lhs, auto const& rhs) {
                auto const& ls = std::get<0>(lhs);
                auto const& rs = std::get<0>(rhs);
                auto lit = entries_.find(ls);
                auto rit = entries_.find(rs);
                if (lit == entries_.end()) {
                    BOOST_ERROR(ls + " not found");
                    if (rit == entries_.end()) {
                        // string comparison
                        BOOST_ERROR(rs + " not found");
                        return ls > rs;
                    }
                    else {
                        return false;
                    }
                }
                else {
                    if (rit == entries_.end()) {
                        BOOST_ERROR(rs + " not found");
                        return true;
                    }
                    else {
                        auto comp =
                            [&](auto const& ls, auto const& rs) {
                                auto lr = graph_.equal_range(ls);
                                for (; lr.first != lr.second; ++lr.first) {
                                    auto const& ds = lr.first->deps;
                                    auto dit = ds.find(rs);
                                    if (dit != ds.cend()) {
                                        // lhs can reach to rhs
                                        return true;
                                    }
                                }
                                return false;
                            };
                        bool bl = comp(ls, rs);
                        bool br = comp(rs, ls);
                        if (bl && br) {
                            BOOST_ERROR(ls + " and " + rs + " depend on each other");
                        }
                        else if (!bl && !br) {
                            BOOST_ERROR(ls + " and " + rs + " don't have dependency");
                        }
                        return bl;
                    }
                }
            }
        );

        for (auto const& e : exps) {
            auto const& s = std::get<0>(e);
            auto const& f = std::get<1>(e);
            auto it = entries_.find(s);
            if (it != entries_.end() && it->passed) {
                f();
                return true;
            }
        }
        return false;

    }

    bool all() {
        bool ret = true;
        for (auto const& e : entries_) {
            if (!e.passed) {
                BOOST_ERROR(e.self + " has not been passed");
                ret = false;
            }
        }
        all_called_ = true;
        return ret;
    }

private:
    void make_exps(
        std::vector<std::tuple<std::string, std::function<void()>>>& exps,
        std::string s,
        std::function<void()> f) const {
        exps.emplace_back(std::move(s), std::move(f));
    }

    template <typename... StrFuncs>
    void make_exps(
        std::vector<std::tuple<std::string, std::function<void()>>>& exps,
        std::string s,
        std::function<void()> f,
        StrFuncs&&... strfuncs) const {
        exps.emplace_back(std::move(s), std::move(f));
        make_exps(exps, std::forward<StrFuncs>(strfuncs)...);
    }

    struct deps_node {
        deps_node(std::string self, std::set<std::string> deps)
            :self(self), deps(deps) {}
        std::string self;
        std::set<std::string> deps;
    };
    using mi_deps_graph = mi::multi_index_container<
        deps_node,
        mi::indexed_by<
            mi::ordered_unique<
                mi::member<
                    deps_node,
                    std::string,
                    &deps_node::self
                >
            >
        >
    >;

    struct depend_visitor {
        depend_visitor(
            mi_entries const& es,
            std::set<std::string>& ds)
            :es(es),
             ds(ds) {}

        void operator()(cond const& v) const {
            ds.insert(v.val);
            auto it = es.find(v.val);
            if (it != es.end()) {
                boost::apply_visitor(depend_visitor(es, ds), *it->cv);
            }
        }

        void operator()(op_and const& v) const {
            boost::apply_visitor(depend_visitor(es, ds), v.lhs);
            boost::apply_visitor(depend_visitor(es, ds), v.rhs);
        }

        void operator()(op_or const& v) const {
            boost::apply_visitor(depend_visitor(es, ds), v.lhs);
            boost::apply_visitor(depend_visitor(es, ds), v.rhs);
        }

        void operator()(op_not const& v) const {
            boost::apply_visitor(depend_visitor(es, ds), v.exp);
        }

        void operator()(lt_true const&) const {
        }

        template <typename T>
        void operator()(T const&) const {
            throw std::runtime_error("invalid type");
        }

        mi_entries const& es;
        std::set<std::string>& ds;
    };


    void create_graph() {

        for (auto it = entries_.cbegin(), end = entries_.cend(); it != end; ++it) {
            std::set<std::string> ds;
            boost::apply_visitor(depend_visitor(entries_, ds), *it->cv);
            graph_.emplace(it->self, std::move(ds));
        }
    }

    bool validate_graph() const {
        for (auto it = entries_.cbegin(), end = entries_.cend(); it != end; ++it) {
            auto r = graph_.equal_range(it->self);
            for (; r.first != r.second; ++r.first) {
                auto const& ds = r.first->deps;
                auto dit = ds.find(it->self);
                if (dit != ds.end()) {
                    return false;
                }
            }
        }
        return true;
    }

    friend
    checker::cond_var operator&&(checker::cond_var&& lhs, checker::cond_var&& rhs) {
        return checker::op_and(std::move(lhs), std::move(rhs));
    }

    friend
    checker::cond_var operator||(checker::cond_var&& lhs, checker::cond_var&& rhs) {
        return checker::op_or(std::move(lhs), std::move(rhs));
    }

    friend
    checker::cond_var operator!(checker::cond_var&& exp) {
        return checker::op_not(std::move(exp));
    }

    friend
    checker::cond_var operator"" _c(char const* str, std::size_t length) {
        return checker::cond(std::string(str, length));
    }

private:
    mi_entries entries_;
    bool all_called_ = false;
    mi_deps_graph graph_;
};


inline checker::entry cont(std::string&& s) {
    return checker::entry(std::move(s), checker::lt_cont());
}

inline checker::entry deps(std::string&& s, checker::cond_var&& cv = checker::lt_true()) {
    return checker::entry(std::move(s), std::move(cv));
}

template <typename T>
inline auto deps_impl(T&& t) {
    return checker::cond(std::forward<T>(t));
}

template <typename T, typename... Ts>
inline auto deps_impl(T&& t, Ts&&... ts) {
    return checker::cond(std::forward<T>(t)) && deps_impl(std::forward<Ts>(ts)...);
}

template <typename... Ts>
inline checker::entry deps(std::string&& s, Ts&&... ts) {
    return checker::entry(std::move(s), deps_impl(std::forward<Ts>(ts)...));
}

#endif // MQTT_TEST_CHECKER_HPP
