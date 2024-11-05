# PlentyChess

PlentyChess is a very strong UCI chess engine using an efficiently updated neural network (NNUE) for evaluation.

The network is trained on 7.2bn self-generated positions of standard chess and Double Fischer Random Chess (DFRC).

PlentyChess 2.1.0 consistently ranks within the top 10 on various chess engine rating lists:
- 9th on [SPCC](https://www.sp-cc.de/), 3672 Elo
- 7th on [Ipman Chess](https://ipmanchess.yolasite.com/r9-7945hx.php), 3519 Elo (excluding duplicate engines)
- 8th on [CEGT](http://www.cegt.net/5Plus3Rating/Purelist/rangliste.html) with pondering, 3585 Elo
- 6th on [CCRL](https://computerchess.org.uk/ccrl/4040/), 3610 Elo
(Last updated: 05th November 2024)

Additionally, it plays live in the [Chess.com Computer Championship](https://www.chess.com/computer-chess-championship) and the [Top Chess Engine Championship](https://tcec-chess.com/).

## Building on Windows
For the build to work on Windows, you need to install MinGW as well as the required packages (make, g++, pthread, etc.).
Then you can use the makefile as you would on a unix system.
