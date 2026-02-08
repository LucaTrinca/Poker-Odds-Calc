# Poker Odds Calculator
## A high-performance, serverless poker equity engine running directly in the browser.
### Author: Luca Trinca

## Overview
This project is a Texas Hold'em Equity Calculator that solves for Win, Tie, and Equity percentages for up to 8 players.

Unlike traditional calculators that rely on server-side processing, this tool runs a custom C engine compiled to WebAssembly. This allows for near-native performance directly within the client's browser, eliminating server latency and operational costs.

## Engineering Decisions

The core odds calculation logic is implemented in low-level C for performance, while the user interface is handled by lightweight JavaScript.
### The Engine: 
Optimized C (poker_engine.c)C was selected for the backend to handle the combinatorial explosion inherent in poker equity calculations. To determine equity exactly, millions of hand combinations must be evaluated in milliseconds. JavaScript overhead is generally too high for this level of bitwise manipulation.
### Core Algorithms:
Prime Number Mapping: Every card rank is assigned a prime number (2, 3, 5... 41). This allows the program to multiply the prime values of a 5-card hand to generate a unique product ID for every possible hand configuration.Hash Lookup: The engine uses this unique product ID to look up hand strength in a pre-computed hash table (approx. 1MB). This reduces hand evaluation to an O(1) operation.Bitwise Flush Check: Suits are encoded as bits. The engine checks for flushes using a single bitwise AND operation (&), avoiding expensive loops or string comparisons.
### Recursive Solver:
A Depth-First Search (DFS) algorithm iterates through every possible remaining card in the deck to determine the exact winner for every board state.

### WebAssembly Implementation
To achieve a serverless architecture, the C code is compiled to a .wasm binary using Emscripten. This allows the heavy computational logic to execute on the client's CPU, providing the speed of C with the accessibility of a web app.3. User InterfaceThe UI is built using Vanilla JavaScript and HTML. Heavy frameworks like React or Vue were deliberately avoided to keep the project lightweight, dependency-free, and easy to deploy on any static host.

## Why Exact Calculation?
A Monte Carlo simulation typically requires approximately 10^6 iterations to achieve a standard error of <0.1%.The total state space of a pre-flop heads-up Hold'em hand is relatively small in computational terms:

binom{48}{5} = 1,712,304 combinations

Because the computational cost of an exact solution is nearly identical to that of a high-precision simulation, this project utilizes a deterministic Exhaustive Search algorithm. This guarantees 0.00% error with no significant performance penalty compared to a stochastic approach.
