// host-tests/calcgraph/test_calcgraph.cpp
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CalcExpr.h"
#include "CalcGraph.h"
#include "CalcTiPad.h"

namespace { int checks = 0; int failures = 0;
void check(bool ok, const char* what, int line) { ++checks; if (!ok) { ++failures; std::printf("FAIL line %d: %s\n", line, what); } }
#define CHECK(c) check((c), #c, __LINE__)
namespace e = calc::expr; namespace g = calc::graph;
double val(const char* text, const e::Context& c = e::Context{}) { return e::evaluate(text, c).value; }
e::Error err(const char* text, const e::Context& c = e::Context{}) { return e::evaluate(text, c).error; }
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
std::string fmt(double v) { char b[40]; e::format(v, b, sizeof b); return b; }
}  // namespace

void testArithmetic() {
    CHECK(near(val("1+2*3"), 7));
    CHECK(near(val("(1+2)*3"), 9));
    CHECK(near(val("2^3^2"), 512));   // right associative
    CHECK(near(val("-2^2"), -4));
    CHECK(near(val("(-2)^2"), 4));
    CHECK(near(val("10/4"), 2.5));
    CHECK(near(val("2(3)"), 6));
    CHECK(near(val("(1+1)(2+1)"), 6));
    CHECK(near(val("2pi"), 2 * 3.14159265358979323846));
    CHECK(near(val("5!"), 120));
    CHECK(near(val("1.5E3"), 1500));
    CHECK(near(val("2E-2"), 0.02));
    CHECK(near(val("(2+3"), 5));      // a missing final paren is allowed
    CHECK(near(val(" 1 + 2 "), 3));
}

void testErrors() {
    CHECK(err("1/0") == e::Error::DivideByZero);
    CHECK(err("sqrt(-1)") == e::Error::Domain);
    CHECK(err("ln(0)") == e::Error::Domain);
    CHECK(err("asin(2)") == e::Error::Domain);
    CHECK(err("(-8)^0.5") == e::Error::Domain);
    CHECK(err("0^-1") == e::Error::DivideByZero);
    CHECK(err("1+") == e::Error::Syntax);
    CHECK(err("") == e::Error::Syntax);
    CHECK(err("2)") == e::Error::Syntax);
    CHECK(err("foo") == e::Error::Syntax);
    CHECK(err("3.5!") == e::Error::Domain);
    CHECK(err("10^400") == e::Error::Overflow);
    CHECK(std::strcmp(e::errorText(e::Error::Domain), "ERR: DOMAIN") == 0);
}

void testFunctionsAndAngles() {
    CHECK(near(val("sin(0)"), 0));
    CHECK(near(val("cos(0)"), 1));
    CHECK(near(val("ln(e)"), 1));
    CHECK(near(val("log(1000)"), 3));
    CHECK(near(val("sqrt(16)"), 4));
    CHECK(near(val("abs(-3)"), 3));
    CHECK(near(val("floor(2.7)"), 2));
    CHECK(near(val("round(2.5)"), 3));
    CHECK(near(val("exp(0)"), 1));

    e::Context d;
    d.angle = e::Angle::Degrees;
    CHECK(near(val("sin(90)", d), 1));
    CHECK(near(val("cos(180)", d), -1));
    CHECK(near(val("asin(1)", d), 90));
    CHECK(err("tan(90)", d) == e::Error::Domain);
    CHECK(near(val("atan(1)"), 3.14159265358979323846 / 4));
}

void testVariablesAndRun() {
    e::Context c;
    auto s = e::run("2+3->A", c);
    CHECK(s.result.error == e::Error::None && s.storedIn == 'A' && near(c.variables[0], 5) && near(c.ans, 5));
    CHECK(near(val("A*2", c), 10));
    CHECK(near(val("Ans+1", c), 6));
    CHECK(near(val("2A", c), 10));

    s = e::run("1/0", c);
    CHECK(s.result.error == e::Error::DivideByZero && near(c.ans, 5)); // a failed line leaves Ans alone

    s = e::run("3->a", c);
    CHECK(s.result.error == e::Error::Syntax && s.storedIn == 0);

    c.variables['X' - 'A'] = 3;
    CHECK(near(val("X^2", c), 9));
}

void testFormat() {
    CHECK(fmt(0.1 + 0.2) == "0.3");
    CHECK(fmt(1.0 / 3) == "0.3333333333");
    CHECK(fmt(2) == "2");
    CHECK(fmt(-0.0) == "0");
    CHECK(fmt(1234567890) == "1234567890");
    CHECK(fmt(12345678901.0) == "1.23456789E10");
    CHECK(fmt(0.000012) == "0.000012");
    CHECK(fmt(0.000001) == "1E-6");
    CHECK(fmt(-2.5) == "-2.5");
}

void testGraph() {
    g::Window w = g::standard();
    CHECK(g::isValid(w));
    g::Window bad = w;
    bad.xMax = bad.xMin;
    CHECK(!g::isValid(bad));

    CHECK(g::rowFor(w, 10, 201) == 0);
    CHECK(g::rowFor(w, -10, 201) == 200);
    CHECK(g::rowFor(w, 0, 201) == 100);
    CHECK(g::columnFor(w, 0, 201) == 100);
    CHECK(near(g::xAt(w, 0, 201), -10) && near(g::xAt(w, 200, 201), 10));
    CHECK(g::rowFor(w, NAN, 201) == g::kOffscreen);
    CHECK(g::rowFor(w, 1e300, 201) < 0); // clamped, not overflowed

    e::Context c;
    auto rows = g::plot("X", c, w, 201, 201);
    CHECK(rows.size() == 201 && rows[0] == 200 && rows[200] == 0 && rows[100] == 100);

    rows = g::plot("sqrt(X)", c, w, 201, 201);
    CHECK(rows[0] == g::kOffscreen && rows[200] != g::kOffscreen);

    CHECK(g::shouldJoin(10, 12, 201));
    CHECK(!g::shouldJoin(g::kOffscreen, 12, 201));
    CHECK(!g::shouldJoin(0, 100000, 201));

    auto v = g::valueAt("X^2+1", c, 3);
    CHECK(v.error == e::Error::None && near(v.value, 10));

    g::Window z = g::zoomed(w, 0.25, 0, 0);
    CHECK(near(z.xMin, -2.5) && near(z.xMax, 2.5) && near(z.yMin, -2.5));

    g::Window f = g::zoomFit("X^2", c, w, 201);
    CHECK(f.yMin < 0.0 + 1e-9 && f.yMax > 100 && f.yMax < 110 && near(f.xMin, -10));
    g::Window same = g::zoomFit("1/0", c, w, 201);
    CHECK(near(same.yMax, w.yMax));
}

void testPad() {
    namespace ti = calc::ti;
    CHECK(ti::kRows == 8);
    const calc::Rect16 grid{0, 0, 480, 400};
    for (int i = 0; i < ti::kKeyCount; i++) {
        const calc::Rect16 r = ti::keyRect(grid, 6, i);
        CHECK(ti::keyAt(grid, 6, r.x + r.w / 2, r.y + r.h / 2) == i);
        CHECK(r.right() <= grid.right() && r.bottom() <= grid.bottom());
    }
    CHECK(ti::keyAt(grid, 6, -1, 10) == -1);
    const calc::Rect16 enter = ti::keyRect(grid, 6, ti::kKeyCount - 1);
    CHECK(enter.right() >= grid.right() - 6);

    ti::Entry entry;
    entry.insert("2");
    entry.insert("sin(");
    entry.insert("X");
    CHECK(entry.text == "2sin(X" && entry.cursor == 6);
    entry.left();
    entry.insert("3");
    CHECK(entry.text == "2sin(3X" && entry.cursor == 6);
    entry.backspace();
    CHECK(entry.text == "2sin(X");
    entry.backspace();
    CHECK(entry.text == "2X" && entry.cursor == 1);
    entry.right();
    entry.right();
    CHECK(entry.cursor == 2);
    entry.clear();
    entry.backspace();
    CHECK(entry.text.empty() && entry.cursor == 0);
}

int main() {
    testArithmetic();
    testErrors();
    testFunctionsAndAngles();
    testVariablesAndRun();
    testFormat();
    testGraph();
    testPad();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
