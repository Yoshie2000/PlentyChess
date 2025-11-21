# PlentyChess

PlentyChess is a very strong UCI chess engine using an efficiently updated neural network (NNUE) for evaluation.

The innovative threat-input network is trained on 15+ billion self-generated chess positions (both standard and fischer random chess), using self-distillation techniques for part of the training, and [bullet](https://github.com/jw1912/bullet) as the training library.

PlentyChess consistently ranks within the top 3 on various chess engine rating lists:
- 3rd on [SPCC](https://www.sp-cc.de/), 3789 Elo
- 3rd on [Ipman Chess](https://ipmanchess.yolasite.com/r9-7945hx.php), 3597 Elo (excluding duplicate engines)
- 3rd on [CEGT](http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html), 3603 Elo
- 2nd on [CCRL](https://computerchess.org.uk/ccrl/4040/), 3642 Elo
- 3rd on [MCERL](https://www.chessengeria.eu/mcerl) (Mac only), 3785 Elo (excluding duplicate engines)
- 3rd on [e4e6](https://e4e6.com/324/) (Chess324), 3803 Elo

(Last updated: 4th November 2025)

Additionally, it plays live in the [Chess.com Computer Championship](https://www.chess.com/computer-chess-championship) and the [Top Chess Engine Championship](https://tcec-chess.com/).

# Credits
A big thank you goes out to:
- My friends on [furybench](https://chess.aronpetkovski.com), the testing instance used to develop PlentyChess and other strong engines
- The [bullet](https://github.com/jw1912/bullet) NNUE trainer
- noobpwnftw, Styxdoto, Viren, Jay Honnold, swedishchef and Andrew Grant for donating hardware time
- [fathom](https://github.com/jdart1/Fathom), which I use to probe tablebases
- [incbin](https://github.com/graphitemaster/incbin), which I use to embed neural network files into my binaries

# How to use

I highly recommend downloading the latest release from [the downloads page](https://github.com/Yoshie2000/PlentyChess/releases).

PlentyChess implements the Universal Chess Interface (UCI) protocol which is supported by basically all chess GUIs. Examples of these include [Nibbler](https://github.com/rooklift/nibbler), [En Croissant](https://encroissant.org/) and [Banksia](https://banksiagui.com/).

If you want to build a development version, the following instructions might help you.

## Building yourself

Running `make` (or `make -j`) is the easiest way to build a PlentyChess binary called `engine`, with the network being downloaded and embedded automatically.

This standard build contains debug symbols and is not profiled. For the fastest builds, compile with `make profile-build EXE=PlentyChess -j`.

## Building for a specific architecture

You can pass an `arch` argument to the Makefile to compile a binary for a specific architecture (e.g. `make -j arch=bmi2`). The following architecture / OS combinations are currently supported:

| Architecture | Windows | Linux | MacOS | Android |
| --- | --- | --- | --- | --- |
| **generic** | ✅ | ✅ | ❌ | ❌ |
| **ssse3** | ✅ | ✅ | ❌ | ❌ |
| **fma** | ✅ | ✅ | ❌ | ❌ |
| **avx** | ✅ | ✅ | ❌ | ❌ |
| **bmi2** | ✅ | ✅ | ❌ | ❌ |
| **avx512** | ✅ | ✅ | ❌ | ❌ |
| **avx512vnni** | ✅ | ✅ | ❌ | ❌ |
| **android** (neon) | ❌ | ❌ | ❌ | ✅ |
| **arm64** (neon) | ❌ | ✅ | ✅ | ❌ |

## Building on Windows

Building Windows binaries requires both a `make` installation (e.g. from MinGW), as well as an LLVM installation for the `clang` toolchain.

For the fastest builds (non-profiled as well as profiled), use the native [LLVM](https://releases.llvm.org/download.html) distribution, instead of the one contained in MinGW/msys2.

Profiled builds do not currently work with compilers installed via MinGW/msys2.

## Building for Android

Building an Android binary requires a linux device with NDK installed.

The Makefile will automatically use the NDK compiler when invoked with `arch=android`.