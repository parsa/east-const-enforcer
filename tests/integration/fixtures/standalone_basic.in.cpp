const int GlobalValue = 5;

struct Widget {
  const int member;
};

int helper();

int main() {
  const int local = GlobalValue;
  const int *ptr = &local;
  return *ptr + Widget{helper()}.member;
}
