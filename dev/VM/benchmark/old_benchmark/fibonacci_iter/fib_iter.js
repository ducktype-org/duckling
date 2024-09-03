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

function main() {
	const x = readline();
	const m = readline();
	let a = 0;
	let b = 1;
	for (let i = 0; i < x; i++) {
		let c = (a + b) % m;
		a = b;
		b = c;
	}
	console.log(a);
}
