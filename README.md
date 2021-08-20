# OutGuess

#### outguess - universal steganographic tool

## What is OutGuess?

Outguess is a universal steganographic tool that allows the insertion of hidden
information into the redundant bits of data sources. The nature of the data
source is irrelevant to the core of outguess. The program relies on data
specific handlers that will extract redundant bits and write them back after
modification. Currently only the PPM (Portable Pixel Map), PNM (Portable  Any
Map), and JPEG image formats are supported, although outguess could use any
kind of data, as long as a handler were provided.

Steganography is the art and science of hiding that communication is happening.
Classical steganography systems depend on keeping the encoding system secret,
but modern steganography are detectable only if secret information is known,
e.g. a secret key. Because of their invasive nature steganography systems leave
detectable traces within a medium's characteristics. This allows an
eavesdropper to detect media that has been modified, revealing that secret
communication is taking place. Although the secrecy of the information is not
degraded, its hidden nature is revealed, defeating the main purpose of
Steganography.

For JPEG images, OutGuess preserves statistics based on frequency counts. As a
result, no known statistical test is able to detect the presence of
steganographic content. Before embedding data into an image, the OutGuess
system can determine the maximum message size that can be hidden while still
being able to maintain statistics based on frequency counts.

OutGuess uses a generic iterator object to select which bits in the data should
be modified. A seed can be used to modify the behavior of the iterator. It is
embedded in the data along with the rest of the message. By altering the seed,
OutGuess tries to find a sequence of bits that minimizes the number of changes
in the data that have to be made.

A sample output from OutGuess is as follows:

```
Reading dscf0001.jpg....
JPEG compression quality set to 75
Extracting usable bits: 40059 bits
Correctable message size: 21194 bits, 52.91%
Encoded 'snark.bz2': 14712 bits, 1839 bytes
Finding best embedding...
    0:  7467(50.6%)[50.8%], bias  8137(1.09), saved:   -13, total: 18.64%
    1:  7311(49.6%)[49.7%], bias  8079(1.11), saved:     5, total: 18.25%
    4:  7250(49.2%)[49.3%], bias  7906(1.09), saved:    13, total: 18.10%
   59:  7225(49.0%)[49.1%], bias  7889(1.09), saved:    16, total: 18.04%
59, 7225: Embedding data: 14712 in 40059
Bits embedded: 14744, changed: 7225(49.0%)[49.1%], bias: 7889, tot: 40032, skip: 25288
Foiling statistics: corrections: 2590, failed: 1, offset: 122.585494 +- 239.664983
Total bits changed: 15114 (change 7225 + bias 7889)
Storing bitmap into data...
Writing foil/dscf0001.jpg....
```

The simple example script `seek_script` uses OutGuess to select an image that
fits the data we want to hide the best, yielding the lowest number of changed
bits. Because we do not care about the actual content of the cover data we
send, this is a very viable approach.

Additionally, OutGuess allows to hide multiple messages in the data. Thus, it
also provides plausible deniability. It keeps track of the bits that have been
modified previously and locks them. A `(23,12,7)` Golay code is used for error
correction to tolerate collisions on locked bits. Artificial errors are
introduced to avoid modifying bits that have a high bias.

Currently OutGuess can insert only two different messages. This is an
experimental feature.

## Help this project ##

OutGuess needs your help. **If you are a programmer** and want to help a nice
project, this is your opportunity.

The original OutGuess went unmaintained; the source of the last version, 0.2,
was [imported from Debian](https://snapshot.debian.org/package/outguess/) or other
repositories of the Internet. After, patches from Debian and elsewhere were
applied to create the 0.2.1 release. The details of each release are registered
in the [ChangeLog](ChangeLog) file. Now, OutGuess is maintained by volunteers
under [Resurrecting Open Source
Projects](https://github.com/resurrecting-open-source-projects).

If you are interested in helping OutGuess, read the [CONTRIBUTING.md](CONTRIBUTING.md) file.

## Building

### Prepare the `jpeg-6b-steg` library

To do so, you need to choose (and potentially edit) an appropriate `jconfig.h`
file. To get an idea which one you might want, have a look at their header
comments.

You might do so like this (POSIX only):

```
head -n 1 src/jpeg-6b-steg/jconfig.*
```

The default one is `jconfig.cfg`. You may use it like this:

```
cd jpeg-6b-steg
ln -s jconfig.cfg jconfig.h
cd ..
```

However, in OutGuess 0.3 or newer, there is the option `--with-generic-jconfig`
that will use `jconfig.cfg` automatically. See the *Build and install OutGuess*
section below.

### Build and install OutGuess

OutGuess has only been tested on OpenBSD, Linux, Solaris and AIX.

If you manually edited `jconfig.h`, you must use the following command
sequence:

```
./autogen.sh
./configure
make
make install
```

Otherwise, if you prefer to use `jconfig.cfg` content as default for
`jconfig.h`, without a manual action, you can use the following sequence:

```
./autogen.sh
./configure --with-generic-jconfig
make
make install
```

## Embedded modified JPEG library

OutGuess needs a modified version of the JPEG library. Currently, the original
lib (without changes) is available at https://www.ijg.org/files/. The tarball
name for version 6b is `jpegsrc.v6b.tar.gz` (or `jpegsr6b.zip`).

There is a complete document about the JPEG in
`src/jpeg-<version>-steg/install.doc` in OutGuess source code (this is plain
text, not a traditional
*.doc*).

The .diff file used to modify the original JPEG library is available at `/doc`
in OutGuess source code.

## Acknowledgments

OutGuess uses code from the following projects.
Attributions can also be found in the sources.

* Markus Kuhn's Stirmark software,
  see [doc/STIRMARK-README](STIRMARK-README)
* the Independent JPEG Group's JPEG software,
  see [src/jpeg-6b-steg/README](src/jpeg-6b-steg/README)
* the Arc4 random number generator for OpenBSD, (C) 1996 by
  David Mazieres <dm@lcs.mit.edu>
* free MD5 code by Colin Plumb

For determining the redundant bits out of a JPEG image,
the `jpeg-jsteg-v4` patches by Derek Upham <upham@cs.ubc.ca> were helpful.

Thanks to:

* Dug Song <dugsong@monkey.org> for helping with the original configure file,
* Andrew Reiter <andrewr@rot26.net> for testing on Solaris.

## Author ##

OutGuess was originally developed by Niels Provos <provos@citi.umich.edu>,
under the BSD software license. It is completely free for any use including
commercial.

Currently, source code is maintained by volunteers. Newer versions are
available at https://github.com/resurrecting-open-source-projects/outguess
