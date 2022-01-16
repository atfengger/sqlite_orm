/**
 *  Examples partially from https://sqlite.org/lang_with.html
 */

#include <tuple>
#include <iostream>
#include <sqlite_orm/sqlite_orm.h>

using std::cout;
using std::endl;
using std::make_tuple;
#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
using std::nullopt;
#endif
using std::string;
using std::system_error;
using namespace sqlite_orm;

void all_integers_between(int from, int end) {
    auto storage = make_storage("");

    // variant 1, where-clause
    {
        //WITH RECURSIVE
        //    cnt(x) AS(VALUES(1) UNION ALL SELECT x + 1 FROM cnt WHERE x < 1000000)
        //    SELECT x FROM cnt;
        auto ast =
            with(cte<cte_1>("x")(
                     union_all(select(from), select(column<cte_1>(0_col) + c(1), where(column<cte_1>(0_col) < end)))),
                 select(column<cte_1>(0_col)));

        string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        cout << "Integer range (where-clause) [" << from << ", " << end
             << "]"
                "\n";
        for(int n: storage.execute(stmt)) {
            cout << n << ", ";
        }
        cout << endl;
    }

    // variant 2, limit-clause
    {
        //WITH RECURSIVE
        //    cnt(x) AS(
        //        SELECT 1
        //        UNION ALL
        //        SELECT x + 1 FROM cnt
        //        LIMIT 1000000
        //    )
        //    SELECT x FROM cnt;
        auto ast = with(cte<cte_1>("x")(union_all(select(from), select(column<cte_1>(0_col) + c(1), limit(end)))),
                        select(column<cte_1>(0_col)));

        string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        cout << "Integer range (limit-clause) [" << from << ", " << end
             << "]"
                "\n";
        for(int n: storage.execute(stmt)) {
            cout << n << ", ";
        }
        cout << endl;
        cout << endl;
    }
}

void supervisor_chain() {
#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
    //CREATE TABLE org(
    //    name TEXT PRIMARY KEY,
    //    boss TEXT REFERENCES org
    //) WITHOUT ROWID;
    //INSERT INTO org VALUES('Alice', NULL);
    //INSERT INTO org VALUES('Bob', 'Alice');
    //INSERT INTO org VALUES('Cindy', 'Alice');
    //INSERT INTO org VALUES('Dave', 'Bob');
    //INSERT INTO org VALUES('Emma', 'Bob');
    //INSERT INTO org VALUES('Fred', 'Cindy');
    //INSERT INTO org VALUES('Gail', 'Cindy');
    //CREATE TABLE org(
    //  name TEXT PRIMARY KEY,
    //  boss TEXT REFERENCES org,
    //  height INT,
    //  -- other content omitted
    //);
    struct Org {
        std::string name;
        std::optional<std::string> boss;
        double height;
    };

    auto storage = make_storage("",
                                make_table<Org>("org",
                                                make_column("name", &Org::name, primary_key()),
                                                make_column("boss", &Org::boss),
                                                foreign_key(&Org::boss).references(&Org::name)));
    storage.sync_schema();

    storage.replace<Org>({"Alice", nullopt});
    storage.replace<Org>({"Bob", "Alice"});
    storage.replace<Org>({"Cindy", "Alice"});
    storage.replace<Org>({"Dave", "Bob"});
    storage.replace<Org>({"Emma", "Bob"});
    storage.replace<Org>({"Fred", "Cindy"});
    storage.replace<Org>({"Gail", "Cindy"});

    // supervisor chain of Fred
    {
        //WITH RECURSIVE
        //    chain AS(
        //        SELECT * from org WHERE name = 'Fred'
        //        UNION ALL
        //        SELECT parent.* FROM org parent, chain
        //        WHERE parent.name = chain.boss
        //    )
        //    SELECT name FROM chain;
        auto ast = with(
            cte<cte_1>()(union_all(select(asterisk<Org>(), where(&Org::name == c("Fred"))),
                                   select(asterisk<alias_a<Org>>(),
                                          where(c(alias_column<alias_a<Org>>(&Org::name)) == column<cte_1>(1_col))))),
            select(column<cte_1>(0_col)));
        string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        auto results = storage.execute(stmt);
        cout << "Hierarchy chain of Fred:\n";
        for(const string& name: results) {
            cout << name << ", ";
        }
        cout << endl;
        cout << endl;
    }
#endif
}

void works_for_alice() {
#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
    //CREATE TABLE org(
    //  name TEXT PRIMARY KEY,
    //  boss TEXT REFERENCES org,
    //  height INT,
    //  -- other content omitted
    //);
    struct Org {
        std::string name;
        std::optional<std::string> boss;
        double height;
    };

    auto storage = make_storage("",
                                make_table<Org>("org",
                                                make_column("name", &Org::name, primary_key()),
                                                make_column("boss", &Org::boss),
                                                make_column("height", &Org::height),
                                                foreign_key(&Org::boss).references(&Org::name)));
    storage.sync_schema();

    storage.replace<Org>({"Alice", nullopt, 160.});
    storage.replace<Org>({"Bob", nullopt, 177.});
    storage.replace<Org>({"Dave", "Alice", 169.});
    storage.replace<Org>({"Cindy", "Dave", 165.});
    storage.replace<Org>({"Bar", "Bob", 159.});

    // average height of Alice's team
    {
        //WITH RECURSIVE
        //    works_for_alice(n) AS(
        //        VALUES('Alice')
    //        UNION
    //        SELECT name FROM org, works_for_alice
    //        WHERE org.boss = works_for_alice.n
        //    )
        //    SELECT avg(height) FROM org
        //    WHERE org.name IN works_for_alice;
        auto ast = with(
            cte<cte_1>("n")(union_(select("Alice"), select(&Org::name, where(c(&Org::boss) == column<cte_1>(0_col))))),
            select(avg(&Org::height), from<Org>(), where(in(&Org::name, select(column<cte_1>(0_col))))));

        //WITH cte_1("n")
    //    AS(
    //        SELECT 'Alice'
    //        UNION
    //        SELECT "org"."name" FROM 'cte_1', 'org'
    //        WHERE("org"."boss" = 'cte_1'."n")
    //    )
    //    SELECT AVG("org"."height")
    //    FROM 'org'
    //    WHERE(
    //        "name" IN(
    //            SELECT 'cte_1'."n" FROM 'cte_1'
    //        )
    //    )
    string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        auto results = storage.execute(stmt);
        cout << "Average height of Alice's team: " << results.at(0) << endl;
        cout << endl;
    }
#endif
}

void family_tree() {
#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
    //CREATE TABLE family(
    //    name TEXT PRIMARY KEY,
    //    mom TEXT REFERENCES family,
    //    dad TEXT REFERENCES family,
    //    born DATETIME,
    //    died DATETIME, --NULL if still alive
    //    -- other content
    //);
    struct Family {
        std::string name;
        std::optional<std::string> mom;
        std::optional<std::string> dad;
        time_t born;
        std::optional<time_t> died;
    };

    auto storage = make_storage("",
                                make_table<Family>("family",
                                                   make_column("name", &Family::name, primary_key()),
                                                   make_column("mom", &Family::mom),
                                                   make_column("dad", &Family::dad),
                                                   make_column("born", &Family::born),
                                                   make_column("died", &Family::died),
                                                   foreign_key(&Family::mom).references(&Family::name),
                                                   foreign_key(&Family::dad).references(&Family::name)));
    storage.sync_schema();

    storage.replace<Family>({"Grandma (Mom)", nullopt, nullopt, 0, nullopt});
    storage.replace<Family>({"Granddad (Mom)", nullopt, nullopt, 1, 1});
    storage.replace<Family>({"Grandma (Dad)", nullopt, nullopt, 0, 0});
    storage.replace<Family>({"Granddad (Dad)", nullopt, nullopt, 1, nullopt});
    storage.replace<Family>({"Mom", "Grandma (Mom)", "Granddad (Mom)", 2, nullopt});
    storage.replace<Family>({"Dad", "Grandma (Dad)", "Granddad (Dad)", 3, nullopt});
    storage.replace<Family>({"Alice", "Mom", "Dad", 4, nullopt});

    //WITH RECURSIVE
    //    parent_of(name, parent) AS
    //    (SELECT name, mom FROM family UNION SELECT name, dad FROM family),
    //    ancestor_of_alice(name) AS
    //    (SELECT parent FROM parent_of WHERE name = 'Alice'
    //        UNION ALL
    //        SELECT parent FROM parent_of JOIN ancestor_of_alice USING(name))
    //    SELECT family.name FROM ancestor_of_alice, family
    //    WHERE ancestor_of_alice.name = family.name
    //    AND died IS NULL
    //    ORDER BY born;
    auto ast = with(make_tuple(cte<cte_1>("name", "parent")(union_(select(columns(&Family::name, &Family::mom)),
                                                                   select(columns(&Family::name, &Family::dad)))),
                               cte<cte_2>("name")(
                                   union_all(select(column<cte_1>(1_col), where(column<cte_1>(0_col) == "Alice")),
                                             select(column<cte_1>(1_col), join<cte_2>(using_(column<cte_1>(0_col))))))),
                    select(&Family::name,
                           where(is_equal(column<cte_2>(0_col), &Family::name) && is_null(&Family::died)),
                           order_by(&Family::born)));

    //WITH cte_1("name", "parent")
    //    AS(
    //        SELECT "family"."name", "family"."mom"
    //        FROM 'family'
    //        UNION
    //        SELECT "family"."name", "family"."dad"
    //        FROM 'family'
    //    ),
    //    cte_2("name")
    //    AS(
    //        SELECT 'cte_1'."parent"
    //        FROM 'cte_1'
    //        WHERE('cte_1'."name" = 'Alice')
    //        UNION ALL
    //        SELECT 'cte_1'."name"
    //        FROM 'cte_1'
    //        JOIN 'cte_2' USING("name")
    //    )
    //    SELECT "family"."name"
    //    FROM 'cte_2',
    //    'family'
    //    WHERE(
    //        (
    //            ('cte_2'."name" = 'family'."name")
    //            AND "family"."died" IS NULL
    //            )
    //    )
    //    ORDER BY "family"."born"
    string sql = storage.dump(ast);

    auto stmt = storage.prepare(ast);
    auto results = storage.execute(stmt);

    cout << "Living ancestor's of Alice:\n";
    for(const string& name: results) {
        cout << name << endl;
    }
    cout << endl;
#endif
}

void depth_or_breadth_first() {
#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
    //CREATE TABLE org(
    //    name TEXT PRIMARY KEY,
    //    boss TEXT REFERENCES org
    //) WITHOUT ROWID;
    //INSERT INTO org VALUES('Alice', NULL);
    //INSERT INTO org VALUES('Bob', 'Alice');
    //INSERT INTO org VALUES('Cindy', 'Alice');
    //INSERT INTO org VALUES('Dave', 'Bob');
    //INSERT INTO org VALUES('Emma', 'Bob');
    //INSERT INTO org VALUES('Fred', 'Cindy');
    //INSERT INTO org VALUES('Gail', 'Cindy');
    //CREATE TABLE org(
    //  name TEXT PRIMARY KEY,
    //  boss TEXT REFERENCES org,
    //  height INT,
    //  -- other content omitted
    //);
    struct Org {
        std::string name;
        std::optional<std::string> boss;
        double height;
    };

    auto storage = make_storage("",
                                make_table<Org>("org",
                                                make_column("name", &Org::name, primary_key()),
                                                make_column("boss", &Org::boss),
                                                foreign_key(&Org::boss).references(&Org::name)));
    storage.sync_schema();

    storage.replace<Org>({"Alice", nullopt});
    storage.replace<Org>({"Bob", "Alice"});
    storage.replace<Org>({"Cindy", "Alice"});
    storage.replace<Org>({"Dave", "Bob"});
    storage.replace<Org>({"Emma", "Bob"});
    storage.replace<Org>({"Fred", "Cindy"});
    storage.replace<Org>({"Gail", "Cindy"});

    // breadth-first pattern
    {
        //WITH RECURSIVE
        //    under_alice(name, level) AS(
        //        VALUES('Alice', 0)
        //        UNION ALL
        //        SELECT org.name, under_alice.level + 1
        //        FROM org JOIN under_alice ON org.boss = under_alice.name
        //        ORDER BY 2
        //    )
        //    SELECT substr('..........', 1, level * 3) || name FROM under_alice;
        auto ast =
            with(cte<cte_1>("name", "level")(union_all(select(columns("Alice", 0)),
                                                       select(columns(&Org::name, column<cte_1>(1_col) + c(1)),
                                                              join<cte_1>(on(c(&Org::boss) == column<cte_1>(0_col))),
                                                              order_by(2_nth_col)))),
                 select(substr("..........", 1, column<cte_1>(1_col) * c(3)) || c(column<cte_1>(0_col))));

        string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        auto results = storage.execute(stmt);

        cout << "List of organization members, breadth-first:\n";
        for(const string& name: results) {
            cout << name << endl;
        }
    }

    // depth-first pattern
    {
        //WITH RECURSIVE
        //    under_alice(name, level) AS(
        //        VALUES('Alice', 0)
        //        UNION ALL
        //        SELECT org.name, under_alice.level + 1
        //        FROM org JOIN under_alice ON org.boss = under_alice.name
        //        ORDER BY 2
        //    )
        //    SELECT substr('..........', 1, level * 3) || name FROM under_alice;
        auto ast =
            with(cte<cte_1>("name", "level")(union_all(select(columns("Alice", 0)),
                                                       select(columns(&Org::name, column<cte_1>(1_col) + c(1)),
                                                              join<cte_1>(on(c(&Org::boss) == column<cte_1>(0_col))),
                                                              order_by(2_nth_col).desc()))),
                 select(substr("..........", 1, column<cte_1>(1_col) * c(3)) || c(column<cte_1>(0_col))));

        string sql = storage.dump(ast);

        auto stmt = storage.prepare(ast);
        auto results = storage.execute(stmt);

        cout << "List of organization members, depth-first:\n";
        for(const string& name: results) {
            cout << name << endl;
        }
        cout << endl;
    }
#endif
}

int main() {
    try {
        all_integers_between(1, 10);
        supervisor_chain();
        works_for_alice();
        family_tree();
        depth_or_breadth_first();
    } catch(const system_error& e) {
        cout << "[" << e.code() << "] " << e.what();
    }

    return 0;
}
