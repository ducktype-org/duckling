import java.util.Scanner;

public class Fib_rec {
	private static int m = 8388449;
	
	private static int fib(int n) {
		if (n <= 1) {
			return n;
		}
		return (fib(n - 1) + fib(n - 2)) % m;
	}
	public static void main(String[] args) {
		Scanner in = new Scanner(System.in);
		
		int n = in.nextInt();		
		System.out.println(fib(n));
	}
}