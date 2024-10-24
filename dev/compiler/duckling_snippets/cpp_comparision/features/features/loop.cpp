bool flag = false;

for (int i = 0; i < 1000; i++) {
  for (int j = 0; j < 1000; j++) {
    if (i + j == 123) {
        flag = true;
        break;
    }
  }
  if (flag) break;
}
