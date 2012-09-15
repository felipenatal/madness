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
/*!
  \file helium_mp2.cc
  \brief Solves the Hartree-Fock and MP2 equations for the helium atom
  \defgroup examplehehf Hartree-Fock and MP2 for the helium atom
  \ingroup examples

  The source is
  <a href=http://code.google.com/p/m-a-d-n-e-s-s/source/browse/local/trunk/src/apps/examples/helium_mp2.cc>here</a>.


*/


#define WORLD_INSTANTIATE_STATIC_TEMPLATES
#include <mra/mra.h>
#include <mra/operator.h>
#include <mra/funcplot.h>
#include <mra/lbdeux.h>


#include <iostream>

using namespace madness;

template<size_t NDIM>
void load_function(World& world, Function<double,NDIM>& pair, const std::string name) {
    if (world.rank()==0)  print("loading function ", name);

    archive::ParallelInputArchive ar(world, name.c_str());

//    Tensor<double> cell;
    ar & pair;// & cell;

    // set the defaults
//    if (NDIM==3) FunctionDefaults<3>::set_cell(cell);
//    if (NDIM==6) FunctionDefaults<6>::set_cell(cell);

    FunctionDefaults<3>::set_k(pair.k());
    FunctionDefaults<6>::set_k(pair.k());

    FunctionDefaults<3>::set_thresh(pair.thresh());
    FunctionDefaults<6>::set_thresh(pair.thresh());

//    pair.truncate();
    std::string line="loaded function "+name;
    pair.print_size(line);

}

template<size_t NDIM>
void draw_line(World& world, Function<double,NDIM>& pair, const std::string restart_name) {

    Vector<double,NDIM> lo(0.0), hi(0.0);
    lo[0]=-8.0;
    hi[0]=8.0;

    {
        std::string filename="line_"+restart_name;
        trajectory<NDIM> line=trajectory<NDIM>::line2(lo,hi,601);
        plot_along<NDIM>(world,line,pair,filename);
    }

}

template<size_t NDIM>
void draw_circle(World& world, Function<double,NDIM>& pair, const std::string restart_name) {

	std::string filename="circle_"+restart_name;
	coord_3d el2(0.0);
	el2[1]=0.5;
	trajectory<NDIM> circ(0.5,el2,601);
	plot_along<NDIM>(world,circ,pair,filename);

}



template<size_t NDIM>
void draw_plane(World& world, Function<double,NDIM>& function, const std::string restart_name) {

    std::string filename="plane_"+restart_name;
    const double scale=0.03;
    // assume a cubic cell
    double lo=-FunctionDefaults<6>::get_cell_width()[0]*0.5;
    lo=lo*scale;
//    const double hi=FunctionDefaults<6>::get_cell_width()[0]*0.5;

    const long nstep=150;
    const double stepsize=FunctionDefaults<6>::get_cell_width()[0]*scale/nstep;

    if(world.rank() == 0) {
      FILE *f =  0;
      f=fopen(filename.c_str(), "w");
      if(!f) MADNESS_EXCEPTION("plot_along: failed to open the plot file", 0);


      for (int i0=0; i0<nstep; i0++) {
        for (int i1=0; i1<nstep; i1++) {
          
          Vector<double,NDIM> coord(0.0);

          // plot x1/y1-plane
          coord[0]=lo+i0*stepsize;
          coord[1]=lo+i1*stepsize;

          // other electron
          coord[3]=0.5;

          fprintf(f,"%4i %4i %12.6f\n",i0,i1,function(coord));

        }
        // gnuplot-style 
        fprintf(f,"\n");
      }
      fclose(f);
   }


}


void do_stuff(World& world, const std::string name) {

    Function<double,6> tmp,tmp2,tmp3,tmp4,GVpair,GVpair1;

    load_function(world,tmp,"tmp");
    draw_plane(world,tmp,"tmp");

    load_function(world,tmp2,"tmp2");
    draw_plane(world,tmp2,"tmp2");

    load_function(world,tmp3,"tmp3");
    draw_plane(world,tmp3,"tmp3");

    load_function(world,tmp4,"tmp4");
    draw_plane(world,tmp4,"tmp4");

    load_function(world,GVpair,"GVpair");
//    draw_plane(world,GVpair,"GVpair");

    load_function(world,GVpair1,"GVpair1");
//    draw_plane(world,GVpair1,"GVpair1");

    // add functions
//    tmp=tmp+tmp2;
//    draw_plane(world,tmp,"tmp+tmp2");
//
//    tmp=tmp+tmp3;
//    draw_plane(world,tmp,"tmp+tmp2+tmp3");
//
    Function<double,6> tmp32=tmp3+tmp2;
    draw_plane(world,tmp32,"tmp32");

    Function<double,6> tmp21=tmp2+tmp;
    draw_plane(world,tmp21,"tmp21");

    double tight= FunctionDefaults<6>::get_thresh()*0.1;
    tmp4=tmp3+tmp2+tmp;
//    tmp4.truncate(tight).print_size("tmp4");
    draw_plane(world,tmp4,"tmp3+tmp2+tmp");


//    tmp4.truncate();
//    tmp4.print_size("tmp4.truncate()");
//    GVpair1.truncate();
//    GVpair1.print_size("GVpair1.truncate()");

    Function<double,6> aa=(tmp4+GVpair1).reduce_rank();
    aa.print_size("tmp4+GVpair1");
    draw_plane(world,aa,"aa");
    draw_line(world,aa,"tmp4+GVpair1");
    Function<double,6> a=(tmp4+GVpair1).truncate(); 
    a.print_size("(tmp4+GVpair1).truncate(eps)");
    draw_plane(world,a,"(tmp4+GVpair1).truncate(eps)");
    draw_line(world,a,"(tmp4+GVpair1).truncate(eps)");
    Function<double,6> b=(tmp4+GVpair1).truncate(FunctionDefaults<6>::get_thresh()*0.1); 
    b.print_size("(tmp4+GVpair1).truncate(eps*0.1)");
    draw_line(world,b,"(tmp4+GVpair1).truncate(eps*0.1)");

}

int main(int argc, char** argv) {
    initialize(argc, argv);
    World world(MPI::COMM_WORLD);
    startup(world,argc,argv);
    std::cout.precision(6);

    const double L=16;
    FunctionDefaults<3>::set_cubic_cell(-L/2,L/2);
    FunctionDefaults<6>::set_cubic_cell(-L/2,L/2);
    FunctionDefaults<6>::set_tensor_type(TT_2D);


    if (world.rank()==0) {
     	    print("cell size:         ", FunctionDefaults<6>::get_cell_width()[0]);
    }

    // load the function of interest
    std::string restart_name;
    bool restart=false;

    for(int i = 1; i < argc; i++) {
        const std::string arg=argv[i];

        // break parameters into key and val
        size_t pos=arg.find("=");
        std::string key=arg.substr(0,pos);
        std::string val=arg.substr(pos+1);

        if (key=="restart") {                               // usage: restart=path/to/mo_file
            restart_name=stringify(val);
            restart=true;
        }
    }

    Function<double,6> pair;
    if (restart) load_function(world,pair,restart_name);

    // make sure we're doing what we want to do
    if (world.rank()==0) {
        print("polynomial order:  ", FunctionDefaults<6>::get_k());
        print("threshold:         ", FunctionDefaults<6>::get_thresh());
        print("cell size:         ", FunctionDefaults<6>::get_cell()(0,1) - FunctionDefaults<6>::get_cell()(0,0));
        print("truncation mode:   ", FunctionDefaults<6>::get_truncate_mode());
        print("tensor type:       ", FunctionDefaults<6>::get_tensor_type());
        print("");
        print("orthogonalization  ", OrthoMethod());
        print("facReduce          ", GenTensor<double>::fac_reduce());
        print("max displacement   ", Displacements<6>::bmax_default());
        print("apply randomize    ", FunctionDefaults<6>::get_apply_randomize());
        print("world.size()       ", world.size());
        print("");
    }

    if (restart) {
    	draw_line(world,pair,restart_name);
    	draw_circle(world,pair,restart_name);
    }
    do_stuff(world,restart_name);

    return 0;
}

