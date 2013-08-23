//
// 
//                This source code is part of
// 
//                        M D S C T K
// 
//       Molecular Dynamics Spectral Clustering ToolKit
// 
//                        VERSION 1.2.0
// Written by Joshua L. Phillips.
// Copyright (c) 2013, Joshua L. Phillips.
// check out http://github.com/douradopalmares/mdsctk/ for more information.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// If you want to redistribute modifications, please consider that
// derived work must not be called official MDSCTK. Details are found
// in the README & LICENSE files - if they are missing, get the
// official version at github.com/douradopalmares/mdsctk/.
// 
// To help us fund MDSCTK development, we humbly ask that you cite
// the papers on the package - you can find them in the top README file.
// 
// For more info, check our website at http://github.com/douradopalmares/mdsctk/
// 
//

// Standard
// C
#include <strings.h>
#include <stdlib.h>
#include <math.h>
// C++
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>

// Boost
#include <boost/program_options.hpp>

// OpenMP
#include <omp.h>

// Local
#include "config.h"
#include "mdsctk.h"

namespace po = boost::program_options;
using namespace std;

// You can change this function to utilize different
// distance metrics. The current implementation
// uses Euclidean distance...
double distance(int size, double* reference, double* fitting) {
  double value = 0.0;
  for (int x = 0; x < size; x++)
    value += (reference[x] - fitting[x]) * (reference[x] - fitting[x]);
  return (sqrt(value));
}

vector<double> fits;

bool compare(int left, int right) {
  return fits[left] < fits[right];
}

int main(int argc, char* argv[]) {

  const char* program_name = "knn_data";
  bool optsOK = true;
  copyright(program_name);
  cout << "   Computes the k nearest neighbors of all pairs of" << endl;
  cout << "   vectors in the given binary data files." << endl;
  cout << endl;
  cout << "   Use -h or --help to see the complete list of options." << endl;
  cout << endl;

  // Option vars...
  int nthreads = 0;
  int k = 0;
  int vector_size = 0;
  string ref_filename;
  string fit_filename;
  string d_filename;
  string i_filename;

  // Declare the supported options.
  po::options_description cmdline_options;
  po::options_description program_options("Program options");
  program_options.add_options()
    ("help,h", "show this help message and exit")
    ("threads,t", po::value<int>(&nthreads)->default_value(2), "Input: Number of threads to start (int)")
    ("knn,k", po::value<int>(&k), "Input:  K-nearest neighbors (int)")
    ("size,s", po::value<int>(&vector_size), "Input:  Data vector length (int)")
    ("reference-file,r", po::value<string>(&ref_filename)->default_value("reference.pts"), "Input:  Reference data file (string:filename)")
    ("fit-file,f", po::value<string>(&fit_filename), "Input:  Fitting data file (string:filename)")
    ("distance-file,d", po::value<string>(&d_filename)->default_value("distances.dat"), "Output: K-nn distances file (string:filename)")
    ("index-file,i", po::value<string>(&i_filename)->default_value("indices.dat"), "Output: K-nn indices file (string:filename)")    
    ;
  cmdline_options.add(program_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);    

  if (vm.count("help")) {
    cout << "usage: " << program_name << " [options]" << endl;
    cout << cmdline_options << endl;
    return 1;
  }
  if (!vm.count("knn")) {
    cout << "ERROR: --knn not supplied." << endl;
    cout << endl;
    optsOK = false;
  }
  if (!vm.count("size")) {
    cout << "ERROR: --size not supplied." << endl;
    cout << endl;
    optsOK = false;
  }
  if (!vm.count("fit-file"))
    fit_filename = ref_filename;

  if (!optsOK) {
    return -1;
  }

  cout << "Running with the following options:" << endl;
  cout << "threads =        " << nthreads << endl;
  cout << "knn =            " << k << endl;
  cout << "size =           " << vector_size << endl;
  cout << "reference-file = " << ref_filename << endl;
  cout << "fit-file =       " << fit_filename << endl;
  cout << "distance-file =  " << d_filename << endl;
  cout << "index-file =     " << i_filename << endl;
  cout << endl;

  // Local vars
  vector<double*> *ref_coords = NULL;
  vector<double*> *fit_coords = NULL;
  int k1 = k + 1;
  int update_interval = 1;
  vector<double> keepers;
  vector<int> permutation;
  ofstream distances;
  ofstream indices;

  // Setup threads
  omp_set_num_threads(nthreads);

  ref_coords = new vector<double*>;
  fit_coords = new vector<double*>;

  // Read coordinates
  cout << "Reading reference coordinates from file: " << ref_filename << " ... ";
  ifstream myfile;
  myfile.open(ref_filename.c_str());
  double* mycoords = new double[vector_size];
  myfile.read((char*) mycoords, sizeof(double) * vector_size);
  while (!myfile.eof()) {
    ref_coords->push_back(mycoords);
    mycoords = new double[vector_size];
    myfile.read((char*) mycoords, sizeof(double) * vector_size);
  }
  myfile.close();
  cout << "done." << endl;

  cout << "Reading fitting coordinates from file: " << fit_filename << " ... ";
  myfile.open(fit_filename.c_str());
  myfile.read((char*) mycoords, sizeof(double) * vector_size);
  while (!myfile.eof()) {
    fit_coords->push_back(mycoords);
    mycoords = new double[vector_size];
    myfile.read((char*) mycoords, sizeof(double) * vector_size);
  }
  myfile.close();
  delete [] mycoords;
  mycoords = NULL;
  cout << "done." << endl;

  // Open output files
  distances.open(d_filename.c_str());
  indices.open(i_filename.c_str());

  // Allocate vectors for storing the RMSDs for a structure
  fits.resize(ref_coords->size());
  permutation.resize(ref_coords->size());

  // Fix k if number of frames is too small
  if (ref_coords->size()-1 < k)
    k = ref_coords->size()-1;
  k1 = k + 1;
  keepers.resize(k1);

  // Timers...
  time_t start = std::time(0);
  time_t last = start;

  // Compute fits
  for (int fit_frame = 0; fit_frame < fit_coords->size(); fit_frame++) {
    
    // Update user of progress
    if (std::time(0) - last > update_interval) {
      last = std::time(0);
      time_t eta = start + ((last-start) * fit_coords->size() / fit_frame);
      cout << "\rFrame: " << fit_frame << ", will finish " 
	   << string(std::ctime(&eta)).substr(0,20);
      cout.flush();
    }

    // Do Work
#pragma omp parallel for
    for (int ref_frame = 0; ref_frame < ref_coords->size(); ref_frame++)
      fits[ref_frame] = distance(vector_size,(*fit_coords)[fit_frame],
				 (*ref_coords)[ref_frame]);

    // Sort
    int x = 0;
    for (vector<int>::iterator p_itr = permutation.begin();
	 p_itr != permutation.end(); p_itr++)
      (*p_itr) = x++;    
    partial_sort(permutation.begin(), permutation.begin()+k1,
		 permutation.end(), compare);
    for (int x = 0; x < k1; x++)
      keepers[x] = (double) fits[permutation[x]];

    // Write out closest k RMSD alignment scores and indices
    distances.write((char*) &(keepers[1]), (sizeof(double) / sizeof(char)) * k);
    indices.write((char*) &(permutation[1]), (sizeof(int) / sizeof(char)) * k);
  }

  cout << endl << endl;

  // Clean coordinates
  for (vector<double*>::iterator itr = ref_coords->begin();
       itr != ref_coords->end(); itr++) delete [] (*itr);
  for (vector<double*>::iterator itr = fit_coords->begin();
       itr != fit_coords->end(); itr++) delete [] (*itr);
  delete ref_coords;
  delete fit_coords;

  return 0;
}
