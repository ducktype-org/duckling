'use strict';

process.stdin.resume();
process.stdin.setEncoding('utf-8');

let inputString = '';
let currentLine = 0;

process.stdin.on('data', inputStdin => {
	inputString += inputStdin;
});

process.stdin.on('end', _ => {
	inputString = inputString.trim().split('\n').map(string => {
		return string.trim();
	});

	main();
});

function readline() {
	return inputString[currentLine++];
}

function collatz(n) {
	let j = 0;
	while (n != 1) {
		if (n % 2 == 0) {
			n /= 2;
		} else {
			n = n * 3 + 1;
		}
		j = Math.max(j, n);
	}
	return j;
}

function main() {
	const x = readline();
	let s = 0;
	for (let i = 1; i < x; i++) {
		s = Math.max(s, collatz(i));
	}
	console.log(s);
}
