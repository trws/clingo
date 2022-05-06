// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#include "gringo/input/literals.hh"
#include "gringo/ground/literals.hh"
#include "gringo/output/output.hh"
#include "gringo/utility.hh"

namespace Gringo { namespace Input {

// {{{1 definition of Literal::projectScore

unsigned RelationLiteral::projectScore() const {
    return left->projectScore() + right->projectScore();
}
unsigned RelationLiteralN::projectScore() const {
    auto score = left_->projectScore();
    for (auto &term : right_) {
        score += term.second->projectScore();
    }
    return score;
}


// {{{1 definition of Literal::print

inline void RelationLiteral::print(std::ostream &out) const  { out << *left << rel << *right; }
inline void RelationLiteralN::print(std::ostream &out) const {
    assert(!right_.empty());
    out << naf_ << *left_;
    for (auto &&term: right_) {
        out << term.first << *term.second;
    }
}
inline void RangeLiteral::print(std::ostream &out) const     { out << "#range(" << *assign << "," << *lower << "," << *upper << ")"; }
inline void FalseLiteral::print(std::ostream &out) const     { out << "#false"; }
inline void ScriptLiteral::print(std::ostream &out) const    {
    out << "#script(" << *assign << "," << name << "(";
    print_comma(out, args, ",", [](std::ostream &out, UTerm const &term) { out << *term; });
    out << ")";
}
inline void CSPLiteral::print(std::ostream &out) const {
    assert(!terms.empty());
    if (auxiliary()) { out << "["; }
    out << terms.front().term;
    for (auto it = terms.begin() + 1, ie = terms.end(); it != ie; ++it) { out << *it; }
    if (auxiliary()) { out << "]"; }
}

// {{{1 definition of Literal::clone

RelationLiteral *RelationLiteral::clone() const {
    return make_locatable<RelationLiteral>(loc(), rel, get_clone(left), get_clone(right)).release();
}
RelationLiteralN *RelationLiteralN::clone() const {
    return make_locatable<RelationLiteralN>(loc(), naf_, get_clone(left_), get_clone(right_)).release();
}
RangeLiteral *RangeLiteral::clone() const {
    return make_locatable<RangeLiteral>(loc(), get_clone(assign), get_clone(lower), get_clone(upper)).release();
}
FalseLiteral *FalseLiteral::clone() const {
    return make_locatable<FalseLiteral>(loc()).release();
}
ScriptLiteral *ScriptLiteral::clone() const {
    return make_locatable<ScriptLiteral>(loc(), get_clone(assign), name, get_clone(args)).release();
}
CSPLiteral *CSPLiteral::clone() const {
    return make_locatable<CSPLiteral>(loc(), get_clone(terms)).release();
}

// {{{1 definition of Literal::simplify

bool RelationLiteral::simplify(Logger &log, Projections &, SimplifyState &state, bool, bool) {
    if (left->simplify(state, false, false, log).update(left, false).undefined()) { return false; }
    if (right->simplify(state, false, false, log).update(right, false).undefined()) { return false; }
    return true;
}
bool RelationLiteralN::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    static_cast<void>(project);
    static_cast<void>(positional);
    static_cast<void>(singleton);
    if (left_->simplify(state, false, false, log).update(left_, false).undefined()) {
        return false;
    }
    for (auto &&term : right_) {
        if (!term.second->simplify(state, false, false, log).update(term.second, false).undefined()) {
            return false;
        }
    }
    return true;
}
bool RangeLiteral::simplify(Logger &, Projections &, SimplifyState &, bool, bool) {
    throw std::logic_error("RangeLiteral::simplify should never be called  if used properly");
}
bool FalseLiteral::simplify(Logger &, Projections &, SimplifyState &, bool, bool) { return true; }
bool ScriptLiteral::simplify(Logger &, Projections &, SimplifyState &, bool, bool) {
    throw std::logic_error("ScriptLiteral::simplify should never be called  if used properly");
}
bool CSPLiteral::simplify(Logger &log, Projections &, SimplifyState &state, bool, bool) {
    for (auto &x : terms) {
        if (!x.simplify(state, log)) { return false; };
    }
    return true;
}

// {{{1 definition of Literal::collect

void RelationLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    left->collect(vars, bound && rel == Relation::EQ);
    right->collect(vars, false);
}
void RelationLiteralN::collect(VarTermBoundVec &vars, bool bound) const {
    left_->collect(vars, bound && naf_ == NAF::POS && right_.front().first == Relation::EQ);
    for (auto &term : right_) {
        term.second->collect(vars, false);
    }
}
void RangeLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    assign->collect(vars, bound);
    lower->collect(vars, false);
    upper->collect(vars, false);
}
void FalseLiteral::collect(VarTermBoundVec &, bool) const { }
void ScriptLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    assign->collect(vars, bound);
    for (auto &x : args) { x->collect(vars, false); }
}
void CSPLiteral::collect(VarTermBoundVec &vars, bool) const {
    for (auto &x : terms) { x.collect(vars); }
}

// {{{1 definition of Literal::operator==

bool RelationLiteral::operator==(Literal const &x) const {
    auto t = dynamic_cast<RelationLiteral const *>(&x);
    return t && rel == t->rel && is_value_equal_to(left, t->left) && is_value_equal_to(right, t->right);
}
bool RelationLiteralN::operator==(Literal const &x) const {
    auto t = dynamic_cast<RelationLiteralN const *>(&x);
    return t != nullptr && naf_ == t->naf_ && is_value_equal_to(left_, t->left_) && is_value_equal_to(right_, t->right_);
}
bool RangeLiteral::operator==(Literal const &x) const {
    auto t = dynamic_cast<RangeLiteral const *>(&x);
    return t && is_value_equal_to(assign, t->assign) && is_value_equal_to(lower, t->lower) && is_value_equal_to(upper, t->upper);
}
bool FalseLiteral::operator==(Literal const &x) const {
    return dynamic_cast<FalseLiteral const *>(&x) != nullptr;
}
bool ScriptLiteral::operator==(Literal const &x) const {
    auto t = dynamic_cast<ScriptLiteral const *>(&x);
    return t && is_value_equal_to(assign, t->assign) && name == t->name && is_value_equal_to(args, t->args);
}
bool CSPLiteral::operator==(Literal const &x) const {
    auto t = dynamic_cast<CSPLiteral const *>(&x);
    return t && is_value_equal_to(terms, t->terms) && (auxiliary_ == t->auxiliary_);
}

// {{{1 definition of Literal::rewriteArithmetics

void RelationLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    if (rel == Relation::EQ) {
        if (right->hasVar()) {
            assign.emplace_back(Relation::EQ, get_clone(right), get_clone(left));
            Term::replace(std::get<1>(assign.back()), std::get<1>(assign.back())->rewriteArithmetics(arith, auxGen));
        }
        Term::replace(left, left->rewriteArithmetics(arith, auxGen));
    }
}
void RelationLiteralN::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    assert(naf_ == NAF::POS);
    UTerm *prev = &left_;
    for (auto &term : right_) {
        if (term.first == Relation::EQ) {
            if (term.second->hasVar()) {
                assign.emplace_back(Relation::EQ, get_clone(term.second), get_clone(*prev));
                Term::replace(std::get<1>(assign.back()), std::get<1>(assign.back())->rewriteArithmetics(arith, auxGen));
            }
            Term::replace(*prev, prev->get()->rewriteArithmetics(arith, auxGen));
        }
        prev = &term.second;
    }
    while (right_.size() > 1) {
        assign.emplace_back(right_.back().first, get_clone((right_.end() - 2)->second), std::move(right_.back().second));
        right_.pop_back();
    }
}
void RangeLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &, AuxGen &auxGen) {
    Term::replace(this->assign, this->assign->rewriteArithmetics(arith, auxGen));
}
void FalseLiteral::rewriteArithmetics(Term::ArithmeticsMap &, RelationVec &, AuxGen &) { }
void ScriptLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &, AuxGen &auxGen) {
    Term::replace(this->assign, this->assign->rewriteArithmetics(arith, auxGen));
}
void CSPLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &, AuxGen &auxGen) {
    for (auto &x : terms) { x.rewriteArithmetics(arith, auxGen); }
}

// {{{1 definition of Literal::hash

size_t RelationLiteral::hash() const {
    return get_value_hash(typeid(RelationLiteral).hash_code(), size_t(rel), left, right);
}
size_t RelationLiteralN::hash() const {
    return get_value_hash(typeid(CSPLiteral).hash_code(), naf_, left_, right_);
}
size_t RangeLiteral::hash() const {
    return get_value_hash(typeid(RangeLiteral).hash_code(), assign, lower, upper);
}
size_t FalseLiteral::hash() const {
    return get_value_hash(typeid(FalseLiteral).hash_code());
}
size_t ScriptLiteral::hash() const {
    return get_value_hash(typeid(RangeLiteral).hash_code(), assign, name, args);
}
size_t CSPLiteral::hash() const {
    return get_value_hash(typeid(CSPLiteral).hash_code(), terms);
}

// {{{1 definition of Literal::unpool

ULitVec RelationLiteral::unpool(bool) const {
    ULitVec value;
    auto f = [&](UTerm &&l, UTerm &&r) { value.emplace_back(make_locatable<RelationLiteral>(loc(), rel, std::move(l), std::move(r))); };
    Term::unpool(left, right, Gringo::unpool, Gringo::unpool, f);
    return value;
}
ULitVec RelationLiteralN::unpool(bool beforeRewrite) const {
    static_cast<void>(beforeRewrite);
    // unpool a term with a relation
    auto unpoolTerm = [&](Terms::value_type const &term) -> Terms {
        Terms ret;
        for (auto &x : Gringo::unpool(term.second)) {
            ret.emplace_back(term.first, std::move(x));
        }
        return ret;
    };
    // unpool a vector of terms with relations
    auto unpoolTerms = [&](Terms const &terms) -> std::vector<Terms> {
        std::vector<Terms> termsPool;
        Term::unpool(terms.begin(), terms.end(), unpoolTerm, [&termsPool](Terms &&unpooled) {
            termsPool.emplace_back(std::move(unpooled));
        });
        return termsPool;
    };
    // unpool the left hand side together with the terms and relations
    ULitVec unpooled;
    auto appendRelN = [&](UTerm &&left, Terms &&right) {
        if (naf_ != NAF::NOT) {
            unpooled.emplace_back(make_locatable<RelationLiteralN>(loc(), naf_, std::move(left), std::move(right)));
        }
        else {
            // since relation literals only occur in conjunctive scopes,
            // we can get rid of negated relation literals here via unpooling
            UTerm prev = std::move(left);
            for (auto &term : right) {
                unpooled.emplace_back(make_locatable<RelationLiteralN>(loc(), naf_, term.first, std::move(prev), get_clone(term.second)));
                prev = std::move(term.second);
            }
        }
    };
    Term::unpool(left_, right_, Gringo::unpool, unpoolTerms, appendRelN);
    return unpooled;
}
ULitVec RangeLiteral::unpool(bool) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}
ULitVec FalseLiteral::unpool(bool) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}
ULitVec ScriptLiteral::unpool(bool) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}
ULitVec CSPLiteral::unpool(bool beforeRewrite) const {
    using namespace std::placeholders;
    ULitVec value;
    auto f = [&](Terms &&args)                { value.emplace_back(make_locatable<CSPLiteral>(loc(), std::move(args))); };
    auto unpoolLit = [&](CSPLiteral const &x) { Term::unpool(x.terms.begin(), x.terms.end(), std::bind(&CSPRelTerm::unpool, _1), f); };
    if (!beforeRewrite) {
        for (auto it = terms.begin() + 1, ie = terms.end(); it != ie; ++it) {
            unpoolLit(*make_locatable<CSPLiteral>(loc(), it->rel, get_clone((it-1)->term), get_clone(it->term)));
        }
    }
    else { unpoolLit(*this); }
    return value;
}

// {{{1 definition of Literal::toTuple

void RelationLiteral::toTuple(UTermVec &tuple, int &id) {
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    tuple.emplace_back(get_clone(left));
    tuple.emplace_back(get_clone(right));
    id++;
}
void RelationLiteralN::toTuple(UTermVec &tuple, int &id) {
    assert(right_.size() == 1);
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    tuple.emplace_back(get_clone(left_));
    tuple.emplace_back(get_clone(right_.begin()->second));
    id++;
}
void RangeLiteral::toTuple(UTermVec &, int &) {
    throw std::logic_error("RangeLiteral::toTuple should never be called  if used properly");
}
void FalseLiteral::toTuple(UTermVec &tuple, int &id) {
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    id++;
}
void ScriptLiteral::toTuple(UTermVec &, int &) {
    throw std::logic_error("ScriptLiteral::toTuple should never be called  if used properly");
}
void CSPLiteral::toTuple(UTermVec &tuple, int &id) {
    VarTermSet vars;
    for (auto &x : terms) { x.collect(vars); }
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    for (auto &x : vars) { tuple.emplace_back(UTerm(x.get().clone())); }
    id++;
}

// {{{1 definition of Literal::isEDB

Symbol Literal::isEDB() const          { return {}; }

// {{{1 definition of Literal::hasPool

inline bool RelationLiteral::hasPool(bool) const  { return left->hasPool() || right->hasPool(); }
inline bool RelationLiteralN::hasPool(bool beforeRewrite) const  {
    static_cast<void>(beforeRewrite);
    if (naf_ == NAF::NOT) {
        return true;
    }
    if (left_->hasPool()) {
        return true;
    }
    for (auto &term : right_) {
        if (term.second->hasPool()) {
            return true;
        }
    }
    return  false;
}
inline bool RangeLiteral::hasPool(bool) const             { return false; }
inline bool FalseLiteral::hasPool(bool) const             { return false; }
inline bool ScriptLiteral::hasPool(bool) const            { return false; }
inline bool CSPLiteral::hasPool(bool beforeRewrite) const {
    if (beforeRewrite) {
        for (auto &x : terms) {
            if (x.hasPool()) { return true; }
        }
        return false;
    }
    else { return terms.size() > 2; }
}

// {{{1 definition of Literal::replace

inline void RelationLiteral::replace(Defines &x) {
    Term::replace(left, left->replace(x, true));
    Term::replace(right, right->replace(x, true));
}
inline void RelationLiteralN::replace(Defines &x) {
    Term::replace(left_, left_->replace(x, true));
    for (auto &term : right_) {
        Term::replace(term.second, term.second->replace(x, true));
    }
}
inline void RangeLiteral::replace(Defines &x) {
    Term::replace(assign, assign->replace(x, true));
    Term::replace(lower, lower->replace(x, true));
    Term::replace(upper, upper->replace(x, true));
}
inline void FalseLiteral::replace(Defines &) { }
inline void ScriptLiteral::replace(Defines &x) {
    Term::replace(assign, assign->replace(x, true));
    for (auto &y : args) { Term::replace(y, y->replace(x, true)); }
}
inline void CSPLiteral::replace(Defines &x) {
    for (auto &y : terms) { y.replace(x); }
}

// {{{1 definition of Literal::toGround

inline Ground::ULit RelationLiteral::toGround(DomainData &, bool) const {
    return gringo_make_unique<Ground::RelationLiteral>(rel, get_clone(left), get_clone(right));
}
inline Ground::ULit RelationLiteralN::toGround(DomainData &data, bool auxiliary) const {
    static_cast<void>(data);
    static_cast<void>(auxiliary);
    assert(right_.size() == 1 && naf_ == NAF::POS);
    return gringo_make_unique<Ground::RelationLiteral>(right_.front().first, get_clone(left_), get_clone(right_.front().second));
}
inline Ground::ULit RangeLiteral::toGround(DomainData &, bool) const {
    return gringo_make_unique<Ground::RangeLiteral>(get_clone(assign), get_clone(lower), get_clone(upper));
}
inline Ground::ULit FalseLiteral::toGround(DomainData &, bool) const {
    throw std::logic_error("FalseLiteral::toGround: must not happen");
}
inline Ground::ULit ScriptLiteral::toGround(DomainData &, bool) const {
    return gringo_make_unique<Ground::ScriptLiteral>(get_clone(assign), name, get_clone(args));
}
inline Ground::ULit CSPLiteral::toGround(DomainData &data, bool auxiliary) const {
    assert(terms.size() == 2);
    return gringo_make_unique<Ground::CSPLiteral>(data, auxiliary_ || auxiliary, terms[1].rel, get_clone(terms[0].term), get_clone(terms[1].term));
}

// {{{1 definition of Literal::shift

ULit RelationLiteral::shift(bool negate) {
    return make_locatable<RelationLiteral>(loc(), negate ? neg(rel) : rel, std::move(left), std::move(right));
}
ULit RelationLiteralN::shift(bool negate) {
    if (naf_ == NAF::NOT) {
        naf_ = NAF::POS;
    }
    else if (right_.size() == 1) {
        naf_ = NAF::POS;
        right_.front().first = neg(right_.front().first);
    }
    else {
        naf_ = NAF::NOT;
    }
    return make_locatable<RelationLiteralN>(loc(), std::move(*this));
}
ULit RangeLiteral::shift(bool)  { throw std::logic_error("RangeLiteral::shift should never be called  if used properly"); }
ULit FalseLiteral::shift(bool)  { return nullptr; }
ULit ScriptLiteral::shift(bool) { throw std::logic_error("ScriptLiteral::shift should never be called  if used properly"); }
ULit CSPLiteral::shift(bool negate) {
    if (negate) {
        assert(terms.size() == 2);
        return make_locatable<CSPLiteral>(loc(), neg(terms[1].rel), std::move(terms[0].term), std::move(terms[1].term));
    }
    else { return make_locatable<CSPLiteral>(loc(), std::move(terms)); }
}

// {{{1 definition of Literal::headRepr

UTerm RelationLiteral::headRepr() const {
    throw std::logic_error("RelationLiteral::toTuple should never be called if used properly");
}
UTerm RelationLiteralN::headRepr() const {
    throw std::logic_error("RelationLiteralN::toTuple should never be called if used properly");
}
UTerm RangeLiteral::headRepr() const {
    throw std::logic_error("RangeLiteral::toTuple should never be called if used properly");
}
UTerm FalseLiteral::headRepr() const {
    return nullptr;
}
UTerm ScriptLiteral::headRepr() const {
    throw std::logic_error("ScriptLiteral::toTuple should never be called if used properly");
}
UTerm CSPLiteral::headRepr() const {
    throw std::logic_error("CSPLiteral::toTuple should never be called if used properly");
}

// }}}1

// {{{1 definition of PredicateLiteral

PredicateLiteral::PredicateLiteral(NAF naf, UTerm &&repr, bool auxiliary)
: naf(naf)
, auxiliary_(auxiliary)
, repr(std::move(repr)) {
    if (!this->repr->isAtom()) {
        throw std::runtime_error("atom expected");
    }
}

PredicateLiteral::~PredicateLiteral() { }

unsigned PredicateLiteral::projectScore() const {
    return 1 + repr->projectScore();
}

inline void PredicateLiteral::print(std::ostream &out) const {
    if (auxiliary()) { out << "["; }
    out << naf << *repr;
    if (auxiliary()) { out << "]"; }
}

PredicateLiteral *PredicateLiteral::clone() const {
    return make_locatable<PredicateLiteral>(loc(), naf, get_clone(repr)).release();
}

bool PredicateLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    if (singleton && positional && naf == NAF::POS) {
        positional = false;
    }
    auto ret(repr->simplify(state, positional, false, log));
    ret.update(repr, false);
    if (ret.undefined()) { return false; }
    if (repr->simplify(state, positional, false, log).update(repr, false).project) {
        auto rep(project.add(*repr));
        Term::replace(repr, std::move(rep));
    }
    return true;
}

void PredicateLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    repr->collect(vars, bound && naf == NAF::POS);
}

inline bool PredicateLiteral::operator==(Literal const &x) const {
    auto t = dynamic_cast<PredicateLiteral const *>(&x);
    return t && naf == t->naf && is_value_equal_to(repr, t->repr) && (auxiliary_ == t->auxiliary_);
}

void PredicateLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &, AuxGen &auxGen) {
    if (naf == NAF::POS) { Term::replace(repr, repr->rewriteArithmetics(arith, auxGen)); }
}

inline size_t PredicateLiteral::hash() const {
    return get_value_hash(typeid(PredicateLiteral).hash_code(), size_t(naf), repr);
}

ULitVec PredicateLiteral::unpool(bool) const {
    ULitVec value;
    auto f = [&](UTerm &&y){ value.emplace_back(make_locatable<PredicateLiteral>(loc(), naf, std::move(y))); };
    Term::unpool(repr, Gringo::unpool, f);
    return  value;
}

void PredicateLiteral::toTuple(UTermVec &tuple, int &) {
    int id = 0;
    switch (naf) {
        case NAF::POS:    { id = 0; break; }
        case NAF::NOT:    { id = 1; break; }
        case NAF::NOTNOT: { id = 2; break; }
    }
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id)));
    tuple.emplace_back(get_clone(repr));
}

Symbol PredicateLiteral::isEDB() const { return naf == NAF::POS ? repr->isEDB() : Symbol(); }

inline bool PredicateLiteral::hasPool(bool) const { return repr->hasPool(); }

inline void PredicateLiteral::replace(Defines &x) { Term::replace(repr, repr->replace(x, false)); }

inline Ground::ULit PredicateLiteral::toGround(DomainData &x, bool auxiliary) const {
    return gringo_make_unique<Ground::PredicateLiteral>(auxiliary_ || auxiliary, x.add(repr->getSig()), naf, get_clone(repr));
}

ULit PredicateLiteral::shift(bool negate) {
    if (naf == NAF::POS) { return nullptr; }
    else {
        NAF inv = (naf == NAF::NOT) == negate ? NAF::NOTNOT : NAF::NOT;
        return make_locatable<PredicateLiteral>(loc(), inv, std::move(repr));
    }
}

UTerm PredicateLiteral::headRepr() const {
    assert(naf == NAF::POS);
    return get_clone(repr);
}

// {{{1 definition of ProjectionLiteral

ProjectionLiteral::ProjectionLiteral(UTerm &&repr)
    : PredicateLiteral(NAF::POS, std::move(repr)), initialized_(false) { }

ProjectionLiteral::~ProjectionLiteral() { }

ProjectionLiteral *ProjectionLiteral::clone() const {
    throw std::logic_error("ProjectionLiteral::clone must not be called!!!");
}

ULitVec ProjectionLiteral::unpool(bool) const {
    throw std::logic_error("ProjectionLiteral::unpool must not be called!!!");
}

inline Ground::ULit ProjectionLiteral::toGround(DomainData &x, bool auxiliary) const {
    bool initialized = initialized_;
    initialized_ = true;
    return gringo_make_unique<Ground::ProjectionLiteral>(auxiliary_ || auxiliary, x.add(repr->getSig()), get_clone(repr), initialized);
}

ULit ProjectionLiteral::shift(bool) {
    throw std::logic_error("ProjectionLiteral::shift must not be called!!!");
}

// {{{1 definition of RelationLiteral

RelationLiteral::RelationLiteral(Relation rel, UTerm &&left, UTerm &&right)
    : rel(rel)
    , left(std::move(left))
    , right(std::move(right)) { }

ULit RelationLiteral::make(Term::LevelMap::value_type &x) {
    Location loc(x.first->loc());
    return make_locatable<RelationLiteral>(loc, Relation::EQ, std::move(x.second), get_clone(x.first));
}

ULit RelationLiteral::make(Literal::RelationVec::value_type &x) {
    Location loc(std::get<1>(x)->loc() + std::get<2>(x)->loc());
    return make_locatable<RelationLiteral>(loc, std::get<0>(x), std::move(std::get<1>(x)), get_clone(std::get<2>(x)));
}

RelationLiteral::~RelationLiteral() { }

// {{{1 definition of RelationLiteralN

RelationLiteralN::RelationLiteralN(NAF naf, Relation rel, UTerm &&left, UTerm &&right)
: left_{std::move(left)}
, naf_{NAF::POS} {
    right_.emplace_back(naf == NAF::NOT ? neg(rel) : rel, std::move(right));
}

RelationLiteralN::RelationLiteralN(NAF naf, UTerm &&left, Terms &&right)
: left_(std::move(left))
, right_(std::move(right))
, naf_{naf == NAF::NOT ? NAF::NOT : NAF::POS} {
    if (naf_ == NAF::NOT && right_.size() == 1) {
        naf_ = NAF::POS;
        right_.front().first = neg(right_.front().first);
    }
}

ULit RelationLiteralN::make(Term::LevelMap::value_type &x) {
    Location loc(x.first->loc());
    return make_locatable<RelationLiteralN>(loc, NAF::POS, Relation::EQ, std::move(x.second), get_clone(x.first));
}

ULit RelationLiteralN::make(Literal::RelationVec::value_type &x) {
    Location loc(std::get<1>(x)->loc() + std::get<2>(x)->loc());
    return make_locatable<RelationLiteralN>(loc, NAF::POS, std::get<0>(x), std::move(std::get<1>(x)), get_clone(std::get<2>(x)));
}

RelationLiteralN::~RelationLiteralN() = default;

// {{{1 definition of RangeLiteral

RangeLiteral::RangeLiteral(UTerm &&assign, UTerm &&lower, UTerm &&upper)
    : assign(std::move(assign))
    , lower(std::move(lower))
    , upper(std::move(upper)) { }

ULit RangeLiteral::make(SimplifyState::DotsMap::value_type &dot) {
    Location loc(std::get<0>(dot)->loc());
    return make_locatable<RangeLiteral>(loc, std::move(std::get<0>(dot)), std::move(std::get<1>(dot)), std::move(std::get<2>(dot)));
}

RangeLiteral::~RangeLiteral() { }

// {{{1 definition of FalseLiteral

FalseLiteral::FalseLiteral() { }
FalseLiteral::~FalseLiteral() { }

// {{{1 definition of ScriptLiteral

ScriptLiteral::ScriptLiteral(UTerm &&assign, String name, UTermVec &&args)
    : assign(std::move(assign))
    , name(name)
    , args(std::move(args)) { }

ULit ScriptLiteral::make(SimplifyState::ScriptMap::value_type &script) {
    Location loc(std::get<0>(script)->loc());
    return make_locatable<ScriptLiteral>(loc, std::move(std::get<0>(script)), std::move(std::get<1>(script)), std::move(std::get<2>(script)));
}

ScriptLiteral::~ScriptLiteral() { }

// {{{1 definition of CSPLiteral

CSPLiteral::CSPLiteral(Relation rel, CSPAddTerm &&left, CSPAddTerm &&right) {
    terms.emplace_back(rel, std::move(left));
    terms.emplace_back(rel, std::move(right));
}
void CSPLiteral::append(Relation rel, CSPAddTerm &&x) { terms.emplace_back(rel, std::move(x)); }
CSPLiteral::CSPLiteral(Terms &&terms) : terms(std::move(terms)) { }
CSPLiteral::~CSPLiteral() { }

// }}}1

} } // namespace Input Gringo

