/* -*- C++ -*- */

/**
 * @file Rule.h
 * @author Roman Schindlauer
 * @date Thu Jun 30 12:39:40 2005
 *
 * @brief Rule and Program class.
 *
 * Momentarily , these rules are 'real' rules, i.e., they have
 * to have a body and a head. Facts are stored somewhere else and
 * disjunctive heads as well as constraints don't exist yet.
 *
 */


#ifndef _RULE_H
#define _RULE_H

#include "dlvhex/Atom.h"
#include "dlvhex/Literal.h"


/*
class RuleBody : public LogicalObject
{
public:
    
    **
     * @brief the body literals are related by conjunction.
     *
    typedef std::vector<Literal*> body_t;

    class const_iterator
    {
        body_t::const_iterator it;

    public:

        const_iterator()
        {
            //assert(0);
        }

        const_iterator(const body_t::const_iterator &it1)
            : it(it1)
        { }

        const Literal*
        operator *() const
        {
            return (*it);
        }

        void
        operator ++()
        {
            it++;
        }

        bool
        operator== (const const_iterator& i2) const
        {
            return it == i2.it;
        }

        bool
        operator != (const const_iterator& i2) const
        {
            return (it != i2.it);
        }
    };

    RuleBody();

    const_iterator
    begin() const
    {
        return const_iterator(body.begin());
    }

    const_iterator
    end() const
    {
        return const_iterator(body.end());
    }

    void
    add(Literal*);

    bool
    exists(const Literal*);

    size_t
    size();

private:

    **
     * @brief the body literals are related by conjunction.
     *
    body_t body;
};
*/


/*
class RuleHead : public LogicalObject
{
public:

    **
     * @brief The head atoms are related by disjunction.
     *
    typedef std::vector<Atom*> head_t;

    class const_iterator
    {
        head_t::const_iterator it;

    public:

        const_iterator()
        {
            //assert(0);
        }

        const_iterator(const head_t::const_iterator &it1)
            : it(it1)
        { }

        const Atom*
        operator *() const
        {
            return (*it);
        }

        void
        operator ++()
        {
            it++;
        }

        bool
        operator== (const const_iterator& i2) const
        {
            return it == i2.it;
        }

        bool
        operator != (const const_iterator& i2) const
        {
            return (it != i2.it);
        }
    };

    RuleHead();

    const_iterator
    begin() const
    {
        return const_iterator(head.begin());
    }

    const_iterator
    end() const
    {
        return const_iterator(head.end());
    }

    void
    add(Atom*);

    size_t
    size();

    bool
    exists(const Atom*);

private:

     head_t head;
};
*/

typedef std::vector<Atom*> RuleHead;

typedef std::vector<Literal*> RuleBody;


/**
 * @brief Class for representing a rule object.
 */
class Rule : public ProgramObject
{
public:
    
    /**
     * @brief Constructs a rule from a head and a body.
     */
    Rule(const RuleHead& h,
         const RuleBody& b);

    /**
     * @brief returns the atom's head.
     */
    const RuleHead&
    getHead() const;

    /**
     * @brief returns the atom's body.
     */
    const RuleBody&
    getBody() const;

    /**
     * @brief Test for equality.
     *
     * Two rules are equal, if they contain the same atoms in the body and the head.
     */
    bool
    operator== (const Rule& rule2) const;

private:

    RuleHead head;

    RuleBody body;
};

//
// only for verbose and debugging.
//
std::ostream&
operator<< (std::ostream& out, const Rule& rule);


//typedef std::vector<Rule> Rules;


/**
 * @brief Program class.
 *
 * The Program class encapsulates rules and external atoms to represent a subprogram
 */
class Program
{
public:

    typedef std::vector<const Rule*> program_t;

    class const_iterator
    {
        program_t::const_iterator it;

    public:

        const_iterator()
        {
            //assert(0);
        }

        const_iterator(const program_t::const_iterator &it1)
            : it(it1)
        { }

        const Rule*
        operator *() const
        {
            return (*it);
        }

        void
        operator ++()
        {
            it++;
        }

        bool
        operator== (const const_iterator& i2) const
        {
            return it == i2.it;
        }

        bool
        operator != (const const_iterator& i2) const
        {
            return (it != i2.it);
        }
    };

    const_iterator
    begin() const
    {
        return const_iterator(rules.begin());
    }

    const_iterator
    end() const
    {
        return const_iterator(rules.end());
    }

    Program();

//    Program(Rules&);

    void
    addRule(const Rule*);

    bool
    exists(const Rule*);

//    void
//    setExternalAtoms(std::vector<ExternalAtom>&);

//    const Rules&
//    getRules() const;

    const std::vector<ExternalAtom*>&
    getExternalAtoms() const;

    /**
     * @brief Returns the pointer to an External Atom that matches the specified name
     * and input parameters, or NULL if such an atom does not exist.
     *
     */
//    ExternalAtom*
//    findExternalAtom(const std::string, const Tuple&);

    /**
     * Only for debugging purposes. The real output functions are implemented
     * by the ProgramBuilder class!
     */
    void
    dump(std::ostream&) const;

private:

    //Rules rules;

    /// @todo: we should use a set here!
    program_t rules;

    std::vector<ExternalAtom*> externalAtoms;
};



#endif /* _RULE_H */
