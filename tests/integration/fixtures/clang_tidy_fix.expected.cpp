namespace sample {

int const Answer = 42;

int compute();

struct Numbers {
  int const head;
  int const tail;
};

int useValues() {
  int const first = Answer;
  int const second = compute();
  int const total = first + second;
  Numbers const values{first, second};
  return total + values.head + values.tail;
}

}  // namespace sample
