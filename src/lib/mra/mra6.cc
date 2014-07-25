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

#ifdef FUNCTION_INSTANTIATE_6
namespace madness {
    template class FunctionDefaults<6>;
    template class Function<double, 6>;
    template class Function<std::complex<double>, 6>;
    template class FunctionImpl<double, 6>;
    template class FunctionImpl<std::complex<double>, 6>;
    template class FunctionCommonData<double, 6>;
    template class FunctionCommonData<double_complex, 6>;
    template class Displacements<6>;
    template class DerivativeBase<double,6>;
    template class DerivativeBase<double_complex,6>;

    template void plotdx<double,6>(const Function<double,6>&, const char*, const Tensor<double>&,
                                   const std::vector<long>&, bool binary);
    template void plotdx<double_complex,6>(const Function<double_complex,6>&, const char*, const Tensor<double>&,
                                           const std::vector<long>&, bool binary);

    template void fcube<double,6>(const Key<6>&, const FunctionFunctorInterface<double,6>&, const Tensor<double>&, Tensor<double>&);
    template Tensor<double> fcube<double, 6>(Key<6> const&, double (*)(Vector<double, 6> const&), Tensor<double> const&);
    template void fcube<std::complex<double>,6>(const Key<6>&, const FunctionFunctorInterface<std::complex<double>,6>&, const Tensor<double>&, Tensor<std::complex<double> >&);
  template Tensor<std::complex<double> > fcube<std::complex<double>, 6>(Key<6> const&, std::complex<double> (*)(Vector<double, 6> const&), Tensor<double> const&);

    volatile std::list<detail::PendingMsg> WorldObject<FunctionImpl<double,6> >::pending;
    Spinlock WorldObject<FunctionImpl<double,6> >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<madness::FunctionImpl<std::complex<double>, 6> >::pending;
    Spinlock WorldObject<FunctionImpl<std::complex<double>, 6> >::pending_mutex;

    volatile std::list<detail::PendingMsg> WorldObject<WorldContainerImpl<Key<6>, FunctionNode<double, 6>, Hash<Key<6> > > >::pending;
    Spinlock WorldObject<WorldContainerImpl<Key<6>, FunctionNode<double, 6>, Hash<Key<6> > > >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<WorldContainerImpl<Key<6>, FunctionNode<std::complex<double>, 6>, Hash<Key<6> > > >::pending;
    Spinlock WorldObject<WorldContainerImpl<Key<6>, FunctionNode<std::complex<double>, 6>, Hash<Key<6> > > >::pending_mutex;

    volatile std::list<detail::PendingMsg> WorldObject<DerivativeBase<double,6> >::pending;
    Spinlock WorldObject<DerivativeBase<double,6> >::pending_mutex;
    volatile std::list<detail::PendingMsg> WorldObject<DerivativeBase<std::complex<double>,6> >::pending;
    Spinlock WorldObject<DerivativeBase<std::complex<double>,6> >::pending_mutex;


}
#endif
