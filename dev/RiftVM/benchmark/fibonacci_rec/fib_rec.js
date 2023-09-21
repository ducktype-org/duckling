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

const m = 8388449;

function fib(n) {
    if (n <= 1) {
        return n;
    }
    return (fib(n - 1) + fib(n - 2)) % m;
}

function main() {
    const x = readline();
    console.log(fib(x));
}
