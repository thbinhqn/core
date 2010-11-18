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
 * @file   TestEvalMCSIE.cpp
 * @author Peter Schueller <ps@kr.tuwien.ac.at>
 * 
 * @brief  Test evaluation using MCSIE sources.
 */

// dlvhex
#define DLVHEX_BENCHMARK

#include "dlvhex/HexParser.hpp"
// #undef NDEBUG here fixes the bug completely
//# undef NDEBUG
#include "dlvhex/PlatformDefinitions.h"
#include "dlvhex/Logger.hpp"
#if 0
# undef NDEBUG
#    define LOG_CLOSURE_ID1 BOOST_PP_CAT(log_closure1_,__LINE__)
#    define LOG_CLOSURE_ID2 BOOST_PP_CAT(log_closure2_,__LINE__)
#    define LOG_CLOSURE_ID3 BOOST_PP_CAT(log_closure3_,__LINE__)
      #  undef LOG
      #  define LOG(streamout) do { std::ostringstream os; os << streamout; } while(false);
      #  undef LOG_FUNCTION
      #  define LOG_FUNCTION(func) \
          const std::string LOG_CLOSURE_ID1(func);
      #  undef LOG_METHOD
      #  define LOG_METHOD(method,object) \
          const std::string LOG_CLOSURE_ID1(method); \
          const void* const LOG_CLOSURE_ID2(object);
      #  undef LOG_SCOPE
      #  define LOG_SCOPE(name,msg) \
          const std::string LOG_CLOSURE_ID1(name); \
          const bool LOG_CLOSURE_ID2(msg);
      #  undef LOG_PSCOPE
      #  define LOG_PSCOPE(name,ptr,msg) \
          const std::string LOG_CLOSURE_ID1(name); \
          const void* const LOG_CLOSURE_ID2(ptr); \
          const bool LOG_CLOSURE_ID3(msg);
#endif

//#  define LOG(streamout) do { \
//       Logger::Instance().startline(); \
//       Logger::Instance().stream() << streamout << std::endl; \
//     } while(false);
#include "dlvhex/ID.hpp"
#include "dlvhex/Table.hpp"
#include "dlvhex/TermTable.hpp"
#include "dlvhex/OrdinaryAtomTable.hpp"
#include "dlvhex/BuiltinAtomTable.hpp"
#include "dlvhex/AggregateAtomTable.hpp"
#include "dlvhex/ExternalAtomTable.hpp"
#include "dlvhex/RuleTable.hpp"


// #undef NDEBUG here fixes the bug partially
//# undef NDEBUG
#include "dlvhex/ProgramCtx.h"
// #undef NDEBUG here does not fix the bug
//#ifdef NDEBUG
//# undef NDEBUG
//#endif
#include "dlvhex/PluginInterface.h"
#include "dlvhex/EvalGraphBuilder.hpp"
#include "dlvhex/EvalHeuristicOldDlvhex.hpp"
#include "dlvhex/EvalHeuristicTrivial.hpp"
#include "dlvhex/EvalHeuristicEasy.hpp"
#include "dlvhex/DependencyGraph.hpp"
#include "dlvhex/ComponentGraph.hpp"
#include "dlvhex/ModelGenerator.hpp"
#include "dlvhex/Benchmarking.h"


#include "dlvhex/OnlineModelBuilder.hpp"
#include "dlvhex/OfflineModelBuilder.hpp"

// mcsie

#include "Global.h"
#include "DLV_ASP_ContextAtom.h"
#include "InputConverter.h"
#include "OutputRewriter.h"
// defining LOG to use stringstream here solves the problem if NDEBUG is #undef'd after Logger.hpp

// other

#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>

#ifndef NDEBUG
# define LOG_REGISTRY_PROGRAM(ctx) \
  LOG(*ctx.registry); \
	RawPrinter printer(std::cerr, ctx.registry); \
	std::cerr << "edb = " << *ctx.edb << std::endl; \
	LOG("idb"); \
	printer.printmany(ctx.idb,"\n"); \
	std::cerr << std::endl; \
	LOG("idb end");
#else
# define LOG_REGISTRY_PROGRAM(ctx) \
  do {} while(false);
#endif

inline void makeGraphVizPdf(const char* fname)
{
  std::ostringstream ss;
  ss << "dot " << fname << " -Tpdf -o " << fname << ".pdf";
  system(ss.str().c_str());
}

typedef boost::function<void (std::ostream&)> GraphVizFunc;

inline void writeGraphVizFunctors(GraphVizFunc vfunc, GraphVizFunc tfunc, const std::string& fnamestart)
{
  /*
  std::string fnamev(fnamestart);
  fnamev += "Verbose.dot";
  LOG("dumping verbose graph to " << fnamev);
  std::ofstream filev(fnamev.c_str());
	vfunc(filev);
  makeGraphVizPdf(fnamev.c_str());

  std::string fnamet(fnamestart);
  fnamet += "Terse.dot";
  LOG("dumping terse graph to " << fnamet);
  std::ofstream filet(fnamet.c_str());
  tfunc(filet);
  makeGraphVizPdf(fnamet.c_str());
  */
}

template<typename GraphVizzyT>
inline void writeGraphViz(const GraphVizzyT& gv, const std::string& fnamestart)
{
	//GraphVizFunc vfunc = boost::bind(&GraphVizzyT::writeGraphViz, boost::cref(gv), _1, true);
	//GraphVizFunc tfunc = boost::bind(&GraphVizzyT::writeGraphViz, boost::cref(gv), _1, false);
	//writeGraphVizFunctors(vfunc, tfunc, fnamestart);
}

DLVHEX_NAMESPACE_USE

void breakLinesAndGraphViz(const std::string& str, std::ostream& o)
{
  unsigned at = 0;
  for(std::string::const_iterator it = str.begin(); it != str.end(); ++it)
  {
    char c = *it;
    if( c == '\\' ) // assume this is a newline
      at = 0;
    if( c == '\"' )
      o << "\\\"";
    else
      o << c;
    // make a new line at least all 25 characters if there is a ','
    at++;
    if( at > 25 && c == ',' )
    {
      at = 0;
      o << "\\n";
    }
  }
}

typedef FinalEvalGraph::EvalUnit EvalUnit;

//
// model graph printing: putting this into ModelGraph is insane (modelgraph has a too abstract view of the model)
// TODO think about improving the above situation
//

/* graphviz schema:
digraph G {
    compound=true;
    subgraph evalunit1 {
      model1 [label1];
    }
    subgraph cluster1 {
      ...
    }
    ...
    model1 -> model2;
    ...
}
*/
// output graph as graphviz source
template<typename ModelGraphT>
void writeEgMgGraphViz(
		std::ostream& o, bool verbose,
		const FinalEvalGraph& eg, const ModelGraphT& mg,
    boost::optional<std::set<typename ModelGraphT::Model> > onlyForModels = boost::none)
{
	typedef typename ModelGraphT::ModelList ModelList;
	typedef typename ModelGraphT::Model Model;
	typedef typename ModelGraphT::PredecessorIterator ModelPredecessorIterator;
	typedef typename ModelGraphT::ModelIterator ModelIterator;
  typedef std::set<typename ModelGraphT::Model> ModelSet;

  ModelSet printModels;
  if( !!onlyForModels )
  {
    // we need a hash map, as component graph is no graph with vecS-storage
    typedef boost::unordered_map<Model, boost::default_color_type> ColorHashMap;
    typedef boost::associative_property_map<ColorHashMap> ColorMap;
    ColorHashMap whiteHashMap;

    // fill white hash map
    ModelIterator mit, mit_end;
    for(boost::tie(mit, mit_end) = mg.getModels();
        mit != mit_end; ++mit)
    {
      whiteHashMap[*mit] = boost::white_color;
    }

    // one hash map for all!
    ColorHashMap myHashMap(whiteHashMap);
    ColorMap myColorMap(myHashMap);
    for(typename ModelSet::const_iterator it = onlyForModels.get().begin();
        it != onlyForModels.get().end(); ++it)
    {
      //LOG("doing dfs visit for " << *it);
      boost::depth_first_visit(
          mg.getInternalGraph(),
          *it, 
          boost::default_dfs_visitor(),
          myColorMap);
    }
    for(boost::tie(mit, mit_end) = mg.getModels();
        mit != mit_end; ++mit)
    {
      if( myHashMap[*mit] == boost::white_color )
      {
        //LOG("model " << *mit << " still white");
      }
      else
      {
        //LOG("model " << *mit << " not white -> printing");
        printModels.insert(*mit);
      }
    }
  }
  else
  {
    // include all
    ModelIterator mit, mit_end;
    for(boost::tie(mit, mit_end) = mg.getModels();
        mit != mit_end; ++mit)
    {
      printModels.insert(*mit);
    }
  }

  // boost::graph::graphviz is horribly broken!
  // therefore we print it ourselves

  o << "digraph G {" << std::endl <<
    "rankdir=BT;" << std::endl << // print root nodes at bottom, leaves at top!
    "concentrate=true;" << std::endl <<
    //"center=false;" << std::endl <<
    "pagedir=BL;" << std::endl <<
    //"ranksep=\"0.5\";" << std::endl <<
    //"nodesep=\"0.5\";" << std::endl <<
    //"page=\"44,35\";" << std::endl <<
    "compound=true;" << std::endl; // print clusters = eval units, inside nodes = models

	// stream deps into this stream
	std::stringstream odeps;

  FinalEvalGraph::EvalUnitIterator uit, ubegin, uend;
  boost::tie(ubegin, uend) = eg.getEvalUnits();
  for(uit = ubegin; uit != uend; ++uit)
  {
    EvalUnit u = *uit;
    o << "subgraph clusteru" << u << "{" << std::endl;
    o << "node [shape=box];" << std::endl;
    o << "label=\"";
    {
      std::stringstream s;
			if( eg.propsOf(u).mgf != 0 )
			{
				s << *eg.propsOf(u).mgf;
			}
			else
			{
				s << "NULL";
			}
      // escape " into \"
      breakLinesAndGraphViz(s.str(), o);
    }
		o << "\";" << std::endl;

    // models in this subgraph
		{
			for(ModelType t = MT_IN; t <= MT_OUTPROJ; t = static_cast<ModelType>(static_cast<unsigned>(t)+1))
			{
				const ModelList& modelsAt = mg.modelsAt(u, t);
				typename ModelList::const_iterator mit;
				for(mit = modelsAt.begin(); mit != modelsAt.end(); ++mit)
				{
          if( printModels.find(*mit) == printModels.end() )
            continue;

					Model m = *mit;
					o << "m" << m << "[label=\"";
					{
						std::stringstream s;
						s << toString(mg.propsOf(m).type) << " " << m << " @" << mg.propsOf(m).location << "\\n";
            if( mg.propsOf(m).interpretation != 0 )
              s << *mg.propsOf(m).interpretation;
            breakLinesAndGraphViz(s.str(), o);
					}
					o << "\"];" << std::endl;

					// model dependencies (preds)
					ModelPredecessorIterator pit, pbegin, pend;
					boost::tie(pbegin, pend) = mg.getPredecessors(m);
					for(pit = pbegin; pit != pend; ++pit)
					{
						odeps <<
							"m" << m << " -> m" << mg.targetOf(*pit) <<
							"[label=\"" << mg.propsOf(*pit).joinOrder << "\"];" << std::endl;
					}
				} // through all models
			}
		}
		o << "}" << std::endl;

		/*
    // unit dependencies
    typename EvalGraphT::PredecessorIterator pit, pbegin, pend;
    boost::tie(pbegin, pend) = eg.getPredecessors(u);
    for(pit = pbegin; pit != pend; ++pit)
    {
      LOG("-> depends on unit " << eg.targetOf(*pit) << "/join order " << eg.propsOf(*pit).joinOrder);
    }
		*/

  }

	// deps between models
	o << odeps.str() << std::endl;
	o << "}" << std::endl;
}

typedef OnlineModelBuilder<FinalEvalGraph> FinalOnlineModelBuilder;
typedef OfflineModelBuilder<FinalEvalGraph> FinalOfflineModelBuilder;

int main(int argn, char** argv)
{
  if( argn != 4 )
  {
    std::cerr << "usage: " << argv[0] << " <heurimode> <mbmode> <inputfile>" << std::endl;
    return -1;
  }

  //
  // setup benchmarking
  //
  benchmark::BenchmarkController& ctr =
    benchmark::BenchmarkController::Instance();
  ctr.setOutput(&std::cerr);
  // for continuous statistics output, display every 1000'th output
  ctr.setPrintInterval(999);
  // deconstruct benchmarking (= output results) at scope exit 
  int dummy; // this is needed, as SCOPE_EXIT is not defined for no arguments
  BOOST_SCOPE_EXIT( (dummy) ) {
	  (void)dummy;
	  benchmark::BenchmarkController::finish();
  }
  BOOST_SCOPE_EXIT_END

  //
  // preprocess arguments
  //
  const std::string heurimode(argv[1]);
  const std::string mbmode(argv[2]);
  const std::string fname(argv[3]);

  // configure mcsie
  mcsdiagexpl::Global::getInstance()->setKR2010rewriting();

  // get input
  std::ifstream infile(fname.c_str());

  // rewrite
  std::stringstream rewrittenfile;
  {
    DLVHEX_BENCHMARK_REGISTER_AND_START(sidrewrite, "rewrite mcsie");
    mcsdiagexpl::InputConverter converter;
    converter.convert(infile, rewrittenfile);
    DLVHEX_BENCHMARK_STOP(sidrewrite);
    #ifndef NDEBUG
    std::cerr <<
      "rewriting yielded the following:" << std::endl <<
      rewrittenfile.str() << std::endl;
    #endif
  }

  // prepare program context
  ProgramCtx ctx;
  ctx.registry.reset(new Registry);

  // create dlv ctx plugin atom
  PluginAtomPtr pa(new mcsdiagexpl::DLV_ASP_ContextAtom);
  pa->setRegistry(ctx.registry);
  ID idpa = pa->getPredicateID();

  // parse HEX program
  LOG("parsing HEX program");
  DLVHEX_BENCHMARK_REGISTER_AND_START(sidhexparse, "HexParser::parse");
  HexParser parser(ctx);
  parser.parse(rewrittenfile);
  DLVHEX_BENCHMARK_STOP(sidhexparse);

  // link parsed external atoms to plugin atoms
  //TODO this should become a common functionality using some pluginAtom registry
  //TODO we should make the ExternalAtom::pluginAtom member mutable
	{
		ExternalAtomTable::PredicateIterator it, it_end;
		for(boost::tie(it, it_end) = ctx.registry->eatoms.getRangeByPredicateID(idpa);
				it != it_end; ++it)
		{
			ExternalAtom ea(*it);
			ea.pluginAtom = pa;
			ctx.registry->eatoms.update(*it, ea);
		}
	}

  //LOG_REGISTRY_PROGRAM(ctx);

      //#  undef LOG
      //#  define LOG(streamout) do { std::ostringstream os; os << streamout; } while(false);

  // create dependency graph
  LOG("creating dependency graph");
  DLVHEX_BENCHMARK_REGISTER_AND_START(siddepgraph, "create dependencygraph");
  std::vector<dlvhex::ID> auxRules;
  dlvhex::DependencyGraph depgraph(ctx.registry);
  depgraph.createDependencies(ctx.idb, auxRules);
  DLVHEX_BENCHMARK_STOP(siddepgraph);
  #ifndef NDEBUG
  //writeGraphViz(depgraph, fname+"MCSIEDepGraph");
  #endif

  // create component graph
  LOG("creating component graph");
  DLVHEX_BENCHMARK_REGISTER_AND_START(sidcompgraph, "create componentgraph");
  dlvhex::ComponentGraph compgraph(depgraph, ctx.registry);
  DLVHEX_BENCHMARK_STOP(sidcompgraph);
  #ifndef NDEBUG
  //writeGraphViz(compgraph, fname+"MCSIECompGraph");
  #endif

  // create eval graph
  LOG("creating eval graph");
  DLVHEX_BENCHMARK_REGISTER_AND_START(sidevalgraph, "create evalgraph");
  FinalEvalGraph evalgraph;
  EvalGraphBuilder egbuilder(ctx, compgraph, evalgraph);


     // #  undef LOG
     // #  define LOG(streamout) do { std::ostringstream os; os << streamout; } while(false);







  // use one of several heuristics
  if( heurimode == "old" )
  {
    // old DLVHEX heuristic
    LOG("building eval graph with old heuristics");
    EvalHeuristicOldDlvhex heuristicOldDlvhex(egbuilder);
    heuristicOldDlvhex.build();
  }
  else if( heurimode == "trivial" )
  {
    // trivial heuristic: just take component graph
    // (maximum number of eval units, probably large overhead)
    LOG("building eval graph with trivial heuristics");
    EvalHeuristicTrivial heuristic(egbuilder);
    heuristic.build();
  }
  else if( heurimode == "easy" )
  {
    // easy heuristic: just make some easy adjustments to improve on the trivial heuristics
    LOG("building eval graph with easy heuristics");
    EvalHeuristicEasy heuristic(egbuilder);
    heuristic.build();
  }
  else
  {
    std::cerr << "usage: <heurimode> must be one of 'old','trivial','easy'" << std::endl;
    return -1;
  }
  DLVHEX_BENCHMARK_STOP(sidevalgraph);

  #ifndef NDEBUG
  //writeGraphViz(compgraph, fname+"MCSIEEvalGraph");
  #endif

  // setup final unit
  LOG("setting up final unit");
  DLVHEX_BENCHMARK_REGISTER_AND_START(sidfinalunit, "creating final unit");
  EvalUnit ufinal;
  {
    ufinal = evalgraph.addUnit(FinalEvalGraph::EvalUnitPropertyBundle());
    LOG("ufinal = " << ufinal);

    FinalEvalGraph::EvalUnitIterator it, itend;
    boost::tie(it, itend) = evalgraph.getEvalUnits();
    for(; it != itend && *it != ufinal; ++it)
    {
      LOG("adding dependency from ufinal to unit " << *it << " join order " << *it);
      // we can do this because we know that eval units (= vertices of a vecS adjacency list) are unsigned integers
      evalgraph.addDependency(ufinal, *it, FinalEvalGraph::EvalUnitDepPropertyBundle(*it));
    }
  }
  DLVHEX_BENCHMARK_STOP(sidfinalunit);

  // prepare for output
  mcsdiagexpl::EQOutputBuilder ob;

  //std::cerr << __FILE__ << ":" << __LINE__ << std::endl << *ctx.registry << std::endl;

  // evaluate
  LOG("evaluating");
  DLVHEX_BENCHMARK_REGISTER(sidoutputmodel, "output model");
  if( mbmode == "online" )
  {
    typedef FinalOnlineModelBuilder::Model Model;
    typedef FinalOnlineModelBuilder::OptionalModel OptionalModel;
    typedef FinalOfflineModelBuilder::MyModelGraph MyModelGraph;
    LOG("creating model builder");
    DLVHEX_BENCHMARK_REGISTER_AND_START(sidonlinemb, "create online mb");
    FinalOnlineModelBuilder mb(evalgraph);
    DLVHEX_BENCHMARK_STOP(sidonlinemb);

    // get and print all models
    OptionalModel m;
    DLVHEX_BENCHMARK_REGISTER(sidgetnextonlinemodel, "get next online model");
    //#ifndef NDEBUG
    unsigned mcount = 0;
    //#endif
    do
    {
      LOG("requesting model");
      DLVHEX_BENCHMARK_START(sidgetnextonlinemodel);
      m = mb.getNextIModel(ufinal);
      DLVHEX_BENCHMARK_STOP(sidgetnextonlinemodel);
      if( !!m )
      {
        InterpretationConstPtr interpretation =
          mb.getModelGraph().propsOf(m.get()).interpretation;
        //#ifndef NDEBUG
        LOG("got model#" << mcount << ":" << *interpretation);
        std::set<Model> onlyFor;
        onlyFor.insert(m.get());
        GraphVizFunc func = boost::bind(&writeEgMgGraphViz<MyModelGraph>, _1,
            true, boost::cref(mb.getEvalGraph()), boost::cref(mb.getModelGraph()), onlyFor);
        std::stringstream smodel;
        smodel << fname << "MCSIEOnlineModel" << mcount;
        writeGraphVizFunctors(func, func, smodel.str());
        mcount++;
        //#endif

        // output model
        {
          DLVHEX_BENCHMARK_SCOPE(sidoutputmodel);
          ob.printEQ(std::cout, interpretation);
        }
        //std::cerr << __FILE__ << ":" << __LINE__ << std::endl << *ctx.registry << std::endl;

        #ifndef NDEBUG
        mb.printEvalGraphModelGraph(std::cerr);
        #endif
      }
    }
    while( !!m );
    //#ifndef NDEBUG
    mb.printEvalGraphModelGraph(std::cerr);
    //#endif
    #ifndef NDEBUG
		GraphVizFunc func = boost::bind(&writeEgMgGraphViz<MyModelGraph>, _1,
				true, boost::cref(mb.getEvalGraph()), boost::cref(mb.getModelGraph()), boost::none);
		writeGraphVizFunctors(func, func, fname+"MCSIEOnlineEgMg");
    #endif
    //std::cerr << __FILE__ << ":" << __LINE__ << std::endl << *ctx.registry << std::endl;
  }
  else if( mbmode == "offline" )
  {
    typedef FinalOfflineModelBuilder::Model Model;
    typedef FinalOfflineModelBuilder::OptionalModel OptionalModel;
    typedef FinalOfflineModelBuilder::MyModelGraph MyModelGraph;

    //#  undef LOG
    //#  define LOG(streamout) do { std::ostringstream os; os << streamout; } while(false);
    //LOG("creating model builder");
    //{ std::ostringstream os; os << "creating model builder"; }
    DLVHEX_BENCHMARK_REGISTER_AND_START(sidofflinemb, "create offline mb");
    FinalOfflineModelBuilder mb(evalgraph);
    DLVHEX_BENCHMARK_STOP(sidofflinemb);

    LOG("creating all final imodels");
    DLVHEX_BENCHMARK_REGISTER_AND_START(sidofflinemodels, "create offline models");
    mb.buildIModelsRecursively(ufinal);
    DLVHEX_BENCHMARK_STOP(sidofflinemodels);
    //#ifndef NDEBUG
    mb.printEvalGraphModelGraph(std::cerr);
    //#endif

    LOG("printing models");
    DLVHEX_BENCHMARK_REGISTER_AND_START(sidprintoffmodels, "print offline models");
    MyModelGraph& mg = mb.getModelGraph();
    const MyModelGraph::ModelList& models = mg.modelsAt(ufinal, MT_IN);
    //#ifndef NDEBUG
    unsigned mcount = 0;
    //#endif

    BOOST_FOREACH(Model m, models)
    {
      InterpretationConstPtr interpretation =
        mg.propsOf(m).interpretation;
      //#ifndef NDEBUG
      LOG("got model#" << mcount << ":" << *interpretation);
      std::set<Model> onlyFor;
      onlyFor.insert(m);
      GraphVizFunc func = boost::bind(&writeEgMgGraphViz<MyModelGraph>, _1,
          true, boost::cref(mb.getEvalGraph()), boost::cref(mb.getModelGraph()), onlyFor);
      std::stringstream smodel;
      smodel << fname << "MCSIEOfflineModel" << mcount;
      writeGraphVizFunctors(func, func, smodel.str());
      mcount++;
      //#endif

      // output model
      {
        DLVHEX_BENCHMARK_SCOPE(sidoutputmodel);
        ob.printEQ(std::cout, interpretation);
      }
    }
    DLVHEX_BENCHMARK_STOP(sidprintoffmodels);
    #ifndef NDEBUG
		GraphVizFunc func = boost::bind(&writeEgMgGraphViz<MyModelGraph>, _1,
				true, boost::cref(mb.getEvalGraph()), boost::cref(mb.getModelGraph()), boost::none);
		writeGraphVizFunctors(func, func, fname+"MCSIEOfflineEgMg");
    #endif
  }
  else
  {
    std::cerr << "usage: <mbmode> must be one of 'online','offline'" << std::endl;
    return -1;
  }
  return 0;
}
