import java.util.Scanner;

public class Collatz {
	private static long collatz(long n) {
		long j = 0;
		while (n != 1) {
			if (n % 2 == 0) {
				n /= 2;
			} else {
				n = n * 3 + 1;
			}
			if (n > j) {
				j = n;
			}
		}
		return j;
	}

	public static void main(String[] args) {
		Scanner in = new Scanner(System.in);

		int x = in.nextInt();
		long s = 0;
		for (int i = 1; i < x; i++) {
			long tmp = collatz(i);
			if (tmp > s) {
				s = tmp;
			}
		}
		System.out.println(s);
	}
}