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



#ifndef FUNCTIONCOMMONDATA_H_
#define FUNCTIONCOMMONDATA_H_

namespace madness {
    /// FunctionCommonData holds all Function data common for given k

    /// Since Function assignment and copy constructors are shallow it
    /// greatly simplifies maintaining consistent state to have all
    /// (permanent) state encapsulated in a single class.  The state
    /// is shared between instances using a shared_ptr.  Also,
    /// separating shared from instance specific state accelerates the
    /// constructor, which is important for massive parallelism, and
    /// permitting inexpensive use of temporaries.  The default copy
    /// constructor and assignment operator are used but are probably
    /// never invoked.
    template<typename T, std::size_t NDIM>
    class FunctionCommonData {
    private:
        static const FunctionCommonData<T, NDIM>* data[MAXK];

        /// Private.  Initialize the twoscale coefficients
        void
        _init_twoscale();

        /// Private.  Do first use initialization via get.
        FunctionCommonData(int k) {
            this->k = k;
            npt = k;
            for (int i = 0; i < 4; ++i)
                s[i] = Slice(i * k, (i + 1) * k - 1);
            s0 = std::vector<Slice>(NDIM);
            sh = std::vector<Slice>(NDIM);
            vk = std::vector<long>(NDIM);
            vq = std::vector<long>(NDIM);
            v2k = std::vector<long>(NDIM);
            for (std::size_t i = 0; i < NDIM; ++i) {
                s0[i] = s[0];
                sh[i] = Slice(0, (k - 1) / 2);
                vk[i] = k;
                vq[i] = npt;
                v2k[i] = 2 * k;
            }
            key0 = Key<NDIM> (0, Vector<Translation, NDIM> (0));

            _init_twoscale();
            _init_quadrature(k, npt, quad_x, quad_w, quad_phi, quad_phiw,
                             quad_phit);
        }

    public:
        typedef Tensor<T> tensorT; ///< Type of tensor used to hold coeff

        int k; ///< order of the wavelet
        int npt; ///< no. of quadrature points
        Slice s[4]; ///< s[0]=Slice(0,k-1), s[1]=Slice(k,2*k-1), etc.
        std::vector<Slice> s0; ///< s[0] in each dimension to get scaling coeff
        std::vector<Slice> sh; ///< Slice(0,(k-1)/2) in each dimension for autorefine test
        std::vector<long> vk; ///< (k,...) used to initialize Tensors
        std::vector<long> v2k; ///< (2k,...) used to initialize Tensors
        std::vector<long> vq; ///< (npt,...) used to initialize Tensors

        Key<NDIM> key0; ///< Key for root node

        Tensor<double> quad_x; ///< quadrature points
        Tensor<double> quad_w; ///< quadrature weights
        Tensor<double> quad_phi; ///< quad_phi(i,j) = at x[i] value of phi[j]
        Tensor<double> quad_phit; ///< transpose of quad_phi
        Tensor<double> quad_phiw; ///< quad_phiw(i,j) = at x[i] value of w[i]*phi[j]

        Tensor<double> h0, h1, g0, g1;      ///< The separate blocks of twoscale coefficients
        Tensor<double> h0T, h1T, g0T, g1T;  ///< The separate blocks of twoscale coefficients
        Tensor<double> hg, hgT; ///< The full twoscale coeff (2k,2k) and transpose
        Tensor<double> hgsonly; ///< hg[0:k,:]

        static const FunctionCommonData<T, NDIM>&
        get(int k) {
            MADNESS_ASSERT(k > 0 && k <= MAXK);
            if (!data[k-1]) data[k-1] = new FunctionCommonData<T,NDIM>(k);
            return *(data[k-1]);
        }

        /// Initialize the quadrature information

        /// Made public with all arguments thru interface for reuse in FunctionImpl::err_box
        static void
        _init_quadrature(int k, int npt, Tensor<double>& quad_x, Tensor<
                         double>& quad_w, Tensor<double>& quad_phi,
                         Tensor<double>& quad_phiw, Tensor<double>& quad_phit);
    };
}


#endif /* FUNCTIONCOMMONDATA_H_ */
