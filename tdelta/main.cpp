#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

int main ()
{
  using namespace std::chrono;

  int offset = 31;

  auto t1 = steady_clock::now();

  float r,i,R,I,b,n;

  for(i=-1;i<1;i+=.06,std::cout << "" << std::endl)
  {
      for(r=-2;I=i,(R=r)<1; r+=.03,std::cout << char(n+offset))
      {
          for(n=0;b=I*I,26>n++&&R*R+b<4;I=2*R*I+i,R=R*R-b+r);
      }
  }
  std::cout << std::endl;

  auto delta = duration<double, std::milli>(steady_clock::now() - t1).count();

  std::cout << "Took " << delta << " ms" << std::endl;

  return 0;
}
