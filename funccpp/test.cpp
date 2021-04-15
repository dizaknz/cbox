#include <iostream>

template<int i, int j>
struct SumTest {
  int _i = i;
  int _j = j;
  int sumFn() {
      return [this](){
          return this->_i + this->_j;
      }();
  }
};

int main(void) {
    SumTest<100,200> s;
    int sum = s.sumFn();

    std::cout << sum << "\n";

    return 0;
}
