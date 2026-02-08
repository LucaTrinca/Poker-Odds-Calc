Poker Odds Calculator
A high-performance, serverless poker equity engine running directly in the browser.

Built by Luca Trinca

Overview
This project is a Texas Hold'em Equity Calculator that solves for Win, Tie, and Equity percentages for up to 8 players.

This project runs a custom C engine compiled to WebAssembly

Architecture & Engineering Decisions
The odds calculations is done in low-level C, while the UI is handled by JavaScript.

1. The Engine: Optimized C (poker_engine.c)
I chose C for the backend because poker equity calculation is a combinatorial explosion problem. To calculate equity exactly, millions of hand combinations must be evaluated. JavaScript is a little slow for this level of bitwise manipulation.

Prime Number Mapping: Every card rank is assigned a prime number (2, 3, 5... 41). This allows the program to multiply the prime values of a 5-card hand to get a unique ID for every hand.

Hash Lookup: using this unique product ID to look up hand strength in a pre-computed hash table (1MB size). This drastically decreases hand evaluation time.

Bitwise Flush Check: Suits are encoded as bits. I check for flushes using a bitwise AND operation (&), avoiding loops or string comparisons.

Recursive Solver: The engine uses a Depth-First Search (DFS) algorithm to iterate through every possible remaining card in the deck to determine the exact winner.

Why Exact Calculation?
A Monte Carlo simulation requires approx. $10^6$ iterations to achieve a standard error $<0.1\%$. 
Since the total state space of a pre-flop heads-up hold'em hand is only $\binom{48}{5} \approx 1.7 \times 10^6$ combinations,
the computational cost of an exact solution is nearly identical to a high-precision simulation. 
Therefore, I utilize a deterministic Exhaustive Search algorithm to guarantee 0.00% error with no performance penalty

2. Using WebAssembly (Emscripten)
Instead of hosting the C code on a server (which costs money and adds latency), I compiled it to a .wasm binary using Emscripten.

3. The UI: Javascript and HTML
! deliberately avoided heavy frameworks like React or Vue for this specific iteration to keep the project lightweight and easy to deploy


📂 Project Structure
Plaintext
/
├── index.html        # The Frontend. Handles UI logic, Wasm loading, and display.
├── poker_engine.c    # The Source. The C logic for hand evaluation and solving.
├── poker.js          # The Glue. Emscripten-generated file to load Wasm.
├── poker.wasm        # The Binary. The compiled C code that runs in the browser.
└── README.md         # Documentation.
