/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005-2007 Roman Schindlauer
 * Copyright (C) 2006-2015 Thomas Krennwallner
 * Copyright (C) 2009-2016 Peter Schüller
 * Copyright (C) 2011-2016 Christoph Redl
 * Copyright (C) 2015-2016 Tobias Kaminski
 * Copyright (C) 2015-2016 Antonius Weinzierl
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
 * @file AggregatePlugin.h
 * @author Christoph Redl
 *
 * @brief Implements DLV aggregates based on external atoms
 */

#ifndef AGGREGATES_PLUGIN__HPP_INCLUDED
#define AGGREGATES_PLUGIN__HPP_INCLUDED

#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/PluginInterface.h"

DLVHEX_NAMESPACE_BEGIN

/** \brief Implements aggregate functions both by native handling or by rewriting them to external atoms. */
class AggregatePlugin:
public PluginInterface
{
    public:
        // stored in ProgramCtx, accessed using getPluginData<HigherOrderPlugin>()
        class CtxData:
    public PluginData
    {
        public:
            /** \brief Stores if plugin is enabled. */
            bool enabled;

            /** \brief Supported plugin modes. */
            enum Mode
            {
                /** \brief Rewrting aggregates to external atoms. */
                ExtRewrite,
                /** \brief Rewrting aggregates to boolean external atoms. */
                ExtBlRewrite,
                /** \brief Simplify them such that they can be natively handled. */
                Simplify
            };
            /** \brief Selected mode. */
            Mode mode;

            CtxData();
            virtual ~CtxData() {};
    };

    public:
        /** \brief Constructor. */
        AggregatePlugin();
        /** \brief Destructor. */
        virtual ~AggregatePlugin();

        // output help message for this plugin
        virtual void printUsage(std::ostream& o) const;

        // accepted options: --aggregate-enable
        //
        // processes options for this plugin, and removes recognized options from pluginOptions
        // (do not free the pointers, the const char* directly come from argv)
        virtual void processOptions(std::list<const char*>& pluginOptions, ProgramCtx&);

        // rewrite program: rewrite aggregate atoms to external atoms
        virtual PluginRewriterPtr createRewriter(ProgramCtx&);

        // register model callback which transforms all auxn(p,t1,...,tn) back to p(t1,...,tn)
        virtual void setupProgramCtx(ProgramCtx&);

        virtual std::vector<PluginAtomPtr> createAtoms(ProgramCtx&) const;

        // no atoms!
};

DLVHEX_NAMESPACE_END
#endif

// vim:expandtab:ts=4:sw=4:
// mode: C++
// End:
