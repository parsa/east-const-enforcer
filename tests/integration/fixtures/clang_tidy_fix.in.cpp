namespace sample {

const int Answer = 42;

int compute();

struct Numbers {
  const int head;
  const int tail;
};

int useValues() {
  const int first = Answer;
  const int second = compute();
  const int total = first + second;
  const Numbers values{first, second};
  return total + values.head + values.tail;
}

}  // namespace sample
