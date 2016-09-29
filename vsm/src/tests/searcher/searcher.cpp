// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/fastos/fastos.h>
#include <vespa/log/log.h>
LOG_SETUP("searcher_test");
#include <vespa/vespalib/testkit/testapp.h>

#include <vespa/document/fieldvalue/stringfieldvalue.h>
#include <vespa/searchlib/query/queryterm.h>
#include <vespa/vsm/searcher/fieldsearcher.h>
#include <vespa/vsm/searcher/floatfieldsearcher.h>
#include <vespa/vsm/searcher/futf8strchrfieldsearcher.h>
#include <vespa/vsm/searcher/intfieldsearcher.h>
#include <vespa/vsm/searcher/strchrfieldsearcher.h>
#include <vespa/vsm/searcher/utf8flexiblestringfieldsearcher.h>
#include <vespa/vsm/searcher/utf8exactstringfieldsearcher.h>
#include <vespa/vsm/searcher/utf8strchrfieldsearcher.h>
#include <vespa/vsm/searcher/utf8substringsearcher.h>
#include <vespa/vsm/searcher/utf8substringsnippetmodifier.h>
#include <vespa/vsm/searcher/utf8suffixstringfieldsearcher.h>
#include <vespa/vsm/vsm/snippetmodifier.h>
#include <vespa/vsm/vsm/fieldsearchspec.h>

using namespace document;
using search::EmptyQueryNodeResult;
using search::QueryTerm;
using search::QueryTermList;

namespace vsm {

template <typename T>
class Vector : public std::vector<T>
{
public:
    Vector<T>() : std::vector<T>() {}
    Vector<T> & add(T v) { this->push_back(v); return *this; }
};

typedef Vector<size_t>       Hits;
typedef Vector<std::string>  StringList;
typedef Vector<Hits>         HitsList;
typedef Vector<bool>         BoolList;
typedef Vector<int64_t>      LongList;
typedef Vector<float>        FloatList;
typedef QueryTerm::FieldInfo QTFieldInfo;
typedef Vector<QTFieldInfo>  FieldInfoList;

class String
{
private:
    const std::string & _str;
public:
    String(const std::string & str) : _str(str) {}
    bool operator==(const String & rhs) const {
        return _str == rhs._str;
    }
};

class Query
{
private:
    void setupQuery(const StringList & terms) {
        for (size_t i = 0; i < terms.size(); ++i) {
            ParsedQueryTerm pqt = parseQueryTerm(terms[i]);
            ParsedTerm pt = parseTerm(pqt.second);
            qtv.push_back(QueryTerm(eqnr, pt.first, pqt.first.empty() ? "index" : pqt.first, pt.second));
        }
        for (size_t i = 0; i < qtv.size(); ++i) {
            qtl.push_back(&qtv[i]);
        }
    }
public:
    typedef std::pair<std::string, std::string> ParsedQueryTerm;
    typedef std::pair<std::string, QueryTerm::SearchTerm> ParsedTerm;
    EmptyQueryNodeResult   eqnr;
    std::vector<QueryTerm> qtv;
    QueryTermList          qtl;
    Query(const StringList & terms) : eqnr(), qtv(), qtl() {
        setupQuery(terms);
    }
    static ParsedQueryTerm parseQueryTerm(const std::string & queryTerm) {
        size_t i = queryTerm.find(':');
        if (i != std::string::npos) {
            return ParsedQueryTerm(queryTerm.substr(0, i), queryTerm.substr(i + 1));
        }
        return ParsedQueryTerm(std::string(), queryTerm);
    }
    static ParsedTerm parseTerm(const std::string & term) {
        if (term[0] == '*' && term[term.size() - 1] == '*') {
            return std::make_pair(term.substr(1, term.size() - 2), QueryTerm::SUBSTRINGTERM);
        } else if (term[0] == '*') {
            return std::make_pair(term.substr(1, term.size() - 1), QueryTerm::SUFFIXTERM);
        } else if (term[term.size() - 1] == '*') {
            return std::make_pair(term.substr(0, term.size() - 1), QueryTerm::PREFIXTERM);
        } else {
            return std::make_pair(term, QueryTerm::WORD);
        }
    }
};

struct SnippetModifierSetup
{
    Query                            query;
    UTF8SubstringSnippetModifier::SP searcher;
    SharedSearcherBuf                buf;
    SnippetModifier                  modifier;
    explicit SnippetModifierSetup(const StringList & terms) :
        query(terms),
        searcher(new UTF8SubstringSnippetModifier()),
        buf(new SearcherBuf(8)),
        modifier(searcher)
    {
        searcher->prepare(query.qtl, buf);
    }
};

class SearcherTest : public vespalib::TestApp
{
private:

    // helper functions
    ArrayFieldValue getFieldValue(const StringList & fv);
    ArrayFieldValue getFieldValue(const LongList & fv);
    ArrayFieldValue getFieldValue(const FloatList & fv);

    bool assertMatchTermSuffix(const std::string & term, const std::string & word);

    /** string field searcher **/
    void assertString(StrChrFieldSearcher & fs, const std::string & term, const std::string & field, const Hits & exp) {
        assertString(fs, StringList().add(term), field, HitsList().add(exp));
    }
    void assertString(StrChrFieldSearcher & fs, const StringList & query, const std::string & field, const HitsList & exp) {
        assertSearch(fs, query, StringFieldValue(field), exp);
    }
    void assertString(StrChrFieldSearcher & fs, const std::string & term, const StringList & field, const Hits & exp) {
        assertString(fs, StringList().add(term), field, HitsList().add(exp));
    }
    void assertString(StrChrFieldSearcher & fs, const StringList & query, const StringList & field, const HitsList & exp) {
        assertSearch(fs, query, getFieldValue(field), exp);
    }

    /** int field searcher **/
    void assertInt(IntFieldSearcher fs, const std::string & term, int64_t field, bool exp) {
        assertInt(fs, StringList().add(term), field, BoolList().add(exp));
    }
    void assertInt(IntFieldSearcher fs, const StringList & query, int64_t field, const BoolList & exp) {
        assertNumeric(fs, query, LongFieldValue(field), exp);
    }
    void assertInt(IntFieldSearcher fs, const std::string & term, const LongList & field, const Hits & exp) {
        assertInt(fs, StringList().add(term), field, HitsList().add(exp));
    }
    void assertInt(IntFieldSearcher fs, const StringList & query, const LongList & field, const HitsList & exp) {
        assertSearch(fs, query, getFieldValue(field), exp);
    }

    /** float field searcher **/
    void assertFloat(FloatFieldSearcher fs, const std::string & term, float field, bool exp) {
        assertFloat(fs, StringList().add(term), field, BoolList().add(exp));
    }
    void assertFloat(FloatFieldSearcher fs, const StringList & query, float field, const BoolList & exp) {
        assertNumeric(fs, query, FloatFieldValue(field), exp);
    }
    void assertFloat(FloatFieldSearcher fs, const std::string & term, const FloatList & field, const Hits & exp) {
        assertFloat(fs, StringList().add(term), field, HitsList().add(exp));
    }
    void assertFloat(FloatFieldSearcher fs, const StringList & query, const FloatList & field, const HitsList & exp) {
        assertSearch(fs, query, getFieldValue(field), exp);
    }

    void assertNumeric(FieldSearcher & fs, const StringList & query, const FieldValue & fv, const BoolList & exp);
    std::vector<QueryTerm> performSearch(FieldSearcher & fs, const StringList & query, const FieldValue & fv);
    void assertSearch(FieldSearcher & fs, const StringList & query, const FieldValue & fv, const HitsList & exp);

    /** string field searcher **/
    bool assertFieldInfo(StrChrFieldSearcher & fs, const std::string & term, const std::string & fv, const QTFieldInfo & exp) {
        return assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    bool assertFieldInfo(StrChrFieldSearcher & fs, const std::string & term, const StringList & fv, const QTFieldInfo & exp) {
        return assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    bool assertFieldInfo(StrChrFieldSearcher & fs, const StringList & query, const std::string & fv, const FieldInfoList & exp) {
        return assertFieldInfo(fs, query, StringFieldValue(fv), exp);
    }
    bool assertFieldInfo(StrChrFieldSearcher & fs, const StringList & query, const StringList & fv, const FieldInfoList & exp) {
        return assertFieldInfo(fs, query, getFieldValue(fv), exp);
    }

    /** int field searcher **/
    void assertFieldInfo(IntFieldSearcher fs, const std::string & term, int64_t fv, const QTFieldInfo & exp) {
        assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    void assertFieldInfo(IntFieldSearcher fs, const std::string & term, const LongList & fv, const QTFieldInfo & exp) {
        assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    void assertFieldInfo(IntFieldSearcher fs, const StringList & query, int64_t fv, const FieldInfoList & exp) {
        assertFieldInfo(fs, query, LongFieldValue(fv), exp);
    }
    void assertFieldInfo(IntFieldSearcher fs, const StringList & query, const LongList & fv, const FieldInfoList & exp) {
        assertFieldInfo(fs, query, getFieldValue(fv), exp);
    }

    /** float field searcher **/
    void assertFieldInfo(FloatFieldSearcher fs, const std::string & term, float fv, const QTFieldInfo & exp) {
        assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    void assertFieldInfo(FloatFieldSearcher fs, const std::string & term, const FloatList & fv, const QTFieldInfo & exp) {
        assertFieldInfo(fs, StringList().add(term), fv, FieldInfoList().add(exp));
    }
    void assertFieldInfo(FloatFieldSearcher fs, const StringList & query, float fv, const FieldInfoList & exp) {
        assertFieldInfo(fs, query, FloatFieldValue(fv), exp);
    }
    void assertFieldInfo(FloatFieldSearcher fs, const StringList & query, const FloatList & fv, const FieldInfoList & exp) {
        assertFieldInfo(fs, query, getFieldValue(fv), exp);
    }

    bool assertFieldInfo(FieldSearcher & fs, const StringList & query, const FieldValue & fv, const FieldInfoList & exp);

    /** snippet modifer searcher **/
    void assertSnippetModifier(const std::string & term, const std::string & fv, const std::string & exp) {
        assertSnippetModifier(StringList().add(term), fv, exp);
    }
    void assertSnippetModifier(const StringList & query, const std::string & fv, const std::string & exp);
    /** snippet modifier **/
    void assertSnippetModifier(SnippetModifierSetup & setup, const FieldValue & fv, const std::string & exp);
    void assertQueryTerms(const SnippetModifierManager & man, FieldIdT fId, const StringList & terms);
    /** count words **/
    bool assertCountWords(size_t numWords, const std::string & field);

    // test functions
    void testParseTerm();
    void testMatchTermSuffix();
    bool testStrChrFieldSearcher(StrChrFieldSearcher & fs);
    void testStrChrFieldSearcher();
    bool testUTF8SubStringFieldSearcher(StrChrFieldSearcher & fs);
    void testUTF8SubStringFieldSearcher();
    void testUTF8SuffixStringFieldSearcher();
    void testUTF8FlexibleStringFieldSearcher();
    void testUTF8ExactStringFieldSearcher();
    void testIntFieldSearcher();
    void testFloatFieldSearcher();
    bool testStringFieldInfo(StrChrFieldSearcher & fs);
    void testSnippetModifierSearcher();
    void testSnippetModifier();
    void testFieldSearchSpec();
    void testSnippetModifierManager();
    void testStripIndexes();
    void requireThatCountWordsIsWorking();

public:
    int Main();
};

ArrayFieldValue
SearcherTest::getFieldValue(const StringList & fv)
{

    static ArrayDataType type(*DataType::STRING);
    ArrayFieldValue afv(type);
    for (size_t i = 0; i < fv.size(); ++i) {
        afv.add(StringFieldValue(fv[i]));
    }
    return afv;
}

ArrayFieldValue
SearcherTest::getFieldValue(const LongList & fv)
{
    static ArrayDataType type(*DataType::LONG);
    ArrayFieldValue afv(type);
    for (size_t i = 0; i < fv.size(); ++i) {
        afv.add(LongFieldValue(fv[i]));
    }
    return afv;
}

ArrayFieldValue
SearcherTest::getFieldValue(const FloatList & fv)
{
    static ArrayDataType type(*DataType::FLOAT);
    ArrayFieldValue afv(type);
    for (size_t i = 0; i < fv.size(); ++i) {
        afv.add(FloatFieldValue(fv[i]));
    }
    return afv;
}

bool
SearcherTest::assertMatchTermSuffix(const std::string & term, const std::string & word)
{
    EmptyQueryNodeResult eqnr;
    QueryTerm qa(eqnr, term, "index", QueryTerm::WORD);
    QueryTerm qb(eqnr, word, "index", QueryTerm::WORD);
    const ucs4_t * a;
    size_t alen = qa.term(a);
    const ucs4_t * b;
    size_t blen = qb.term(b);
    return UTF8StringFieldSearcherBase::matchTermSuffix(a, alen, b, blen);
}

void
SearcherTest::assertNumeric(FieldSearcher & fs, const StringList & query, const FieldValue & fv, const BoolList & exp)
{
    HitsList hl;
    for (size_t i = 0; i < exp.size(); ++i) {
        hl.push_back(exp[i] ? Hits().add(0) : Hits());
    }
    assertSearch(fs, query, fv, hl);
}

std::vector<QueryTerm>
SearcherTest::performSearch(FieldSearcher & fs, const StringList & query, const FieldValue & fv)
{
    Query q(query);

    // prepare field searcher
    SharedSearcherBuf ssb = SharedSearcherBuf(new SearcherBuf());
    fs.prepare(q.qtl, ssb);

    // setup document
    SharedFieldPathMap sfim(new FieldPathMapT());
    sfim->push_back(FieldPath());
    StorageDocument doc(sfim);
    doc.setFieldCount(1);
    doc.init();
    doc.setField(0, document::FieldValue::UP(fv.clone()));

    fs.search(doc);
    return q.qtv;
}

void
SearcherTest::assertSearch(FieldSearcher & fs, const StringList & query, const FieldValue & fv, const HitsList & exp)
{
    std::vector<QueryTerm> qtv = performSearch(fs, query, fv);
    EXPECT_EQUAL(qtv.size(), exp.size());
    ASSERT_TRUE(qtv.size() == exp.size());
    for (size_t i = 0; i < qtv.size(); ++i) {
        const search::HitList & hl = qtv[i].getHitList();
        EXPECT_EQUAL(hl.size(), exp[i].size());
        ASSERT_TRUE(hl.size() == exp[i].size());
        for (size_t j = 0; j < hl.size(); ++j) {
            EXPECT_EQUAL((size_t)hl[j].pos(), exp[i][j]);
        }
    }
}

bool
SearcherTest::assertFieldInfo(FieldSearcher & fs, const StringList & query,
                              const FieldValue & fv, const FieldInfoList & exp)
{
    std::vector<QueryTerm> qtv = performSearch(fs, query, fv);
    if (!EXPECT_EQUAL(qtv.size(), exp.size())) return false;
    bool retval = true;
    for (size_t i = 0; i < qtv.size(); ++i) {
        if (!EXPECT_EQUAL(qtv[i].getFieldInfo(0).getHitOffset(), exp[i].getHitOffset())) retval = false;
        if (!EXPECT_EQUAL(qtv[i].getFieldInfo(0).getHitCount(), exp[i].getHitCount())) retval = false;
        if (!EXPECT_EQUAL(qtv[i].getFieldInfo(0).getFieldLength(), exp[i].getFieldLength())) retval = false;
    }
    return retval;
}

void
SearcherTest::assertSnippetModifier(const StringList & query, const std::string & fv, const std::string & exp)
{
    UTF8SubstringSnippetModifier mod;
    performSearch(mod, query, StringFieldValue(fv));
    EXPECT_EQUAL(mod.getModifiedBuf().getPos(), exp.size());
    std::string actual(mod.getModifiedBuf().getBuffer(), mod.getModifiedBuf().getPos());
    EXPECT_EQUAL(actual.size(), exp.size());
    EXPECT_EQUAL(actual, exp);
}

void
SearcherTest::assertSnippetModifier(SnippetModifierSetup & setup, const FieldValue & fv, const std::string & exp)
{
    FieldValue::UP mfv = setup.modifier.modify(fv);
    const document::LiteralFieldValueB & lfv = static_cast<const document::LiteralFieldValueB &>(*mfv.get());
    const std::string & actual = lfv.getValue();
    EXPECT_EQUAL(actual.size(), exp.size());
    EXPECT_EQUAL(actual, exp);
}

void
SearcherTest::assertQueryTerms(const SnippetModifierManager & man, FieldIdT fId, const StringList & terms)
{
    if (terms.size() == 0) {
        ASSERT_TRUE(man.getModifiers().getModifier(fId) == NULL);
        return;
    }
    ASSERT_TRUE(man.getModifiers().getModifier(fId) != NULL);
    UTF8SubstringSnippetModifier * searcher =
        (static_cast<SnippetModifier *>(man.getModifiers().getModifier(fId)))->getSearcher().get();
    EXPECT_EQUAL(searcher->getQueryTerms().size(), terms.size());
    ASSERT_TRUE(searcher->getQueryTerms().size() == terms.size());
    for (size_t i = 0; i < terms.size(); ++i) {
        EXPECT_EQUAL(std::string(searcher->getQueryTerms()[i]->getTerm()), terms[i]);
    }
}

bool
SearcherTest::assertCountWords(size_t numWords, const std::string & field)
{
    FieldRef ref(field.c_str(), field.size());
    return EXPECT_EQUAL(numWords, FieldSearcher::countWords(ref));
}

void
SearcherTest::testParseTerm()
{
    ASSERT_TRUE(Query::parseQueryTerm("index:term").first == "index");
    ASSERT_TRUE(Query::parseQueryTerm("index:term").second == "term");
    ASSERT_TRUE(Query::parseQueryTerm("term").first == "");
    ASSERT_TRUE(Query::parseQueryTerm("term").second == "term");
    ASSERT_TRUE(Query::parseTerm("*substr*").first == "substr");
    ASSERT_TRUE(Query::parseTerm("*substr*").second == QueryTerm::SUBSTRINGTERM);
    ASSERT_TRUE(Query::parseTerm("*suffix").first == "suffix");
    ASSERT_TRUE(Query::parseTerm("*suffix").second == QueryTerm::SUFFIXTERM);
    ASSERT_TRUE(Query::parseTerm("prefix*").first == "prefix");
    ASSERT_TRUE(Query::parseTerm("prefix*").second == QueryTerm::PREFIXTERM);
    ASSERT_TRUE(Query::parseTerm("term").first == "term");
    ASSERT_TRUE(Query::parseTerm("term").second == QueryTerm::WORD);
}

void
SearcherTest::testMatchTermSuffix()
{
    EXPECT_EQUAL(assertMatchTermSuffix("a",      "vespa"), true);
    EXPECT_EQUAL(assertMatchTermSuffix("spa",    "vespa"), true);
    EXPECT_EQUAL(assertMatchTermSuffix("vespa",  "vespa"), true);
    EXPECT_EQUAL(assertMatchTermSuffix("vvespa", "vespa"), false);
    EXPECT_EQUAL(assertMatchTermSuffix("fspa",   "vespa"), false);
    EXPECT_EQUAL(assertMatchTermSuffix("v",      "vespa"), false);
}

bool
SearcherTest::testStrChrFieldSearcher(StrChrFieldSearcher & fs)
{
    std::string field = "operators and operator overloading with utf8 char oe = \xc3\x98";
    assertString(fs, "oper",  field, Hits());
    assertString(fs, "tor",   field, Hits());
    assertString(fs, "oper*", field, Hits().add(0).add(2));
    assertString(fs, "and",   field, Hits().add(1));

    assertString(fs, StringList().add("oper").add("tor"), field, HitsList().add(Hits()).add(Hits()));
    assertString(fs, StringList().add("and").add("overloading"), field, HitsList().add(Hits().add(1)).add(Hits().add(3)));

    fs.setMatchType(FieldSearcher::PREFIX);
    assertString(fs, "oper",  field, Hits().add(0).add(2));
    assertString(fs, StringList().add("oper").add("tor"), field, HitsList().add(Hits().add(0).add(2)).add(Hits()));

    fs.setMatchType(FieldSearcher::REGULAR);
    if (!EXPECT_TRUE(testStringFieldInfo(fs))) return false;

    { // test handling of several underscores
        StringList query = StringList().add("foo").add("bar");
        HitsList exp = HitsList().add(Hits().add(0)).add(Hits().add(1));
        assertString(fs, query, "foo_bar", exp);
        assertString(fs, query, "foo__bar", exp);
        assertString(fs, query, "foo___bar", exp);
        assertString(fs, query, "foo________bar", exp);
        assertString(fs, query, "foo____________________bar", exp);
        assertString(fs, query, "________________________________________foo________________________________________bar________________________________________", exp);
        query = StringList().add("foo").add("thisisaveryveryverylongword");
        assertString(fs, query, "foo____________________thisisaveryveryverylongword", exp);

        assertString(fs, "bar", "foo                    bar", Hits().add(1));
        assertString(fs, "bar", "foo____________________bar", Hits().add(1));
        assertString(fs, "bar", "foo____________________thisisaveryveryverylongword____________________bar", Hits().add(2));
    }
    return true;
}

void
SearcherTest::testStrChrFieldSearcher()
{
    {
        UTF8StrChrFieldSearcher fs(0);
        EXPECT_TRUE(testStrChrFieldSearcher(fs));
    }
    {
        FUTF8StrChrFieldSearcher fs(0);
        EXPECT_TRUE(testStrChrFieldSearcher(fs));
    }
}

bool
SearcherTest::testUTF8SubStringFieldSearcher(StrChrFieldSearcher & fs)
{
    std::string field = "operators and operator overloading";
    assertString(fs, "rsand", field, Hits());
    assertString(fs, "ove",   field, Hits().add(3));
    assertString(fs, "ing",   field, Hits().add(3));
    assertString(fs, "era",   field, Hits().add(0).add(2));
    assertString(fs, "a",     field, Hits().add(0).add(1).add(2).add(3));

    assertString(fs, StringList().add("dn").add("gn"), field, HitsList().add(Hits()).add(Hits()));
    assertString(fs, StringList().add("ato").add("load"), field, HitsList().add(Hits().add(0).add(2)).add(Hits().add(3)));

    assertString(fs, StringList().add("aa").add("ab"), "aaaab",
                 HitsList().add(Hits().add(0).add(0).add(0)).add(Hits().add(0)));

    if (!EXPECT_TRUE(testStringFieldInfo(fs))) return false;
    return true;
}

void
SearcherTest::testUTF8SubStringFieldSearcher()
{
    {
        UTF8SubStringFieldSearcher fs(0);
        EXPECT_TRUE(testUTF8SubStringFieldSearcher(fs));
        assertString(fs, "aa", "aaaa", Hits().add(0).add(0));
    }
    {
        UTF8SubStringFieldSearcher fs(0);
        EXPECT_TRUE(testUTF8SubStringFieldSearcher(fs));
        assertString(fs, "abc", "abc bcd abc", Hits().add(0).add(2));
        fs.maxFieldLength(4);
        assertString(fs, "abc", "abc bcd abc", Hits().add(0));
    }
    {
        UTF8SubstringSnippetModifier fs(0);
        EXPECT_TRUE(testUTF8SubStringFieldSearcher(fs));
        // we don't have 1 term optimization
        assertString(fs, "aa", "aaaa", Hits().add(0).add(0).add(0));
    }
}

void
SearcherTest::testUTF8SuffixStringFieldSearcher()
{
    UTF8SuffixStringFieldSearcher fs(0);
    std::string field = "operators and operator overloading";
    assertString(fs, "rsand", field, Hits());
    assertString(fs, "tor",   field, Hits().add(2));
    assertString(fs, "tors",  field, Hits().add(0));

    assertString(fs, StringList().add("an").add("din"), field, HitsList().add(Hits()).add(Hits()));
    assertString(fs, StringList().add("nd").add("g"), field, HitsList().add(Hits().add(1)).add(Hits().add(3)));

    EXPECT_TRUE(testStringFieldInfo(fs));
}

void
SearcherTest::testUTF8ExactStringFieldSearcher()
{
    UTF8ExactStringFieldSearcher fs(0);
    // regular
    TEST_DO(assertString(fs, "vespa", "vespa", Hits().add(0)));
    TEST_DO(assertString(fs, "vespar", "vespa", Hits()));
    TEST_DO(assertString(fs, "vespa", "vespar", Hits()));
    TEST_DO(assertString(fs, "vespa", "vespa vespa", Hits()));
    TEST_DO(assertString(fs, "vesp",  "vespa", Hits()));
    TEST_DO(assertString(fs, "vesp*",  "vespa", Hits().add(0)));
    TEST_DO(assertString(fs, "hutte",  "hutte", Hits().add(0)));
    TEST_DO(assertString(fs, "hütte",  "hütte", Hits().add(0)));
    TEST_DO(assertString(fs, "hutte",  "hütte", Hits()));
    TEST_DO(assertString(fs, "hütte",  "hutte", Hits()));
    TEST_DO(assertString(fs, "hütter", "hütte", Hits()));
    TEST_DO(assertString(fs, "hütte",  "hütter", Hits()));
}

void
SearcherTest::testUTF8FlexibleStringFieldSearcher()
{
    UTF8FlexibleStringFieldSearcher fs(0);
    // regular
    assertString(fs, "vespa", "vespa", Hits().add(0));
    assertString(fs, "vesp",  "vespa", Hits());
    assertString(fs, "esp",   "vespa", Hits());
    assertString(fs, "espa",  "vespa", Hits());

    // prefix
    assertString(fs, "vesp*",  "vespa", Hits().add(0));
    fs.setMatchType(FieldSearcher::PREFIX);
    assertString(fs, "vesp",   "vespa", Hits().add(0));

    // substring
    fs.setMatchType(FieldSearcher::REGULAR);
    assertString(fs, "*esp*",  "vespa", Hits().add(0));
    fs.setMatchType(FieldSearcher::SUBSTRING);
    assertString(fs, "esp",  "vespa", Hits().add(0));

    // suffix
    fs.setMatchType(FieldSearcher::REGULAR);
    assertString(fs, "*espa",  "vespa", Hits().add(0));
    fs.setMatchType(FieldSearcher::SUFFIX);
    assertString(fs, "espa",  "vespa", Hits().add(0));

    fs.setMatchType(FieldSearcher::REGULAR);
    EXPECT_TRUE(testStringFieldInfo(fs));
}

void
SearcherTest::testIntFieldSearcher()
{
    IntFieldSearcher fs;
    assertInt(fs,     "10",  10, true);
    assertInt(fs,      "9",  10, false);
    assertInt(fs,     ">9",  10, true);
    assertInt(fs,     ">9",   9, false);
    assertInt(fs,    "<11",  10, true);
    assertInt(fs,    "<11",  11, false);
    assertInt(fs,    "-10", -10, true);
    assertInt(fs,     "-9", -10, false);
    assertInt(fs,      "a",  10, false);
    assertInt(fs, "[-5;5]",  -5, true);
    assertInt(fs, "[-5;5]",   0, true);
    assertInt(fs, "[-5;5]",   5, true);
    assertInt(fs, "[-5;5]",  -6, false);
    assertInt(fs, "[-5;5]",   6, false);

    assertInt(fs, StringList().add("9").add("11"),  10, BoolList().add(false).add(false));
    assertInt(fs, StringList().add("9").add("10"),  10, BoolList().add(false).add(true));
    assertInt(fs, StringList().add("10").add(">9"), 10, BoolList().add(true).add(true));

    assertInt(fs, "10", LongList().add(10).add(20).add(10).add(30), Hits().add(0).add(2));
    assertInt(fs, StringList().add("10").add("20"), LongList().add(10).add(20).add(10).add(30),
              HitsList().add(Hits().add(0).add(2)).add(Hits().add(1)));

    assertFieldInfo(fs, "10", 10, QTFieldInfo(0, 1, 1));
    assertFieldInfo(fs, "10", LongList().add(10).add(20).add(10).add(30), QTFieldInfo(0, 2, 4));
    assertFieldInfo(fs, StringList().add("10").add("20"), 10,
                    FieldInfoList().add(QTFieldInfo(0, 1, 1)).add(QTFieldInfo(0, 0, 1)));
    assertFieldInfo(fs, StringList().add("10").add("20"), LongList().add(10).add(20).add(10).add(30),
                    FieldInfoList().add(QTFieldInfo(0, 2, 4)).add(QTFieldInfo(0, 1, 4)));
}

void
SearcherTest::testFloatFieldSearcher()
{
    FloatFieldSearcher fs;
    assertFloat(fs,         "10",    10, true);
    assertFloat(fs,       "10.5",  10.5, true);
    assertFloat(fs,      "-10.5", -10.5, true);
    assertFloat(fs,      ">10.5",  10.6, true);
    assertFloat(fs,      ">10.5",  10.5, false);
    assertFloat(fs,      "<10.5",  10.4, true);
    assertFloat(fs,      "<10.5",  10.5, false);
    assertFloat(fs,       "10.4",  10.5, false);
    assertFloat(fs,      "-10.4", -10.5, false);
    assertFloat(fs,          "a",  10.5, false);
    assertFloat(fs, "[-5.5;5.5]",  -5.5, true);
    assertFloat(fs, "[-5.5;5.5]",     0, true);
    assertFloat(fs, "[-5.5;5.5]",   5.5, true);
    assertFloat(fs, "[-5.5;5.5]",  -5.6, false);
    assertFloat(fs, "[-5.5;5.5]",   5.6, false);

    assertFloat(fs, StringList().add("10").add("11"),      10.5, BoolList().add(false).add(false));
    assertFloat(fs, StringList().add("10").add("10.5"),    10.5, BoolList().add(false).add(true));
    assertFloat(fs, StringList().add(">10.4").add("10.5"), 10.5, BoolList().add(true).add(true));

    assertFloat(fs, "10.5", FloatList().add(10.5).add(20.5).add(10.5).add(30.5), Hits().add(0).add(2));
    assertFloat(fs, StringList().add("10.5").add("20.5"), FloatList().add(10.5).add(20.5).add(10.5).add(30.5),
                HitsList().add(Hits().add(0).add(2)).add(Hits().add(1)));

    assertFieldInfo(fs, "10.5", 10.5, QTFieldInfo(0, 1, 1));
    assertFieldInfo(fs, "10.5", FloatList().add(10.5).add(20.5).add(10.5).add(30.5), QTFieldInfo(0, 2, 4));
    assertFieldInfo(fs, StringList().add("10.5").add("20.5"), 10.5,
                    FieldInfoList().add(QTFieldInfo(0, 1, 1)).add(QTFieldInfo(0, 0, 1)));
    assertFieldInfo(fs, StringList().add("10.5").add("20.5"), FloatList().add(10.5).add(20.5).add(10.5).add(30.5),
                    FieldInfoList().add(QTFieldInfo(0, 2, 4)).add(QTFieldInfo(0, 1, 4)));
}

bool
SearcherTest::testStringFieldInfo(StrChrFieldSearcher & fs)
{
    assertString(fs,    "foo", StringList().add("foo bar baz").add("foo bar").add("baz foo"), Hits().add(0).add(3).add(6));
    assertString(fs,    StringList().add("foo").add("bar"), StringList().add("foo bar baz").add("foo bar").add("baz foo"),
                 HitsList().add(Hits().add(0).add(3).add(6)).add(Hits().add(1).add(4)));

    bool retval = true;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "foo", "foo", QTFieldInfo(0, 1, 1)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "bar", "foo", QTFieldInfo(0, 0, 1)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "foo", "foo bar baz", QTFieldInfo(0, 1, 3)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "bar", "foo bar baz", QTFieldInfo(0, 1, 3)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "baz", "foo bar baz", QTFieldInfo(0, 1, 3)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "qux", "foo bar baz", QTFieldInfo(0, 0, 3)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, "foo", "foo foo foo", QTFieldInfo(0, 3, 3)))) retval = false;
    // query term size > last term size
    if (!EXPECT_TRUE(assertFieldInfo(fs, "runner", "Road Runner Disco", QTFieldInfo(0, 1, 3)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, StringList().add("roadrun").add("runner"), "Road Runner Disco",
                               FieldInfoList().add(QTFieldInfo(0, 0, 3)).add(QTFieldInfo(0, 1, 3))))) retval = false;
    // multiple terms
    if (!EXPECT_TRUE(assertFieldInfo(fs, "foo", StringList().add("foo bar baz").add("foo bar"),
                                    QTFieldInfo(0, 2, 5)))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, StringList().add("foo").add("baz"), "foo bar baz",
                    FieldInfoList().add(QTFieldInfo(0, 1, 3)).add(QTFieldInfo(0, 1, 3))))) retval = false;
    if (!EXPECT_TRUE(assertFieldInfo(fs, StringList().add("foo").add("baz"), StringList().add("foo bar baz").add("foo bar"),
                    FieldInfoList().add(QTFieldInfo(0, 2, 5)).add(QTFieldInfo(0, 1, 5))))) retval = false;
    return retval;
}

void
SearcherTest::testSnippetModifierSearcher()
{
    // ascii
    assertSnippetModifier("f", "foo", "\x1F""f\x1Foo");
    assertSnippetModifier("o", "foo", "f\x1Fo\x1F\x1Fo\x1F");
    assertSnippetModifier("r", "bar", "ba\x1Fr\x1F");
    assertSnippetModifier("foo", "foo foo", "\x1F""foo\x1F \x1F""foo\x1F");
    assertSnippetModifier("aa", "aaaaaa", "\x1F""aa\x1F\x1F""aa\x1F\x1F""aa\x1F");
    assertSnippetModifier("ab", "abcd\x1F""efgh", "\x1F""ab\x1F""cd\x1F""efgh");
    assertSnippetModifier("ef", "abcd\x1F""efgh", "abcd\x1F\x1F""ef\x1Fgh");
    assertSnippetModifier("fg", "abcd\x1F""efgh", "abcd\x1F""e\x1F""fg\x1Fh");
    // the separator overlapping the match is skipped
    assertSnippetModifier("cdef", "abcd\x1F""efgh", "ab\x1F""cdef\x1F""gh");
    // no hits
    assertSnippetModifier("bb", "aaaaaa", "aaaaaa");


    // multiple query terms
    assertSnippetModifier(StringList().add("ab").add("cd"), "abcd", "\x1F""ab\x1F\x1F""cd\x1F");
    // when we have overlap we only get the first match
    assertSnippetModifier(StringList().add("ab").add("bc"), "abcd", "\x1F""ab\x1F""cd");
    assertSnippetModifier(StringList().add("bc").add("ab"), "abcd", "\x1F""ab\x1F""cd");
    // the separator overlapping the match is skipped
    assertSnippetModifier(StringList().add("de").add("ef"), "abcd\x1F""efgh", "abc\x1F""de\x1F""fgh");

    // cjk
    assertSnippetModifier("\xe7\x9f\xb3", "\xe7\x9f\xb3\xe6\x98\x8e\xe5\x87\xb1\xe5\x9c\xa8",
                                      "\x1f\xe7\x9f\xb3\x1f\xe6\x98\x8e\xe5\x87\xb1\xe5\x9c\xa8");
    assertSnippetModifier("\xe6\x98\x8e\xe5\x87\xb1", "\xe7\x9f\xb3\xe6\x98\x8e\xe5\x87\xb1\xe5\x9c\xa8",
                                                      "\xe7\x9f\xb3\x1f\xe6\x98\x8e\xe5\x87\xb1\x1f\xe5\x9c\xa8");
    // the separator overlapping the match is skipped
    assertSnippetModifier("\xe6\x98\x8e\xe5\x87\xb1", "\xe7\x9f\xb3\xe6\x98\x8e\x1f\xe5\x87\xb1\xe5\x9c\xa8",
                                                      "\xe7\x9f\xb3\x1f\xe6\x98\x8e\xe5\x87\xb1\x1f\xe5\x9c\xa8");

    { // check that resizing works
        UTF8SubstringSnippetModifier mod;
        EXPECT_EQUAL(mod.getModifiedBuf().getLength(), 32u);
        EXPECT_EQUAL(mod.getModifiedBuf().getPos(), 0u);
        performSearch(mod, StringList().add("a"), StringFieldValue("aaaaaaaaaaaaaaaa"));
        EXPECT_EQUAL(mod.getModifiedBuf().getPos(), 16u + 2 * 16u);
        EXPECT_TRUE(mod.getModifiedBuf().getLength() >= mod.getModifiedBuf().getPos());
    }
}

void
SearcherTest::testSnippetModifier()
{
    { // string field value
        SnippetModifierSetup sms(StringList().add("ab"));
        // multiple invokations
        assertSnippetModifier(sms, StringFieldValue("ab"), "\x1F""ab\x1F");
        assertSnippetModifier(sms, StringFieldValue("xxxxabxxxxabxxxx"), "xxxx\x1F""ab\x1Fxxxx\x1F""ab\x1Fxxxx");
        assertSnippetModifier(sms, StringFieldValue("xxabxx"), "xx\x1F""ab\x1Fxx");
    }
    { // collection field value
        SnippetModifierSetup sms(StringList().add("ab"));
        // multiple invokations
        assertSnippetModifier(sms, getFieldValue(StringList().add("ab")), "\x1F""ab\x1F");
        assertSnippetModifier(sms, getFieldValue(StringList().add("xxabxx")), "xx\x1F""ab\x1Fxx");
        assertSnippetModifier(sms, getFieldValue(StringList().add("ab").add("xxabxx").add("xxxxxx")),
                              "\x1F""ab\x1F\x1E""xx\x1F""ab\x1F""xx\x1E""xxxxxx");
        assertSnippetModifier(sms, getFieldValue(StringList().add("cd").add("ef").add("gh")),
                              "cd\x1E""ef\x1E""gh");
    }
    { // check that resizing works
        SnippetModifierSetup sms(StringList().add("a"));
        EXPECT_EQUAL(sms.modifier.getValueBuf().getLength(), 32u);
        EXPECT_EQUAL(sms.modifier.getValueBuf().getPos(), 0u);
        sms.modifier.modify(StringFieldValue("aaaaaaaaaaaaaaaa"));
        EXPECT_EQUAL(sms.modifier.getValueBuf().getPos(), 16u + 2 * 16u);
        EXPECT_TRUE(sms.modifier.getValueBuf().getLength() >= sms.modifier.getValueBuf().getPos());
    }
}

void
SearcherTest::testFieldSearchSpec()
{
    {
        FieldSearchSpec f;
        EXPECT_FALSE(f.valid());
        EXPECT_EQUAL(0u, f.id());
        EXPECT_EQUAL("", f.name());
        EXPECT_EQUAL(0x100000u, f.maxLength());
    }
    {
        FieldSearchSpec f(7, "f0", VsmfieldsConfig::Fieldspec::AUTOUTF8, "substring", 789);
        EXPECT_TRUE(f.valid());
        EXPECT_EQUAL(7u, f.id());
        EXPECT_EQUAL("f0", f.name());
        EXPECT_EQUAL(789u, f.maxLength());
        EXPECT_EQUAL(789u, f.searcher().maxFieldLength());
    }
}

void
SearcherTest::testSnippetModifierManager()
{
    FieldSearchSpecMapT specMap;
    specMap[0] = FieldSearchSpec(0, "f0", VsmfieldsConfig::Fieldspec::AUTOUTF8, "substring", 1000);
    specMap[1] = FieldSearchSpec(1, "f1", VsmfieldsConfig::Fieldspec::AUTOUTF8, "", 1000);
    IndexFieldMapT indexMap;
    indexMap["i0"].push_back(0);
    indexMap["i1"].push_back(1);
    indexMap["i2"].push_back(0);
    indexMap["i2"].push_back(1);

    {
        SnippetModifierManager man;
        Query query(StringList().add("i0:foo"));
        man.setup(query.qtl, specMap, indexMap);
        assertQueryTerms(man, 0, StringList().add("foo"));
        assertQueryTerms(man, 1, StringList());
    }
    {
        SnippetModifierManager man;
        Query query(StringList().add("i1:foo"));
        man.setup(query.qtl, specMap, indexMap);
        assertQueryTerms(man, 0, StringList());
        assertQueryTerms(man, 1, StringList());
    }
    {
        SnippetModifierManager man;
        Query query(StringList().add("i1:*foo*"));
        man.setup(query.qtl, specMap, indexMap);
        assertQueryTerms(man, 0, StringList());
        assertQueryTerms(man, 1, StringList().add("foo"));
    }
    {
        SnippetModifierManager man;
        Query query(StringList().add("i2:foo").add("i2:*bar*"));
        man.setup(query.qtl, specMap, indexMap);
        assertQueryTerms(man, 0, StringList().add("foo").add("bar"));
        assertQueryTerms(man, 1, StringList().add("bar"));
    }
    { // check buffer sizes
        SnippetModifierManager man;
        Query query(StringList().add("i2:foo").add("i2:*bar*"));
        man.setup(query.qtl, specMap, indexMap);
        {
            SnippetModifier * sm = static_cast<SnippetModifier *>(man.getModifiers().getModifier(0));
            UTF8SubstringSnippetModifier * searcher = sm->getSearcher().get();
            EXPECT_EQUAL(sm->getValueBuf().getLength(), 128u);
            EXPECT_EQUAL(searcher->getModifiedBuf().getLength(), 64u);
        }
        {
            SnippetModifier * sm = static_cast<SnippetModifier *>(man.getModifiers().getModifier(1));
            UTF8SubstringSnippetModifier * searcher = sm->getSearcher().get();
            EXPECT_EQUAL(sm->getValueBuf().getLength(), 128u);
            EXPECT_EQUAL(searcher->getModifiedBuf().getLength(), 64u);
        }
    }
}

void
SearcherTest::testStripIndexes()
{
    EXPECT_EQUAL("f", FieldSearchSpecMap::stripNonFields("f"));
    EXPECT_EQUAL("f", FieldSearchSpecMap::stripNonFields("f[0]"));
    EXPECT_EQUAL("f[a]", FieldSearchSpecMap::stripNonFields("f[a]"));

    EXPECT_EQUAL("f.value", FieldSearchSpecMap::stripNonFields("f{a}"));
    EXPECT_EQUAL("f.value", FieldSearchSpecMap::stripNonFields("f{a0}"));
    EXPECT_EQUAL("f{a 0}", FieldSearchSpecMap::stripNonFields("f{a 0}"));
    EXPECT_EQUAL("f.value", FieldSearchSpecMap::stripNonFields("f{\"a 0\"}"));
}

void
SearcherTest::requireThatCountWordsIsWorking()
{
    EXPECT_TRUE(assertCountWords(0, ""));
    EXPECT_TRUE(assertCountWords(0, "?"));
    EXPECT_TRUE(assertCountWords(1, "foo"));
    EXPECT_TRUE(assertCountWords(2, "foo bar"));
    EXPECT_TRUE(assertCountWords(2, "? foo bar"));
    EXPECT_TRUE(assertCountWords(2, "foo bar ?"));

    // check that 'a' is counted as 1 word
    UTF8StrChrFieldSearcher fs(0);
    StringList field = StringList().add("a").add("aa bb cc");
    assertString(fs, "bb", field, Hits().add(2));
    assertString(fs, StringList().add("bb").add("not"), field, HitsList().add(Hits().add(2)).add(Hits()));
}

int
SearcherTest::Main()
{
    TEST_INIT("searcher_test");

    testFieldSearchSpec();
    testParseTerm();
    testMatchTermSuffix();
    testStrChrFieldSearcher();
    testUTF8SubStringFieldSearcher();
    testUTF8SuffixStringFieldSearcher();
    testUTF8FlexibleStringFieldSearcher();
    testUTF8ExactStringFieldSearcher();
    testIntFieldSearcher();
    testFloatFieldSearcher();

    testSnippetModifierSearcher();
    testSnippetModifier();
    testSnippetModifierManager();
    testStripIndexes();
    requireThatCountWordsIsWorking();

    TEST_DONE();
}

}

TEST_APPHOOK(vsm::SearcherTest);

