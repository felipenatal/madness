#ifndef MRA_H
#define MRA_H

/// \file mra.h
/// \brief Header for Function and friends

#include <vector>
#include <string>
#include <functional>
#include <cmath>

#include <madness_config.h>
#include <tensor/tensor.h>
#include <serialize/archive.h>
#include <serialize/mpiar.h>
#include <octtree/octtree.h>
//#include <octtree/sendrecv.h>
#include <mra/sepop.h>
#include <misc/communicator.h>
#include <misc/madexcept.h>
#include <tasks/sav.h>

#include <serialize/textfsar.h>
using madness::archive::TextFstreamInputArchive;
using madness::archive::TextFstreamOutputArchive;

#include <serialize/binfsar.h>
using madness::archive::BinaryFstreamInputArchive;
using madness::archive::BinaryFstreamOutputArchive;

#include <serialize/vecar.h>
using madness::archive::VectorInputArchive;
using madness::archive::VectorOutputArchive;

namespace std {
    /// This to make norm work as desired for both complex and real
    static inline double norm(const double d) {
        return d*d;
    };
}

namespace madness {

    Communicator& startup(int argc, char** argv);

    /**

    The distributed data structure and class hierarchy underlying the
    Function class is pieced together as follows.

    A FunctionOctTree combines an OctTree with management of indices.
    Each independent communicator (or data distribution) must have its
    own FunctionOctTree.  Not yet implemented (but coming soon) is the
    ability to move Functions between different FunctionOctTrees using
    MPI inter-communicators.  In the meantime, a program only needs
    to create one FunctionOctTree and store it as the default
    inside the FunctionDefaults structure.  

    The OctTree class template provides the overall oct-tree
    structure, data distribution (layout) and connectivity between
    processes.  The OctTree is also associated with a communicator.

    Each node of the OctTree contains a FunctionNode which is little
    more than a vaguely type-safe array of pointers to BaseTensor.

    Each new function is allocated a unique index that is used to
    access its coefficients stored in each FunctionNode.  As functions
    are dynamically created and destroyed, the indices are managed by
    the FunctionOctTree that is uniquely associated with the OctTree.

    Dynamic load balancing according to a variety of criteria is an 
    essential component that is yet to be implemented.

    */

#define FOREACH_ACTIVE_CHILD(OctTreeT, t, expr) \
    do { \
      for (int i=0; i<2; i++) \
        for (int j=0; j<2; j++) \
	  for (int k=0; k<2; k++) { \
            OctTreeT *child = ((t)->child(i,j,k)); \
            if (child && isactive(child)) {expr} \
          } \
    } while(0)

    class FunctionNode;         ///< Forward definition
    typedef OctTree<FunctionNode> OctTreeT; ///< Type of OctTree used to hold coeffs
    //template <typename T>

    /// Used to hold data for all functions at each node of the OctTree

    /// Mildly unsatisfactory feature is we are messing with pointers
    /// to the tensor base class which was envisoned primarily to
    /// support the interface to Python.  Currently, this is statically
    /// sized, but dynamic resizing is straightforward.
    class FunctionNode {
    public:
        static const int size = 64; ///< Size of new vectors.

    private:
        std::vector<BaseTensor *> v; ///< Pointers to base tensor, NULL if not set
        std::vector<bool> a;    ///< Flags for active
        std::vector<bool> acflag; ///< Flags for autocleaning temporary values from tree

        /// Private. Copy constructor not supported
//        FunctionNode(const FunctionNode& f);

        /// Private. Assignment not supported
//        FunctionNode& operator=(const FunctionNode& f);

    public:
        /// Constructor initializes all data pointers to NULL
        FunctionNode() : v(size), a(size), acflag(size) {
            // The explicit initialization may not be necessary, but
            // better safe than sorry.
            for (int i = 0; i < size; i++) {
                v[i] = 0;
                a[i] = false;
                acflag[i] = false;
            };
        };

        // I know, I know, naughty for doing this, but yanno, it needs to be done
        FunctionNode(const FunctionNode& f) {
            int i;
            int n = f.v.size();
            for (i = 0; i < n; i++) {
                BaseTensor* t = f.v[i];
                if (t->id == TensorTypeData<double>::id) {
                    Tensor<double> *d = new Tensor<double>();
                    *d = *(const Tensor<double> *) t;
                    v.push_back(d);
                    a.push_back(f.a[i]);
                } else {
                    throw "not yet";
                }
            };
        };

        FunctionNode& operator=(const FunctionNode& fn) {
            if (this == &fn) return *this;
            int i;
            int n = fn.v.size();
            v.clear();
            a.clear();
            // RJH ... THIS SEEMS WRONG.  IS IT ACTUALLY BEING USED?????
            for (i = 0; i < n; i++) {
                BaseTensor* t = fn.v[i];
                if (t->id == TensorTypeData<double>::id) {
                    Tensor<double> *d = new Tensor<double>();
                    *d = *(const Tensor<double> *) t;
                    v.push_back(d);
                    a.push_back(fn.a[i]);
                } else {
                    throw "not yet";
                }
            };
            return *this;
        };

        /// Puts vector of tensors and vector of bools into supplied arguments
        template <typename T>
        inline void getTensorList(std::vector<Tensor<T> > *tv, std::vector<bool> *av) {
            int i = 0, n = size;
//std::cout << "beginning of getTensorList, n = " << n << std::endl;
            for (i = 0; i < n; i++) {
//std::cout << "loop of getTensorList, i = " << i << std::endl;
                tv->push_back(*(get<T>(i)));
                av->push_back(isactive(i));
            }
//std::cout << "end of getTensorList, n = " << n << std::endl;
        }

        /// Sets v and a using supplied arguments
        template <typename T>
        inline void setTensorList(int n, std::vector<Tensor<T> > tv, std::vector<bool> av) {
            int i = 0;
            for (i = 0; i < n; i++) {
                set(i, tv[i]);
                set_active_status(i, av[i]);
            }
        }

        /// Returns a pointer to data for entry ind.  NULL indicates no data.
        template <typename T>
        inline Tensor<T>* get(int ind) const {
                if (ind < 0 || ind >= (int)v.size())
                    throw "FunctionNode: get: invalid index";
                return (Tensor<T> *) v[ind];
            }

        /// Sets data (and active) by taking a \em shallow copy of the tensor value

        /// Returns pointer to data as if from get()
        template <typename T>
        inline Tensor<T>* set(int ind, const Tensor<T>& value) {
            if (ind < 0 || ind >= (int)v.size())
                throw "FunctionNode: set: invalid index";
            v[ind] = new Tensor<T>(value);
            set_active(ind);
            return (Tensor<T> *) v[ind];
        }

        /// Unsets the data pointer (if any) and frees any associated data

        /// This does \em not change the active status of the function in
        /// this node.
        inline void unset(int ind) {
            if (ind < 0 || ind >= size)
                throw "FunctionNode: unset: invalid index";
            if (v[ind]) {
                delete v[ind];
                v[ind] = 0;
            }
        };

        /// Return true if function ind is active in this node
        inline bool isactive(int ind) const {
            return a[ind];
        };

        /// Mark function ind as active in this node
        inline void set_active(int ind) {
            a[ind] = true;
        };

        /// Set active status of function ind in this node
        inline void set_active_status(int ind, bool status) {
            a[ind] = status;
        };

        /// Mark function ind as inactive in this node
        inline void set_inactive(int ind) {
            a[ind] = false;
        };
        
        /// Set auto clean flag to value
        inline void set_acflag(int ind, bool value) {
            acflag[ind] = value;
        };
        
        inline bool get_acflag(int ind) const {
            return acflag[ind];
        }; 

        /// Destructor frees all data
        ~FunctionNode() {
            for (int i = 0; i < size; i++) unset(i);
        };

        template <class Archive>
        inline void serialize(const Archive& ar) {
            ar & v & a;
        }
    };

    /// A FunctionOctTree marries an OctTree<FunctionNode> with an index 

    /// The constructor takes ownership of the provided pointer to an already
    /// constructed OctTree<FunctionNode>.  I.e., don't delete the OctTree.
    class FunctionOctTree {
    private:
        int _nfree;
        std::vector<int> _free;
        SharedPtr<OctTreeT> _tree;

        /// Verboten
        FunctionOctTree(const FunctionOctTree&);

        /// Verboten
        FunctionOctTree& operator=(const FunctionOctTree&);

        /// Cleans up data of function being freed
        void free_data(OctTreeT* tree, int ind) {
            tree->data().unset(ind);
            tree->data().set_inactive(ind);
            tree->data().set_acflag(ind,false);
            FOREACH_CHILD(OctTreeT, tree, free_data(child, ind););
        };

    public:
        FunctionOctTree(OctTree<FunctionNode>* tree)
                : _nfree(FunctionNode::size)
                , _free(_nfree)
        , _tree(tree) {
            for (int i = 0; i < _nfree; i++) _free[i] = _nfree - i - 1;
        };

        /// Return a pointer to the contained octtree
        inline OctTree<FunctionNode>* tree() {
            return _tree.get();
        };

        /// Return a const pointer to the contained octtree
        inline const OctTree<FunctionNode>* tree() const {
            return _tree.get();
        };

        /// Allocate an index for a new function
        int alloc() {
            _nfree--;
            if (_nfree < 0)
                throw "FunctionIndexManager: too many functions: no resize yet";
//            std::cout << "Allocated Function: " << _free[_nfree] << " " << std::endl;
            return _free[_nfree];
        };

        /// Free an index for a now deleted function
        void free(int ind) {
            if (ind < 0 || ind >= (int)_free.size())
                throw "FunctionIndexManager: freeing invalid index";
            for (int i = 0; i < _nfree; i++)
                if (_free[i] == ind)
                    throw "FunctionIndexManager: freeing already free index";

            free_data(_tree, ind);

//            std::cout << "Deallocated Function: " << ind << std::endl;
            _free[_nfree] = ind;
            _nfree++;
        };
    };

    template <typename T> class FunctionFactory; // Forward definition to define factory before Function

    /// FunctionDefaults holds default paramaters as static class members

    /// Defined and initialized in mra.cc.  Note that it is currently
    /// not possible to combine functions with different simulation
    /// cells and this is is not even checked for.  Therefore, the
    /// only recommended approach is to set the simulation cell once
    /// and for all at the start of a calculation and to use it for
    /// all Function instances.  A little work could improve upon this.
    class FunctionDefaults {
    public:
        static SharedPtr<FunctionOctTree> tree; ///< The default tree/distribution used for new functions
        static int k;                  ///< Wavelet order
        static double thresh;          ///< Truncation threshold
        static int initial_level;      ///< Initial level for fine scale projection
        static int max_refine_level;   ///< Level at which to stop refinement
        static int truncate_method;    ///< Truncation method
        static bool compress;          ///< Whether to compress new functions
        static bool refine;            ///< Whether to refine new functions
        static bool autorefine;        ///< Whether to autorefine in multiplication, etc.
        static bool debug;             ///< Controls output of debug info
        static double cell[3][2];      ///< Simulation cell, cell[0][0]=xlo, cell[0][1]=xhi, ...
    };

    /// FunctionCommonData holds all Function data common for given k

    /// Since Function assignment and copy constructors are shallow it
    /// greatly simplifies maintaining consistent state to have all
    /// (permanent) state encapsulated in a single class.  The state
    /// is shared between instances using a SharedPtr.   Also, separating
    /// shared from instance specific state greatly accelerates the constructor.
    /// which is important for massive parallelism.  The default copy constructor and
    /// assignment operator are used.
    template <typename T>
    class FunctionCommonData {
    private:
        /// Private.  Make the level-0 blocks of the periodic central difference derivative operator
        void _make_dc_periodic();

        /// Private.  Initialize the twoscale coefficients
        void _init_twoscale();

        /// Private.  Initialize the quadrature information
        void _init_quadrature();

    public:
        int k;                  ///< Wavelet order
        int npt;                ///< no. of quadrature points
        double cell[3][2];      ///< Simulation cell (range over function is defined).
        double cell_width[3];   ///< Size of simulation cell in each dimension
        double cell_volume;     ///< Volume of simulation cell

        Slice s[4];              ///< s[0]=Slice(0,k-1), s[1]=Slice(k,2*k-1), etc.
        std::vector<Slice> s0;  ///< s[0] in each dimension to get scaling coeffs
        std::vector<long> vk;   ///< (k,k,k) used to initialize Tensors
        std::vector<long> v2k;  ///< *2k,2k,2k) used to initialize Tensors

        typedef Tensor<T> TensorT; ///< Type of tensor used to hold coeffs
        Tensor<T> work1;        ///< work space of size (k,k,k)
        Tensor<T> work2;        ///< work space of size (2k,2k,2k)
        Tensor<T> workq;        ///< work space of size (npt,npt,npt)
        Tensor<T> zero_tensor;    ///< Zero (2k,2k,2k) tensor for internal convenience of diff

        Tensor<double> quad_x;  ///< quadrature points
        Tensor<double> quad_w;  ///< quadrature weights
        Tensor<double> quad_phi; ///< quad_phi(i,j) = at x[i] value of phi[j]
        Tensor<double> quad_phit; ///< transpose of quad_phi
        Tensor<double> quad_phiw; ///< quad_phiw(i,j) = at x[i] value of w[i]*phi[j]

        Tensor<double> h0, h1, g0, g1; ///< The separate blocks of twoscale coefficients
        Tensor<double> hg, hgT; ///< The full twoscale coeffs (2k,2k) and transpose
        Tensor<double> hgsonly; ///< hg[0:k,:]

        Tensor<double> rm, r0, rp;        ///< Blocks of the derivative operator
        Tensor<double> rm_left, rm_right, rp_left, rp_right; ///< Rank-1 forms rm & rp

        /// Default constructor necessary for array/vector construction
        FunctionCommonData() {}
        ;

        /// Constructor presently forces choice npt=k
        FunctionCommonData(int k) : k(k), npt(k) {
            for (int i=0; i<4; i++) s[i] = Slice(i*k,(i+1)*k-1);
            s0 = std::vector<Slice>(3);
            s0[0] = s0[1] = s0[2] = s[0];
            cell_volume = 1.0;
            for (int i = 0; i < 3; i++) {
                cell[i][0] = FunctionDefaults::cell[i][0];
                cell[i][1] = FunctionDefaults::cell[i][1];
                cell_width[i] = cell[i][1] - cell[i][0];
                cell_volume *= cell_width[i];
            }
            vk = vector_factory((long)k,(long)k,(long)k);
            v2k = vector_factory(2L*k,2L*k,2L*k);
            work1 = TensorT(vk);
            work2 = TensorT(v2k);
            zero_tensor = TensorT(v2k);

            _init_twoscale();
            _init_quadrature();
            _make_dc_periodic();
        };

        /// Convert user coords (cell[][]) to simulation coords ([0,1])
        inline void user_to_sim(double& x, double& y, double& z) const {
            x = (x - cell[0][0]) / cell_width[0];
            y = (y - cell[1][0]) / cell_width[1];
            z = (z - cell[2][0]) / cell_width[2];
        };

        /// Convert simulation coords ([0,1]) to user coords (cell[][])
        inline void sim_to_user(double& x, double& y, double& z) const {
            x = x * cell_width[0] + cell[0][0];
            x = x * cell_width[1] + cell[1][0];
            x = x * cell_width[2] + cell[2][0];
        };
    };

    /// FunctionData holds all Function state to facilitate shallow copy semantics

    /// Since Function assignment and copy constructors are shallow it
    /// greatly simplifies maintaining consistent state to have all
    /// (permanent) state encapsulated in a single class.  The state
    /// is shared between instances using a SharedPtr.
    template <typename T>
    class FunctionData {
    private:
        static const int MAXK = 17;
        static FunctionCommonData<T> commondata[MAXK + 1]; 	///< Defined in mra.cc
        static bool initialized;	///< Defined and initialized to false in mra.cc

    public:
        typedef Tensor<T> TensorT; ///< Type of tensor used to hold coeffs

        int k;                  ///< Wavelet order
        double thresh;          ///< Screening threshold
        int initial_level;      ///< Initial level for refinement
        int max_refine_level;   ///< Do not refine below this level
        int truncate_method;    ///< 0=default=(|d|<thresh), 1=(|d|<thresh/2^n);
        bool debug;             ///< If true, verbose printing ... unused?
        bool autorefine;        ///< If true, autorefine where appropriate

        FunctionCommonData<T>* cdata;

        T (*f)(double, double, double); ///< Scalar interface to function to compress
        void (*vf)(long, const double*, const double*, const double*,
                   T* restrict); ///< Vector interface to function to compress

        SharedPtr<FunctionOctTree> tree; ///< The tree of coeffs
        int ind;		 ///< The unique index into the tree for this function

        bool compressed;        ///< Compression status
        long nterminated;               ///< No. of boxes where adaptive refinement was too deep


        /// Initialize function data from data in factory
        FunctionData(const FunctionFactory<T>& factory) {
            // might be cleaner to assign function factory data
            // via a struct so that don't need to manually modify here
            // every time a new element is added.
            if (!initialized) initialize();
            k = factory._k;
            cdata = commondata + k;

            tree = factory._tree;
            ind = tree->alloc();

            thresh = factory._thresh;
            initial_level = factory._initial_level;
            max_refine_level = factory._max_refine_level;
            truncate_method = factory._truncate_method;
            debug = factory._debug;
            autorefine = factory._autorefine;
            f = factory._f;
            vf = factory._vf;

            compressed = false;
            nterminated = 0;
        };

        /// Copy constructor

        /// Allocates a \em new function index in preparation for a deep copy
        FunctionData(const FunctionData<T>& other) {
            if (!initialized) initialize();
            k = other.k;
            cdata = commondata + k;

            tree = other.tree;
            ind = tree->alloc();

            thresh = other.thresh;
            initial_level = other.initial_level;
            max_refine_level = other.max_refine_level;
            truncate_method = other.truncate_method;
            debug = other.debug;
            f = other.f;
            vf = other.vf;

            compressed = other.compressed;
            nterminated = other.nterminated;
        };

        ~FunctionData() {
            tree->free(ind);
        };

    private:
        /// Initialize static data
        void initialize() {
            for (int k = 1; k <= MAXK; k++) commondata[k] = FunctionCommonData<T>(k);
            initialized = true;
        };

        /// Assignment is not allowed
        FunctionData<T>& operator=(const FunctionData<T>& other);
    };


    /// Private.  Helps with unary negation operator.
    template <typename T>
    static inline Tensor<T> _negate_helper(const Tensor<T>& t) {
        return -t;
    };
    
    

    /// Private.  Helps with scaling by a constant.
    template <typename T>
    class _scale_helper {
    private:
        T s;
    public:
        _scale_helper(T value) : s(value) {}
        ;

        inline Tensor<T> operator()(const Tensor<T>& t) const {
            return t*s;
        };
    };

    /// Private.  Helps with adding/subtracting a constant.
    template <typename T>
    class _add_helper {
    private:
        T s;
    public:
        _add_helper(T value) : s(value) {}
        ;

        inline Tensor<T> operator()(const Tensor<T>& t) const {
            return t + s;
        };
    };


    /// Multiresolution 3d function of given type
    template <typename T>
    class Function {
    public:
        typedef Tensor<T> TensorT; ///< Type of tensor used to hold coeffs
        typedef SAV<TensorT> ArgT; ///< Type of single assignment variable for tasks

    private:
        /// Private.  This funky constructor makes a partial deep copy.

        /// It takes a deep copy of data, but does NOT copy the
        /// coefficients.  Used to initialize the result of application
        /// of an operator to the original function.  No communication.
        explicit Function(const Function<T>* t)
                : data(new FunctionData<T>(*(t->data)))
                , k(data->k)
        , ind(data->ind) {}

    public:
        SharedPtr< FunctionData<T> > data; ///< Holds all function data
        int k;                  ///< For convenience replicates k from data
        int ind;                ///< For convenience replicates ind from data

        /// Constructor from FunctionFactory provides named parameter idiom

        /// No communication is involved.  Default constructor makes a
        /// zero compressed function.
        Function(const FunctionFactory<T>& factory = FunctionFactory<T>())
                : data(new FunctionData<T>(factory))
                , k(data->k)
        , ind(data->ind) {
            _init(factory);
        };


        /// Copy constructor is \em shallow

        /// No communication is involved.
        Function(const Function<T>& f)
                : data(f.data)
                , k(f.k)
        , ind(f.ind) {}
        ;

        /// Print out a summary of the tree with norms
        void pnorms() {
            if (isactive(tree())) _pnorms(tree());
        };


        /// Deep copy in either scaling function or wavelet basis.

        /// No communication involved.
        Function<T> copy() const {
            Function<T> result(this); // Partial deep copy

            typedef Tensor<T> (*theop)(const Tensor<T>& t);
            if (isactive(tree())) _unaryop<theop>(result, result.tree(), madness::copy<T>);
            return result;
        };

        /// Assignment is \em shallow.

        /// No communication is involved, works in either basis.
        Function<T>& operator=(const Function<T>& f) {
            if (this != &f) {
                data = f.data;
                k = f.k;
                ind = f.ind;
            }
            return *this;
        };

        /// Unary negation.  Produces new function.

        /// No communication involved.  Works in either basis.
        Function<T> operator-() const {
            Function<T> result = FunctionFactory<T>().k(k).compress(iscompressed()); //.empty();
            if (isactive(tree()))
                _unaryop(result, result.tree(), _negate_helper<T>);
            return result;
        };

        /// Function times a scalar.  Produces new function.
        /// No communication involved.  Works in either basis.
        Function<T> operator*(T s) const {
            Function<T> result = FunctionFactory<T>().k(k).compress(iscompressed()).empty();
            if (isactive(tree()))
                _unaryop(result, result.tree(), _scale_helper<T>(s));
            return result;
        };

        /// Multiplication of two functions.
    
    	/// Crude version using squaring is inaccurate, slow and implies global sync
        Function<T> operator*(Function<T>& other) {
            return ((*this+other).square()-(*this-other).square()).scale(0.25);
        };

        /// scalar * function (see Function<T>::operator*(T t))
        friend inline Function<T> operator*(T t, const Function<T>& f) {
            return f*t;
        };

        /// Binary addition.  Works in wavelet basis generating new Function

        /// No communication is involved \em unless one of the
        /// functions is not already compressed, in which case
        /// compression is forced.
        Function<T> operator+(const Function<T>& other) const {
            // Constness of this and other is only logical
            if (!iscompressed()) const_cast<Function<T>*>(this)->compress();
            if (!other.iscompressed()) const_cast<Function<T>*>(&other)->compress();

            Function<T> result = FunctionFactory<T>().k(k).compress(true).empty();

            if (isactive(tree()) ||
                    other.isactive(other.tree()))
                _add_sub(result, *this, other, result.tree(), false);

            return result;
        };

        /// Binary addition.  Works in wavelet basis generating new Function

        /// No communication is involved \em unless one of the
        /// functions is not already compressed, in which case
        /// compression is forced.
        Function<T> operator-(const Function<T>& other) const {
            // Constness of this and other is only logical
            if (!iscompressed()) const_cast<Function<T>*>(this)->compress();
            if (!other.iscompressed()) const_cast<Function<T>*>(&other)->compress();

            Function<T> result = FunctionFactory<T>().k(k).compress(true).empty();

            if (isactive(tree()) ||
                    other.isactive(other.tree()))
                _add_sub(result, *this, other, result.tree(), true);

            return result;
        };


        /// Returns the square of the norm of the \em local subtree

        /// No communication involved.  Works in either the scaling
        /// function or wavelet basis.
        double norm2sq_local() const {
            return _norm2sq_local(tree());
        };

        /// Returns the square of the norm of the entire tree

        /// Global communication is implied.   Works in either the scaling
        /// function or wavelet basis.
        double norm2sq() {
            compress();
            return comm()->global_sum(norm2sq_local());
        };
        
        /// Differentiation
        Function<T> diff(int axis) {
            reconstruct();
            Function<T> df = FunctionFactory<T>().k(k).compress(false).empty();
            _diff(df,tree(),axis);
            _auto_clean(tree()); // Could do this on the way back up.
            return df;
        };

        /// Compress function (scaling function to wavelet)

        /// Communication streams up the tree.
        /// Returns self for chaining.
        Function<T>& compressOLD() {
            if (!data->compressed) {
                if (isactive(tree())) _compress(tree());
                data->compressed = true;
            }
            return *this;
        };
        
        /// Autorefine (e.g. for squaring or multiplication)
        
        /// Communication, if any, flows down the tree
        /// Returns self for chaining.
        Function<T>& autorefine() {
        	if (data->compressed) reconstruct();
        	_autorefine(tree());
        	return *this;
        };
        
        void ptree() {_ptree(tree());};

        Function<T>& compress();
        void _compress2(OctTreeT* tree, ArgT& parent);
        void _compress2op(OctTreeT* tree, ArgT args[2][2][2], ArgT& parent);
        ArgT input_arg(const OctTreeT* consumer, const OctTreeT* producer);

        /// Reconstruct compressed function (wavelet to scaling function)

        /// Communication streams down the tree
        /// Returns self for chaining.
        Function<T>& reconstruct() {
            if (data->compressed) {
                if (isactive(tree())) _reconstruct(tree());
                data->compressed = false;
            }
            return *this;
        };

        /// Inplace scale by a constant

        /// No communication.
        /// Returns self for chaining.
        Function<T>& scale(T s) {
            if (isactive(tree())) _scale(tree(), s);
            return *this;
        };

        /// Inplace square (pointwise multiplication by self)

        /// If not reconstructed, reconstruct() is called which communicates.
        /// If not autorefining, no communication is involved in
        /// the squaring.  If autorefinining, some downward communication
        /// may be involved depending on the distribution of tree nodes.
        ///
        /// Returns self for chaining.
        Function<T>& square() {
           if (this->iscompressed()) this->reconstruct();
           if (isactive(tree())) _square(tree());
           return *this;
        };

        /// Inplace Generalized SAXPY to scale and add two functions.

        /// Works with both functions in either the scaling function or
        /// wavelet basis.  If only one function is already compressed, the
        /// other will also be compressed.
        /// \code
        ///   a = a*alpha + b*beta
        /// \endcode
        /// where \c a() and \c b() are Functions and \c alpha and \c beta are constants
        ///
        /// !!!!! If the operation is performed in the scaling
        /// function basis, you must eventually compress the result in
        /// order to correctly sum the scaling function coefficients
        /// at multiple levels in the tree.  This is useful if you
        /// have to produce and accumulate multiple intermediates in the
        /// scaling function basis, so you only have to run the
        /// compression on the final result rather than all
        /// intermediates.
        ///
        /// If no compression is required, no communication is involved.
        /// Returns self for chaining.
        /// The Functions \em must share the same tree (data distribution).
        /// Copying between distributions can only be done with the
        /// redistribute method (yet to be implemented when needed).
        Function<T>& gaxpy(double alpha, const Function<T>& b, double beta) {
            check_trees(b);
            if (!(this->iscompressed() ^ b.iscompressed())) {
                this->compress();
                const_cast<Function<T>*>(&b)->compress(); // constness of b is logical
            }
            if (isactive(tree()) || b.isactive(b.tree()))
                _gaxpy(*this, alpha, b, beta, tree());
            return *this;
        };


        /// Return true if the function is compressed
        bool iscompressed() const {
            return data->compressed;
        };

        /// This class stores informations on each branch.
        class localTreeMember {
        public:
            Translation x, y, z;
            Level n;
            ProcessID rank;
            bool remote, active, have_child;
            template <class Archive>
            inline void serialize(const Archive& ar) {
                ar & x & y & z & n & rank & remote & active & have_child;
            };
        };

	/// Saving Function members into the file. This function is prepared for sequential job.
	template <class Archive>
	void save_local(Archive& ar);

	/// Saving Function members into the file. This member was prepared for save_local.
	template <class Archive>
	void _save_local(Archive& ar, const OctTreeT *tree); 

	/// save member is the member to serialize coefficients. This member is already parallelized. 
	void save(const char* f, const long partLevel, Communicator& comm);

	/// saveMain member controlls before and after treatments of saveManager. 
	template <class Archive>
	void saveMain(const char* f, Archive& ar, const long partLevel, Communicator& comm);

	/// saveManager Function member serialize coefficients on local (rank 0 ) branch. 
	template <class Archive>
	void saveManager(const char* f, Archive& ar, const OctTreeT *tree, const long partLevel, Communicator& commFunc);

	/// This member serializes coefficients on client branch.
	template <class Archive>
	void shadowManager_save(const char* f, Archive& ar, const OctTreeT *tree, Level n, Translation x, Translation y, Translation z, 
        ProcessID remoteRank, const long partLevel, Communicator& commFunc);

	/// This member cotrols distributes coefficients to client branch.
	template <class Archive>
	void shadowManager_load(const char* f, const Archive& ar, OctTreeT* tree, 
              Level n, Translation x, Translation y, Translation z, 
              ProcessID remoteRank, Communicator& commFunc,
	      bool active_flag, bool have_child);

	/// This member is worker to send/recieve msg from rank 0.
	void saveLoadWorker(OctTreeT *tree, 
			Communicator& commFunc, bool save);

	/// This member is member for DiskDir class.
	void saveLoadWorker4DD(Communicator& comm, bool save){
             saveLoadWorker(tree(), comm, save);
        } 

	/// This member Returns localsubtreelist of client computer to Master.
	void localTreeList(std::vector<localTreeMember> &subtreeList, 
			OctTreeT *tree);

	/// Send Coefficients to Rank 0. 
	void sendRecvDataWorker_save(OctTreeT *tree, 
			Communicator& commFunc);

	/// Receive Coefficients from Rank 0. 
	void sendRecvDataWorker_load(OctTreeT *treeclient,  
			Communicator& commFunc);

	/// Loading Function members from the file. This member is not parallelized.
	template <class Archive>
	void load_local(const Archive& ar); 

	/// Loading Function members from the file. This member is prepared for called from load_local. 
	template <class Archive>
	void _load_local(const Archive& ar, OctTreeT *tree);

	/// Loading Function members from the file. This member is already parallelized.
	//template <class Archive>
	//void load(const Archive& iar, Communicator& comm);
	void load(const char* f, Communicator& comm);

	/// Load Managing Function member. 
	template <class Archive>
	void loadManager(const char* f, const Archive& ar, OctTreeT *tree, Communicator& commFunc, bool active_flag, const long partLevel, bool have_child);

	/// Load Managing Function member for DiskDir class. 
	template <class Archive>
	void loadManager4DD(const Archive& ar, Communicator& commFunc, bool active_flag){
             loadManager(ar, tree(), commFunc, active_flag, true);
        };

	/// Making Archive class's file name for 2-layer Serialization.
	void produceNewFilename(const char* f, const long partLevel, const OctTreeT *tree, char ftest[256]);

	/// Making Archive class's file name for 2-layer Serialization.
	void produceNewFilename2(const char* f, const long partLevel, localTreeMember& subtreeList, char ftest[256]);

        /// Save Client brach's data on master.
        /// Save Client brach's data on master.
	template <class Archive>
        void dataSaveInShaManSave(const char* f, Archive& ar, const OctTreeT *tree, localTreeMember& subtreeList, ProcessID remoteRank, const long partLevel, Communicator& commFunc); 

	/// Inplace truncation of small wavelet coefficients
    
    /// Works in the wavelet basis and compression is performed if the
    /// function is not already compressed.  Communication streams up the
    /// tree.  The default truncation threshold is that used to construct
    /// the function.  If the threshold is zero, nothing is done.
    /// Returns self for chaining.
	Function<T>& truncate(double tol = -1.0) {
        if (tol < 0.0) tol = this->data->thresh;
	    if (tol == 0.0) return *this;
        compress();
        if (isactive(tree())) _truncate(tol, tree());
        return *this;
	};
    
    T inner_local(const Function<T>& other) const {
        const_cast<Function<T>*>(this)->compress();
        const_cast<Function<T>*>(&other)->compress();
        return _inner_local(*this, other, tree());
    };

        /// Inner product between two Function classess. 
        
       /// Works in the wavelet basis.  Global communication is implied.
        T inner(const Function<T>& other) const {
            return comm()->global_sum(inner_local(other));
        };

	/// This member is the member called from inner member.
	T _inner_local(const Function<T>& a, const Function<T>& b, const OctTreeT* tree) const ;

        /// Local evaluation of the function at a point in user coordinates

        /// If the coordinate is local to this process, returns true
        /// and sets value to the function value.  Otherwise, returns
        /// false and sets value to zero.
        ///
        /// If the function is not reconstructed an \em exception is
        /// thrown.  Also, node 0 throws an exception if the coordinate is
        /// outside the box.
        ///
        /// No communication is involved.
        bool eval_local(double x, double y, double z, T* value) const {
            if (iscompressed())
                throw "Function:eval_local:must not be compressed";

            *value = T(0.0);
            const OctTreeT* tree = this->tree();
            if (isactive(tree)) {
                // Determine if point might be local to this process
                data->cdata->user_to_sim(x, y, z);
                double twon = two_to_power(tree->n());
                x *= twon;
                y *= twon;
                z *= twon;
                x -= tree->x();
                y -= tree->y();
                z -= tree->z();

                if (x < 0.0 || y < 0.0 || z < 0.0 ||
                        x > 1.0 || y > 1.0 || z > 1.0) {
                    if (tree->n() == 0) {
                        throw "Function::eval_local:out of range point";
                    } else {
                        return false;
                    }
                }

                // Recur down to find it ... inline for maximum efficiency
                while (tree && isactive(tree)) {
                    x *= 2.0;
                    y *= 2.0;
                    z *= 2.0;
                    int lx = int(x);
                    int ly = int(y);
                    int lz = int(z);
                    if (lx == 2) --lx;
                    if (ly == 2) --ly;
                    if (lz == 2) --lz;
                    x -= lx;
                    y -= ly;
                    z -= lz;

                    const TensorT* t = coeff(tree);
                    if (t) {
                        *value = _eval_cube(tree->n(), x, y, z, lx, ly, lz, *t);
                         return true;
                    } else {
                        tree = tree->child(lx, ly, lz);
                    }
                }
            }
            return false;
        };


        /// Evaluate the function at a point in user coordinates

        /// !!! This is a \em collective operation.  In addition,
        /// if the function is not in the scaling function basis
        /// it is first reconstructed.
        ///
        /// On a sequential computer, this is a perfectly sensible
        /// function to use.  On a massively parallel computer, this
        /// is a shockingly inefficient way to evaluate anything more
        /// than a few values.  Instead, consider eval_local which
        /// does no communication, or eval_list which evaluates many
        /// values and only communicates/synchronizes once.
        T operator()(double x, double y, double z) const {
            T value;
            eval_list(1L, &x, &y, &z, &value);
            return value;
        };

        /// Evaluate function at a list of points, returning results to everyone

        /// Involves global communication and possible reconstruction.
        void eval_list(long n, const double* x, const double* y, const double*z,
                       T *value) const {
            if (iscompressed()) const_cast< Function<T>* >(this)->reconstruct();          
            for (long i = 0; i < n; i++) {
                eval_local(x[i], y[i], z[i], value + i);
            }
            comm()->global_sum(value, n);
        };

    private:
        /// Private.  Initialization required constructing function from stratch

        /// No communication involved.
        void _init(const FunctionFactory<T>& factory);


        /// Private.  Applies truncation method to give truncation threshold

        /// No communication involved.
        /// Truncate method m scales tol by 2^(-n*m/2)
        inline double truncate_tol(double tol, Level n) {
            if (data->truncate_method == 0) {
                return tol;
            } else {
            	return tol*std::pow(0.5,0.5*n*data->truncate_method);
            }
        };


        /// Private.  Projects function in given box

        /// No communication involved.
        void _project(OctTreeT* s);


        /// Private.  Projects function at given level.  No communication.

        /// No communication involved.  It is assumed that this is
        /// being done to an initially zero Function by _init
        /// immediately after construction
        void _fine_scale_projection(OctTreeT* coeff, Level initial_level);


        /// Evaluate f(x,y,z) on the quadrature grid in box (n,lx,ly,lz)

        /// No communication involved.
        void _fcube(long n, long lx, long ly, long lz,
                    T (*f)(double, double, double), TensorT& fcube);


        /// Vector interface to evaluating function on the quadrature grid in box (n,lx,ly,lz)

        /// No communication involved.
        void _vfcube(long n, long lx, long ly, long lz,
                     void (*vf)(long l, const double*, const double*, const double*,
                                T* RESTRICT),
                     TensorT& fcube);


        /// Private.  Transform sum coefficients at level n to sums+differences at level n-1

        /// Given scaling function coefficients s[n][l][i] and s[n][l+1][i]
        /// return the scaling function and wavelet coefficients at the
        /// coarser level.  I.e., decompose Vn using Vn = Vn-1 + Wn-1.
        ///
        /// s_i = sum(j) h0_ij*s0_j + h1_ij*s1_j
        /// d_i = sum(j) g0_ij*s0_j + g1_ij*s1_j
        ///
        /// Returns a new tensor and has no side effects.  Works for any
        /// number of dimensions.
        ///
        /// No communication involved.
        inline TensorT filter(const TensorT& s) const {
            return transform3d(s, data->cdata->hgT);
            //return transform(s,hgT);
        };

        /// Private: Optimized filter (inplace, contiguous, no err checking)

        /// Transforms coefficients in s returning result also in s.
        /// Uses work2 from common data to eliminate temporary creation and
        /// to increase cache locality.
        ///
        /// No communication involved.
        inline void filter_inplace(TensorT& s) {
            transform3d_inplace(s, data->cdata->hgT, data->cdata->work2);
        };


        /// Private: Optimized unfilter (see info about filter_inplace)

        /// No communication involved.
        inline void unfilter_inplace(TensorT& s) {
            transform3d_inplace(s, data->cdata->hg, data->cdata->work2);
        };



        /// Private.  Transform sums+differences at level n to sum coefficients at level n+1

        ///  Given scaling function and wavelet coefficients (s and d)
        ///  return the scaling function coefficients at the next finer
        ///  level.  I.e., reconstruct Vn using Vn = Vn-1 + Wn-1.
        ///
        ///  s0 = sum(j) h0_ji*s_j + g0_ji*d_j
        ///  s1 = sum(j) h1_ji*s_j + g1_ji*d_j
        ///
        ///  Returns a new tensor and has no side effects
        ///
        ///  If (sonly) ... then ss is only the scaling function coeffs (and
        ///  assume the d are zero).  Works for any number of dimensions.
        ///
        /// No communication involved.
        inline TensorT unfilter(const TensorT& ss,
                                bool sonly = false) const {
            if (sonly)
                //return transform(ss,data->cdata->hgsonly);
                throw "unfilter: sonly : not yet";
            else
                return transform3d(ss, data->cdata->hg);
            //return transform(ss,hg);
        };


        /// Private. Returns the tree of coeffs

        /// No communication involved.
        inline OctTreeT* tree() {
            return data->tree->tree();
        };


        /// Private. Returns the const tree of coeffs

        /// No communication involved.
        inline const OctTreeT* tree() const {
            return data->tree->tree();
        };


        /// Private.  Retrieve pointer to coeffs of this Function from tree pointer

        /// No communication involved.
        inline TensorT* coeff(OctTreeT* tree) const {
            return tree->data(). template get<T>(ind);
        };


        /// Private.  Retrieve const pointer to coeffs of this Function from tree pointer

        /// No communication involved.
        inline const TensorT* coeff(const OctTreeT* tree) const {
            return tree->data(). template get<T>(ind);
        };


        /// Private.  Unset any coeffs of this Function from tree pointer

        /// No communication involved.
        inline void unset_coeff(OctTreeT* tree) {
            return tree->data().unset(ind);
        };


        /// Private.  Set coeffs of this Function from tree pointer

        /// No communication involved.
        inline TensorT* set_coeff(OctTreeT* tree, const TensorT& t) {
            return tree->data().set(ind, t);
        };


        /// Private.  Set Function active in tree node

        /// No communication involved.
        inline void set_active(OctTreeT* tree) const {
            tree->data().set_active(ind);
        };


        /// Private.  Set Function inactive in tree node

        /// No communication involved.
        inline void set_inactive(OctTreeT* tree) const {
            tree->data().set_inactive(ind);
        };


    public:
        /// Private.  Determine if Function is active in tree node

        /// No communication involved.
        inline bool isactive(const OctTreeT* tree) const {
            return tree->data().isactive(ind);
        };

    private:
        /// Private.  For semantic similarity to isactive.

        /// No communication involved.
        inline bool islocal(const OctTreeT* tree) const {
            return tree->islocal();
        };


        /// Private.  For semantic similarity to isactive.

        /// No communication involved.
        inline bool isremote(const OctTreeT* tree) const {
            return tree->isremote();
        };
        
        /// Private.  For semantic similarity to isactive.

        /// No communication involved.
        inline void set_acflag(OctTreeT* tree, bool value) {
            tree->data().set_acflag(ind,value);
        };
        
        /// Private.  For semantic similarity to isactive.

        /// No communication involved.
        inline bool get_acflag(const OctTreeT* tree) const {
            return tree->data().get_acflag(ind);
        };
        

        /// Private.  Retrieve a pointer to the communicator

        /// No communication involved.
        inline Communicator* comm() const {
            return (Communicator *) (tree()->comm());
        };


        /// Private.  Recursive implementation of local 2-normsq

        /// No communication involved.
        double _norm2sq_local(const OctTreeT* tree) const;

        /// Private.  Recursive function to refine from initial projection.

        /// Communication streams all the way down the tree.
        void _refine(OctTreeT* tree);


        /// Private.  Recursive function to compress tree.

        /// Communication streams up the tree.
        void _compress(OctTreeT* tree);


        /// Private.  Recursive function to reconstruct the tree.

        /// Communication streams down the tree.
        void _reconstruct(OctTreeT* tree);


        /// Private.  Recursive function to support local unary operations
        /// including deep copy, scaling by constant, addition of constant,
        /// negation, etc.  Works in either basis.  No communication.
        /// The operation should be a function or functor that takes a
        /// reference to a const Tensor<T> as an argument and returns a
        /// new Tensor<T> as a result.
        template <class Op>
        void _unaryop(Function<T>& result, OctTreeT *tree, const Op op) const {
            // !!! passing op by reference causes SEGV in gcc 4.0
            result.set_active(tree);
            const TensorT *t = coeff(tree);
            if (t) {
                Tensor<T> q = op(*t);
                result.set_coeff(tree, q);
            }

            FOREACH_CHILD(OctTreeT, tree,
                          if (isactive(child))
                          _unaryop(result, child, op););
        };

        /// Private:  Recursive kernel for truncation
        void _truncate(double tol, OctTreeT *tree);


        /// Private.  Recur down the tree printing out the norm of
        /// the coefficients.
        void _pnorms(const OctTreeT *tree) const {
            const TensorT *t = coeff(tree);
            for (long i=0; i<tree->n(); i++) std::printf("  ");
            if (t) {
                std::printf("%4d (%8lu, %8lu, %8lu) = %9.1e\n",
                            tree->n(),tree->x(),tree->y(),tree->z(),t->normf());
            } else {
                std::printf("%4d (%8lu, %8lu, %8lu) None\n",
                            tree->n(),tree->x(),tree->y(),tree->z());
            }
            FOREACH_CHILD(OctTreeT, tree, if (isactive(child)) _pnorms(child););
        };

        /// Private.  Recursive function to support inplace scaling.
        void _scale(OctTreeT *tree, T s) {
            if (coeff(tree)) coeff(tree)->scale(s);
            FOREACH_CHILD(OctTreeT, tree,
                          if (isactive(child)) _scale(child, s););
        };

        /// Private:  Compute norms of polyn with order <k/2 & >=k/2.

        /// No communication is involved.
        void _tnorms(const Tensor<T>& t, double *lo, double *hi) {
            // t is a k*k*k tensor.  In order to screen the autorefinement
            // during squaring or multiplication, compute the norms of
            // ... lo ... the block of t for all polynomials of order < k/2
            // ... hi ... the block of t for all polynomials of order >= k/2

            // This routine exists to provide higher precision and in order to
            // optimize away the tensor slicing otherwise necessary to compute
            // the norm using
            Slice s0(0,(k-1)/2);
            double anorm = t.normf();
            *lo = t(s0,s0,s0).normf();
            *hi = sqrt(anorm*anorm - *lo**lo);
/*
            // k=5   0,1,2,3,4     --> 0,1,2 ... 3,4
            // k=6   0,1,2,3,4,5   --> 0,1,2 ... 3,4,5

            // k=number of wavelets, so k=5 means max order is 4, so max exactly
            // representable squarable polynomial is of order 2 which is the third
            // polynomial.
            int k2 = (k - 1) / 2 + 1;
            double slo = 0.0, shi = 0.0;

            for (int p = 0; p < k2; p++)
                for (int q = 0; q < k2; q++)
                    for (int r = 0; r < k2; r++)
                        slo += std::norm(t(p, q, r));

            for (int p = 0; p < k; p++)
                for (int q = k2; q < k; q++)
                    for (int r = 0; r < k; r++)
                        shi += std::norm(t(p, q, r));

            for (int p = k2; p < k; p++)
                for (int q = 0; q < k2; q++)
                    for (int r = 0; r < k; r++)
                        shi += std::norm(t(p, q, r));

            for (int p = 0; p < k2; p++)
                for (int q = 0; q < k2; q++)
                    for (int r = k2; r < k; r++)
                        shi += std::norm(t(p, q, r));

            *lo = sqrt(slo);
            *hi = sqrt(shi);*/
        };
        
        /// Returns list of unique processes with active remote children of this node
        
        /// Return value is the no. of unique remote processes with 
        /// active children and a list of their MPI ranks
        int unique_active_child_procs(const OctTreeT *tree, ProcessID ranks[8]) const {
            int np = 0;
            FOREACH_REMOTE_CHILD(const OctTreeT, tree,
                if (isactive(child)) {
                    bool found = false;
                    for (int p=0; p<np; p++) {
                        if (child->rank() == ranks[p]) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) ranks[np++] = child->rank();
                }
            );
            return np;
        };
        
        
        void _ptree(OctTreeT *tree) {
            tree->print_coords();
            FOREACH_ACTIVE_CHILD(OctTreeT, tree, _ptree(child););
        };

        /// Private.  Recursive function to provide autorefinement for squaring and multiplication.
        void _autorefine(OctTreeT *tree) {
            bool msg[2], refine=false;
            const Slice* s = data->cdata->s;
            const Slice& s0 = s[0];
            long k2 = k*2;
            Tensor<T>& work1 = data->cdata->work1;
            
            if (islocal(tree)) {
                ProcessID ranks[8];
                int np = tree->unique_child_procs(ranks);
                if (coeff(tree)) { // Test for refinement
                    double tol = truncate_tol(data->thresh,tree->n()+1);
                    const Tensor<T>& c = *coeff(tree);
                    double lo,hi;
                    FORIJK(_tnorms(c(s[i],s[j],s[k]),&lo,&hi);
                           refine = hi > tol;
                           if (refine) goto done;);
                    done:;
                }
    
                // Tell remote clones what is going on
                msg[0] = isactive(tree); 
                msg[1] = refine;
                
                for (int i=0; i<np; i++) comm()->Send(msg,2,ranks[i],AUTOREF_TAG1);
                
                if (refine) { // refine, sending data as necessary;
                    const Tensor<T>& c = *coeff(tree);
                    FORIJK(OctTreeT* child = tree->child(i,j,k);
                           if (!child) child = tree->insert_local_child(i,j,k);
                           set_active(child);
                           if (child->islocal()) {
                              Tensor<T>*t = set_coeff(child, Tensor<T>(k2, k2, k2));
                              (*t)(s0, s0, s0) = c(s[i],s[j],s[k]);
                              unfilter_inplace(*t);  // sonly!
                           }
                           else {
                              work1(s0,s0,s0) = c(s[i],s[j],s[k]); // contig. copy
                              comm()->Send(work1.ptr(), work1.size, child->rank(), AUTOREF_TAG2);
                           }
                        );
                    unset_coeff(tree);               
                }
            } else if (isremote(tree) && tree->islocalsubtreeparent()) {
                comm()->Recv(msg,2,tree->rank(),AUTOREF_TAG1);
                bool active=msg[0], refine=msg[1];
                if (!active && isactive(tree)) {
                    MADNESS_EXCEPTION("Remote clone thinks it is inactive but I think I am active",0);
                } else if (active) {
                    set_active(tree);
                    if (refine) {
                        FORIJK(OctTreeT* child = tree->child(i,j,k);
                              if (!child) child = tree->insert_local_child(i,j,k);
                              set_active(child);
                              comm()->Recv(work1.ptr(), work1.size, tree->rank(), AUTOREF_TAG2);
                              Tensor<T>*c = set_coeff(child, Tensor<T>(k2, k2, k2));
                              (*c)(s0, s0, s0) = work1;
                              unfilter_inplace(*c); // sonly needed!
                        );
                    }
                }
            }
                
            FOREACH_CHILD(OctTreeT, tree, if (islocal(child)) _autorefine(child););    
        };

        /// Private.  Recursive function to support inplace squaring.
        
        /// No communication.
        void _square(OctTreeT *tree) {
        	 FOREACH_ACTIVE_CHILD(OctTreeT, tree, _square(child););

             if (coeff(tree)) {
                Tensor<T>& t = *coeff(tree);
                Tensor<T> r(k, k, k);
                const Slice* s = data->cdata->s;
                double scale = std::pow(8.0, 0.5 * (tree->n()+1));
                FORIJK(r(___) = t(s[i], s[j], s[k]);
                       transform3d_inplace(r, data->cdata->quad_phit,
                                           data->cdata->work1);
                       r.emul(r).scale(scale);
                       transform3d_inplace(r, data->cdata->quad_phiw,
                                           data->cdata->work1);
                       t(s[i], s[j], s[k]) = r;
                      );
             }
        };

        /// Private.  Recursive function to support gaxpy
        void _gaxpy(Function<T>& afun, double alpha,
                    const Function<T>& bfun, double beta, OctTreeT* tree);

        /// Private.  Recursive function to support addition and subtraction
        void _add_sub(Function<T>& c, const Function<T>& a, const Function<T>& b,
                      OctTreeT* tree, bool subtract) const;


        /// Private.  Check that this function and another share the same OctTree
        void check_trees(const Function<T>& other) const {
            if (tree() != other.tree()) throw "Function: check_trees: trees are different";
        };


        /// Private.  Evaluates function in scaling function basis within a cube
        T _eval_cube(Level n,
                     double xx, double yy, double zz,
                     int lx, int ly, int lz,
                     const Tensor<T>& s) const;
        
        void _diff(Function<T>& df, OctTreeT* tree, int axis) {
            if (isactive(tree)) {
                df.set_active(tree);
                if (coeff(tree)) _dodiff(df, tree, axis);
                else FOREACH_CHILD(OctTreeT, tree, _diff(df, child, axis););
            }
        };

        void _dodiff(Function<T>& df, OctTreeT* tree, int axis);
           
        void _dodiff_kernel(Function<T>& df, OctTreeT* tree, int axis,
                    const TensorT& t0, const TensorT& tm, const TensorT& tp);
                    
        void _recur_coeff_down(OctTreeT *tree, bool keep);
        
        const Tensor<T>* _get_scaling_coeffs(OctTreeT* t, int axis, int inc);
        
        void _auto_clean(OctTreeT* tree) {
            if (isactive(tree)) {
                if (get_acflag(tree)) {
                    //print(tree->n(),tree->x(),tree->y(),tree->z(),"autocleaning");
                    unset_coeff(tree);
                    set_inactive(tree);
                    set_acflag(tree,false);
                };
                FOREACH_CHILD(OctTreeT, tree, _auto_clean(child););
            }
        };
                    
       
    };

    /// Deep copy function ... invokes f.copy()
    template <typename T>
    inline Function<T> copy(const Function<T>& f) {
        return f.copy();
    }


    /// FunctionFactory implements the named-parameter idiom for Function

    /// C++ does not provide named arguments (as does, e.g., Python).
    /// This class provides something very close.  Create functions as follows
    /// \code
    /// double myfunc(double x,double y,double z);
    /// Function f = FunctionFactory(myfunc).k(11).thresh(1e-9).debug().nocompress()
    /// \endcode
    /// where the methods of function factory, which specify the non-default
    /// arguments eventually passed to the \c Function constructor, can be
    /// used in any order.
    template <typename T> class FunctionFactory {
    protected:
        T (*_f)(double, double, double);
        void (*_vf)(long, const double*, const double*, const double*, T* restrict);
        int _k;

        double _thresh;
        int _initial_level;
        int _max_refine_level;
        int _truncate_method;
        bool _compress;
        bool _refine;
        bool _debug;
        bool _empty;
        bool _autorefine;
        SharedPtr<FunctionOctTree> _tree;
        friend class Function<T>;
        friend class FunctionData<T>;
    public:
        FunctionFactory(T (*f)(double, double, double) = 0)
                : _f(f)
                , _vf(0)
                , _k(FunctionDefaults::k)
                , _thresh(FunctionDefaults::thresh)
                , _initial_level(FunctionDefaults::initial_level)
                , _max_refine_level(FunctionDefaults::max_refine_level)
                , _truncate_method(FunctionDefaults::truncate_method)
                , _compress(FunctionDefaults::compress)
                , _refine(FunctionDefaults::refine)
                , _debug(FunctionDefaults::debug)
                , _empty(false)
                , _autorefine(FunctionDefaults::autorefine)
        , _tree(FunctionDefaults::tree) {}
        ;
        inline FunctionFactory& vf(void (*vf)(long, const double*, const double*, const double*, T* restrict)) {
            _vf = vf;
            return *this;
        };
        inline FunctionFactory& k(int k) {
            _k = k;
            return *this;
        };
        inline FunctionFactory& thresh(double thresh) {
            _thresh = thresh;
            return *this;
        };
        inline FunctionFactory& initial_level(int initial_level) {
            _initial_level = initial_level;
            return *this;
        };
        inline FunctionFactory& max_refine_level(int max_refine_level) {
            _max_refine_level = max_refine_level;
            return *this;
        };
        inline FunctionFactory& truncate_method(int truncate_method) {
            _truncate_method = truncate_method;
            return *this;
        };
        inline FunctionFactory& compress(bool compress = true) {
            _compress = compress;
            return *this;
        };
        inline FunctionFactory& nocompress(bool nocompress = true) {
            _compress = !nocompress;
            return *this;
        };
        inline FunctionFactory& debug() {
            _debug = true;
            return *this;
        };
        inline FunctionFactory& nodebug() {
            _debug = false;
            return *this;
        };
        inline FunctionFactory& refine(bool refine = true) {
            _refine = refine;
            return *this;
        };
        inline FunctionFactory& norefine(bool norefine = true) {
            _refine = !norefine;
            return *this;
        };
        inline FunctionFactory& empty() {
            _empty = true;
            return *this;
        };
        inline FunctionFactory& autorefine() {
            _autorefine = true;
            return *this;
        };
        inline FunctionFactory& noautorefine() {
            _autorefine = false;
            return *this;
        };
        inline FunctionFactory& tree(const SharedPtr<FunctionOctTree>& tree) {
            _tree = tree;
        };
    };
}

#endif
