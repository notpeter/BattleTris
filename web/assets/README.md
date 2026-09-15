# Original artwork

`gimp.png` is a lossless PNG conversion of `usr/src/art/btgimp2.ppm`, the
original game's default (non-r-rated) Gimp tile. Its pixels are unchanged.
Canvas renders it without smoothing. The original asset remains in place for
the Motif build.

To regenerate with ImageMagick:

```sh
magick usr/src/art/btgimp2.ppm web/assets/gimp.png
```
