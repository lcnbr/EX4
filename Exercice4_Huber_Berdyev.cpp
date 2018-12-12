#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <string>
#include <valarray>
#include "ConfigFile.tpp" // Fichier .tpp car inclut un template

using namespace std;

typedef valarray<double> dArray;

double square (double x) {return x*x;}

double norm( dArray x){return sqrt((x.apply(square)).sum());}

class Exercice4{

private:

  //Simulation parameters
  int sampling, last;

  //Physical parameters

  size_t d; //dimension
  double G;


  void printOut(bool force)
  {
    if((!force && last>=sampling) || (force && last!=1))
    {
      affiche();
      last = 1;
    }
    else
    {
      last++;
    }
  }

  virtual void affiche() = 0;

  double dist(size_t i, size_t j, const dArray& y){
    dArray distance(y[slice(d * i, d, 1)] - y[slice(d * j, d, 1)]);
    dArray distSquare(distance.apply(square));
    return (sqrt(distSquare.sum()));
  }

  double relVel(size_t i, size_t j, const dArray& y){
    dArray relVel(y[slice(d * i + N*d, d, 1)] - y[slice(d * j+N*d,  d, 1)]);
    dArray relVelSquare(relVel.apply(square));
    return (sqrt(relVelSquare.sum()));
  }


  double potGrav(size_t i, size_t j, const dArray& y){
    double normD(dist(i,j,y));
    return - G * M[i] * M[j] / normD;
  }

  double kinEng(size_t i, const dArray& y){
    dArray Vel(y[slice(d * i + N * d, d, 1)]);
    dArray VelSquare(Vel.apply(square));
    double normV(norm(VelSquare));
    return M[i] * normV * normV * 0.5;
  }


  dArray Fgrav(size_t i, size_t j, const dArray& y){
    dArray distance(y[slice(d*i,  d, 1)] - y[slice(d*j,  d, 1)]) ;
    dArray distSquare(distance.apply(square));
    double normD(sqrt(distSquare.sum()));
    return -G * M[i] * M[j] / pow(normD,3.0) * distance;
  }

  dArray Fdrag(size_t body, size_t other, const dArray& y){

    //Fdrag of other on body
    double normD(dist(body,other,y));

    dArray relVel(y[slice(d *body + N*d, d, 1)] - y[slice(d * other+N*d,  d, 1)]);
    dArray relVelSquare(relVel.apply(square));
    double normV(sqrt(relVelSquare.sum()));

    return -0.5 * rho(normD,other) * S[body] * Cx[body] * normV * relVel;
  }

  double rho(double dist, size_t i){
    return Rho[i]*exp(-(dist-R[i])/Lambda[i]);
  }

  virtual void step() = 0;

protected:
    double dt;
    
  size_t N; //number of bodies
    double t;
    dArray Y;//position and velocity
    dArray M;
    dArray R;
    dArray Rho;
    dArray Cx;
    dArray S;
    dArray Lambda;

    double tFin;

    double nSteps;

    ofstream *outputFile;

    dArray a(size_t body, const dArray& y){

      dArray acc(d);

      for(size_t other(0); other<N; ++other){
          if(body!=other){
            acc +=  Fgrav(body, other, y) + Fdrag(body,other,y);
          }
        }
      acc /= M[body];
      return acc;

    }

    dArray f(const dArray& y, double t){
      dArray dy(y);
      dy[slice(0,N*d,1)] = y[slice(N*d,N*d,1)];

      for(size_t i(0); i<N; ++i){
        dy[slice(d * i + N * d, d, 1)] = a(i,y);
      }
      return dy;
    }

    dArray RK4(const dArray& yi, double ti, double dt)
    {
      dArray k1(dt * f(yi, ti));
      dArray k2(dt * f(yi+0.5*k1, ti+0.5*dt));
      dArray k3(dt * f(yi+0.5*k2, ti+0.5*dt));
      dArray k4(dt * f(yi+k3, ti+dt));
      return yi + 1/6.0*(k1+2.0*k2+2.0*k3+k4);
    }
    double energyTot(){
      double potSum(0);
      double kinSum(0);

      for(size_t i = 0; i < N; i++)
      {
        for(size_t j = 0; j < N; j++)
        {
          if(i!=j) potSum += potGrav(i,j,Y);
        }
        kinSum += kinEng(i,Y);
      }
      return kinSum + potSum * 0.5;
    }



  public:
    Exercice4(ConfigFile configFile)
    {

      tFin     = configFile.get<double>("tFin");
      nSteps   = configFile.get<int>("nSteps");
      dt       = tFin/nSteps;

      sampling = configFile.get<unsigned int>("sampling");
      N        = configFile.get<size_t>("N");
      d        = configFile.get<size_t>("d");
      G        = configFile.get<double>("G");

      Y      = dArray(2*N*d);
      M      = dArray(N);
      R      = dArray(N);
      Rho    = dArray(N);
      Cx     = dArray(N);
      S      = dArray(N);
      Lambda = dArray(N);

      for(size_t i(0); i < N;++i){
        for(size_t j(0); j < d;++j){
          string key("x" + to_string(j + 1) + "_" + to_string(i + 1)); //Convention xj_i
          Y[i * d + j] = configFile.get<double>(key);

          key = ("v" + to_string(j + 1) + "_" + to_string(i + 1)); //Convention vj_i
          Y[i * d + j+N*d] = configFile.get<double>(key);
        }
      }

      for (size_t i(0); i < N; ++i){
        string key("m_" + to_string(i + 1)); //Convention m_i
        M[i] = configFile.get<double>(key);

        key = ("r_" + to_string(i + 1)); //Convention r_i
        R[i] = configFile.get<double>(key);

        key = ("rho_" + to_string(i + 1)); //Convention rho_i
        Rho[i] = configFile.get<double>(key);

        if(Rho[i]!=0){
        key = ("lambda_" + to_string(i + 1)); //Convention lambda_i
        Lambda[i] = configFile.get<double>(key);
        }else{
          Lambda[i] = 0;
        }

        key = ("Cx_" + to_string(i + 1)); //Convention Cx_i
        Cx[i] = configFile.get<double>(key);

        if(Cx[i]!=0){
          key = ("S_" + to_string(i + 1)); //Convention S_i
          S[i] = configFile.get<double>(key);
        }else{
          S[i] = 0;
        }
      }

      // Ouverture du fichier de sortie
      outputFile = new ofstream(configFile.get<string>("output").c_str());
      outputFile->precision(15);
  };

  virtual ~Exercice4()
  {
    outputFile->close();
    delete outputFile;
  };

  void run()
  {
    t = 0.;
    last = 0;
    printOut(true);

    while( t < tFin-0.5*dt )
    {
      step();
      t += dt;
      printOut(false);
    }
    printOut(true);
  };

};

class Ex4Fixed : public Exercice4
{
public:
  Ex4Fixed(ConfigFile configFile) : Exercice4(configFile) {}

  void affiche(){
    *outputFile << t << " ";
    for (auto &el : Y)
    {
      *outputFile << el << " ";
    }
    *outputFile << energyTot() << " ";
    for (size_t i(0); i < N; ++i)
    {
      *outputFile << norm(a(i,Y)) << " ";
    }
    *outputFile<< endl;
  }

  void step(){
    Y=RK4(Y,t,dt);
  }
};

class Ex4Adapt : public Exercice4
{
public:
  Ex4Adapt(ConfigFile configFile) : Exercice4(configFile) {
    e = configFile.get<double>("e");
    c = 0;
  }

  double e;

  dArray Y1;
  dArray Y2;
  dArray Y_;
  double d;
  bool c;

      void step()
  {
    AdaptDt();
  }

  void affiche()
  {
    *outputFile << t << " ";
    for (auto &el : Y)
    {
      *outputFile << el << " ";
    }
    *outputFile << energyTot() << " ";
    for (size_t i(0); i < N; ++i)
    {
      *outputFile << norm(a(i,Y)) << " ";
    }
    *outputFile << dt << " " << endl;
  }

  void AdaptDt(){
    dArray Y1(RK4(Y, t, dt));
    dArray Y2(RK4(Y, t, dt*0.5));
    dArray Y_(RK4(Y2, t+dt*0.5, dt*0.5));

    Y2 = Y_ - Y1;
    Y2 = abs(Y2);
    d = Y2.max();

    if (d >= e){
      dt *= 0.98 * pow((e / d), 1.0 / 5.0);
      //++c;
      AdaptDt();
    }else {
      dt *= pow((e/d),1.0/5.0);
      dt = min(dt, tFin-dt);
      Y = Y_;

    }
  }
};

int main(int argc, char *argv[])
{
  string inputPath("configuration.in"); // Fichier d'input par defaut
  if (argc > 1)                         // Fichier d'input specifie par l'utilisateur 
    inputPath = argv[1];

  ConfigFile configFile(inputPath); // Les parametres sont lus et stockes dans une "map" de strings.

  for (int i(2); i < argc; ++i)
    configFile.process(argv[i]);

  string dtType(configFile.get<string>("schema"));

  Exercice4 *ex4;
  if (dtType == "Adaptive" || dtType == "A")
  {
    ex4 = new Ex4Adapt(configFile);
  }
  else if (dtType == "Fixed" || dtType == "F")
  {
    ex4 = new Ex4Fixed(configFile);
  }
  else
  {
    cerr << "Schema inconnu" << endl;
    return -1;
  }

  ex4->run();

  delete ex4;
  cout << "Fin de la simulation." << endl;
  return 0;
}
