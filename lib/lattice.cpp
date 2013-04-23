#include <Eigen/Dense>
#include <Eigen/QR>
#include <complex>
#include <boost/python.hpp>
#include <boost/python/list.hpp>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unsupported/Eigen/MatrixFunctions>

using namespace Eigen;
namespace py = boost::python;
using namespace std;

namespace lattice
{
  const complex<double> i (0,1);
  const double pi = 3.1415926535897932384626433;
  
  int mod(int n, const int &d)
  {
    while(n < 0) {
      n += d;
    }
    return n%d;
  }
  
  void copyarray(int a1[], const int a2[], const int &length)
  {
    for(int i = 0; i < length; i++) {
      a1[i] = a2[i];
    }
  }
}

class Lattice
{
public:
  Lattice(const int n = 8,
	  const double beta = 5.5,
	  const int Ncor = 50,
	  const int Ncf = 1000,
	  const double eps = 0.24,
	  const double a = 0.25,
	  const double smear_eps = 1./12,
	  const double u0 = 1,
	  const int action = 0);
  Lattice(const Lattice& L);
  double init_u0();

  ~Lattice();
  Matrix3cd calcPath(const vector<vector<int> > path);
  Matrix3cd calcLine(const int start[4], const int finish[4]);
  double W(const int c1[4], const int c2[4], const int n_smears = 0);
  double W(const int c[4], const int r, const int t, const int dim, const int n_smears = 0);
  double Wav(const int r, const int t, const int n_smears = 0);
  double W_p(const py::list cnr, const int r, const int t, const int dim, const int n_smears = 0);
  double P(const int site[4], const int mu, const int nu);
  double P_p(const py::list site2,const int mu, const int nu);
  double R(const int site[4], const int mu, const int nu);
  double R_p(const py::list site2,const int mu, const int nu);
  double T(const int site[4],const int mu, const int nu);
  double T_p(const py::list site2,const int mu, const int nu);
  double Pav();
  double Rav();
  double (Lattice::*Si)(const int link[5]);
  Matrix3cd randomSU3();
  void thermalize();
  void nextConfig();
  void update();
  void printL();
  Matrix3cd link(const int link[5]);
  void smear(const int time, const int n_smears);
  Matrix3cd fdiff(const int link[5]);
  py::list getLink(const int i, const int j, const int k, const int l, const int m) const;
  py::list getRandSU3(const int i) const;

  int Ncor, Ncf, n;

  friend struct my_pickle_suite;

private:
  double beta, eps, a, u0, smear_eps;
  int nupdates, action;
  double SiW(const int link[5]);
  double SiImp(const int link[5]);
  vector< vector< vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > > links;
  vector<Matrix3cd, aligned_allocator<Matrix3cd> > randSU3s;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  
};

Lattice::Lattice(const int n, const double beta, const int Ncor, const int Ncf, const double eps, const double a, const double smear_eps, const double u0, const int action)
{
  /*Default constructor. Assigns function arguments to member variables
   and initializes links.*/
  this->n = n;
  this->beta = beta;
  this->Ncor = Ncor;
  this->Ncf = Ncf;
  this->eps = eps;
  this->a = a;
  this->smear_eps = smear_eps;
  this->nupdates = 0;
  this->u0 = u0;
  this->action = action;

  srand(time(NULL));
  //Resize the link vector and assign each link a random SU3 matrix
  this->links.resize(this->n);
  for(int i = 0; i < this->n; i++) {
    this->links[i].resize(this->n);
    for(int j = 0; j < this->n; j++) {
      this->links[i][j].resize(this->n);
      for(int k = 0; k < this->n; k++) {
	this->links[i][j][k].resize(this->n);
	for(int l = 0; l < this->n; l++) {
	  this->links[i][j][k][l].resize(4);
	  for(int m = 0; m < 4; m++) {
	    this->links[i][j][k][l][m] = this->randomSU3();
	  }
	}
      }
    }
  }
  //Generate a set of random SU3 matrices for use in the updates
  for(int i = 0; i < 200; i++) {
    Matrix3cd randSU3 = this->randomSU3();
    this->randSU3s.push_back(randSU3);
    this->randSU3s.push_back(randSU3.adjoint());
  }

  //Set the action to point to the correct function
  if(action == 0) {
    this->Si = &Lattice::SiW;
  }
  else if(action == 1) {
    this->Si = &Lattice::SiImp;
  }
  else {
    cout << "Warning! Specified action does not exist." << endl;
    this->Si = &Lattice::SiW;
  }
}

Lattice::Lattice(const Lattice& L)
{
  /*Default constructor. Assigns function arguments to member variables
   and initializes links.*/
  this->n = L.n;
  this->beta = L.beta;
  this->Ncor = L.Ncor;
  this->Ncf = L.Ncf;
  this->eps = L.eps;
  this->a = L.a;
  this->smear_eps = L.smear_eps;
  this->nupdates = L.nupdates;
  this->u0 = L.u0;
  this->links = L.links;
  this->randSU3s = L.randSU3s;
  this->Si = L.Si;
  this->action = action;
}

double Lattice::init_u0()
{
  /*Calculate u0*/
  this->thermalize();
  this->u0 = pow(this->Pav(),0.25);
  return pow(this->Pav(),0.25);
}

Lattice::~Lattice()
{
  /*Destructor*/
}

Matrix3cd Lattice::link(const int link[5])
{
  /*Return link specified by index (sanitizes link indices)*/
  int link2[5];
  for(int i = 0; i < 5; i++) {
    link2[i] = lattice::mod(link[i],this->n);
  }
  return this->links[link2[0]][link2[1]][link2[2]][link2[3]][link2[4]];
}

Matrix3cd Lattice::fdiff(const int link[5])
{
  /*Calculate the finite difference at the given link*/
  Matrix3cd out = Matrix3cd::Zero();
  //Array to keep track of current link
  int clink[5] = {0,0,0,0,0};
  //Loop over *spatial* dimensions
  for(int rho = 1; rho < 4; rho++) {
    //Temporary matrix to apply operations to
    Matrix3cd temp = Matrix3cd::Identity();
    //Update the link index
    lattice::copyarray(clink,link,5);
    clink[4] = rho;
    temp *= this->link(clink);
    //Update the link
    clink[4] = link[4];
    clink[rho] = link[rho] + 1;
    temp *= this->link(clink);
    //Update the link
    lattice::copyarray(clink,link,5);
    clink[4] = rho;
    clink[link[4]] += 1;
    temp *= this->link(clink).adjoint();

    //Add the first term then reset
    out += temp;

    //Next term is just the given link
    out -= 2*pow(this->u0,2) * this->link(link);

    //Reset temp
    temp = Matrix3cd::Identity();
    //Set the current link
    lattice::copyarray(clink,link,5);
    clink[4] = rho;
    clink[rho] -= 1;
    temp *= this->link(clink).adjoint();
    //Set the link again
    clink[4] = link[4];
    temp *= this->link(clink);
    //And again
    clink[4] = rho;
    clink[link[4]] += 1;
    temp *= this->link(clink);
    
    out += temp;
  }
  return out / pow(this->a * this->u0, 2);
}

void Lattice::smear(const int time, const int n_smears)
{
  /*Smear the specified time slice by iterating calling this function*/
  for(int i = 0; i < n_smears; i++) {
    //Iterate through all the links and calculate the new ones from the existing ones.    
    vector< vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > newlinks(this->n, vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > >(this->n,vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > >(this->n, vector< Matrix3cd, aligned_allocator<Matrix3cd> >(4))));
    for(int i = 0; i < this->n; i++) {
      for(int j = 0; j < this->n; j++) {
	for(int k = 0; k < this->n; k++) {
	  for(int l = 0; l < 4; l++) {
	    //Create a temporary matrix to store the new link
	    int link[5] = {time,i,j,k,l};
	    Matrix3cd temp_mat = this->link(link);;
	    temp_mat += this->smear_eps * pow(this->a,2) * this->fdiff(link);
	    newlinks[i][j][k][l] = temp_mat;
	  }
	}
      }
    }
    //Apply the changes to the existing lattice.
    this->links[lattice::mod(time,this->n)] = newlinks;
  }
}

Matrix3cd Lattice::calcPath(const vector<vector<int> > path)
{
  /*Multiplies the matrices together specified by the indices in path*/
  Matrix3cd out = Matrix3cd::Identity();
  
  for(int i = 0; i < path.size() - 1; i++) {
    //Which dimension are we moving in?
    int dim = path[i][4];
    int dim_diff = path[i+1][dim] - path[i][dim];
    if(abs(dim_diff) != 1) {
      // Consecutive points don't match link direction, so throw an error
      cout << "Error! Path contains non-consecutive link variables." << endl;
    }
    else if(dim_diff == -1) {
      /*We're going backwards, so the link must be the adjoint of the link 
	matrix, which we get by using the next site on the lattice.*/
      int link[5] = {path[i+1][0],path[i+1][1],path[i+1][2],path[i+1][3],path[i][4]};
      out *= this->link(link).adjoint();
    }
    else {
      //We're going forwards, so it's just the normal matrix
      int link[5] = {path[i][0],path[i][1],path[i][2],path[i][3],path[i][4]};
      out *= this->link(link);
    }
  }

  return out;
}

Matrix3cd Lattice::calcLine(const int start[4], const int finish[4])
{
  /*Multiplies all gauge links along line from start to finish*/
  //First check that's actually a straight path
  Matrix3cd out = Matrix3cd::Identity();
  int count_dims = 0;
  int dim = 0;
  for(int i = 0; i < 4; i++) {
    if(abs(start[i] - finish[i]) != 0) {
      /*Keep track of the most recent dimension that differs between start
	and finish, as if we're good to go then we'll need this when
	defining the path.*/
      dim = i;
      count_dims++;
    }
  }

  if(count_dims != 1) {
    cout << "Error! Start and end points do not form a straight line." << endl;
  }
  else {
    //If the two points are on the same line, we're good to go
    vector<vector<int> >  line; //Stores the path

    //Now need to know if we're going backwards or forwards
    if(start[dim] > finish[dim]) {
      for(int i = start[dim]; i >= finish[dim]; i--) {
	//Create the link vector to append, and initialize it's elements
	vector<int> link;
	link.assign(start,start+4);
	//Update the index that's parallel to the line with the current
	//location
	link[dim] = i;
	//The direction is going to be equal to the direction of the line
	link.push_back(dim);
	//Push it onto the line
	line.push_back(link);
      }
      out = this->calcPath(line);
    }
    else {
      //Same again, but this time we deal with the case of going backwards
      for(int i = start[dim]; i <= finish[dim]; i++) {
	vector<int> link;
	link.assign(start,start+4);
	link[dim] = i;
	link.push_back(dim);
	line.push_back(link);
      }
      out = this->calcPath(line);
    }
  }
  return out;
}

double Lattice::W(const int c1[4], const int c2[4], const int n_smears)
{
  /*Calculates the loop specified by corners c1 and c2 (which must
    lie in the same plane)*/
  Matrix3cd out = Matrix3cd::Identity();
  vector<vector<vector<vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > linkstore1;
  vector<vector<vector<vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > linkstore2;
  //Smear the links if specified, whilst storing the non-smeared ones.
  if(n_smears > 0) {
    linkstore1 = this->links[lattice::mod(c1[0],this->n)];
    linkstore2 = this->links[lattice::mod(c2[0],this->n)];
    this->smear(c1[0],n_smears);
    this->smear(c2[0],n_smears);
  }

  //Check that c1 and c2 are on the same plane
  int dim_count = 0;
  for(int i = 0; i < 4; i++) {
    if(c1[i] != c2[i]) {
      dim_count++;
    }
  }
  
  if(dim_count != 2 || c1[0] == c2[0]) {
    cout << "Error! The two corner points do not form a rectangle with two spatial and two temporal sides." << endl;
  }
  else {
    //Get the second corner (going round the loop)
    int c3[4] = {c1[0],c1[1],c1[2],c1[3]};
    c3[0] = c2[0];
    //Calculate the line segments between the first three corners
    out *= this->calcLine(c1,c3);
    out *= this->calcLine(c3,c2);
    //And repeat for the second set of sides
    int c4[4] = {c2[0],c2[1],c2[2],c2[3]};
    c4[0] = c1[0];
    out *= this->calcLine(c2,c4);
    out *= this->calcLine(c4,c1);
  }
  //Restore the old links
  if(n_smears > 0) {
    this->links[lattice::mod(c1[0],this->n)] = linkstore1;
    this->links[lattice::mod(c2[0],this->n)] = linkstore2;
  }
  
  return 1./3 * out.trace().real();
}

double Lattice::W(const int c[4], const int r, const int t, const int dim, const int n_smears)
{
  /*Calculates the loop specified by initial corner, width, height and 
   dimension*/
  int c2[4];
  lattice::copyarray(c2,c,4);
  c2[dim] += r;
  c2[0] += t;
  return this->W(c,c2,n_smears);
}

double Lattice::Wav(const int r, const int t, const int n_smears)
{
  /*Calculates the average of all possible Wilson loops of a given
    dimension*/
  double Wtot = 0;
  for(int i = 0; i < this->n; i++) {
    for(int j = 0; j < this->n; j++) {
      for(int k = 0; k < this->n; k++) {
	for(int l = 0; l < this->n; l++) {
	  for(int m = 1; m < 4; m++) {
	    int site[4] = {i,j,k,l};
	    Wtot += this->W(site,r,t,m,n_smears);
	  }
	}
      }
    }
  }
  return Wtot / (pow(this->n,4)*3);
}

double Lattice::W_p(const py::list cnr, const int r, const int t, const int dim, const int n_smears)
{
  /*Calculates the loop specified by corners c1 and c2 (which must
    lie in the same plane)*/
  int c[4] = {py::extract<int>(cnr[0]),py::extract<int>(cnr[1]),py::extract<int>(cnr[2]),py::extract<int>(cnr[3])};
  return this->W(c,r,t,dim,n_smears);
}

double Lattice::T(const int site[4],const int mu, const int nu)
{
  /*Calculate the twisted rectangle operator*/
  
  //Define some variables to offset the given site
  int mu_vec[4] = {0,0,0,0};
  mu_vec[mu] = 1;
  int nu_vec[4] = {0,0,0,0};
  nu_vec[nu] = 1;
  //Links also contain direction information, so must create a new set of
  //variables
  int link1[5] = {0,0,0,0,mu};
  int link2[5] = {0,0,0,0,nu};
  int link3[5] = {0,0,0,0,mu};
  int link4[5] = {0,0,0,0,nu};
  int link5[5] = {0,0,0,0,mu};
  int link6[5] = {0,0,0,0,nu};
  int link7[5] = {0,0,0,0,mu};
  int link8[5] = {0,0,0,0,nu};

  for(int i = 0; i < 4; i++) {
    link1[i] = site[i];
    link2[i] = site[i] + mu_vec[i];
    link3[i] = site[i] + mu_vec[i] + nu_vec[i];
    link4[i] = site[i] + 2 * mu_vec[i];
    link5[i] = site[i] + mu_vec[i];
    link6[i] = site[i] + mu_vec[i];
    link7[i] = site[i] + nu_vec[i];
    link8[i] = site[i];
  }

  //Multiply all the links together to get the product.
  Matrix3cd product = this->link(link1);
  product *= this->link(link2);
  product *= this->link(link3);
  product *= this->link(link4).adjoint();
  product *= this->link(link5).adjoint();
  product *= this->link(link6);
  product *= this->link(link7).adjoint();
  product *= this->link(link8).adjoint();

  return 1./3 * product.trace().real();
}

double Lattice::T_p(const py::list site2,const int mu, const int nu)
{
  //Python wrapper for rectangle function
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->T(site,mu,nu);
  
}

double Lattice::R(const int site[4],const int mu, const int nu)
{
  /*Calculate the rectangle operator at the given site, on the rectangle
    specified by directions mu and nu.*/
  
  //Define some variables to offset the given site
  int mu_vec[4] = {0,0,0,0};
  mu_vec[mu] = 1;
  int nu_vec[4] = {0,0,0,0};
  nu_vec[nu] = 1;
  //Links also contain direction information, so must create a new set of
  //variables
  int link1[5] = {0,0,0,0,mu};
  int link2[5] = {0,0,0,0,mu};
  int link3[5] = {0,0,0,0,nu};
  int link4[5] = {0,0,0,0,mu};
  int link5[5] = {0,0,0,0,mu};
  int link6[5] = {0,0,0,0,nu};

  for(int i = 0; i < 4; i++) {
    link1[i] = site[i];
    link2[i] = site[i] + mu_vec[i];
    link3[i] = site[i] + 2 * mu_vec[i];
    link4[i] = site[i] + mu_vec[i] + nu_vec[i];
    link5[i] = site[i] + nu_vec[i];
    link6[i] = site[i];
  }

  //Multiply all the links together to get the product.
  Matrix3cd product = this->link(link1);
  product *= this->link(link2);
  product *= this->link(link3);
  product *= this->link(link4).adjoint();
  product *= this->link(link5).adjoint();
  product *= this->link(link6).adjoint();

  return 1./3 * product.trace().real();  
}

double Lattice::R_p(const py::list site2,const int mu, const int nu)
{
  //Python wrapper for rectangle function
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->R(site,mu,nu);
  
}

double Lattice::P(const int site[4],const int mu, const int nu)
{
  /*Calculate the plaquette operator at the given site, on plaquette
    specified by directions mu and nu.*/

  //We define some variables to contain the offsets of the various
  //links around the lattice
  int mu_vec[4] = {0,0,0,0};
  mu_vec[mu] = 1;
  int nu_vec[4] = {0,0,0,0};
  nu_vec[nu] = 1;
  //The links also contain direction information, so we must create a new
  //set of variables
  int link1[5] = {0,0,0,0,mu};
  int link2[5] = {0,0,0,0,nu};
  int link3[5] = {0,0,0,0,mu};
  int link4[5] = {0,0,0,0,nu};

  //Do some assignment
  for(int i = 0; i < 4; i++) {
    link1[i] = site[i];
    link2[i] = site[i] + mu_vec[i];
    link3[i] = site[i] + nu_vec[i];
    link4[i] = site[i];
  }

  //Run through the links and multiply them together.
  Matrix3cd product = this->link(link1);
  product *= this->link(link2);
  product *= this->link(link3).adjoint();
  product *= this->link(link4).adjoint();
  return 1./3 * product.trace().real();
}

double Lattice::P_p(const py::list site2,const int mu, const int nu)
{
  /*Python wrapper for the plaquette function.*/
  int site[4] = {py::extract<int>(site2[0]),py::extract<int>(site2[1]),py::extract<int>(site2[2]),py::extract<int>(site2[3])};
  return this->P(site,mu,nu);
}

double Lattice::SiImp(const int link[5])
{
  /*Calculate contribution to improved action from given link*/

  //First contrbution is from standard Wilson action, so add that in
  double out = 5./3 * this->SiW(link);
  double Rsum = 0;

  int planes[3];

  //Work out which dimension the link is in, since it'll be irrelevant here
  int j = 0;
  for(int i = 0; i < 4; i++) {
    if(link[4] != i) {
      planes[j] = i;
      j++;
    }
  }
  
  for(int i = 0; i < 3; i++) {
    int site[4] = {link[0],link[1],link[2],link[3]};
    Rsum += this->R(site,link[4],planes[i]);
    site[link[4]] -= 1;
    Rsum += this->R(site,link[4],planes[i]);

    lattice::copyarray(site,link,4);
    site[planes[i]] -= 1;
    Rsum += this->R(site,link[4],planes[i]);
    site[link[4]] -= 1;
    Rsum += this->R(site,link[4],planes[i]);
    
    lattice::copyarray(site,link,4);
    Rsum += this->R(site,planes[i],link[4]);
    site[planes[i]] -= 2;
    Rsum += this->R(site, planes[i],link[4]);
  }
  out += this->beta / (12 * pow(this->u0,6)) * Rsum;
  return out;
}

double Lattice::SiW(const int link[5])
{
  /*Calculate the contribution to the Wilson action from the given link*/
  int planes[3];
  double Psum = 0;

  //Work out which dimension the link is in, since it'll be irrelevant here
  int j = 0;
  for(int i = 0; i < 4; i++) {
    if(link[4] != i) {
      planes[j] = i;
      j++;
    }    
  }

  /*For each plane, calculate the two plaquettes that share the given link*/
  for(int i = 0; i < 3; i++) {
    int site[4] = {link[0],link[1],link[2],link[3]};
    Psum += this->P(site,link[4],planes[i]);
    site[planes[i]] -= 1;
    Psum += this->P(site,link[4],planes[i]);
  }

  return -this->beta * Psum / pow(this->u0,4);
}

Matrix3cd Lattice::randomSU3()
{
  /*Generate a random SU3 matrix, weighted by eps*/
  Matrix3cd A;
  //First generate a random matrix whos elements all lie in/on unit circle  
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 3; j++) {
      A(i,j) = double(rand()) / double(RAND_MAX);
      A(i,j) *= exp(2 * lattice::pi * lattice::i * double(rand()) / double(RAND_MAX));
    }
  }
  //Weight the matrix with weighting eps
  A *= this->eps;
  //Make the matrix traceless and Hermitian
  A(2,2) = -(A(1,1) + A(0,0));
  Matrix3cd B = 0.5 * (A - A.adjoint());
  Matrix3cd U = B.exp();
  //Compute the matrix exponential to get a special unitary matrix
  return U;
}

void Lattice::thermalize()
{
  /*Update all links until we're at thermal equilibrium*/

  while(this->nupdates < 30 * this->Ncor) {
    this->update();
  }
}

void Lattice::nextConfig()
{
  /*Run Ncor updates*/
  for(int i = 0; i < this->Ncor; i++) {
    this->update();
  }
}

void Lattice::update()
{
  /*Iterate through the lattice and update the links using Metropolis
    algorithm*/
  for(int i = 0; i < this->n; i++) {
    for(int j = 0; j < this->n; j++) {
      for(int k = 0; k < this->n; k++) {
	for(int l = 0; l < this->n; l++) {
	  for(int m = 0; m < 4; m++) {
	    //We'll need an array with the link indices
	    int link[5] = {i,j,k,l,m};
	    //Record the old action contribution
	    double Si_old = (this->*Si)(link);
	    //Record the old link in case we need it
	    Matrix3cd linki_old = this->links[i][j][k][l][m];
	    //Get ourselves a random SU3 matrix for the update
	    Matrix3cd randSU3 = this->randSU3s[rand() % this->randSU3s.size()];
	    //Multiply the site
	    this->links[i][j][k][l][m] = randSU3 * this->links[i][j][k][l][m];
	    //What's the change in the action?
	    double dS = (this->*Si)(link) - Si_old;
	    //Was the change favourable? If not, revert the change
	    if((dS > 0) && (exp(-dS) < double(rand()) / double(RAND_MAX))) {
	      this->links[i][j][k][l][m] = linki_old;
	    }
	  }
	}
      }
    }
  }
  this->nupdates++;
}

double Lattice::Pav()
{
  /*Calculate average plaquette operator value*/
  //mu > nu, so there are six plaquettes at each site.
  int nus[6] = {0,0,0,1,1,2};
  int mus[6] = {1,2,3,2,3,3};
  double Ptot = 0;
  //Pretty simple: step through the matrix and add all plaquettes up
  for(int i = 0; i < this->n; i++) {
    for(int j = 0; j < this->n; j++) {
      for(int k = 0; k < this->n; k++) {
	for(int l = 0; l < this->n; l++) {
	  for(int m = 0; m < 6; m++) {
	    int site[4] = {i,j,k,l};
	    Ptot += this->P(site,mus[m],nus[m]);
	  }
	}
      }
    }
  }
  //Divide through by number of plaquettes to get mean (simples!)
  return Ptot / (pow(this->n,4) * 6);
}

double Lattice::Rav()
{
  /*Calculate average plaquette operator value*/
  //mu > nu, so there are six plaquettes at each site.
  int nus[6] = {0,0,0,1,1,2};
  int mus[6] = {1,2,3,2,3,3};
  double Rtot = 0;
  //Pretty simple: step through the matrix and add all plaquettes up
  for(int i = 0; i < this->n; i++) {
    for(int j = 0; j < this->n; j++) {
      for(int k = 0; k < this->n; k++) {
	for(int l = 0; l < this->n; l++) {
	  for(int m = 0; m < 6; m++) {
	    int site[4] = {i,j,k,l};
	    Rtot += this->R(site,mus[m],nus[m]);
	  }
	}
      }
    }
  }
  //Divide through by number of plaquettes to get mean (simples!)
  return Rtot / (pow(this->n,4) * 6);
}

void Lattice::printL()
{
  /*Print the links out. A bit redundant due to the interfaces library,
   but here in case it's needed.*/
  for(int i = 0; i < this->n; i++) {
    for(int j = 0; j < this->n; j++) {
      for(int k = 0; k < this->n; k++) {
	for(int l = 0; l < this->n; l++) {
	  for(int m = 0; m < 4; m++) {
	    cout << this->links[i][j][k][l][m] << endl;
	  }
	}
      }
    }
  }
}

py::list Lattice::getLink(const int i, const int j, const int k, const int l, const int m) const
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

py::list Lattice::getRandSU3(const int i) const
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

struct my_pickle_suite : py::pickle_suite
{
  static py::tuple getinitargs(const Lattice& L)
  {
    return py::make_tuple(L.n,L.beta,L.Ncor,L.Ncf,L.eps,L.a,L.smear_eps,L.u0,L.action);
  }
  
  static py::tuple getstate(const Lattice& L)
  {
    //Convert the links vector to a list for python compatability
    py::list links;
    for(int i = 0; i < L.n; i++) {
      py::list temp1;
      for(int j = 0; j < L.n; j++) {
	py::list temp2;
	for(int k = 0; k < L.n; k++) {
	  py::list temp3;
	  for(int l = 0; l < L.n; l++) {
	    py::list temp4;
	    for(int m = 0; m < 4; m++) {
	      temp4.append(L.getLink(i,j,k,l,m));
	    }
	    temp3.append(temp4);
	  }
	  temp2.append(temp3);
	}
	temp1.append(temp2);
      }
      links.append(temp1);
    }
    
    //Same for the randSU3s
    py::list randSU3s;
    int index = 0;
    for(int i = 0; i < 20; i++) {
      py::list temp_list;
      for(int j = 0; j < 20; j++) {
	temp_list.append(L.getRandSU3(index));
	index++;
      }
      randSU3s.append(temp_list);
    }
    return py::make_tuple(links,randSU3s);
  }
  
  static void setstate(Lattice& L, py::tuple state)
  {
    if(len(state) != 2) {
      PyErr_SetObject(PyExc_ValueError,
		      ("expected 2-item tuple in call to __setstate__; got %s"
		       % state).ptr());
      py::throw_error_already_set();
    }
    
    vector< vector< vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > > links;
    vector<Matrix3cd, aligned_allocator<Matrix3cd> > randSU3s;
    py::list link_states = py::extract<py::list>(state[0]);
    py::list randSU3_states = py::extract<py::list>(state[1]);

    //Convert the compound list of links back to a vector...
    for(int i = 0; i < L.n; i++) {
      vector< vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > > temp1;
      for(int j = 0; j < L.n; j++) {
	vector< vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > > temp2;
	for(int k = 0; k < L.n; k++) {
	  vector< vector<Matrix3cd, aligned_allocator<Matrix3cd> > > temp3;
	  for(int l = 0; l < L.n; l++) {
	    vector<Matrix3cd, aligned_allocator<Matrix3cd> > temp4;
	    for(int m = 0; m < 4; m++) {
	      Matrix3cd temp_mat;
	      for(int n = 0; n < 3; n++) {
		for(int o = 0; o < 3; o++) {
		  temp_mat(n,o) = py::extract<complex<double> >(link_states[i][j][k][l][m][n][o]);
		}
	      }
	      temp4.push_back(temp_mat);
	    }
	    temp3.push_back(temp4);
	  }
	  temp2.push_back(temp3);
	}
	temp1.push_back(temp2);
      }
      links.push_back(temp1);
    }
    //And the same for the random SU3 matrices.
    int index = 0;
    for(int i = 0; i < 20; i++) {
      for(int j = 0; j < 20; j++) {
	Matrix3cd temp_mat;
	for(int k = 0; k < 3; k++) {
	  for(int l = 0; l < 3; l++) {
	    temp_mat(k,l) = py::extract<complex<double> >(randSU3_states[i][j][k][l]);
	  }
	}
	randSU3s.push_back(temp_mat);
      }
    }
    
    L.links = links;
    L.randSU3s = randSU3s;
  }
};
  
//Boost python wrapping of the class
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(LatticeWOverload,W_p,4,5)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(LatticeWavOverload,Wav,2,3)

BOOST_PYTHON_MODULE(lattice)
{
  py::class_<Lattice>("Lattice", py::init<py::optional<int,double,int,int,double,double,double,double,int> >())
    .def(py::init<Lattice&>())
    .def_pickle(my_pickle_suite())
    .def("update",&Lattice::update)
    .def("nextConfig",&Lattice::nextConfig)
    .def("thermalize",&Lattice::thermalize)
    .def("init_u0",&Lattice::init_u0)
    .def("P",&Lattice::P_p)
    .def("Pav",&Lattice::Pav)
    .def("R",&Lattice::R_p)
    .def("Rav",&Lattice::Rav)
    .def("T",&Lattice::T_p)
    .def("W",&Lattice::W_p,LatticeWOverload(py::args("cnr","r","t","dim","n_smears"), "Calculate Wilson loop"))
    .def("Wav",&Lattice::Wav,LatticeWavOverload(py::args("r","t","n_smears"), "Calculate average Wilson loop"))
    .def("printL",&Lattice::printL)
    .def("getLink",&Lattice::getLink)
    .def("getRandSU3",&Lattice::getRandSU3)
    .def_readonly("Ncor",&Lattice::Ncor)
    .def_readonly("Ncf",&Lattice::Ncf)
    .def_readonly("n",&Lattice::n);
}
