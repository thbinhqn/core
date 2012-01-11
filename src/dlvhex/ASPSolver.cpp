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
 * @file ASPSolver.cpp
 * @author Thomas Krennwallner
 * @author Peter Schueller
 * @date Tue Jun 16 14:34:00 CEST 2009
 *
 * @brief ASP Solvers
 *
 *
 */

#include "dlvhex/ASPSolver.h"

#warning perhaps we need to reactivate this
//#define __STDC_LIMIT_MACROS
//#include <boost/cstdint.hpp>

#include "dlvhex/PlatformDefinitions.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "dlvhex/Benchmarking.h"
#include "dlvhex/DLVProcess.h"
#include "dlvhex/DLVresultParserDriver.h"
#include "dlvhex/Printer.hpp"
#include "dlvhex/Registry.hpp"
#include "dlvhex/ProgramCtx.h"
#include "dlvhex/AnswerSet.hpp"

#ifdef HAVE_LIBDLV
# include "dlv.h"
#endif

#ifdef HAVE_LIBCLINGO
#include <clingo/clingo_app.h>
#include <boost/tokenizer.hpp>
#endif

#include <boost/thread.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <list>

DLVHEX_NAMESPACE_BEGIN

namespace
{

struct MaskedResultAdder
{
  ConcurrentQueueResults& queue;
  InterpretationConstPtr mask;

  MaskedResultAdder(ConcurrentQueueResults& queue, InterpretationConstPtr mask):
    queue(queue), mask(mask)
  {
  }

  void operator()(AnswerSetPtr as)
  {
    if( mask )
      as->interpretation->getStorage() -= mask->getStorage();
    queue.enqueueAnswerset(as);
    boost::this_thread::interruption_point();
  }
};

} // anonymous namespace

namespace ASPSolver
{

//
// DLVSoftware
//

DLVSoftware::Options::Options():
  ASPSolverManager::GenericOptions(),
  rewriteHigherOrder(false),
  dropPredicates(false),
  arguments()
{
  arguments.push_back("-silent");
}

DLVSoftware::Options::~Options()
{
}

//
// ConcurrentQueueResultsImpl
//

// Delegate::Impl is used to prepare the result
// this object would be destroyed long before the result will be destroyed
// therefore its ownership is passed to the results
struct DLVSoftware::Delegate::ConcurrentQueueResultsImpl:
  public ConcurrentQueueResults
{
public:
  Options options;
  DLVProcess proc;
  RegistryPtr reg;
  InterpretationConstPtr mask;
  bool shouldTerminate;
  boost::thread answerSetProcessingThread;

public:
  ConcurrentQueueResultsImpl(const Options& options):
    ConcurrentQueueResults(),
    options(options),
    shouldTerminate(false)
  {
    DBGLOG(DBG,"DLVSoftware::Delegate::ConcurrentQueueResultsImpl()" << this);
  }

  virtual ~ConcurrentQueueResultsImpl()
  {
    DBGLOG(DBG,"DLVSoftware::Delegate::~ConcurrentQueueResultsImpl()" << this);
    DBGLOG(DBG,"setting termination bool, emptying queue, and joining thread");
    shouldTerminate = true;
    queue->flush();
    DBGLOG(DBG,"joining thread");
    answerSetProcessingThread.join();
    DBGLOG(DBG,"closing (probably killing) process");
    proc.close(true);
    DBGLOG(DBG,"done");
  }

  void setupProcess()
  {
    proc.setPath(DLVPATH);
    if( options.includeFacts )
      proc.addOption("-facts");
    else
      proc.addOption("-nofacts");
    BOOST_FOREACH(const std::string& arg, options.arguments)
    {
      proc.addOption(arg);
    }
  }

  void answerSetProcessingThreadFunc();

  void startThread()
  {
    DBGLOG(DBG,"starting answer set processing thread");
    answerSetProcessingThread = boost::thread(
	boost::bind(
	  &ConcurrentQueueResultsImpl::answerSetProcessingThreadFunc,
	  boost::ref(*this)));
    DBGLOG(DBG,"started answer set processing thread");
  }

  void closeAndCheck()
  {
    int retcode = proc.close();

    // check for errors
    if (retcode == 127)
    {
      throw FatalError("LP solver command `" + proc.path() + "´ not found!");
    }
    else if (retcode != 0) // other problem
    {
      std::stringstream errstr;

      errstr <<
	"LP solver `" << proc.path() << "´ "
	"bailed out with exitcode " << retcode << ": "
	"re-run dlvhex with `strace -f´.";

      throw FatalError(errstr.str());
    }
  }
};

void DLVSoftware::Delegate::ConcurrentQueueResultsImpl::answerSetProcessingThreadFunc()
{
  #warning create multithreaded logger by using thread-local storage for logger indent
  DBGLOG(DBG,"[" << this << "]" " starting answerSetProcessingThreadFunc");
  try
  {
    // parse results and store them into the queue

    // parse result
    assert(!!reg);
    DLVResultParser parser(reg);
    MaskedResultAdder adder(*this, mask);
    std::istream& is = proc.getInput();
    do
    {
      // get next input line
      DBGLOG(DBG,"[" << this << "]" "getting input from stream");
      std::string input;
      std::getline(is, input);
      DBGLOG(DBG,"[" << this << "]" "obtained " << input.size() <<
	  " characters from input stream via getline");
      if( input.empty() || is.bad() )
      {
	DBGLOG(DBG,"[" << this << "]" "leaving loop because got input size " << input.size() <<
	    ", stream bits fail " << is.fail() << ", bad " << is.bad() << ", eof " << is.eof());
	break;
      }

      // discard weak answer set cost lines
      if( 0 == input.compare(0, 22, "Cost ([Weight:Level]):") )
      {
	DBGLOG(DBG,"[" << this << "]" "discarding weak answer set cost line");
      }
      else
      {
	// parse line
	DBGLOG(DBG,"[" << this << "]" "parsing");
	std::istringstream iss(input);
	parser.parse(iss, adder);
      }
    }
    while(!shouldTerminate);
    DBGLOG(DBG,"[" << this << "]" "after loop " << shouldTerminate);

    // do clean shutdown if we were not terminated from outside
    if( !shouldTerminate )
    {
      // closes process and throws on errors
      // (all results have been parsed above)
      closeAndCheck();
      enqueueEnd();
    }
  }
  catch(const GeneralError& e)
  {
    int retcode = proc.close();
    std::stringstream s;
    s << proc.path() << " (exitcode = " << retcode << "): " << e.getErrorMsg();
    LOG(ERROR, "[" << this << "]" + s.str());
    enqueueException(s.str());
  }
  catch(const std::exception& e)
  {
    std::stringstream s;
    s << proc.path() + ": " + e.what();
    LOG(ERROR, "[" << this << "]" + s.str());
    enqueueException(s.str());
  }
  catch(...)
  {
    std::stringstream s;
    s << proc.path() + " other exception";
    LOG(ERROR, "[" << this << "]" + s.str());
    enqueueException(s.str());
  }
  DBGLOG(DBG,"[" << this << "]" "exiting answerSetProcessingThreadFunc");
}


#warning TODO certain interfaces deactivated
#if 0
void
DLVSoftware::Delegate::useStringInput(const std::string& program)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVSoftware::Delegate::useStringInput");

  try
  {
    setupProcess();
    // request stdin as last parameter
    proc.addOption("--");
    // fork dlv process
    proc.spawn();
    proc.getOutput() << program << std::endl;
    proc.endoffile();
    pimpl->closeAndCheck(); // closes process and throws on errors
  }
  CATCH_RETHROW_DLVDELEGATE
}

void
DLVSoftware::Delegate::useFileInput(const std::string& fileName)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVSoftware::Delegate::useFileInput");

  try
  {
    setupProcess();
    proc.addOption(fileName);
    // fork dlv process
    proc.spawn();
    proc.endoffile();
    pimpl->closeAndCheck(); // closes process and throws on errors
  }
  CATCH_RETHROW_DLVDELEGATE
}
#endif


/////////////////////////////////////
// DLVSoftware::Delegate::Delegate //
/////////////////////////////////////
DLVSoftware::Delegate::Delegate(const Options& options):
  results(new ConcurrentQueueResultsImpl(options))
{
}

DLVSoftware::Delegate::~Delegate()
{
}

void
DLVSoftware::Delegate::useInputProviderInput(InputProvider& inp, RegistryPtr reg)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVSoftw:Delegate:useInputProvInp");

  DLVProcess& proc = results->proc;
  results->reg = reg;
  assert(results->reg);
  #warning TODO set results->mask?

  try
  {
    results->setupProcess();
    // request stdin as last parameter
    proc.addOption("--");
    LOG(DBG,"external process was setup with path '" << proc.path() << "'");

    // fork dlv process
    proc.spawn();

    std::ostream& programStream = proc.getOutput();

    // copy stream
    programStream << inp.getAsStream().rdbuf();
    programStream.flush();

    proc.endoffile();

    // start thread
    results->startThread();
  }
  catch(const GeneralError& e)
  {
    std::stringstream errstr;
    int retcode = results->proc.close();
    errstr << results->proc.path() << " (exitcode = " << retcode <<
      "): " << e.getErrorMsg();
    throw FatalError(errstr.str());
  }
  catch(const std::exception& e)
  {
    throw FatalError(results->proc.path() + ": " + e.what());
  }
}

void
DLVSoftware::Delegate::useASTInput(const ASPProgram& program)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVSoftw:Delegate:useASTInput");

  DLVProcess& proc = results->proc;
  results->reg = program.registry;
  assert(results->reg);
  results->mask = program.mask;

  #warning TODO HO checks
  //if( idb.isHigherOrder() && !options.rewriteHigherOrder )
  //  throw SyntaxError("Higher Order Constructions cannot be solved with DLVSoftware without rewriting");

  // in higher-order mode we cannot have aggregates, because then they would
  // almost certainly be recursive, because of our atom-rewriting!
  //if( idb.isHigherOrder() && idb.hasAggregateAtoms() )
  //  throw SyntaxError("Aggregates and Higher Order Constructions must not be used together");

  try
  {
    results->setupProcess();
    // handle maxint
    if( program.maxint > 0 )
    {
      std::ostringstream os;
      os << "-N=" << program.maxint;
      proc.addOption(os.str());
    }
    // request stdin as last parameter
    proc.addOption("--");
    LOG(DBG,"external process was setup with path '" << proc.path() << "'");

    // fork dlv process
    proc.spawn();

    std::ostream& programStream = proc.getOutput();

    // output program
    RawPrinter printer(programStream, program.registry);

    if( program.edb != 0 )
    {
      // print edb interpretation as facts
      program.edb->printAsFacts(programStream);
      programStream << "\n";
      programStream.flush();
    }

    printer.printmany(program.idb, "\n");
    programStream << "\n";
    programStream.flush();

    proc.endoffile();

    // start thread
    results->startThread();
  }
  catch(const GeneralError& e)
  {
    std::stringstream errstr;
    int retcode = results->proc.close();
    errstr << results->proc.path() << " (exitcode = " << retcode <<
      "): " << e.getErrorMsg();
    throw FatalError(errstr.str());
  }
  catch(const std::exception& e)
  {
    throw FatalError(results->proc.path() + ": " + e.what());
  }
}

ASPSolverManager::ResultsPtr 
DLVSoftware::Delegate::getResults()
{
  DBGLOG(DBG,"DLVSoftware::Delegate::getResults");
  return results;
}


//
// DLVLibSoftware
//
#ifdef HAVE_LIBDLV

// if this does not work, maybe the old other branch helps
// (it was not fully working back then either, but maybe there are hints)
// https://dlvhex.svn.sourceforge.net/svnroot/dlvhex/dlvhex/branches/dlvhex-libdlv-integration@2879

struct DLVLibSoftware::Delegate::Impl
{
  Options options;
  PROGRAM_HANDLER *ph;
  RegistryPtr reg;

  Impl(const Options& options):
    options(options),
    ph(create_program_handler())
  {
    if( options.includeFacts )
      ph->setOption(INCLUDE_FACTS, 1);
    else
      ph->setOption(INCLUDE_FACTS, 0);
    BOOST_FOREACH(const std::string& arg, options.arguments)
    {
      if( arg == "-silent" )
      {
	// do nothing?
      }
      else
	throw std::runtime_error("dlv-lib commandline option not implemented: " + arg);
    }
  }

  ~Impl()
  {
    destroy_program_handler(ph);
  }
};

DLVLibSoftware::Delegate::Delegate(const Options& options):
  pimpl(new Impl(options))
{
}

DLVLibSoftware::Delegate::~Delegate()
{
}

void
DLVLibSoftware::Delegate::useASTInput(const ASPProgram& program)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVLibSoftware::Delegate::useASTInput");

  // TODO HO checks

  try
  {
    pimpl->reg = program.registry;
    assert(pimpl->reg);
    if( program.maxint != 0 )
      pimpl->ph->setMaxInt(program.maxint);

    pimpl->ph->clearProgram();

    // output program
    std::stringstream programStream;
    RawPrinter printer(programStream, program.registry);
    // TODO HO stuff

    if( program.edb != 0 )
    {
      // print edb interpretation as facts
      program.edb->printAsFacts(programStream);
      programStream << "\n";
      programStream.flush();
    }

    printer.printmany(program.idb, "\n");
    programStream.flush();

    DBGLOG(DBG,"sending program to dlv-lib:===");
    DBGLOG(DBG,programStream.str());
    DBGLOG(DBG,"==============================");
    pimpl->ph->Parse(programStream);
    pimpl->ph->ResolveProgram(SYNCRONOUSLY);
  }
  catch(const std::exception& e)
  {
    LOG(ERROR,"EXCEPTION: " << e.what());
    throw;
  }
}

// reuse DLVResults class from above

ASPSolverManager::ResultsPtr 
DLVLibSoftware::Delegate::getResults()
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"DLVLibSoftware::Delegate::getResults");

  //DBGLOG(DBG,"getting results");
  try
  {
    // for now, we parse all results and store them into the result container
    // later we should do kind of an online processing here

    boost::shared_ptr<DLVResults> ret(new DLVResults);

    // TODO really do incremental model fetching
    const std::vector<MODEL> *models = pimpl->ph->getAllModels();
    std::vector<MODEL>::const_iterator itm;
    // iterate over models
    for(itm = models->begin();
        itm != models->end(); ++itm)
    {
      AnswerSet::Ptr as(new AnswerSet(pimpl->reg));

      // iterate over atoms
      for(MODEL_ATOMS::const_iterator ita = itm->begin();
  	ita != itm->end(); ++ita)
      {
        const char* pname = ita->getName();
        assert(pname);
  
        // i have a hunch this might be the encoding
        assert(pname[0] != '-');

	typedef std::list<const char*> ParmList;
	ParmList parms;
  	//DBGLOG(DBG,"creating predicate with first term '" << pname << "'");
	parms.push_back(pname);
  
	// TODO HO stuff
	// TODO integer terms

        for(MODEL_TERMS::const_iterator itt = ita->getParams().begin();
  	  itt != ita->getParams().end(); ++itt)
        {
	  switch(itt->type)
	  {
	  case 1:
	    // string terms
	    //std::cerr << "creating string term '" << itt->data.item << "'" << std::endl;
	    parms.push_back(itt->data.item);
	    break;
	  case 2:
	    // int terms
	    //std::cerr << "creating int term '" << itt->data.number << "'" << std::endl;
	    assert(false);
	    //ptuple.push_back(new dlvhex::Term(itt->data.number));
	    break;
	  default:
	    throw std::runtime_error("unknown term type!");
	  }
        }

	// for each param in parms: find id and put into tuple
#warning TODO create something like inline ID TermTable::getByStringOrRegister(const std::string& symbol, IDKind kind)
	Tuple ptuple;
	ptuple.reserve(parms.size());
	assert(pimpl->reg);
	for(ParmList::const_iterator itp = parms.begin();
	    itp != parms.end(); ++itp)
	{
	  // constant term
	  ID id = pimpl->reg->terms.getIDByString(*itp);
	  if( id == ID_FAIL )
	  {
	    Term t(ID::MAINKIND_TERM  | ID::SUBKIND_TERM_CONSTANT, *itp);
	    id = pimpl->reg->terms.storeAndGetID(t);
	  }
	  assert(id != ID_FAIL);
	  DBGLOG(DBG,"got term " << *itp << " with id " << id);
	  ptuple.push_back(id);
	}

	// ogatom
	ID fid = pimpl->reg->ogatoms.getIDByTuple(ptuple);
	if( fid == ID_FAIL )
	{
	  OrdinaryAtom a(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
	  a.tuple.swap(ptuple);
	  {
	    #warning parsing efficiency problem see HexGrammarPTToASTConverter
	    std::stringstream ss;
	    RawPrinter printer(ss, pimpl->reg);
	    Tuple::const_iterator it = ptuple.begin();
	    printer.print(*it);
	    it++;
	    if( it != ptuple.end() )
	    {
	      ss << "(";
	      printer.print(*it);
	      it++;
	      while(it != ptuple.end())
	      {
		ss << ",";
		printer.print(*it);
		it++;
	      }
	      ss << ")";
	    }
	    a.text = ss.str();
	  }
	  fid = pimpl->reg->ogatoms.storeAndGetID(a);
	  DBGLOG(DBG,"added fact " << a << " with id " << fid);
	}
	DBGLOG(DBG,"got fact with id " << fid);
	assert(fid != ID_FAIL);
	as->interpretation->setFact(fid.address);
      }

      ret->add(as);
    }
  
    // TODO: is this necessary?
    // delete models;
   
    ASPSolverManager::ResultsPtr baseret(ret);
    return baseret;
  }
  catch(const std::exception& e)
  {
    LOG(ERROR,"EXCEPTION: " << e.what());
    throw;
  }
}
#endif // HAVE_LIBDLV


#warning TODO reactivate dlvdb

#if 0
#if defined(HAVE_DLVDB)
DLVDBSoftware::Options::Options():
  DLVSoftware::Options(),
  typFile()
{
}

DLVDBSoftware::Options::~Options()
{
}

DLVDBSoftware::Delegate::Delegate(const Options& opt):
  DLVSoftware::Delegate(opt),
  options(opt)
{
}

DLVDBSoftware::Delegate::~Delegate()
{
}

void DLVDBSoftware::Delegate::setupProcess()
{
  DLVSoftware::Delegate::setupProcess();

  proc.setPath(DLVDBPATH);
  proc.addOption("-DBSupport"); // turn on database support
  proc.addOption("-ORdr-"); // turn on rewriting of false body rules
  if( !options.typFile.empty() )
    proc.addOption(options.typFile);

}

#endif // defined(HAVE_DLVDB)
#endif

#warning TODO clingo nightly tests

//
// ClingoSoftware
//
#ifdef HAVE_LIBCLINGO
ClingoSoftware::Options::Options():
  ASPSolverManager::GenericOptions()
{
}

ClingoSoftware::Options::~Options()
{
}

namespace
{

class GringoPrinter:
  public RawPrinter
{
public:
  typedef RawPrinter Base;
  GringoPrinter(std::ostream& out, RegistryPtr registry):
    RawPrinter(out, registry) {}

  virtual void print(ID id)
  {
    if( id.isRule() && id.isRegularRule() )
    {
      // disjunction in rule heads is | not v
      const Rule& r = registry->rules.getByID(id);
      printmany(r.head, " | ");
      if( !r.body.empty() )
      {
	out << " :- ";
	printmany(r.body, ", ");
      }
      out << ".";
    }
    else
    {
      Base::print(id);
    }
  }
};

class ClingoResults:
  public ASPSolverManager::Results
{
public:
  typedef std::list<AnswerSet::Ptr> Storage;
  Storage answersets;
  bool resetCurrent;
  Storage::const_iterator current;

  ClingoResults():
    resetCurrent(true),
    current() {}
  virtual ~ClingoResults() {}

  void add(AnswerSet::Ptr as)
  {
    answersets.push_back(as);

    // we do this because I'm not sure if a begin()==end() iterator
    // becomes begin() or end() after insertion of the first element
    // (this is the failsafe version)
    if( resetCurrent )
    {
      current = answersets.begin();
      resetCurrent = false;
    }
  }

  virtual AnswerSet::Ptr getNextAnswerSet()
  {
    // if no answer set was ever added, or we reached the end
    if( (resetCurrent == true) ||
	(current == answersets.end()) )
    {
      return AnswerSet::Ptr();
    }
    else
    {
      Storage::const_iterator ret = current;
      current++;
      return *ret;
    }
  }
};


class MyClaspOutputFormat:
  public Clasp::OutputFormat
{
public:
  typedef Clasp::OutputFormat Base;
  boost::shared_ptr<ClingoResults> results;
  RegistryPtr registry;

  MyClaspOutputFormat(
      boost::shared_ptr<ClingoResults> results,
      RegistryPtr registry):
    Base(),
    results(results),
    registry(registry)
  {
  }

  virtual ~MyClaspOutputFormat()
  {
  }

  virtual void printModel(
      const Clasp::Solver& s, const Clasp::Enumerator&)
  {
    DBGLOG(DBG,"getting model from clingo!");

    AnswerSet::Ptr as(new AnswerSet(registry));

    const Clasp::AtomIndex& index = *s.strategies().symTab;
    for (Clasp::AtomIndex::const_iterator it = index.begin(); it != index.end(); ++it)
    {
      if (s.value(it->second.lit.var()) == Clasp::trueValue(it->second.lit) && !it->second.name.empty())
      {
	const char* groundatom = it->second.name.c_str();

	// try to do it via string (unstructured)
	ID idga = registry->ogatoms.getIDByString(groundatom);
	if( idga == ID_FAIL )
	{
	  // parse groundatom, register and store
	  DBGLOG(DBG,"parsing clingo ground atom '" << groundatom << "'");
	  OrdinaryAtom ogatom(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
	  ogatom.text = groundatom;
	  {
	    // create ogatom.tuple
	    boost::char_separator<char> sep(",()");
	    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	    tokenizer tok(ogatom.text, sep);
	    for(tokenizer::iterator it = tok.begin();
		it != tok.end(); ++it)
	    {
	      DBGLOG(DBG,"got token '" << *it << "'");
	      Term term(ID::MAINKIND_TERM, *it);
	      // the following takes care of int vs const/string
	      ID id = registry->storeTerm(term);
	      assert(id != ID_FAIL);
	      assert(!id.isVariableTerm());
	      ogatom.tuple.push_back(id);
	    }
	  }
	  idga = registry->ogatoms.storeAndGetID(ogatom);
	}
	assert(idga != ID_FAIL);
	as->interpretation->setFact(idga.address);
      }
    }

    LOG(INFO,"got model from clingo: " << *as);
    results->add(as);
  }

  virtual void printStats(
      const Clasp::SolverStatistics&, const Clasp::Enumerator&)
  {
  }
};

class MyClingoApp: public ClingoApp<CLINGO>
{
public:
  typedef ClingoApp<CLINGO> Base;
  boost::shared_ptr<ClingoResults> results;

  MyClingoApp():
    Base(),
    results(new ClingoResults())
  {
    DBGLOG(DBG,"MyClingoApp()");
  }
  
  ~MyClingoApp()
  {
    DBGLOG(DBG,"~MyClingoApp()");
  }

  void solve(std::string& program, RegistryPtr registry)
  {
    try
    {

      // configure like a binary
      {
	char execname[] = "clingo_within_dlvhex";
	char shiftopt[] = "--shift";
	char allmodelsopt[] = "-n 0";
	char noverboseopt[] = "--verbose=0";
	char* argv[] = {
	  execname,
	  shiftopt,
	  allmodelsopt,
	  noverboseopt,
	  NULL
	};
	int argc = 0;
	while(argv[argc] != NULL)
	  argc++;
	DBGLOG(DBG,"passing " << argc << " arguments to gringo:" <<
	    printrange(std::vector<const char*>(&argv[0], &argv[argc])));
	if(!parse(argc, argv))
	    throw std::runtime_error( messages.error.c_str() );
	#ifndef NDEBUG
	printWarnings();
	#endif
      }

      // configure in out
      Streams s;
      LOG(DBG,"sending to clingo:" << std::endl << "===" << std::endl << program << std::endl << "===");
      s.appendStream(
	  Streams::StreamPtr(new std::istringstream(program)),
	  "dlvhex_to_clingo");
      in_.reset(new FromGringo<CLINGO>(*this, s));
      out_.reset(new MyClaspOutputFormat(results, registry));

      Clasp::ClaspFacade clasp;
      facade_ = &clasp;
      clingo.iStats = false;
      clasp.solve(*in_, config_, this);
      DBGLOG(DBG,"after clasp.solve: results contains " << results->answersets.size() << " answer sets");
    }
    catch(const std::exception& e)
    {
      std::cerr << "got clingo exception " << e.what() << std::endl;
      throw;
    }
    catch(...)
    {
      std::cerr << "got very strange clingo exception!" << std::endl;
      throw;
    }
  }
};
 
}

struct ClingoSoftware::Delegate::Impl
{
  Options options;
  MyClingoApp myclingo;

  Impl(const Options& options):
    options(options),
    myclingo()
  {
  }

  ~Impl()
  {
  }
};

ClingoSoftware::Delegate::Delegate(const Options& options):
  pimpl(new Impl(options))
{
}

ClingoSoftware::Delegate::~Delegate()
{
}

void
ClingoSoftare::Delegate::useInputProviderInput(InputProvider& inp, RegistryPtr reg)
{
  throw std::runtime_error("TODO implement ClingoSoftare::Delegate::useInputProviderInput(const InputProvider& inp, RegistryPtr reg)");
}

void
ClingoSoftware::Delegate::useASTInput(const ASPProgram& program)
{
  DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"ClingoSoftware useASTInput");

  #warning TODO handle program.maxint for clingo

  // output program to stream
  std::string str;
  {
    DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidprepare,"prepare clingo input");

    std::ostringstream programStream;
    GringoPrinter printer(programStream, program.registry);

    if( program.edb != 0 )
    {
      // print edb interpretation as facts
      program.edb->printAsFacts(programStream);
      programStream << "\n";
    }

    printer.printmany(program.idb, "\n");
    programStream << std::endl;
    str = programStream.str();
  }

  pimpl->myclingo.solve(str, program.registry);
}

ASPSolverManager::ResultsPtr 
ClingoSoftware::Delegate::getResults()
{
  return pimpl->myclingo.results;
}
#endif // HAVE_LIBCLINGO

} // namespace ASPSolver

DLVHEX_NAMESPACE_END

/* vim: set noet sw=2 ts=8 tw=80: */
// Local Variables:
// mode: C++
// End:
