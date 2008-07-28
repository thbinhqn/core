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
 * 02110-1301 USA.
 */


/**
 * @file   BaseQuery.h
 * @author Thomas Krennwallner
 * @date   Tue Jul 15 12:09:08 2008
 * 
 * @brief  The base class for all query types.
 * 
 * 
 */


#if !defined(_DLVHEX_BASEQUERY_H)
#define _DLVHEX_BASEQUERY_H

#include "dlvhex/PlatformDefinitions.h"

#include "dlvhex/ProgramNode.h"
#include "dlvhex/QueryTraits.h"
#include "dlvhex/Body.h"

DLVHEX_NAMESPACE_BEGIN

class BaseVisitor;

/**
 * @brief The baseclass for all query types.
 */
class DLVHEX_EXPORT BaseQuery : public ProgramNode
{
public:
  virtual
  ~BaseQuery();

  virtual void
  evaluate() = 0;

  virtual BodyPtr&
  body() = 0;

  virtual const BodyPtr&
  body() const = 0;


  /// the comparison template method
  virtual int
  compare(const BaseQuery&) const = 0;


  /**
   * @brief Test for equality.
   */
  inline bool
  operator== (const BaseQuery& query2) const
  {
    return compare(query2) == 0;
  }


  /**
   * @brief Test for inequality.
   */
  inline bool
  operator!= (const BaseQuery& query2) const
  {
    return compare(query2) != 0;
  }


  /**
   * @brief Less-than comparison.
   */
  inline bool
  operator< (const BaseQuery& query2) const
  {
    return compare(query2) < 0;
  }

  virtual void
  accept(BaseVisitor* const) = 0;
};


std::ostream&
operator<<(std::ostream&, BaseQuery&);


DLVHEX_NAMESPACE_END

#endif /* _DLVHEX_BASEQUERY_H */


// Local Variables:
// mode: C++
// End:
