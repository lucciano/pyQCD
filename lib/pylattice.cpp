#include "lattice.cpp"
#include "pylattice.hpp"

pyLattice::pyLattice(const int n, const double beta, const int Ncor, const int Ncf, const double eps, const double a, const double smear_eps, const double u0, const int action) :
  Lattice::Lattice(n, beta, Ncor, Ncf, eps, a, smear_eps, u0, action)
{
  
}

pyLattice::~pyLattice()
{
  
}

double pyLattice::W_p(const py::list cnr, const int r, const int t, const int dim, const int n_smears)
{
  /*Calculates the loop specified by corners c1 and c2 (which must
    lie in the same plane)*/
  int c[4] = {py::extract<int>(cnr[0]),py::extract<int>(cnr[1]),py::extract<int>(cnr[2]),py::extract<int>(cnr[3])};
  return this->W(c,r,t,dim,n_smears);
}

double pyLattice::T_p(const py::list site2,const int mu, const int nu)
{
  //Python wrapper for rectangle function
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->T(site,mu,nu);
  
}

double pyLattice::R_p(const py::list site2,const int mu, const int nu)
{
  //Python wrapper for rectangle function
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->R(site,mu,nu);
}

double pyLattice::P_p(const py::list site2,const int mu, const int nu)
{
  /*Python wrapper for the plaquette function.*/
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->P(site,mu,nu);
}

void pyLattice::runThreads(const int size, const int n_updates, const int remainder)
{
  ScopedGILRelease scope = ScopedGILRelease();
  Lattice::runThreads(size,n_updates,remainder);
}

py::list pyLattice::getLink(const int i, const int j, const int k, const int l, const int m) const
{
  /*Returns the given link as a python nested list. Used in conjunction
   with python interfaces library to extract the links as a nested list
   of numpy matrices.*/
  py::list out;
  for(int n = 0; n < 3; n++) {
    py::list temp;
    for(int o = 0; o < 3; o++) {
      temp.append(this->links[i][j][k][l][m](n,o));
    }
    out.append(temp);
  }
  return out;
}

py::list pyLattice::getRandSU3(const int i) const
{
  /*Returns the given random SU3 matrix as a python list*/
  py::list out;
  for(int n = 0; n < 3; n++) {
    py::list temp;
    for(int o = 0; o < 3; o++) {
      temp.append(this->randSU3s[i](n,o));
    }
    out.append(temp);
  }
  return out;
}