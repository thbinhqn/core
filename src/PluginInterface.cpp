/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005, 2006, 2007 Roman Schindlauer
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Thomas Krennwallner
 * Copyright (C) 2009, 2010 Peter Schüller
 * 
 * This file is part of dlvhex.
 *
 * dlvhex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * dlvhex is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlvhex; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */


/**
 * @file   PluginInterface.cpp
 * @author Roman Schindlauer
 * @date   Mon Oct 17 14:37:07 CEST 2005
 * 
 * @brief Definition of Classes PluginAtom, PluginRewriter,
 * and PluginInterface.
 * 
 *      
 */     

#include "dlvhex2/PluginInterface.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "dlvhex2/Registry.h"
#include "dlvhex2/ProgramCtx.h"
#include "dlvhex2/Term.h"
#include "dlvhex2/ID.h"
#include "dlvhex2/Benchmarking.h"

DLVHEX_NAMESPACE_BEGIN

#if 0
bool PluginAtom::Query::operator<(const Query& other) const
{
	/*
  return
    ( interpretation < other.interpretation ) ||
    ( interpretation == other.interpretation &&
      input < other.input ) ||
    ( interpretation == other.interpretation &&
      input == other.input &&
      pattern < other.pattern );
			*/
}
#endif
        
bool PluginAtom::Query::operator==(const Query& other) const
{
  return
      (input == other.input) &&
      (pattern == other.pattern) &&
      (
        (interpretation == other.interpretation) ||
        (interpretation != 0 && other.interpretation != 0 &&
         *interpretation == *other.interpretation)
      );
}

// hash function for QueryAnswerCache
std::size_t hash_value(const PluginAtom::Query& q)
{
  std::size_t seed = 0;
  boost::hash_combine(seed, q.input);
  //LOG("hash_combine inp " << printrange(q.input) << " yields " << seed);
  boost::hash_combine(seed, q.pattern);
  //LOG("hash_combine pat " << printrange(q.pattern) << " yields " << seed);
  // TODO: can we take hash of pointer to interpretation here?
  if( q.interpretation == 0 )
  {
    boost::hash_combine(seed, 0);
  }
  else
  {
    // TODO: outsource this
    //boost::hash_combine(seed, q.interpretation->getStorage());
    const Interpretation::Storage& bits = q.interpretation->getStorage();
    for(Interpretation::Storage::enumerator en = bits.first();
        en != bits.end(); ++en)
    {
      boost::hash_combine(seed, *en);
      //LOG("hash_combine at " << *en << " yields " << seed);
    }
  }
  //LOG("hash_combine returning " << seed);
  return seed;
}
        
PluginAtom::Answer::Answer():
  output(new std::vector<Tuple>),
  used(false)
{
}

void
PluginAtom::addInputPredicate()
{
	// throw error if last input term was tuple
	if (inputType.size() > 0)
		if (inputType.back() == TUPLE)
			throw GeneralError("Tuple inputs must be specified last in input list");

    inputType.push_back(PREDICATE);
}


void
PluginAtom::addInputConstant()
{
	// throw error if last input term was tuple
	if (inputType.size() > 0)
		if (inputType.back() == TUPLE)
			throw GeneralError("Tuple inputs must be specified last in input list");

    inputType.push_back(CONSTANT);
}


void
PluginAtom::addInputTuple()
{
    inputType.push_back(TUPLE);
}


bool
PluginAtom::checkInputArity(const unsigned arity) const
{
  bool ret = (inputType.size() == arity);

  if (!inputType.empty())
    {
      return inputType.back() == TUPLE ? true : ret;
    }
  else
    {
      return ret;
    }
}


void
PluginAtom::setOutputArity(const unsigned arity)
{
    outputSize = arity;
}


bool
PluginAtom::checkOutputArity(const unsigned arity) const
{
    return arity == outputSize;
}


void PluginAtom::retrieveCached(const Query& query, Answer& answer)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidrc,"PluginAtom::retrieveCached");
  // Cache answer for queries which were already done once:
  //
  // The most efficient way would be:
  // * use cache for same inputSet + same *inputi + more specific pattern
  // * store new cache for new inputSet/*inputi combination or unrelated (does not unify) pattern
  // * replace cache for existing inputSet/*inputi combination and less specific (unifies in one direction) pattern
  // 
  // The currently implemented "poor (wo)man's version" is:
  // * store answers in cache with queries as keys, disregard relations between patterns
  ///@todo: efficiency could be increased for certain programs by considering pattern relationships as indicated above

#if 0
  #include "dlvhex2/PrintVisitor.h"
  #include <iostream>
  std::cerr << "cache:" << std::endl;
  for( QueryAnswerCache::const_iterator i = queryAnswerCache.begin(); i != queryAnswerCache.end(); ++i)
  {
	  std::cerr << "  query: <";
	  RawPrintVisitor visitor(std::cerr);
	  i->first.getInterpretation().accept(visitor);
	  std::cerr << "|" << i->first.getInputTuple() << "|" << i->first.getPatternTuple() << ">" << std::endl;
  }


	  std::cerr << "retrieving query: <";
	  RawPrintVisitor visitor(std::cerr);
	  query.getInterpretation().accept(visitor);
	  std::cerr << "|" << query.getInputTuple() << "|" << query.getPatternTuple() << ">";
#endif

  //LOG("before queryAnswerCache");
  Answer& ans = queryAnswerCache[query];
  //LOG("after queryAnswerCache");
  if( ans.hasBeenUsed() )
  {
    // answer was not default constructed
    // -> use cache
    answer = ans;
  }
  else
  {
    // answer was default constructed
    // -> retrieve and replace in cache
    {
      DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidr,"PluginAtom retrieve");
      #if 0
      #ifndef NDEBUG
      std::stringstream o;
      RawPrinter printer(o, query.interpretation->getRegistry());
      o << "retrieving for ";
      printer.printmany(query.input, ",");
      o << "/";
      printer.printmany(query.pattern, ",");
      o << "/" << *query.interpretation;
      LOG(o.str());
      #endif
      #endif
      retrieve(query, ans);
      // if there was no answer, perhaps it has never been used, so we use it manually
      ans.use();
      //LOG("after retrieve: answ is used = " << ans.hasBeenUsed());
    }
    answer = ans;
  }
}

PluginAtom::InputType
PluginAtom::getInputType(const unsigned index) const
{
	if (index >= inputType.size())
	{
		assert(inputType.back() == TUPLE);
		return inputType.back();
	}

    return inputType[index];
}

void PluginAtom::setRegistry(RegistryPtr reg)
{
  // i think we really don't want to change registry during the lifetime,
  // it would invalidate the cache and more bad things would happen
  assert(registry == 0);
  assert(reg != 0);
  registry = reg;
  predicateID = registry->terms.getIDByString(predicate);
  if( predicateID == ID_FAIL )
  {
    Term t(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, predicate);
    predicateID = registry->terms.storeAndGetID(t);
  }
  assert(predicateID != ID_FAIL);
}

std::vector<PluginAtomPtr>
PluginInterface::createAtoms(ProgramCtx& ctx) const
{
	return std::vector<PluginAtomPtr>();
}

// call printUsage for each loaded plugin
void PluginInterface::printUsage(std::ostream& o) const
{
  // don't fail if no usage message has been defined, simply do not give one
}

// call processOptions for each loaded plugin
// (this is supposed to remove "recognized" options from pluginOptions)
void PluginInterface::processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx)
{
  // don't fail if no option processing has been defined, simply do not process
}

PluginConverterPtr
PluginInterface::createConverter(ProgramCtx&)
{
	return PluginConverterPtr();
}

std::vector<PluginConverterPtr>
PluginInterface::createConverters(ProgramCtx& ctx)
{
	// per default return the single converter created by createConverter
	std::vector<PluginConverterPtr> ret;
	PluginConverterPtr pc = this->createConverter(ctx);
	if( pc )
		ret.push_back(pc);
	return ret;
}

std::vector<HexParserModulePtr>
PluginInterface::createParserModules(ProgramCtx&)
{
	return std::vector<HexParserModulePtr>();
}

HexParserPtr PluginInterface::createParser(ProgramCtx&)
{
	return HexParserPtr();
}

PluginRewriterPtr PluginInterface::createRewriter(ProgramCtx&)
{
	return PluginRewriterPtr();
}

PluginOptimizerPtr PluginInterface::createOptimizer(ProgramCtx&)
{
	return PluginOptimizerPtr();
}

DLVHEX_NAMESPACE_END

// Local Variables:
// mode: C++
// End: