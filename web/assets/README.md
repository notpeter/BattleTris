# Original artwork

`gimp.png` is a lossless PNG conversion of `usr/src/art/btgimp2.ppm`, the
original game's default (non-r-rated) Gimp tile. Its pixels are unchanged.
Canvas renders it without smoothing. The original asset remains in place for
the Motif build.

To regenerate with ImageMagick:

```sh
magick usr/src/art/btgimp2.ppm web/assets/gimp.png
```

`startup.png` is the original `usr/src/art/btstartup2.png`, copied unchanged
for the browser welcome screen.

`bazaar.png` is a lossless conversion of `usr/src/art/btbazaar.ppm`:

```sh
magick usr/src/art/btbazaar.ppm web/assets/bazaar.png
```

`shield.png` uses the native About artwork, with its gray background made transparent:

```sh
magick usr/src/art/btbiff1.ppm -transparent '#cccccc' web/assets/shield.png
```
