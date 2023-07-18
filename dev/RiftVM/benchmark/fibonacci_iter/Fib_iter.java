import java.util.Scanner;

public class Fib_iter {
	public static void main(String[] args) {
		Scanner in = new Scanner(System.in);
		
		int n = in.nextInt();
		int mod = in.nextInt();
		long a = 0;
		long b = 1;
		for(int i = 0; i < n; i++){
			long c = (a + b) % mod;
			a = b;
			b = c;
		}
		
		System.out.println(a);
	}
}