# PlentyChess

PlentyChess is a very strong UCI chess engine using an efficiently updated neural network (NNUE) for evaluation.

The network is trained on 7.2bn self-generated positions of standard chess and Double Fischer Random Chess (DFRC) using [bullet](https://github.com/jw1912/bullet).

PlentyChess 3.0.1 consistently ranks within the top 10 on various chess engine rating lists:
- 9th on [SPCC](https://www.sp-cc.de/), 3686 Elo
- 8th on [Ipman Chess](https://ipmanchess.yolasite.com/r9-7945hx.php), 3561 Elo (excluding duplicate engines)
- 5th on [CEGT](http://www.cegt.net/5Plus3Rating/Purelist/rangliste.html) with pondering, 3613 Elo
- 7th on [CCRL](https://computerchess.org.uk/ccrl/4040/), 3610 Elo

(Last updated: 28th December 2024)

Additionally, it plays live in the [Chess.com Computer Championship](https://www.chess.com/computer-chess-championship) and the [Top Chess Engine Championship](https://tcec-chess.com/).

# Credits
A big thanks goes out to:
- My friends on [furybench](https://chess.aronpetkovski.com), the testing instance used to develop PlentyChess
- The [bullet](https://github.com/jw1912/bullet) NNUE trainer
- noobpwnftw, Andrew Grant and Styxdoto for donating hardware time
- [fathom](https://github.com/jdart1/Fathom), which is used to probe tablebases
- [incbin](https://github.com/graphitemaster/incbin), which I use to embed neural network files into my binaries

## Building on Windows
For the build to work on Windows, you need to install MinGW as well as the required packages (make, g++, pthread, etc.).
Then you can use the makefile as you would on a unix system.
