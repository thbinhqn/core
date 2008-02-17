/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005, 2006, 2007 Roman Schindlauer
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
 * 02110-1301 USA
 */


/**
 * @file   dlvhex.cpp
 * @author Roman Schindlauer
 * @date   Thu Apr 28 15:00:10 2005
 * 
 * @brief  main().
 * 
 */

/** @mainpage dlvhex Source Documentation
 *
 * \section intro_sec Overview
 *
 * You will look into the documentation of dlvhex most likely to implement a
 * plugin. In this case, please continue with the \ref pluginframework "Plugin
 * Interface Module", which contains all necessary information.
 *
 * For an overview on the logical primitives and datatypes used by dlvhex, see
 * \ref dlvhextypes "dlvhex Datatypes".
 */


/**
 * \defgroup dlvhextypes dlvhex Types
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H



#include "dlvhex/GraphProcessor.h"
#include "dlvhex/GraphBuilder.h"
#include "dlvhex/ComponentFinder.h"
#include "dlvhex/BoostComponentFinder.h"
#include "dlvhex/globals.h"
#include "dlvhex/Error.h"
#include "dlvhex/ResultContainer.h"
#include "dlvhex/OutputBuilder.h"
#include "dlvhex/TextOutputBuilder.h"
#include "dlvhex/RuleMLOutputBuilder.h"
#include "dlvhex/SafetyChecker.h"
#include "dlvhex/HexParserDriver.h"
#include "dlvhex/PrintVisitor.h"
#include "dlvhex/PluginContainer.h"
#include "dlvhex/DependencyGraph.h"
#include "dlvhex/ProgramBuilder.h"
#include "dlvhex/GraphProcessor.h"
#include "dlvhex/Component.h"
#include "dlvhex/URLBuf.h"


#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

#include <getopt.h>

#include <boost/tokenizer.hpp>

#ifdef DLVHEX_DEBUG
#include <boost/date_time/posix_time/posix_time.hpp>
#endif // DLVHEX_DEBUG


DLVHEX_NAMESPACE_USE


/// argv[0]
const char*  WhoAmI;



/**
 * @brief Print logo.
 */
void
printLogo()
{
	std::cout
		<< "DLVHEX "
#ifdef HAVE_CONFIG_H
		<< VERSION << " "
#endif // HAVE_CONFIG_H
		<< "[build "
		<< __DATE__ 
#ifdef __GNUC__
		<< "   gcc " << __VERSION__ 
#endif
		<< "]" << std::endl
		<< std::endl;
}


/**
 * @brief Print usage help.
 */
void
printUsage(std::ostream &out, bool full)
{
	//      123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	out << "Usage: " << WhoAmI 
		<< " [OPTION] FILENAME [FILENAME ...]" << std::endl
		<< std::endl;

	out << "   or: " << WhoAmI 
		<< " [OPTION] --" << std::endl
		<< std::endl;

	if (!full)
	{
		out << "Specify -h or --help for more detailed usage information." << std::endl
			<< std::endl;

		return;
	}

	//
	// As soos as we have more options, we can introduce sections here!
	//
	//      123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	out << "     --               Parse from stdin." << std::endl
		<< " -s, --silent         Do not display anything than the actual result." << std::endl
		//        << "--strongsafety     Check rules also for strong safety." << std::endl
		<< " -p, --plugindir=dir  Specify additional directory where to look for plugin" << std::endl
		<< "                      libraries (additionally to the installation plugin-dir" << std::endl
		<< "                      and $HOME/.dlvhex/plugins)." << std::endl
		<< " -f, --filter=foo[,bar[,...]]" << std::endl
		<< "                      Only display instances of the specified predicate(s)." << std::endl
		<< " -a, --allmodels      Display all models also under weak constraints." << std::endl
		<< " -r, --reverse        Reverse weak constraint ordering." << std::endl
		<< "     --firstorder     No higher-order reasoning." << std::endl
		<< "     --ruleml         Output in RuleML-format (v0.9)." << std::endl
		<< "     --noeval         Just parse the program, don't evaluate it (only useful" << std::endl
		<< "                      with --verbose)." << std::endl
		<< "     --keepnsprefix   Keep specified namespace-prefixes in the result." << std::endl
		<< " -v, --verbose[=N]    Specify verbose category (default: 1):" << std::endl
		<< "                      1  - program analysis information (including dot-file)" << std::endl
		<< "                      2  - program modifications by plugins" << std::endl
		<< "                      4  - intermediate model generation info" << std::endl
		<< "                      8  - timing information (only if configured with" << std::endl
		<< "                                               --enable-debug)" << std::endl
		<< "                      add values for multiple categories." << std::endl;
//		<< std::endl;
}


/**
 * @brief Print a fatal error message and terminate.
 */
void
InternalError (const char *msg)
{
  std::cerr << std::endl
	    << "An internal error occurred (" << msg << ")."
	    << std::endl
	    << "Please contact <" PACKAGE_BUGREPORT ">." << std::endl;
  exit (99);
}



///@brief predicate returns true iff argument is not alpha-numeric and
///is not one of {_,-,.} characters, i.e., it returns true if
///characater does not belong to XML's NCNameChar character class.
struct NotNCNameChar : public std::unary_function<char, bool>
{
  bool
  operator() (char c)
  {
    c = std::toupper(c);
    return
      (c < 'A' || c > 'Z') &&
      (c < '0' || c > '9') &&
      c != '-' &&
      c != '_' &&
      c != '.';
  }
};


void
insertNamespaces()
{
  ///@todo move this stuff to Term, this has nothing to do here!

  if (Term::namespaces.size() == 0)
    return;

  std::string prefix;

  for (NamesTable<std::string>::const_iterator nm = Term::names.begin();
       nm != Term::names.end();
       ++nm)
    {
      for (std::vector<std::pair<std::string, std::string> >::iterator ns = Term::namespaces.begin();
	   ns != Term::namespaces.end();
	   ++ns)
	{
	  prefix = ns->second + ':';

	  //
	  // prefix must occur either at beginning or right after quote
	  //
	  unsigned start = 0;
	  unsigned end = (*nm).length();

	  if ((*nm)[0] == '"')
	    {
	      ++start;
	      --end;
	    }

	    
	  //
	  // accourding to http://www.w3.org/TR/REC-xml-names/ QNames
	  // consist of a prefix followed by ':' and a LocalPart, or
	  // just a LocalPart. In case of a single LocalPart, we would
	  // not find prefix and leave that Term alone. If we find a
	  // prefix in the Term, we must disallow non-NCNames in
	  // LocalPart, otw. we get in serious troubles when replacing
	  // proper Terms:
	  // NameChar ::= Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender  
	  //

	  std::string::size_type colon = (*nm).find(":", start);
					  
	  if (colon != std::string::npos) // Prefix:LocalPart
	    {
	      std::string::const_iterator it =
		std::find_if((*nm).begin() + colon + 1, (*nm).begin() + end - 1, NotNCNameChar());

	      // prefix starts with ns->second, LocalPart does not
	      // contain non-NCNameChars, hence we can replace that
	      // Term
	      if ((*nm).find(prefix, start) == start &&
		  (it == (*nm).begin() + end - 1)
		  )
		{
		  std::string r(*nm);
	      
		  r.replace(start, prefix.length(), ns->first); // replace ns->first from start to prefix + 1
		  r.replace(0, 1, "\"<");
		  r.replace(r.length() - 1, 1, ">\"");
	      
		  Term::names.modify(nm, r);
		}
	    }
	}
    }
}



void
removeNamespaces()
{
  ///@todo move this stuff to Term, this has nothing to do here!

  if (Term::namespaces.size() == 0)
    return;

  std::string prefix, fullns;

  for (NamesTable<std::string>::const_iterator nm = Term::names.begin();
       nm != Term::names.end();
       ++nm)
    {
      for (std::vector<std::pair<std::string, std::string> >::iterator ns = Term::namespaces.begin();
	   ns != Term::namespaces.end();
	   ++ns)
	{
	  fullns = ns->first;

	  prefix = ns->second + ":";

	  //
	  // original ns must occur either at beginning or right after quote
	  //
	  unsigned start = 0;
	  if ((*nm)[0] == '"')
	    start = 1;

	  if ((*nm).find(fullns, start) == start)
	    {
	      std::string r(*nm);

	      r.replace(start, fullns.length(), prefix);

	      Term::names.modify(nm, r);
	    }
	}
    }
}



#include <ext/stdio_filebuf.h> 

int
main (int argc, char *argv[])
{
	//
	// Stores the rules of the program.
	//
	Program IDB;

	//
	// Stores the facts of the program.
	//
	AtomSet EDB;


	WhoAmI = argv[0];

	/////////////////////////////////////////////////////////////////
	//
	// Option handling
	//
	/////////////////////////////////////////////////////////////////

	// global defaults:
	Globals::Instance()->setOption("NoPredicate", 1);
	Globals::Instance()->setOption("Silent", 0);
	Globals::Instance()->setOption("Verbose", 0);
	Globals::Instance()->setOption("NoPredicate", 1);
	Globals::Instance()->setOption("StrongSafety", 1);
	Globals::Instance()->setOption("AllModels", 0);
	Globals::Instance()->setOption("ReverseAllModels", 0);

	// options only used here in main():
	bool optionPipe = false;
	bool optionXML = false;
	bool optionNoEval = false;
	bool optionKeepNSPrefix = false;

	std::string optionPlugindir("");

	//
	// dlt switch should be temporary until we have a proper rewriter for flogic!
	//
	bool optiondlt = false;

	std::vector<std::string> allFiles;

	std::vector<std::string> remainingOptions;


	extern char* optarg;
	extern int optind, opterr;

	bool helpRequested = 0;

	//
	// prevent error message for unknown options - they might be known to
	// plugins later!
	//
	opterr = 0;

	int ch;
	int longid;

	static const char* shortopts = "f:hsvp:ar";
	static struct option longopts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "silent", no_argument, 0, 's' },
		{ "verbose", optional_argument, 0, 'v' },
		{ "filter", required_argument, 0, 'f' },
		{ "plugindir", required_argument, 0, 'p' },
		{ "allmodels", no_argument, 0, 'a' },
		{ "reverse", no_argument, 0, 'r' },
		{ "firstorder", no_argument, &longid, 1 },
		{ "weaksafety", no_argument, &longid, 2 },
		{ "ruleml",     no_argument, &longid, 3 },
		{ "dlt",        no_argument, &longid, 4 },
		{ "noeval",     no_argument, &longid, 5 },
		{ "keepnsprefix", no_argument, &longid, 6 },
		{ NULL, 0, NULL, 0 }
	};

	while ((ch = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (ch)
		{
			case 'h':
				//printUsage(std::cerr, true);
				helpRequested = 1;
				break;
			case 's':
				Globals::Instance()->setOption("Silent", 1);
				break;
			case 'v':
				if (optarg)
					Globals::Instance()->setOption("Verbose", atoi(optarg));
				else
					Globals::Instance()->setOption("Verbose", 1);
				break;
			case 'f':
			  {
			    boost::char_separator<char> sep(",");
			    std::string oa(optarg); // g++ 3.3 is unable to pass that at the ctor line below
			    boost::tokenizer<boost::char_separator<char> > tok(oa, sep);
			    
			    for (boost::tokenizer<boost::char_separator<char> >::const_iterator f = tok.begin();
				 f != tok.end(); ++f)
			      {
				Globals::Instance()->addFilter(*f);
			      }
			  }
			  break;

			case 'p':
				optionPlugindir = std::string(optarg);
				break;
			case 'a':
				Globals::Instance()->setOption("AllModels", 1);
				break;
			case 'r':
				Globals::Instance()->setOption("ReverseOrder", 1);
				break;
			case 0:
			  switch (longid)
			    {
			    case 1:
			      Globals::Instance()->setOption("NoPredicate", 0);
			      break;
			    case 2:
			      Globals::Instance()->setOption("StrongSafety", 0);
			      break;
			    case 3:
			      optionXML = true;
			      // XML output makes only sense with silent:
			      Globals::Instance()->setOption("Silent", 1);
			      break;
			    case 4:
			      optiondlt = true;
			      break;
			    case 5:
			      optionNoEval = true;
			      break;
			    case 6:
			      optionKeepNSPrefix = true;
			      break;
			    }
			  break;
			case '?':
				remainingOptions.push_back(argv[optind - 1]);
				break;
		}
	}

	//
	// before anything else we dump the logo
	//

	if (!Globals::Instance()->getOption("Silent"))
		printLogo();

	//
	// no arguments at all: shorthelp
	//
	if (argc == 1)
	{
		printUsage(std::cerr, false);

		exit(1);
	}

	bool inputIsWrong = 0;

	//
	// check if we have any input (stdin or file)
	// if inout is not or badly specified, remember this and show shorthelp
	// later if everthing was ok with the options
	//

	//
	// stdin requested
	//
	if (!strcmp(argv[optind - 1], "--"))
	{
		optionPipe = true;
	}

	if (optind == argc)
	{
		//
		// no files and no stdin - error
		//
		if (!optionPipe)
			inputIsWrong = 1;
	}
	else
	{
		//
		// files and stdin - error
		//
		if (optionPipe)
			inputIsWrong = 1;

		//
		// collect filenames
		//
		for (int i = optind; i < argc; ++i)
		{
			allFiles.push_back(argv[i]);
		}
	}



	/////////////////////////////////////////////////////////////////
	//
	// now search for plugins
	//
	/////////////////////////////////////////////////////////////////
	
#ifdef DLVHEX_DEBUG
	DEBUG_START_TIMER
#endif // DLVHEX_DEBUG

	PluginContainer* container = PluginContainer::instance(optionPlugindir);

	std::vector<PluginInterface*> plugins = container->importPlugins();

	std::stringstream allpluginhelp;


	//
	// set options in the found plugins
	//
	for (std::vector<PluginInterface*>::const_iterator pi = plugins.begin();
	     pi != plugins.end(); ++pi)
	  {
	    try
	      {
		PluginInterface* plugin = *pi;

		if (plugin != 0)
		  {
		    // print plugin's version information
		    if (!Globals::Instance()->getOption("Silent"))
		      {
			Globals::Instance()->getVerboseStream() << "opening "
								<< plugin->getPluginName()
								<< " version "
								<< plugin->getVersionMajor() << "."
								<< plugin->getVersionMinor() << "."
								<< plugin->getVersionMicro() << std::endl;
		      }

		    std::stringstream pluginhelp;
			  
		    plugin->setOptions(helpRequested, remainingOptions, pluginhelp);

		    if (!pluginhelp.str().empty())
		      {
			allpluginhelp << std::endl << pluginhelp.str();
		      }
		  }
	      }
	    catch (GeneralError &e)
	      {
		std::cerr << e.getErrorMsg() << std::endl;
		
		exit(1);
	      }
	  }

#ifdef DLVHEX_DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Importing plugins                      ")
#endif // DLVHEX_DEBUG

	if (!Globals::Instance()->getOption("Silent"))
		std::cout << std::endl;


	//
	// help was requested?
	//
	if (helpRequested)
	{
		printUsage(std::cerr, true);
		std::cerr << allpluginhelp.str() << std::endl;
		exit(0);
	}

	//
	// any unknown options left?
	//
	if (!remainingOptions.empty())
	{
		std::cerr << "Unknown option(s):";

		std::vector<std::string>::const_iterator opb = remainingOptions.begin();

		while (opb != remainingOptions.end())
			std::cerr << " " << *opb++;

		std::cerr << std::endl;
		printUsage(std::cerr, false);

		exit(1);
	}

	//
	// options are all ok, but input is missing
	//
	if (inputIsWrong)
	{
		printUsage(std::cerr, false);

		exit(1);
	}


	/////////////////////////////////////////////////////////////////
	//
	// parse input
	//
	/////////////////////////////////////////////////////////////////

#ifdef DLVHEX_DEBUG
	DEBUG_RESTART_TIMER
#endif // DLVHEX_DEBUG

	HexParserDriver driver;

	if (optionPipe)
	{
		//
		// if we are piping, use a dummy file-name in order to enter the
		// file-loop below
		//
		allFiles.push_back(std::string("dummy"));

		Globals::Instance()->lpfilename = "lpgraph.dot";
	}

		//
		// store filename of (first) logic program, we might use this somewhere
		// else (e.g., when writing the graphviz file in the boost-part
		//
		std::string lpfile = allFiles[0];
		Globals::Instance()->lpfilename = lpfile.substr(lpfile.find_last_of("/") + 1) + ".dot";

		for (std::vector<std::string>::const_iterator f = allFiles.begin();
		     f != allFiles.end();
		     ++f)
		  {
		    try
		      {
			//
			// stream to store the url/file/stdin content
			//
			std::stringstream tmpin;

			if (!optionPipe)
			  {
			    // URL
			    if (f->find("http://") == 0)
			      {
				URLBuf ubuf;
				ubuf.open(*f);
				std::istream is(&ubuf);

				driver.setOrigin(*f);

				tmpin << is.rdbuf();

				if (ubuf.responsecode() == 404)
				  {
				    throw GeneralError("Requested URL " + *f + " was not found");
				  }
			      }
			    else // file
			      {
				std::ifstream ifs;

				ifs.open(f->c_str());

				if (!ifs.is_open())
				  {
				    throw GeneralError("File " + *f + " not found");
				  }

				//
				// tell the parser driver where the rules are actually coming
				// from (needed for error-messages)
				//
				driver.setOrigin(*f);

				tmpin << ifs.rdbuf();
				ifs.close();
			      }
			  }
			else
			  {
			    //
			    // stdin
			    //
			    tmpin << std::cin.rdbuf();
			  }

			//
			// create a stringbuffer on the heap (will be deleted later) to
			// hold the file-content. put it into a stream "input"
			//	
			std::istream input(new std::stringbuf(tmpin.str()));

			//
			// new output stream with stringbuffer on the heap
			//
			std::ostream converterResult(new std::stringbuf);

			bool wasConverted(0);

			for (std::vector<PluginInterface*>::iterator pi = plugins.begin();
			     pi != plugins.end();
			     ++pi)
			  {
			    PluginConverter* pc = (*pi)->createConverter();

			    if (pc != NULL)
			      {
				//
				// rewrite input -> converterResult
				//
				pc->convert(input, converterResult);

				wasConverted = 1;

				//
				// old input buffer can be deleted now
				// (but not if we are piping from stdin and this is the
				// first conversion, because in this case input is set to
				// std::cin.rdbuf() (see above) and cin takes care of
				// its buffer deletion itself, so better don't
				// interfere!)
				//
				if (optionPipe && !wasConverted)
				  delete input.rdbuf();

				//
				// store the current output buffer
				//
				std::streambuf* tmp = converterResult.rdbuf();

				//
				// make a new buffer for the output (=reset the output)
				//
				converterResult.rdbuf(new std::stringbuf);

				//
				// set the input buffer to be the output of the last
				// rewriting. now, after each loop, the converted
				// program is in input.
				//
				input.rdbuf(tmp);

			      }
			  }
			char tempfile[L_tmpnam];


			//
			// at this point, the program is in the stream "input" - wither
			// directly read from the file or as a result of some previous
			// rewriting!
			//

			if (Globals::Instance()->doVerbose(Globals::DUMP_CONVERTED_PROGRAM) && wasConverted)
			  {
			    //
			    // we need to read the input-istream now - use a stringstream
			    // for output and initialize the input-istream to its
			    // content again
			    //
			    std::stringstream ss;
			    ss << input.rdbuf();
			    Globals::Instance()->getVerboseStream() << "Converted input:" << std::endl;
			    Globals::Instance()->getVerboseStream() << ss.str();
			    Globals::Instance()->getVerboseStream() << std::endl;
			    delete input.rdbuf(); 
			    input.rdbuf(new std::stringbuf(ss.str()));
			  }

			FILE* fp = 0;

			//
			// now call dlt if needed
			//
			if (optiondlt)
			  {
			    mkstemp(tempfile);
			    
			    std::ofstream dlttemp(tempfile);
			    
			    //
			    // write program into tempfile
			    //
			    dlttemp << input.rdbuf();
			    
			    dlttemp.close();

			    std::string execPreParser("dlt -silent -preparsing " + std::string(tempfile));
			    
			    fp = popen(execPreParser.c_str(), "r");
			    
			    if (fp == NULL)
			      {
				throw GeneralError("Unable to call Preparser dlt");
			      }

			    __gnu_cxx::stdio_filebuf<char>* fb;
			    fb = new __gnu_cxx::stdio_filebuf<char>(fp, std::ios::in);
			    
			    std::istream inpipe(fb);

			    //
			    // now we have a program rewriten by dlt - since it should
			    // be in the stream "input", we have to delete the old
			    // input-buffer and set input to the buffer from the
			    // dlt-call
			    //
			    delete input.rdbuf(); 
			    input.rdbuf(fb);
			  }

			// run the parser
			driver.parse(input, IDB, EDB);

			if (optiondlt)
			  {
			    int dltret = pclose(fp);
			    
			    if (dltret != 0)
			      {
				throw GeneralError("Preparser dlt returned error");
			      }
			  }

			//
			// wherever the input-buffer was created before - now we don't
			// need it anymore
			//
			delete input.rdbuf();

			if (optiondlt)
			  {
			    unlink(tempfile);
			  }
		      }
		    catch (SyntaxError& e)
		      {
			std::cerr << e.getErrorMsg() << std::endl;
			exit(1);
		      }
		    catch (GeneralError& e)
		      {
			std::cerr << e.getErrorMsg() << std::endl;
			exit(1);
		      }
		  }
//	}

#ifdef DLVHEX_DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Parsing and converting input           ")
#endif // DLVHEX_DEBUG

	//
	// expand constant names
	//
	insertNamespaces();

	if (Globals::Instance()->doVerbose(Globals::DUMP_PARSED_PROGRAM))
	{
		Globals::Instance()->getVerboseStream() << "Parsed Rules: " << std::endl;
		RawPrintVisitor rpv(Globals::Instance()->getVerboseStream());
		IDB.dump(rpv);
		Globals::Instance()->getVerboseStream() << std::endl << "Parsed EDB: " << std::endl;
		EDB.accept(rpv);
		Globals::Instance()->getVerboseStream() << std::endl << std::endl;
	}


#ifdef DLVHEX_DEBUG
	DEBUG_RESTART_TIMER
#endif // DLVHEX_DEBUG
			
	//
	// now call rewriters
	//
	bool wasRewritten(0);

	for (std::vector<PluginInterface*>::iterator pi = plugins.begin();
			pi != plugins.end();
			++pi)
	{
		PluginRewriter* pr = (*pi)->createRewriter();

		if (pr != NULL)
		{
			pr->rewrite(IDB, EDB);

			wasRewritten = 1;
		}
	}

#ifdef DLVHEX_DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Calling plugin rewriters               ")
#endif // DLVHEX_DEBUG

	if (Globals::Instance()->doVerbose(Globals::DUMP_REWRITTEN_PROGRAM) && wasRewritten)
	{
		Globals::Instance()->getVerboseStream() << "Rewritten rules:" << std::endl;
		RawPrintVisitor rpv(Globals::Instance()->getVerboseStream());
		IDB.dump(rpv);
		Globals::Instance()->getVerboseStream() << std::endl << "Rewritten EDB:" << std::endl;
		EDB.accept(rpv);
		Globals::Instance()->getVerboseStream() << std::endl << std::endl;
	}


	/// @todo: when exiting after an exception, we have to cleanup things!
	// maybe using boost-pointers!

#ifdef DLVHEX_DEBUG
	DEBUG_RESTART_TIMER
#endif // DLVHEX_DEBUG

	//
	// The GraphBuilder creates nodes and dependency edges from the raw program.
	//
	GraphBuilder gb;

	NodeGraph nodegraph;

    try
    {
      gb.run(IDB, nodegraph, *container);
    }
    catch (GeneralError& e)
    {
		std::cerr << e.getErrorMsg() << std::endl << std::endl;

		exit(1);
    }

#ifdef DLVHEX_DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Building dependency graph              ")
#endif // DLVHEX_DEBUG

    if (Globals::Instance()->doVerbose(Globals::DUMP_DEPENDENCY_GRAPH))
    {
        gb.dumpGraph(nodegraph, Globals::Instance()->getVerboseStream());
    }
    

#ifdef DLVHEX_DEBUG
	DEBUG_RESTART_TIMER
#endif // DLVHEX_DEBUG

	//
	// now call optimizers
	//
	bool wasOptimized(10);

	for (std::vector<PluginInterface*>::iterator pi = plugins.begin();
			pi != plugins.end();
			++pi)
	{
		PluginOptimizer* po = (*pi)->createOptimizer();

		if (po != NULL)
		{
			po->optimize(nodegraph, EDB);

			wasOptimized = 1;
		}
	}

#ifdef DLVHEX_DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Calling plugins optimizers             ")
#endif // DLVHEX_DEBUG

	if (Globals::Instance()->doVerbose(Globals::DUMP_OPTIMIZED_PROGRAM) && wasOptimized)
	{
		Globals::Instance()->getVerboseStream() << "Optimized graph:" << std::endl;
        gb.dumpGraph(nodegraph, Globals::Instance()->getVerboseStream());
		Globals::Instance()->getVerboseStream() << std::endl << "Optimized EDB:" << std::endl;
		RawPrintVisitor rpv(Globals::Instance()->getVerboseStream());
		EDB.accept(rpv);
		Globals::Instance()->getVerboseStream() << std::endl << std::endl;
	}



	//
	// The ComponentFinder provides functions for finding SCCs and WCCs from a
	// set of nodes.
	//
	ComponentFinder* cf;

	//
	// The DependencyGraph identifies and creates the graph components that will
	// be processed by the GraphProcessor.
	//
	DependencyGraph* dg;

	try
	  {
	    cf = new BoostComponentFinder;

	    //
	    // Initializing the DependencyGraph. Its constructor uses the
	    // ComponentFinder to find relevant graph
	    // properties for the subsequent processing stage.
	    //
	    dg = new DependencyGraph(nodegraph, cf, *container);


	    //
	    // Performing the safety check
	    //
	    SafetyChecker schecker(IDB);
	    schecker();
	    
	    ///@todo why should we turn off strong safety check?
	    if (Globals::Instance()->getOption("StrongSafety"))
	      {
		StrongSafetyChecker sschecker(*dg);
		sschecker();
	      }
	  }
	catch (GeneralError &e)
	  {
	    std::cerr << e.getErrorMsg() << std::endl << std::endl;

	    exit(1);
	  }
	

	if (optionNoEval)
	{
		delete dg;
		delete cf;

		exit(0);
	}

	//
	// The GraphProcessor provides the actual strategy of how to compute the
	// hex-models of a given dependency graph.
	//
	GraphProcessor gp(dg);


	try
	{
		//
		// The GraphProcessor starts its computation with the program's ground
		// facts as input.
		// But only if the original EDB is consistent, otherwise, we can skip it
		// anyway.
		//
		if (EDB.isConsistent())
			gp.run(EDB);
	}
	catch (GeneralError &e)
	{
		std::cerr << e.getErrorMsg() << std::endl << std::endl;

		exit(1);
	}

	//
	// contract constant names again, if specified
	//
	if (optionKeepNSPrefix)
		removeNamespaces();


	///@todo weak contraint prefixes are a bit clumsy here. How can we do better?

	//
	// prepare result container
	//
	// if we had any weak constraints, we have to tell the result container the
	// prefix in order to be able to compute each asnwer set's costs!
	//
	std::string wcprefix;

	if (IDB.getWeakConstraints().size() > 0)
		wcprefix = "wch__";

	ResultContainer result(wcprefix);

#ifdef DLVHEX_DEBUG
	DEBUG_RESTART_TIMER
#endif // DLVHEX_DEBUG

	//
	// put GraphProcessor result into ResultContainer
	//
	AtomSet* res;

	while ((res = gp.getNextModel()) != NULL)
	{
		try
		{
			result.addSet(*res);
		}
		catch (GeneralError &e)
		{
			std::cerr << e.getErrorMsg() << std::endl << std::endl;

			exit(1);
		}
	}


	///@todo filtering the atoms here is maybe to costly, how
	///about ignoring the aux names when building the output,
	///since the custom output builders of the plugins may need
	///the aux names? Likewise for --filter predicates...

	//
	// remove auxiliary atoms
	//
	result.filterOut(Term::getAuxiliaryNames());

	///@todo quick hack for dlt
	if (optiondlt)
		result.filterOutDLT();


	//
	// apply filter
	//
	//if (optionFilter.size() > 0)
	result.filterIn(Globals::Instance()->getFilters());


#ifdef DEBUG
	//                123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789-
	DEBUG_STOP_TIMER("Postprocessing GraphProcessor result             ")
#endif // DEBUG



	//
	// output format
	//
	OutputBuilder* outputbuilder = 0;

	// first look if some plugin has an OutputBuilder
	for (std::vector<PluginInterface*>::const_iterator pi = plugins.begin();
	     pi != plugins.end(); ++pi)
	  {
	    ///@todo this is very clumsy, what should we do if there
	    ///are more than one output builders available from the
	    ///atoms?
	    outputbuilder = (*pi)->createOutputBuilder();
	  }

	// if no plugin provides an OutputBuilder, we use our own to output the models
	if (outputbuilder == 0)
	  {
	    if (optionXML)
	      {
		outputbuilder = new RuleMLOutputBuilder;
	      }
	    else
	      {
		outputbuilder = new TextOutputBuilder;
	      }
	  }

	result.print(std::cout, outputbuilder);
	    

	//
	// was there anything non-error the user should know? dump it directly
	/*
	   for (std::vector<std::string>::const_iterator l = global::Messages.begin();
	   l != global::Messages.end();
	   l++)
	   {
	   std::cout << *l << std::endl;
	   }
	   */

	//
	// cleaning up:
	//
	delete outputbuilder;

	delete cf;

	delete dg;

	exit(0);
}


/* vim: set noet sw=4 ts=4 tw=80: */


// Local Variables:
// mode: C++
// End:
