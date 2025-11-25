int const GlobalValue = 5;

struct Widget {
  int const member;
};

int helper();

int main() {
  int const local = GlobalValue;
  int const *ptr = &local;
  return *ptr + Widget{helper()}.member;
}
