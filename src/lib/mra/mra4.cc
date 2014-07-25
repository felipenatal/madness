/*
  This file is part of MADNESS.
  
  Copyright (C) 2007,2010 Oak Ridge National Laboratory
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
  
  For more information please contact:
  
  Robert J. Harrison
  Oak Ridge National Laboratory
  One Bethel Valley Road
  P.O. Box 2008, MS-6367
  
  email: harrisonrj@ornl.gov
  tel:   865-241-3937
  fax:   865-572-0680
  
  $Id$
*/

#include <mra/mra.h>
#define MPRAIMPLX
#include <mra/mraimpl.h>
#include <world/worldobj.h>
#include <world/worldmutex.h>
#include <list>


#ifdef FUNCTION_INSTANTIATE_4

namespace madness {
    template class FunctionDefaults<4>;
    template class Function<double, 4>;
    template class Function<std::complex<double>, 4>;
    template class FunctionImpl<double, 4>;
    template class FunctionImpl<std::complex<double>, 4>;
    template class FunctionCommonData<double, 4>;
    template class FunctionCommonData<double_complex, 4>;
    template class Displacements<4>;
    template class DerivativeBase<double,4>;
    template class DerivativeBase<double_complex,4>;

    template void plotdx<double,4>(const Function<double,4>&, const char*, const Tensor<double>&,
                                   const std::vector<long>&, bool binary);
    template void plotdx<double_complex,4>(const Function<double_complex,4>&, const char*, const Tensor<double>&,
                                           const std::vector<long>&, bool binary);

    template void fcube<double,4>(const Key<4>&, const FunctionFunctorInterface<double,4>&, const Tensor<double>&, Tensor<double>&);
    template Tensor<double> fcube<double, 4>(Key<4> const&, double (*)(Vector<double, 4> const&), Tensor<double> const&);
    template void fcube<std::complex<double>,4>(const Key<4>&, const FunctionFunctorInterface<std::complex<double>,4>&, const Tensor<double>&, Tensor<std::complex<double> >&);
  template Tensor<std::complex<double> > fcube<std::complex<double>, 4>(Key<4> const&, std::complex<double> (*)(Vector<double, 4> const&), Tensor<double> const&);

    volatile std::list<detail::PendingMsg> WorldObject<FunctionImpl<double,4> >::pending;
    Spinlock WorldObject<FunctionImpl<double,4> >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<madness::FunctionImpl<std::complex<double>, 4> >::pending;
    Spinlock WorldObject<FunctionImpl<std::complex<double>, 4> >::pending_mutex;

    volatile std::list<detail::PendingMsg> WorldObject<WorldContainerImpl<Key<4>, FunctionNode<double, 4>, Hash<Key<4> > > >::pending;
    Spinlock WorldObject<WorldContainerImpl<Key<4>, FunctionNode<double, 4>, Hash<Key<4> > > >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<WorldContainerImpl<Key<4>, FunctionNode<std::complex<double>, 4>, Hash<Key<4> > > >::pending;
    Spinlock WorldObject<WorldContainerImpl<Key<4>, FunctionNode<std::complex<double>, 4>, Hash<Key<4> > > >::pending_mutex;

    volatile std::list<detail::PendingMsg> WorldObject<DerivativeBase<double,4> >::pending;
    Spinlock WorldObject<DerivativeBase<double,4> >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<DerivativeBase<std::complex<double>,4> >::pending;
    Spinlock WorldObject<DerivativeBase<std::complex<double>,4> >::pending_mutex;

}
#endif

