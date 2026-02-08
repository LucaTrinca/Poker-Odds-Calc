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

"worst-case" scenario for computation: Heads-up, Pre-flop.Deck: 52 cards.Known Cards: 4 (2 players $\times$ 2 cards).Unknown Cards: 48.Board Cards to Come: 5.The number of possible future board states is a simple combination:$$C(n, k) = \binom{48}{5} = \frac{48!}{5!(48-5)!} = 1,712,304 \text{ outcomes}$$

2. The Bridge: WebAssembly (Emscripten)
Instead of hosting the C code on a server (which costs money and adds latency), we compiled it to a .wasm binary using Emscripten.

Zero Latency: The calculation happens on the user's CPU, not a cloud server.

Memory Management: The JavaScript frontend manually allocates memory in the Wasm heap (_malloc) to pass hand strings and pointers, ensuring strict type safety between JS and C.

Portability: The result is a static poker.wasm file that can be hosted anywhere (GitHub Pages, S3, Netlify) for free.

3. The UI: "Zero-Build" Vanilla Stack
We deliberately avoided heavy frameworks like React or Vue for this specific iteration to keep the project lightweight and "instantly deployable."

Tailwind CSS (CDN): Used for rapid, responsive styling without a complex Node.js build chain.

Responsive Design: The layout automatically shifts from a row-based layout (Desktop) to a column-based layout (Mobile) with a persistent bottom card picker.

Canvas-Free Rendering: The poker table and cards are rendered using CSS geometry (Sine/Cosine positioning) rather than an HTML Canvas, keeping the DOM accessible and crisp on high-DPI displays.

📂 Project Structure
Plaintext
/
├── index.html        # The Frontend. Handles UI logic, Wasm loading, and display.
├── poker_engine.c    # The Source. The C logic for hand evaluation and solving.
├── poker.js          # The Glue. Emscripten-generated file to load Wasm.
├── poker.wasm        # The Binary. The compiled C code that runs in the browser.
└── README.md         # Documentation.
