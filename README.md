# PlentyChess

PlentyChess is a very strong UCI chess engine using an efficiently updated neural network (NNUE) for evaluation.

The network is trained on 4.8 billion self-generated standard chess positions, using self-distillation techniques for part of the training, and [bullet](https://github.com/jw1912/bullet) as the training library.

PlentyChess consistently ranks within the top 5 on various chess engine rating lists:
- 5th on [SPCC](https://www.sp-cc.de/), 3722 Elo
- 4th on [Ipman Chess](https://ipmanchess.yolasite.com/r9-7945hx.php), 3578 Elo (excluding duplicate engines)
- 5th on [CEGT](http://www.cegt.net/5Plus3Rating/Purelist/rangliste.html) with pondering, 3613 Elo
- 5th on [CCRL](https://computerchess.org.uk/ccrl/4040/), 3618 Elo
- 3rd on [MCERL](https://www.chessengeria.eu/mcerl) (Mac only), 3785 Elo (excluding duplicate engines)
- 4th on [e4e6](https://e4e6.com/324/) (Chess324), 3753 Elo

(Last updated: 27th January 2025)

Additionally, it plays live in the [Chess.com Computer Championship](https://www.chess.com/computer-chess-championship) and the [Top Chess Engine Championship](https://tcec-chess.com/).

# Credits
A big thanks goes out to:
- My friends on [furybench](https://chess.aronpetkovski.com), the testing instance used to develop PlentyChess and other strong engines
- The [bullet](https://github.com/jw1912/bullet) NNUE trainer
- noobpwnftw, Viren, Styxdoto and Andrew Grant for donating hardware time
- [fathom](https://github.com/jdart1/Fathom), which I use to probe tablebases
- [incbin](https://github.com/graphitemaster/incbin), which I use to embed neural network files into my binaries

# How to use

I highly recommend downloading the latest release from [the downloads page](https://github.com/Yoshie2000/PlentyChess/releases).

PlentyChess implements the Universal Chess Interface (UCI) protocol which is supported by basically all chess GUIs. Examples of these include [Nibbler](https://github.com/rooklift/nibbler), [En Croissant](https://encroissant.org/) and [Banksia](https://banksiagui.com/).

If you want to build a development version, the following instructions might help you.

## Building yourself

Running `make` (or `make -j`) is the easiest way to build a PlentyChess binary called `engine`, with the network being downloaded and embedded automatically.

This standard build contains debug symbols and is not profiled. For the fastest builds, compile with `make profile-build EXE=PlentyChess -j`.

## Building on Windows
For the build to work on Windows, you need to install MinGW as well as the required packages (make, g++, pthread, etc.).
Then you can use the makefile as you would on a unix system.
